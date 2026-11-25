#!/bin/bash
# Installs Kalibre Player for you alone (no administrator needed): the app in
# ~/.local/opt/kalibre-player, "kalibre-player" in ~/.local/bin, and an entry
# in the applications menu. Run it again to update; uninstall.sh takes it out.
set -e
here="$(cd "$(dirname "$0")" && pwd)"
app="$HOME/.local/opt/kalibre-player"
mkdir -p "$app" "$HOME/.local/bin" "$HOME/.local/share/applications" "$HOME/.local/share/icons/hicolor/512x512/apps"
cp "$here/Kalibre Player" "$app/"
cp -R "$here/Add-on Examples" "$app/" 2>/dev/null || true
ln -sf "$app/Kalibre Player" "$HOME/.local/bin/kalibre-player"
cp "$here/kalibre-player.png" "$HOME/.local/share/icons/hicolor/512x512/apps/kalibre-player.png"
sed "s|^Exec=.*|Exec=\"$app/Kalibre Player\" %F|" "$here/kalibre-player.desktop" > "$HOME/.local/share/applications/kalibre-player.desktop"
command -v update-desktop-database >/dev/null && update-desktop-database "$HOME/.local/share/applications" || true
echo "Kalibre Player is installed: it's in your applications menu, and 'kalibre-player' starts it."
