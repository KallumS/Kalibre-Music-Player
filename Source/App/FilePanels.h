/*
    FilePanels - where the Mac's Open and Save panels appear.

    A plug-in in a window of its own (0056) could cover the Save panel, and
    nothing brought the panel forward (reported 22 November). Every Open and
    Save panel Kalibre shows is put near the top of the screen, at the middle,
    and above plug-ins' windows. Nothing happens off the Mac, where JUCE's own
    file browser is a window in front.
*/

#pragma once

namespace kal::ui
{
// Called once when Kalibre starts: from then on, every Open and Save panel is placed so.
void placeFilePanelsAtTop();
} // namespace kal::ui
