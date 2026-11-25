-- name: Sleep Timer
-- version: 1.0
-- author: Kalibre Player
-- description: Stops the music after 15, 30 or 60 minutes, fading it out over the last 20 seconds.

local timer, fader

local function cancel()
  if timer then kalibre.cancel(timer) timer = nil end
  if fader then kalibre.cancel(fader) fader = nil end
end

for _, minutes in ipairs({ 15, 30, 60 }) do
  kalibre.menu("Sleep in " .. minutes .. " Minutes", function()
    cancel()
    local start = kalibre.volume()
    timer = kalibre.after(minutes * 60 - 20, function()
      local steps = 0
      fader = kalibre.every(1, function()
        steps = steps + 1
        kalibre.volume(start - steps * 2)
        if steps >= 20 then
          cancel()
          kalibre.stop()
          kalibre.volume(start)
          kalibre.message("Sleep timer: stopped.")
        end
      end)
    end)
    kalibre.message("The music stops in " .. minutes .. " minutes.")
  end)
end

kalibre.menu("Cancel the Sleep Timer", function() cancel() kalibre.message("Sleep timer off.") end)
