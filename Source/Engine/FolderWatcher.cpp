// FolderWatcher's shared part: the folders a changed path belongs to.
#include "FolderWatcher.h"

namespace kp
{
juce::StringArray foldersHolding (const juce::StringArray& watched, const juce::StringArray& changed)
{
    juce::StringArray out;
    for (auto& w : watched)
    {
        // Either separator, so a path from any system compares.
        const auto norm = [] (juce::String s) { s = s.replaceCharacter ('\\', '/'); return s.endsWithChar ('/') ? s : s + "/"; };
        const auto prefix = norm (w);
        for (auto& c : changed)
            if (norm (c).startsWith (prefix) || prefix.startsWith (norm (c))) { out.addIfNotAlreadyThere (w); break; }
    }
    return out;
}
} // namespace kp
