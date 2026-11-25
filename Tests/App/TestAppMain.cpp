// The engine's and the window's tests: JUCE is up, no audio device is needed.
#include "Check.h"
#include "AppTestHelpers.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstring>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    int ran = 0;
    for (const auto& t : check::all())
    {
        if (argc > 1 && std::strstr (t.name, argv[1]) == nullptr) continue;
        check::current() = t.name;
        std::printf ("%s\n", t.name);
        std::fflush (stdout);
        t.fn();
        ++ran;
    }
    test::scratch().deleteRecursively();
    if (check::failures() > 0)
    {
        std::printf ("%d failure(s) in %d tests\n", check::failures(), ran);
        return 1;
    }
    std::printf ("%d tests passed\n", ran);
    return 0;
}
