# Writing Add-ons

An add-on is a Lua 5.4 script: a .lua file, or a folder with main.lua in it (or a .zip of one). File > Import Add-on puts one in the Add-ons folder; View > Add-ons lists them. The first comment lines name it:

```
-- name: Now Playing Notifier
-- version: 1.0
-- author: You
-- description: Says what's playing.
```

It reaches the player through the table "kalibre":

EVENTS  kalibre.on(event, function(...) end)
```
"startup", "shutdown", "track_started" (track), "track_finished" (track), "paused", "resumed", "stopped",
"seeked" (seconds), "volume_changed" (dB), "library_changed", "selection_changed"
```

MENUS AND TIMERS
```
kalibre.menu("Title", fn)          an item in the Add-ons menu (and the playlist's right-click menu)
kalibre.every(seconds, fn)         again and again; returns an id
kalibre.after(seconds, fn)         once; returns an id
kalibre.cancel(id)
```

PLAYBACK
```
kalibre.play()  pause()  play_pause()  stop()  next()  previous()  seek(seconds)
kalibre.position()  is_playing()  is_paused()  volume([dB])  now_playing()  queue(location or tracks)
```

THE LIBRARY AND PLAYLISTS (counted from 1)
```
kalibre.library.query("genre IS jazz")  library.count()
kalibre.playlist.names()  active([n])  items([n])  create(name)  add([n,] locations or tracks)  clear([n])  play(n, item)
kalibre.selection()                the tracks chosen in the playlist
```

TAGS
```
kalibre.tags.get(location)         a track
kalibre.tags.set(location, { mood = "calm", comment = false })   false takes a field out
```

WORDS AND SETTINGS
```
kalibre.message(text)              in the message line at the top
print(...)                         in the add-on console (View > Add-on Console)
kalibre.format(pattern[, track])   title formatting
kalibre.format_time(seconds)       "3:05"
kalibre.settings.get(key[, default])  settings.set(key, value)   kept for this add-on
```

A track is a table: location, length, codec, samplerate, channels, bitrate, bitspersample, filesize, tags (each a list of values), and artist, title, album, album_artist, date, genre, tracknumber, discnumber for ease.

An add-on that runs for more than a few seconds at once is stopped, and its error shown in View > Add-ons.


(The same words are in the app under Help.)
