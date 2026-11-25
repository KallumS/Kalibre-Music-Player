# 0015: Every action a command, every key changeable

Accepted, 25 November 2026.

Menus, keys and buttons all go through JUCE's ApplicationCommandManager, so every action has a name and default keys, and Preferences > Keyboard Shortcuts changes them (kept in Settings.xml). The main component is always the command target, whatever has the focus.
