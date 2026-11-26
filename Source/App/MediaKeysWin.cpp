// Media keys on Windows: the keyboard's play/pause, next, previous and stop
// keys, wherever the focus is, as system-wide hot keys (decision 0017).
#include "MediaKeys.h"
#include "Controller.h"

#include <windows.h>

namespace kp::media
{
namespace
{
Controller* target = nullptr;
HWND window = nullptr;
const wchar_t* className = L"KalibrePlayerMediaKeys";

LRESULT CALLBACK proc (HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_HOTKEY && target)
    {
        auto* c = target;
        switch (wp)
        {
            case 1: juce::MessageManager::callAsync ([c] { c->playPause(); }); break;
            case 2: juce::MessageManager::callAsync ([c] { c->next(); }); break;
            case 3: juce::MessageManager::callAsync ([c] { c->previous(); }); break;
            case 4: juce::MessageManager::callAsync ([c] { c->stop(); }); break;
            default: break;
        }
        return 0;
    }
    return DefWindowProcW (h, msg, wp, lp);
}
} // namespace

void install (Controller& c)
{
    target = &c;
    WNDCLASSW wc {};
    wc.lpfnWndProc = proc;
    wc.hInstance = GetModuleHandleW (nullptr);
    wc.lpszClassName = className;
    RegisterClassW (&wc);
    window = CreateWindowW (className, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
    if (! window) return;
    // Another player holding a key keeps it; the others still work.
    RegisterHotKey (window, 1, MOD_NOREPEAT, VK_MEDIA_PLAY_PAUSE);
    RegisterHotKey (window, 2, MOD_NOREPEAT, VK_MEDIA_NEXT_TRACK);
    RegisterHotKey (window, 3, MOD_NOREPEAT, VK_MEDIA_PREV_TRACK);
    RegisterHotKey (window, 4, MOD_NOREPEAT, VK_MEDIA_STOP);
}

void uninstall()
{
    if (window)
    {
        for (int k = 1; k <= 4; ++k) UnregisterHotKey (window, k);
        DestroyWindow (window);
        window = nullptr;
    }
    target = nullptr;
}

} // namespace kp::media
