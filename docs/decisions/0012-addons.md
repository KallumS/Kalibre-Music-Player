# 0012: Add-ons in Lua

Accepted, 25 November 2026.

Lua 5.4, as Kalibre's scripts and REAPER's: the family's habit. Each add-on has its own Lua state (one can't break another), a time limit per call, no os.exit or io.popen. The API (the kalibre table) is events, menus, timers, playback, library queries, playlists, tags and per-add-on settings, behind an interface (addons::Api) the tests fake. Add-ons are a .lua file, a folder with main.lua, or a .zip of one.
