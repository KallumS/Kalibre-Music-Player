#include "CdRom.h"

#include <cstring>

#if JUCE_WINDOWS
 #include <windows.h>
 #include <winioctl.h>
 #include <ntddcdrm.h>
#elif JUCE_LINUX
 #include <fcntl.h>
 #include <linux/cdrom.h>
 #include <sys/ioctl.h>
 #include <unistd.h>
#endif

namespace kp::cdrom
{

std::string location (const std::string& device, int track) { return "cdda:" + device + "|" + std::to_string (track); }
bool isLocation (const std::string& l) { return l.rfind ("cdda:", 0) == 0; }

std::optional<std::pair<std::string, int>> split (const std::string& l)
{
    if (! isLocation (l)) return std::nullopt;
    const auto bar = l.find_last_of ('|');
    if (bar == std::string::npos) return std::nullopt;
    const int track = std::atoi (l.c_str() + bar + 1);
    if (track <= 0) return std::nullopt;
    return std::make_pair (l.substr (5, bar - 5), track);
}

Disc discFrom (int first, const std::vector<long>& starts, const std::vector<bool>& isData, long leadOut)
{
    Disc d;
    d.toc.firstTrack = first;
    // Audio tracks only; an enhanced CD's data session at the end isn't sound, and
    // MusicBrainz counts the audio's end as 11,400 sectors before the data starts.
    long audioEnd = leadOut;
    std::vector<long> audio;
    for (size_t k = 0; k < starts.size(); ++k)
    {
        if (k < isData.size() && isData[k]) { if (! audio.empty()) audioEnd = std::min (audioEnd, starts[k] - 11400); continue; }
        audio.push_back (starts[k]);
    }
    for (size_t k = 0; k < audio.size(); ++k)
    {
        const long end = k + 1 < audio.size() ? audio[k + 1] : audioEnd;
        d.firstLba.push_back (audio[k]);
        d.sectors.push_back (std::max (0L, end - audio[k]));
        d.toc.offsets.push_back (audio[k] + 150);
    }
    d.toc.lastTrack = first + (int) audio.size() - 1;
    d.toc.leadOut = audioEnd + 150;
    return d;
}

//==============================================================================
namespace
{
class TrackStream : public juce::InputStream
{
public:
    TrackStream (std::unique_ptr<SectorReader> r, long first, long count) : reader (std::move (r)), firstLba (first), sectors (count)
    {
        // A WAV header for 44.1 kHz, 16-bit, stereo: what a CD's sectors are.
        const auto dataBytes = (juce::uint32) (sectors * sectorBytes);
        juce::MemoryOutputStream h (header, false);
        h.write ("RIFF", 4); h.writeInt ((int) (36 + dataBytes)); h.write ("WAVE", 4);
        h.write ("fmt ", 4); h.writeInt (16); h.writeShort (1); h.writeShort (2); h.writeInt (44100); h.writeInt (44100 * 4); h.writeShort (4); h.writeShort (16);
        h.write ("data", 4); h.writeInt ((int) dataBytes);
    }

    juce::int64 getTotalLength() override { return (juce::int64) header.getSize() + (juce::int64) sectors * sectorBytes; }
    bool isExhausted() override { return position >= getTotalLength(); }
    juce::int64 getPosition() override { return position; }
    bool setPosition (juce::int64 p) override { position = juce::jlimit ((juce::int64) 0, getTotalLength(), p); return true; }

