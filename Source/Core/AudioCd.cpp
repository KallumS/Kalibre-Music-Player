#include "AudioCd.h"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <cstdio>

namespace kp::cd
{

double Toc::trackSeconds (int i) const
{
    if (i < 0 || i >= count()) return 0;
    const long end = i + 1 < count() ? offsets[(size_t) i + 1] : leadOut;
    return (double) (end - offsets[(size_t) i]) / 75.0;
}

std::string sha1 (const std::string& in)
{
    std::uint32_t h[5] = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0 };
    std::string msg = in;
    const std::uint64_t bits = (std::uint64_t) in.size() * 8;
    msg += (char) 0x80;
    while (msg.size() % 64 != 56) msg += (char) 0;
    for (int k = 7; k >= 0; --k) msg += (char) ((bits >> (k * 8)) & 0xff);
    auto rol = [] (std::uint32_t v, int n) { return (v << n) | (v >> (32 - n)); };
    for (size_t chunk = 0; chunk < msg.size(); chunk += 64)
    {
        std::uint32_t w[80];
        for (int k = 0; k < 16; ++k)
            w[k] = ((std::uint32_t) (unsigned char) msg[chunk + (size_t) k * 4] << 24) | ((std::uint32_t) (unsigned char) msg[chunk + (size_t) k * 4 + 1] << 16)
                 | ((std::uint32_t) (unsigned char) msg[chunk + (size_t) k * 4 + 2] << 8) | (std::uint32_t) (unsigned char) msg[chunk + (size_t) k * 4 + 3];
        for (int k = 16; k < 80; ++k) w[k] = rol (w[k - 3] ^ w[k - 8] ^ w[k - 14] ^ w[k - 16], 1);
        std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int k = 0; k < 80; ++k)
        {
            std::uint32_t f, kk;
            if (k < 20) { f = (b & c) | (~b & d); kk = 0x5A827999; }
            else if (k < 40) { f = b ^ c ^ d; kk = 0x6ED9EBA1; }
            else if (k < 60) { f = (b & c) | (b & d) | (c & d); kk = 0x8F1BBCDC; }
            else { f = b ^ c ^ d; kk = 0xCA62C1D6; }
            const std::uint32_t t = rol (a, 5) + f + e + kk + w[k];
            e = d; d = c; c = rol (b, 30); b = a; a = t;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
    }
    std::string out;
    for (auto v : h) for (int k = 3; k >= 0; --k) out += (char) ((v >> (k * 8)) & 0xff);
    return out;
}

std::string discId (const Toc& toc)
{
    // MusicBrainz: SHA-1 of the first and last track (2 hex digits each), the
    // lead-out and 99 offsets (8 each, 0 for none), upper-case; then base64
    // with + / = written . _ -.
    char buf[16];
    std::string text;
    std::snprintf (buf, sizeof buf, "%02X", toc.firstTrack); text += buf;
    std::snprintf (buf, sizeof buf, "%02X", toc.lastTrack); text += buf;
    std::snprintf (buf, sizeof buf, "%08lX", toc.leadOut); text += buf;
    for (int k = 0; k < 99; ++k)
    {
        std::snprintf (buf, sizeof buf, "%08lX", k < toc.count() ? toc.offsets[(size_t) k] : 0L);
        text += buf;
    }
    const auto digest = sha1 (text);
    static const char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789._";
    std::string out;
    for (size_t k = 0; k < digest.size(); k += 3)
    {
        const std::uint32_t b0 = (unsigned char) digest[k];
        const std::uint32_t b1 = k + 1 < digest.size() ? (unsigned char) digest[k + 1] : 0;
        const std::uint32_t b2 = k + 2 < digest.size() ? (unsigned char) digest[k + 2] : 0;
        const std::uint32_t v = (b0 << 16) | (b1 << 8) | b2;
        out += alphabet[(v >> 18) & 63];
        out += alphabet[(v >> 12) & 63];
        out += k + 1 < digest.size() ? alphabet[(v >> 6) & 63] : '-';
        out += k + 2 < digest.size() ? alphabet[v & 63] : '-';
    }
    return out;
}

std::string lookupUrl (const Toc& toc)
{
    std::string url = "https://musicbrainz.org/ws/2/discid/" + discId (toc) + "?inc=recordings+artist-credits&fmt=json&toc="
                    + std::to_string (toc.firstTrack) + "+" + std::to_string (toc.lastTrack) + "+" + std::to_string (toc.leadOut);
    for (auto o : toc.offsets) url += "+" + std::to_string (o);
    return url;
}

namespace
{
using json = nlohmann::json;
std::string credit (const json& j)
{
    std::string out;
    if (auto it = j.find ("artist-credit"); it != j.end() && it->is_array())
        for (auto& c : *it)
        {
            out += c.value ("name", c.contains ("artist") ? c["artist"].value ("name", "") : "");
            out += c.value ("joinphrase", "");
        }
    return out;
}
} // namespace

std::vector<Album> parseLookup (const std::string& text, const std::string& id)
{
    std::vector<Album> out;
    auto j = json::parse (text, nullptr, false);
    if (j.is_discarded() || ! j.contains ("releases") || ! j["releases"].is_array()) return out;
    for (auto& r : j["releases"])
    {
        Album a;
        a.title = r.value ("title", "");
        a.artist = credit (r);
        a.date = r.value ("date", "");
        a.releaseId = r.value ("id", "");
        if (! r.contains ("media") || ! r["media"].is_array()) continue;
        const auto& media = r["media"];
        a.totalDiscs = (int) media.size();
        // The medium this disc is: the one listing its ID, or the first.
        const json* chosen = nullptr;
        int number = 1;
        for (auto& m : media)
        {
            if (m.contains ("discs") && m["discs"].is_array())
                for (auto& d : m["discs"])
                    if (d.value ("id", "") == id) { chosen = &m; number = m.value ("position", number); }
        }
        if (! chosen && ! media.empty()) { chosen = &media[0]; number = media[0].value ("position", 1); }
        if (! chosen) continue;
        a.discNumber = number;
        if (chosen->contains ("tracks") && (*chosen)["tracks"].is_array())
            for (auto& t : (*chosen)["tracks"])
            {
                std::string title = t.value ("title", "");
                if (title.empty() && t.contains ("recording")) title = t["recording"].value ("title", "");
                a.trackTitles.push_back (title);
                auto who = credit (t);
                if (who.empty() && t.contains ("recording")) who = credit (t["recording"]);
                a.trackArtists.push_back (who.empty() ? a.artist : who);
            }
        out.push_back (std::move (a));
    }
    return out;
}

Tags tagsFor (const Album& a, int index, int trackCount)
{
    Tags t;
    auto put = [&t] (const char* k, const std::string& v) { if (! v.empty()) t[k] = { v }; };
    put ("album", a.title);
    put ("album artist", a.artist);
    put ("date", a.date);
    put ("title", index < (int) a.trackTitles.size() ? a.trackTitles[(size_t) index] : "Track " + std::to_string (index + 1));
    put ("artist", index < (int) a.trackArtists.size() ? a.trackArtists[(size_t) index] : a.artist);
    put ("tracknumber", std::to_string (index + 1));
    put ("totaltracks", std::to_string (trackCount));
    if (a.totalDiscs > 1)
    {
        put ("discnumber", std::to_string (a.discNumber));
        put ("totaldiscs", std::to_string (a.totalDiscs));
    }
    put ("musicbrainz_albumid", a.releaseId);
    return t;
}

} // namespace kp::cd
