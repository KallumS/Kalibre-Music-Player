#include "Sources.h"
#include "Track.h"
#include "CdRom.h"

#include <archive.h>
#include <archive_entry.h>

namespace kp::sources
{

namespace
{
struct Reader
{
    archive* a = archive_read_new();
    ~Reader() { archive_read_free (a); }
};

bool openArchive (Reader& r, const juce::File& file, juce::String& error)
{
    archive_read_support_filter_all (r.a);
    archive_read_support_format_all (r.a);
    // A .gz of one file: libarchive reads it as a "raw" archive of one entry.
    archive_read_support_format_raw (r.a);
   #if JUCE_WINDOWS
    const bool opened = archive_read_open_filename_w (r.a, file.getFullPathName().toWideCharPointer(), 64 * 1024) == ARCHIVE_OK;
   #else
    const bool opened = archive_read_open_filename (r.a, file.getFullPathName().toRawUTF8(), 64 * 1024) == ARCHIVE_OK;
   #endif
    if (! opened)
    {
        error = archive_error_string (r.a) ? juce::String (archive_error_string (r.a)) : juce::String ("it couldn't be opened");
        return false;
    }
    return true;
}

// A raw (gzip) archive's one entry is called "data": named after the archive, without ".gz".
std::string entryName (archive_entry* e, const juce::File& file)
{
    const char* p = archive_entry_pathname_utf8 (e);
    if (! p) p = archive_entry_pathname (e);
    std::string name = p ? p : "";
    if (name.empty() || name == "data")
    {
        auto n = file.getFileName();
        if (n.endsWithIgnoreCase (".gz")) n = n.dropLastCharacters (3);
        else if (n.endsWithIgnoreCase (".gzip")) n = n.dropLastCharacters (5);
        name = n.toStdString();
    }
    return name;
}
} // namespace

juce::File fileOf (const std::string& l) { return juce::File (juce::String::fromUTF8 (location::container (l).c_str())); }

std::vector<Entry> list (const juce::File& file, juce::String& error)
{
    std::vector<Entry> out;
    Reader r;
    if (! openArchive (r, file, error)) return out;
    archive_entry* e = nullptr;
    while (archive_read_next_header (r.a, &e) == ARCHIVE_OK)
    {
        if (archive_entry_filetype (e) != AE_IFREG && archive_entry_filetype (e) != 0) { archive_read_data_skip (r.a); continue; }
        auto name = entryName (e, file);
        if (isAudioExtension (location::extension (name)))
            out.push_back ({ name, archive_entry_size_is_set (e) ? (std::int64_t) archive_entry_size (e) : 0 });
        archive_read_data_skip (r.a);
    }
    return out;
}

bool extract (const juce::File& file, const std::string& entry, juce::MemoryBlock& out, juce::String& error)
{
    Reader r;
    if (! openArchive (r, file, error)) return false;
    archive_entry* e = nullptr;
    while (archive_read_next_header (r.a, &e) == ARCHIVE_OK)
    {
        if (entryName (e, file) != entry) { archive_read_data_skip (r.a); continue; }
        out.reset();
        char buffer[64 * 1024];
        for (;;)
        {
            const auto n = archive_read_data (r.a, buffer, sizeof buffer);
            if (n == 0) return true;
            if (n < 0)
            {
                error = archive_error_string (r.a) ? juce::String (archive_error_string (r.a)) : juce::String ("it couldn't be read");
                return false;
            }
            out.append (buffer, (size_t) n);
            if (out.getSize() > (size_t) 2 * 1024 * 1024 * 1024u) { error = "it's too big to play from inside an archive"; return false; }
        }
    }
    error = "it isn't in the archive any more";
    return false;
}

std::unique_ptr<juce::InputStream> open (const std::string& l, juce::String& error)
{
    if (cdrom::isLocation (l)) return cdrom::openLocation (l, error);
    if (auto parts = location::split (l))
    {
        juce::MemoryBlock data;
        const juce::File archiveFile (juce::String::fromUTF8 (parts->first.c_str()));
        if (! archiveFile.existsAsFile()) { error = "the archive isn't there"; return nullptr; }
        if (! extract (archiveFile, parts->second, data, error)) return nullptr;
        return std::make_unique<juce::MemoryInputStream> (std::move (data));
    }
    const juce::File f (juce::String::fromUTF8 (l.c_str()));
    if (! f.existsAsFile()) { error = "the file isn't there"; return nullptr; }
    auto in = std::make_unique<juce::FileInputStream> (f);
    if (in->failedToOpen()) { error = "the file couldn't be opened"; return nullptr; }
    return in;
}

} // namespace kp::sources
