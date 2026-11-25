/*
    MediaKeys - the keyboard's play, next and previous keys, AirPods and
    Control Centre's Now Playing (decision 0017), through the Mac's remote
    command centre. Elsewhere, nothing.
*/

#pragma once

namespace kp
{
class Controller;
namespace media
{
void install (Controller&);
void uninstall();
} // namespace media
} // namespace kp
