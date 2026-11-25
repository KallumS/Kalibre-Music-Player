-- name: Rate Tracks
-- version: 1.0
-- author: Kalibre Player
-- description: Gives the chosen tracks 1 to 5 stars, kept in their RATING tag (query it: rating GREATER 3).

for stars = 1, 5 do
  kalibre.menu("Rate " .. string.rep("\u{2605}", stars), function()
    local tracks = kalibre.selection()
    if #tracks == 0 then kalibre.message("Choose tracks in the playlist first.") return end
    for _, t in ipairs(tracks) do kalibre.tags.set(t.location, { rating = tostring(stars) }) end
    kalibre.message(#tracks .. " rated " .. stars .. (stars == 1 and " star." or " stars."))
  end)
end

kalibre.menu("Remove Rating", function()
  for _, t in ipairs(kalibre.selection()) do kalibre.tags.set(t.location, { rating = false }) end
end)
