#!/bin/bash
# Takes Kalibre Player out again. Your library, playlists and settings stay in ~/.config/Kalibre Player.
rm -rf "$HOME/.local/opt/kalibre-player" "$HOME/.local/bin/kalibre-player" \
       "$HOME/.local/share/applications/kalibre-player.desktop" "$HOME/.local/share/icons/hicolor/512x512/apps/kalibre-player.png"
echo "Kalibre Player is uninstalled."
