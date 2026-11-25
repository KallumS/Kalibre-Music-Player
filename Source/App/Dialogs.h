/*
    Dialogs - the windows the player opens for work on files (decision
    0002 amended, as Kalibre's settings windows): editing tags, renaming
    and moving files, converting, ripping, the add-ons, Preferences, help.
    Each is a window of its own that closes with Escape.
*/

#pragma once

#include "CdDrive.h"
#include "Panels.h"

namespace kp::dialogs
{

// A window holding `content`, closed (and deleted) with its close button or Escape.
void open (const juce::String& title, std::unique_ptr<juce::Component> content, int width, int height, bool resizable = true);
void closeAll();

void tagEditor (Context&, const std::vector<std::string>& locations);
void guessTags (Context&, const std::vector<std::string>& locations);
enum class FileOp { rename, move, copy };
void fileOperation (Context&, const std::vector<std::string>& locations, FileOp);
void convert (Context&, const std::vector<std::string>& locations);
void rip (Context&, const AudioCdDisc&);
void preferences (Context&, int tab = 0);
void addons (Context&);
void text (const juce::String& title, const juce::String& body);

// ReplayGain scans, run as jobs; tags written when they're done.
void scanReplayGain (Context&, const std::vector<std::string>& locations, bool albums);
void removeReplayGain (Context&, const std::vector<std::string>& locations);

// The help pages' words.
juce::String titleFormatHelp();
juce::String queryHelp();
juce::String addonsHelp();

} // namespace kp::dialogs
