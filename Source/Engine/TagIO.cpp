#include "TagIO.h"
#include "Decoder.h"
#include "Sources.h"

#include <taglib/aifffile.h>
#include <taglib/apefile.h>
#include <taglib/asffile.h>
#include <taglib/audioproperties.h>
#include <taglib/dsffile.h>
#include <taglib/fileref.h>
#include <taglib/flacfile.h>
#include <taglib/mp4file.h>
#include <taglib/mpcfile.h>
#include <taglib/mpegfile.h>
#include <taglib/mpegproperties.h>
#include <taglib/oggflacfile.h>
#include <taglib/opusfile.h>
#include <taglib/speexfile.h>
#include <taglib/tbytevectorstream.h>
#include <taglib/tpropertymap.h>
#include <taglib/trueaudiofile.h>
#include <taglib/vorbisfile.h>
#include <taglib/wavfile.h>
#include <taglib/wavpackfile.h>

#include <map>

namespace kp::tagio
{

namespace
{
// TagLib's names (upper case) and the player's (lower case, foobar's).
const std::map<std::string, std::string>& toPlayer()
{
    static const std::map<std::string, std::string> m {
        { "ALBUMARTIST", "album artist" }, { "ALBUM ARTIST", "album artist" },
        { "TRACKTOTAL", "totaltracks" }, { "TOTALTRACKS", "totaltracks" },
        { "DISCTOTAL", "totaldiscs" }, { "TOTALDISCS", "totaldiscs" },
    };
    return m;
}

std::string playerName (const TagLib::String& key)
{
    const auto k = key.upper().to8Bit (true);
    auto it = toPlayer().find (k);
    return it != toPlayer().end() ? it->second : lower (k);
}

TagLib::String tl (const std::string& s) { return TagLib::String (s, TagLib::String::UTF8); }
std::string st (const TagLib::String& s) { return s.to8Bit (true); }

struct Opened
{
    std::unique_ptr<TagLib::ByteVectorStream> memory;   // for a file inside an archive
    std::unique_ptr<TagLib::FileRef> ref;
};

#if JUCE_WINDOWS
using NativeName = const wchar_t*;
#endif

bool openRef (const std::string& location, Opened& o, bool readOnly, juce::String& error)
{
    if (auto parts = location::split (location))
    {
        if (! readOnly) { error = "files inside archives can't be changed"; return false; }
        juce::MemoryBlock data;
        if (! sources::extract (juce::File (juce::String::fromUTF8 (parts->first.c_str())), parts->second, data, error)) return false;
        o.memory = std::make_unique<TagLib::ByteVectorStream> (TagLib::ByteVector ((const char*) data.getData(), (unsigned) data.getSize()));
        o.ref = std::make_unique<TagLib::FileRef> (o.memory.get(), true, TagLib::AudioProperties::Average);
    }
    else
    {
        const juce::File f (juce::String::fromUTF8 (location.c_str()));
        if (! f.existsAsFile()) { error = "the file isn't there"; return false; }
       #if JUCE_WINDOWS
        o.ref = std::make_unique<TagLib::FileRef> (f.getFullPathName().toWideCharPointer(), true, TagLib::AudioProperties::Average);
       #else
        o.ref = std::make_unique<TagLib::FileRef> (f.getFullPathName().toRawUTF8(), true, TagLib::AudioProperties::Average);
       #endif
    }
    if (o.ref->isNull() || ! o.ref->file() || ! o.ref->file()->isValid()) { error = "its tags couldn't be read"; return false; }
    return true;
}

std::string codecOf (TagLib::File* f, const std::string& ext)
{
    if (auto* m = dynamic_cast<TagLib::MPEG::File*> (f))
    {
        if (auto* p = m->audioProperties())
            return p->layer() == 1 ? "MP1" : p->layer() == 2 ? "MP2" : "MP3";
        return "MP3";
    }
    if (auto* m = dynamic_cast<TagLib::MP4::File*> (f))
    {
        if (auto* p = m->audioProperties()) return p->codec() == TagLib::MP4::Properties::ALAC ? "ALAC" : "AAC";
        return "AAC";
    }
    if (dynamic_cast<TagLib::FLAC::File*> (f) || dynamic_cast<TagLib::Ogg::FLAC::File*> (f)) return "FLAC";
    if (dynamic_cast<TagLib::Ogg::Vorbis::File*> (f)) return "Vorbis";
    if (dynamic_cast<TagLib::Ogg::Opus::File*> (f)) return "Opus";
    if (dynamic_cast<TagLib::Ogg::Speex::File*> (f)) return "Speex";
    if (dynamic_cast<TagLib::ASF::File*> (f)) return "WMA";
    if (dynamic_cast<TagLib::MPC::File*> (f)) return "Musepack";
    if (dynamic_cast<TagLib::WavPack::File*> (f)) return "WavPack";
    if (dynamic_cast<TagLib::APE::File*> (f)) return "Monkey's Audio";
    if (dynamic_cast<TagLib::TrueAudio::File*> (f)) return "TTA";
    if (dynamic_cast<TagLib::DSF::File*> (f)) return "DSD";
    if (dynamic_cast<TagLib::RIFF::WAV::File*> (f) || dynamic_cast<TagLib::RIFF::AIFF::File*> (f)) return "PCM";
    return upper (ext);
}

// Formats whose track number holds its total ("3/12"): ID3, MP4, ASF, RIFF.
bool totalsInNumber (TagLib::File* f)
{
    return dynamic_cast<TagLib::MPEG::File*> (f) || dynamic_cast<TagLib::MP4::File*> (f) || dynamic_cast<TagLib::ASF::File*> (f)
        || dynamic_cast<TagLib::RIFF::WAV::File*> (f) || dynamic_cast<TagLib::RIFF::AIFF::File*> (f);
}
} // namespace

bool read (const std::string& location, Track& t, juce::String& error)
{
    t.location = location;
    t.tags.clear();
    const auto file = sources::fileOf (location);
    t.fileSize = file.getSize();
    t.modified = file.getLastModificationTime().toMilliseconds();

    Opened o;
    juce::String tagError;
    if (openRef (location, o, true, tagError))
    {
        auto* f = o.ref->file();
        for (auto& [key, values] : f->properties())
        {
            auto name = playerName (key);
            auto& out = t.tags[name];
            for (auto& v : values) if (! v.isEmpty()) out.push_back (st (v));
            if (out.empty()) t.tags.erase (name);
        }
        // "3/12" kept as the number and the total, as foobar shows them.
        for (auto [number, total] : { std::pair { "tracknumber", "totaltracks" }, std::pair { "discnumber", "totaldiscs" } })
        {
            auto v = t.first (number);
            auto slash = v.find ('/');
            if (slash == std::string::npos) continue;
            t.tags[number] = { trim (v.substr (0, slash)) };
            if (! t.has (total) && ! trim (v.substr (slash + 1)).empty()) t.tags[total] = { trim (v.substr (slash + 1)) };
        }
        if (auto* p = f->audioProperties())
        {
            t.length = p->lengthInMilliseconds() / 1000.0;
            t.sampleRate = p->sampleRate();
            t.channels = p->channels();
            t.bitrate = p->bitrate();
        }
        t.codec = codecOf (f, location::extension (location));
        if (auto* fl = dynamic_cast<TagLib::FLAC::File*> (f); fl && fl->audioProperties()) t.bitsPerSample = fl->audioProperties()->bitsPerSample();
        else if (auto* w = dynamic_cast<TagLib::RIFF::WAV::File*> (f); w && w->audioProperties()) t.bitsPerSample = w->audioProperties()->bitsPerSample();
        else if (auto* a = dynamic_cast<TagLib::RIFF::AIFF::File*> (f); a && a->audioProperties()) t.bitsPerSample = a->audioProperties()->bitsPerSample();
        else if (auto* wv = dynamic_cast<TagLib::WavPack::File*> (f); wv && wv->audioProperties()) t.bitsPerSample = wv->audioProperties()->bitsPerSample();
        else if (auto* m4 = dynamic_cast<TagLib::MP4::File*> (f); m4 && m4->audioProperties() && m4->audioProperties()->codec() == TagLib::MP4::Properties::ALAC)
            t.bitsPerSample = m4->audioProperties()->bitsPerSample();
        if (t.length > 0 && t.sampleRate > 0) return true;
    }
    // TagLib doesn't know it (AU, SND, MKA, AC-3...): FFmpeg says what it is.
    auto in = sources::open (location, error);
    if (! in) return false;
    auto d = Decoder::open (std::move (in), juce::String::fromUTF8 (location::fileName (location).c_str()), error);
    if (! d) return false;
    const auto& info = d->info();
    t.length = info.length;
    t.sampleRate = info.sampleRate;
    t.channels = info.channels;
    t.bitrate = info.bitrate;
    t.bitsPerSample = info.bitsPerSample;
    t.codec = info.codec;
    return true;
}

bool canWrite (const std::string& location)
{
    if (location::isArchive (location)) return false;
    const juce::File f (juce::String::fromUTF8 (location.c_str()));
    return f.existsAsFile() && f.hasWriteAccess();
}

bool write (const std::string& location, const Tags& tags, juce::String& error)
{
    if (! canWrite (location)) { error = location::isArchive (location) ? "files inside archives can't be changed" : "the file can't be changed"; return false; }
    Opened o;
    if (! openRef (location, o, false, error)) return false;
    auto* f = o.ref->file();
    const bool combine = totalsInNumber (f);
    TagLib::PropertyMap map;
    auto get = [&tags] (const char* k) { auto it = tags.find (k); return it == tags.end() || it->second.empty() ? std::string() : it->second.front(); };
    for (auto& [name, values] : tags)
    {
        if (values.empty()) continue;
        std::string key = upper (name);
        if (name == "album artist") key = "ALBUMARTIST";
        if (name == "totaltracks") { if (combine) continue; key = "TRACKTOTAL"; }
        if (name == "totaldiscs") { if (combine) continue; key = "DISCTOTAL"; }
        TagLib::StringList list;
        for (auto& v : values) list.append (tl (v));
        if (combine && name == "tracknumber" && ! get ("totaltracks").empty()) list = TagLib::StringList (tl (values.front() + "/" + get ("totaltracks")));
        if (combine && name == "discnumber" && ! get ("totaldiscs").empty()) list = TagLib::StringList (tl (values.front() + "/" + get ("totaldiscs")));
        map.insert (tl (key), list);
    }
    auto refused = f->setProperties (map);
    if (! f->save()) { error = "the file couldn't be saved"; return false; }
    if (! refused.isEmpty())
    {
        juce::StringArray names;
        for (auto& [k, v] : refused) names.add (juce::String::fromUTF8 (st (k).c_str()));
        error = "this kind of file can't keep " + names.joinIntoString (", ");
    }
    return true;
}

juce::MemoryBlock embeddedCover (const std::string& location, juce::String& mime)
{
    Opened o;
    juce::String error;
    if (! openRef (location, o, true, error)) return {};
    auto pictures = o.ref->file()->complexProperties ("PICTURE");
    const TagLib::VariantMap* chosen = nullptr;
    for (auto& p : pictures)
    {
        if (! chosen) chosen = &p;
        if (p.value ("pictureType").toString() == "Front Cover") { chosen = &p; break; }
    }
    if (! chosen) return {};
    const auto data = chosen->value ("data").toByteVector();
    mime = juce::String::fromUTF8 (st (chosen->value ("mimeType").toString()).c_str());
    return juce::MemoryBlock (data.data(), data.size());
}

bool writeCover (const std::string& location, const juce::MemoryBlock& data, const juce::String& mime)
{
    if (! canWrite (location) || data.isEmpty()) return false;
    Opened o;
    juce::String error;
    if (! openRef (location, o, false, error)) return false;
    TagLib::VariantMap picture;
    picture["data"] = TagLib::ByteVector ((const char*) data.getData(), (unsigned) data.getSize());
    picture["mimeType"] = tl (mime.isNotEmpty() ? mime.toStdString() : std::string ("image/jpeg"));
    picture["pictureType"] = TagLib::String ("Front Cover");
    picture["description"] = TagLib::String ("");
    o.ref->file()->setComplexProperties ("PICTURE", { picture });
    return o.ref->file()->save();
}

juce::Image cover (const std::string& location)
{
    juce::String mime;
    auto data = embeddedCover (location, mime);
    if (! data.isEmpty())
        if (auto img = juce::ImageFileFormat::loadFrom (data.getData(), data.getSize()); img.isValid()) return img;
    // A picture beside it (beside the archive, for an entry).
    const auto dir = sources::fileOf (location).getParentDirectory();
    for (auto* name : { "cover", "folder", "front", "album", "albumart", "AlbumArtLarge", "AlbumArt" })
        for (auto* ext : { ".jpg", ".jpeg", ".png" })
        {
            auto f = dir.getChildFile (juce::String (name) + ext);
            if (! f.existsAsFile())
            {
                // The Finder's names aren't case-sensitive; Linux's are.
                auto found = dir.findChildFiles (juce::File::findFiles, false, juce::String (name) + ext, juce::File::FollowSymlinks::no);
                if (found.isEmpty()) continue;
                f = found.getFirst();
            }
            if (auto img = juce::ImageFileFormat::loadFrom (f); img.isValid()) return img;
        }
    return {};
}

} // namespace kp::tagio
