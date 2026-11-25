-- name: Listening Log
-- version: 1.0
-- author: Kalibre Player
-- description: Keeps a log of every track heard past its middle (~/Music/Listening Log.tsv), and counts plays in a PLAY_COUNT tag.

local path = (os.getenv("HOME") or ".") .. "/Music/Listening Log.tsv"
local current, heard = nil, 0

kalibre.on("track_started", function(t) current = t heard = 0 end)

-- What's heard is counted each second, so seeking past the middle doesn't count.
kalibre.every(1, function()
  if current and kalibre.is_playing() then heard = heard + 1 end
end)

kalibre.on("track_finished", function(t)
  if not current or t.location ~= current.location then return end
  if current.length > 0 and heard >= current.length / 2 then
    local f = io.open(path, "a")
    if f then
      f:write(os.date("%Y-%m-%d %H:%M:%S"), "\t", t.artist, "\t", t.album, "\t", t.title, "\t", kalibre.format_time(t.length), "\n")
      f:close()
    end
    local plays = tonumber(t.tags.play_count and t.tags.play_count[1] or "0") or 0
    kalibre.tags.set(t.location, { play_count = tostring(plays + 1) })
  end
  current = nil
end)

kalibre.menu("Open the Listening Log", function()
  kalibre.message("The log is " .. path .. " (tab-separated: it opens in Numbers).")
end)
