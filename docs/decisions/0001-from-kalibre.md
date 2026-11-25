# 0001: Built with JUCE and C++, for Apple silicon, from Kalibre

Accepted, 25 November 2026.

The musician asked for Kalibre adapted into a music player. Kalibre is a DAW: its arrangement, piano roll and devices don't belong in a player, so the player is a new app that takes what does: JUCE 9 at the same version, the theme (Theme.h, the twelve palettes, the 7:1 contrast rule), the BS.1770 loudness meter, vendored Lua 5.4 built as C++, the test runner (Check.h), the render tool and the CI's shape. Apple silicon only for now, as asked; Kalibre's Windows build could follow.
