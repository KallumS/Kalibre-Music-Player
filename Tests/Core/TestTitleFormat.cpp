#include "Check.h"
#include "TitleFormat.h"

using namespace kp;

static Track sample()
{
    Track t;
    t.location = "/Music/The Beatles/Abbey Road/07 Here Comes the Sun.flac";
    t.tags["artist"] = { "The Beatles" };
    t.tags["title"] = { "Here Comes the Sun" };
    t.tags["album"] = { "Abbey Road" };
    t.tags["date"] = { "1969-09-26" };
    t.tags["tracknumber"] = { "7/17" };
    t.tags["genre"] = { "Rock", "Pop" };
    t.length = 185.4;
    t.sampleRate = 44100;
    t.channels = 2;
    t.bitsPerSample = 16;
    t.codec = "FLAC";
    return t;
}

TEST ("title formatting: fields and fallbacks")
{
    auto t = sample();
    CHECK_EQ (tf::format ("%artist% - %title%", t), std::string ("The Beatles - Here Comes the Sun"));
    CHECK_EQ (tf::format ("%tracknumber%", t), std::string ("07"));
    CHECK_EQ (tf::format ("%totaltracks%", t), std::string ("17"));
    CHECK_EQ (tf::format ("%year%", t), std::string ("1969"));
    CHECK_EQ (tf::format ("%album artist%", t), std::string ("The Beatles"));
    CHECK_EQ (tf::format ("%genre%", t), std::string ("Rock, Pop"));
    CHECK_EQ (tf::format ("%length%", t), std::string ("3:05"));
    CHECK_EQ (tf::format ("%codec% %samplerate% %channels%", t), std::string ("FLAC 44100 stereo"));
    CHECK_EQ (tf::format ("%filename%", t), std::string ("07 Here Comes the Sun"));
    CHECK_EQ (tf::format ("%directoryname%", t), std::string ("Abbey Road"));
    CHECK_EQ (tf::format ("%composer%", t), std::string ("?"));
    t.tags.erase ("title");
    CHECK_EQ (tf::format ("%title%", t), std::string ("07 Here Comes the Sun"));
}

TEST ("title formatting: optional sections and quotes")
{
    auto t = sample();
    CHECK_EQ (tf::format ("[%composer% - ]%title%", t), std::string ("Here Comes the Sun"));
    CHECK_EQ (tf::format ("[%album% - ]%title%", t), std::string ("Abbey Road - Here Comes the Sun"));
    CHECK_EQ (tf::format ("'[%artist%]' it''s", t), std::string ("[%artist%] it's"));
    CHECK_EQ (tf::format ("a''b", t), std::string ("a'b"));
    CHECK_EQ (tf::format ("[[%composer%] %album%]", t), std::string (" Abbey Road"));
}

TEST ("title formatting: functions")
{
    auto t = sample();
    CHECK_EQ (tf::format ("$if(%composer%,yes,no)", t), std::string ("no"));
    CHECK_EQ (tf::format ("$if2(%composer%,%artist%)", t), std::string ("The Beatles"));
    CHECK_EQ (tf::format ("$if3(%composer%,%performer%,%album%)", t), std::string ("Abbey Road"));
    CHECK_EQ (tf::format ("$upper(%album%)", t), std::string ("ABBEY ROAD"));
    CHECK_EQ (tf::format ("$left(%album%,5)", t), std::string ("Abbey"));
    CHECK_EQ (tf::format ("$right(%album%,4)", t), std::string ("Road"));
    CHECK_EQ (tf::format ("$num(7,3)", t), std::string ("007"));
    CHECK_EQ (tf::format ("$add(1,2,3)", t), std::string ("6"));
    CHECK_EQ (tf::format ("$replace(%album%,Road,Lane)", t), std::string ("Abbey Lane"));
    CHECK_EQ (tf::format ("$stripprefix(%artist%)", t), std::string ("Beatles"));
    CHECK_EQ (tf::format ("$caps(hello wORLD)", t), std::string ("Hello World"));
    CHECK_EQ (tf::format ("$len(Björk)", t), std::string ("5"));
    CHECK_EQ (tf::format ("$pad_right(7,3,0)", t), std::string ("007"));
    CHECK_EQ (tf::format ("$meta(genre,1)", t), std::string ("Pop"));
    CHECK_EQ (tf::format ("$meta_sep(genre, / )", t), std::string ("Rock / Pop"));
    CHECK_EQ (tf::format ("$ifgreater(%samplerate%,44000,hi,lo)", t), std::string ("hi"));
    CHECK_EQ (tf::format ("$if($and(%artist%,%album%),both)", t), std::string ("both"));
    CHECK_EQ (tf::format ("$if($not(%composer%),none)", t), std::string ("none"));
    CHECK_EQ (tf::format ("$directory(%path%,2)", t), std::string ("The Beatles"));
    CHECK_EQ (tf::format ("$abbr(Abbey Road)", t), std::string ("AR"));
}

TEST ("title formatting: errors are reported, not thrown")
{
    CHECK (! tf::Script ("%artist").error().empty());
    CHECK (! tf::Script ("$if(a,b").error().empty());
    CHECK (! tf::Script ("$nosuch(a)").error().empty());
    CHECK (tf::Script ("$if(a,b,c)").error().empty());
}

TEST ("title formatting: paths keep folders but not slashes in values")
{
    auto t = sample();
    t.tags["artist"] = { "AC/DC" };
    CHECK_EQ (tf::formatPath ("%artist%/%year% - %album%/%tracknumber% %title%", t),
              std::string ("AC_DC/1969 - Abbey Road/07 Here Comes the Sun"));
    t.tags["title"] = { "What? Now: Yes" };
    CHECK_EQ (tf::formatPath ("%title%", t), std::string ("What_ Now_ Yes"));
}
