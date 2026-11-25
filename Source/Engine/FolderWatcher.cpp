// FolderWatcher's shared part: the folders a changed path belongs to.
#include "FolderWatcher.h"

namespace kp
{
juce::StringArray foldersHolding (const juce::StringArray& watched, const juce::StringArray& changed)
{
    juce::StringArray out;
    for (auto& w : watched)
    {
        const auto prefix = w.endsWithChar ('/') ? w : w + "/";
        for (auto& c : changed)
            if (c == w || c.startsWith (prefix) || prefix.startsWith (c.endsWithChar ('/') ? c : c + "/")) { out.addIfNotAlreadyThere (w); break; }
    }
    return out;
}
} // namespace kp
