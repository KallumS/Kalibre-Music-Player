-- name: Now Playing File
-- version: 1.0
-- author: Kalibre Player
-- description: Writes what's playing to a text file (~/Music/Now Playing.txt), for streaming software to show.

local path = (os.getenv("HOME") or os.getenv("USERPROFILE") or ".") .. "/Music/Now Playing.txt"
local pattern = kalibre.settings.get("pattern", "%artist% - %title%")

local function write(text)
  local f = io.open(path, "w")
  if f then f:write(text) f:close() end
end

kalibre.on("track_started", function(t) write(kalibre.format(pattern, t)) end)
kalibre.on("stopped", function() write("") end)
kalibre.on("shutdown", function() write("") end)

kalibre.menu("Now Playing File: Show the Pattern", function()
  kalibre.message("Writing \"" .. pattern .. "\" to " .. path .. ". Change it with kalibre.settings in this add-on.")
end)