    int read (void* dest, int bytes) override
    {
        auto* out = static_cast<char*> (dest);
        int done = 0;
        while (done < bytes && position < getTotalLength())
        {
            const auto headerSize = (juce::int64) header.getSize();
            if (position < headerSize)
            {
                const int n = (int) std::min<juce::int64> (bytes - done, headerSize - position);
                std::memcpy (out + done, static_cast<const char*> (header.getData()) + position, (size_t) n);
                done += n;
                position += n;
                continue;
            }
            const auto at = position - headerSize;
            const long sector = (long) (at / sectorBytes);
            if (sector < cachedFrom || sector >= cachedFrom + cachedCount)
            {
                // A few sectors at a time: drives read more in one go happily, not too many.
                const int count = (int) std::min<long> (chunk, sectors - sector);
                cache.resize ((size_t) count * sectorBytes);
                if (! reader->read (firstLba + sector, count, cache.data()))
                {
                    // A scratch: silence for those sectors, and on.
                    std::fill (cache.begin(), cache.end(), (unsigned char) 0);
                }
                cachedFrom = sector;
                cachedCount = count;
            }
            const auto within = at - (juce::int64) cachedFrom * sectorBytes;
            const int n = (int) std::min<juce::int64> (bytes - done, (juce::int64) cachedCount * sectorBytes - within);
            std::memcpy (out + done, cache.data() + within, (size_t) n);
            done += n;
            position += n;
        }
        return done;
    }

private:
    static constexpr int chunk = 24;
    std::unique_ptr<SectorReader> reader;
    long firstLba, sectors;
    juce::MemoryBlock header;
    juce::int64 position = 0;
    std::vector<unsigned char> cache;
    long cachedFrom = -1;
    int cachedCount = 0;
};
} // namespace

std::unique_ptr<juce::InputStream> trackStream (std::unique_ptr<SectorReader> r, long first, long count)
{
    return std::make_unique<TrackStream> (std::move (r), first, count);
}

std::unique_ptr<juce::InputStream> openLocation (const std::string& l, juce::String& error)
{
    auto where = split (l);
    if (! where) { error = "it isn't a CD track"; return nullptr; }
    auto disc = readToc (where->first, error);
    if (! disc) return nullptr;
    const int index = where->second - disc->toc.firstTrack;
    if (index < 0 || index >= (int) disc->firstLba.size()) { error = "the disc has no track " + juce::String (where->second); return nullptr; }
    auto reader = open (where->first, error);
    if (! reader) return nullptr;
    return trackStream (std::move (reader), disc->firstLba[(size_t) index], disc->sectors[(size_t) index]);
}

//==============================================================================
#if JUCE_WINDOWS

namespace
{
HANDLE openHandle (const std::string& device)
{
    const auto p = juce::String ("\\\\.\\") + juce::String::fromUTF8 (device.c_str());
    return CreateFileW (p.toWideCharPointer(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
}

class WinReader : public SectorReader
{
public:
    explicit WinReader (HANDLE h) : handle (h) {}
    ~WinReader() override { CloseHandle (handle); }
    bool read (long lba, int count, unsigned char* out) override
    {
        RAW_READ_INFO info {};
        info.DiskOffset.QuadPart = (LONGLONG) lba * 2048;
        info.SectorCount = (ULONG) count;
        info.TrackMode = CDDA;
        DWORD got = 0;
        return DeviceIoControl (handle, IOCTL_CDROM_RAW_READ, &info, sizeof info, out, (DWORD) (count * sectorBytes), &got, nullptr) != 0
            && got == (DWORD) (count * sectorBytes);
    }

private:
    HANDLE handle;
};
} // namespace

std::vector<Drive> drives()
{
    std::vector<Drive> out;
    const DWORD mask = GetLogicalDrives();
    for (int k = 0; k < 26; ++k)
    {
        if (! (mask & (1u << k))) continue;
        const wchar_t root[] = { (wchar_t) (L'A' + k), L':', L'\\', 0 };
        if (GetDriveTypeW (root) != DRIVE_CDROM) continue;
        const std::string letter = std::string (1, (char) ('A' + k)) + ":";
        out.push_back ({ letter, "Drive " + letter });
    }
    return out;
}

std::unique_ptr<SectorReader> open (const std::string& device, juce::String& error)
{
    HANDLE h = openHandle (device);
    if (h == INVALID_HANDLE_VALUE) { error = "the drive couldn't be opened"; return nullptr; }
    return std::make_unique<WinReader> (h);
}

std::optional<Disc> readToc (const std::string& device, juce::String& error)
{
    HANDLE h = openHandle (device);
    if (h == INVALID_HANDLE_VALUE) { error = "the drive couldn't be opened"; return std::nullopt; }
    CDROM_TOC toc {};
    DWORD got = 0;
    const bool ok = DeviceIoControl (h, IOCTL_CDROM_READ_TOC, nullptr, 0, &toc, sizeof toc, &got, nullptr) != 0;
    CloseHandle (h);
    if (! ok) { error = "there's no audio CD in the drive"; return std::nullopt; }
    auto lbaOf = [] (const TRACK_DATA& t) { return (long) ((t.Address[1] * 60 + t.Address[2]) * 75 + t.Address[3]) - 150; };
    std::vector<long> starts;
    std::vector<bool> data;
    const int n = toc.LastTrack - toc.FirstTrack + 1;
    for (int k = 0; k < n; ++k)
    {
        starts.push_back (lbaOf (toc.TrackData[k]));
        data.push_back ((toc.TrackData[k].Control & 4) != 0);
    }
    auto disc = discFrom (toc.FirstTrack, starts, data, lbaOf (toc.TrackData[n]));
    if (disc.firstLba.empty()) { error = "the disc has no audio"; return std::nullopt; }
    return disc;
}

#elif JUCE_LINUX

namespace
{
class LinuxReader : public SectorReader
{
public:
    explicit LinuxReader (int f) : fd (f) {}
    ~LinuxReader() override { ::close (fd); }
    bool read (long lba, int count, unsigned char* out) override
    {
        cdrom_read_audio ra {};
        ra.addr.lba = (int) lba;
        ra.addr_format = CDROM_LBA;
        ra.nframes = count;
        ra.buf = out;
        return ioctl (fd, CDROMREADAUDIO, &ra) == 0;
    }

private:
    int fd;
};
} // namespace

std::vector<Drive> drives()
{
    std::vector<Drive> out;
    for (int k = 0; k < 8; ++k)
    {
        const std::string dev = "/dev/sr" + std::to_string (k);
        if (juce::File (dev).exists()) out.push_back ({ dev, dev });
    }
    return out;
}

std::unique_ptr<SectorReader> open (const std::string& device, juce::String& error)
{
    const int fd = ::open (device.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd < 0) { error = "the drive couldn't be opened (is the user in the cdrom group?)"; return nullptr; }
    return std::make_unique<LinuxReader> (fd);
}

std::optional<Disc> readToc (const std::string& device, juce::String& error)
{
    const int fd = ::open (device.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd < 0) { error = "the drive couldn't be opened"; return std::nullopt; }
    cdrom_tochdr hdr {};
    if (ioctl (fd, CDROMREADTOCHDR, &hdr) != 0) { ::close (fd); error = "there's no audio CD in the drive"; return std::nullopt; }
    std::vector<long> starts;
    std::vector<bool> data;
    long leadOut = 0;
    for (int t = hdr.cdth_trk0; t <= hdr.cdth_trk1 + 1; ++t)
    {
        cdrom_tocentry e {};
        e.cdte_track = (unsigned char) (t > hdr.cdth_trk1 ? CDROM_LEADOUT : t);
        e.cdte_format = CDROM_LBA;
        if (ioctl (fd, CDROMREADTOCENTRY, &e) != 0) { ::close (fd); error = "the disc's contents couldn't be read"; return std::nullopt; }
        if (t > hdr.cdth_trk1) leadOut = e.cdte_addr.lba;
        else { starts.push_back (e.cdte_addr.lba); data.push_back ((e.cdte_ctrl & CDROM_DATA_TRACK) != 0); }
    }
    ::close (fd);
    auto disc = discFrom (hdr.cdth_trk0, starts, data, leadOut);
    if (disc.firstLba.empty()) { error = "the disc has no audio"; return std::nullopt; }
    return disc;
}

#else   // The Mac reads CDs as files (CdDrive.cpp).

std::vector<Drive> drives() { return {}; }
std::unique_ptr<SectorReader> open (const std::string&, juce::String& error) { error = "not on this system"; return nullptr; }
std::optional<Disc> readToc (const std::string&, juce::String& error) { error = "not on this system"; return std::nullopt; }

#endif

} // namespace kp::cdrom
