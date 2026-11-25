// Draws the real window into PNGs, one per layout and theme, so a change can
// be seen from Linux (Kalibre's decision 0023):
//   KPRender <folder>
#include "AppTestHelpers.h"
#include "MainComponent.h"
#include "TagIO.h"

using namespace kp;

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "renders"));
    out.createDirectory();
    theme::LookAndFeel look;
    juce::LookAndFeel::setDefaultLookAndFeel (&look);

    // A small library to show.
    const auto music = test::scratch().getChildFile ("Music");
    struct Song { const char* artist; const char* album; const char* title; const char* date; const char* genre; };
    const Song songs[] = {
        { "Air", "Moon Safari", "La Femme d'Argent", "1998", "Electronic" }, { "Air", "Moon Safari", "Sexy Boy", "1998", "Electronic" },
        { "Air", "Moon Safari", "All I Need", "1998", "Electronic" }, { "Miles Davis", "Kind of Blue", "So What", "1959", "Jazz" },
        { "Miles Davis", "Kind of Blue", "Freddie Freeloader", "1959", "Jazz" }, { "Miles Davis", "Kind of Blue", "Blue in Green", "1959", "Jazz" },
        { "Radiohead", "OK Computer", "Airbag", "1997", "Rock" }, { "Radiohead", "OK Computer", "Paranoid Android", "1997", "Rock" },
        { "Daft Punk", "Discovery", "One More Time", "2001", "Electronic" }, { "Nina Simone", "Pastel Blues", "Sinnerman", "1965", "Jazz" },
    };
    int k = 0;
    for (auto& s : songs)
    {
        auto f = test::write (juce::String ("Music/") + s.artist + "/" + s.album + "/" + juce::String (++k).paddedLeft ('0', 2) + " " + s.title + ".flac",
                              test::fmt ("flac"), 44100, 44100 * 3, [] (int c, int i) { return test::sine (c, i) * (float) (0.6 + 0.4 * std::sin (i / 3000.0)); });
        juce::String e;
        tagio::write (f.getFullPathName().toStdString(),
                      { { "artist", { s.artist } }, { "album", { s.album } }, { "title", { s.title } }, { "date", { s.date } }, { "genre", { s.genre } },
                        { "tracknumber", { std::to_string (k) } } }, e);
    }

    Controller c (test::scratch().getChildFile ("settings"), false);
    c.addFolder (music);
    test::waitFor ([&] { return c.library.size() == std::size (songs) && ! c.scanning(); });
    c.addFiles (0, { music });
    test::waitFor ([&] { return c.jobs.running().empty(); });
    MainComponent main (c);
    main.setSize (1400, 860);
    c.choosePlaylist (0);
    c.playItem (0, 1);
    // Sound through the meters, as if heard.
    std::vector<float> l (512), r (512);
    test::waitFor ([&] { return c.player.bufferedFrames() > 20000; });
    for (int b = 0; b < 60; ++b) { c.player.renderForTest (l.data(), r.data(), 512); test::waitFor ([] { return false; }, 2); }
    test::waitFor ([&] { return c.nowPlayingTrack().has_value(); });
    c.setSelection ({ 1 });
    c.say ("Playing from the Default playlist.");

    auto snap = [&] (const juce::String& name)
    {
        test::waitFor ([] { return false; }, 200);
        auto img = main.createComponentSnapshot (main.getLocalBounds(), true, 1.0f);
        juce::FileOutputStream os (out.getChildFile (name + ".png"));
        os.setPosition (0);
        os.truncate();
        juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("%s\n", out.getChildFile (name + ".png").getFullPathName().toRawUTF8());
    };
    int index = 0;
    for (auto& p : layout::presets())
    {
        main.layoutView().setLayout (p.root);
        snap (juce::String (++index) + " " + juce::String (p.name));
    }
    main.layoutView().setLayout (layout::presets().front().root);
    main.layoutView().setEditing (true);
    snap ("Editing the Layout");
    main.layoutView().setEditing (false);
    main.setMini (true);
    main.setSize (MiniPlayer::size().x, MiniPlayer::size().y);
    snap ("Mini Player");
    main.setMini (false);
    main.setSize (1400, 860);
    theme::usePalette (4);
    main.layoutView().setLayout (layout::presets()[2].root);
    snap ("Theme Porcelain");
    theme::usePalette (0);
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    test::scratch().deleteRecursively();
    return 0;
}
