#include "FilePanels.h"

#import <AppKit/AppKit.h>
#import <objc/runtime.h>

namespace kal::ui
{
namespace
{
// Near the top of the screen Kalibre's window is on, across its middle; above
// floating windows, as plug-ins' own windows can be.
void place (NSWindow* w)
{
    if (! [w isKindOfClass: [NSSavePanel class]] || [w isSheet]) return;
    [w setLevel: NSModalPanelWindowLevel];
    // Placed once, when it first comes forward: moved by hand, it stays where it was put.
    static char placedKey;
    if (objc_getAssociatedObject (w, &placedKey) != nil) return;
    objc_setAssociatedObject (w, &placedKey, @YES, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    NSScreen* screen = [NSApp mainWindow] != nil ? [[NSApp mainWindow] screen] : [w screen];
    if (screen == nil) screen = [NSScreen mainScreen];
    if (screen == nil) return;
    const NSRect area = [screen visibleFrame];
    NSRect f = [w frame];
    f.origin.x = NSMidX (area) - f.size.width / 2;
    f.origin.y = NSMaxY (area) - f.size.height - 24;   // just under the menu bar
    if (f.origin.y < NSMinY (area)) f.origin.y = NSMinY (area);
    [w setFrameOrigin: f.origin];
    [w orderFrontRegardless];
}
} // namespace

void placeFilePanelsAtTop()
{
    static bool watching = false;
    if (watching) return;
    watching = true;
    // A panel is placed as it comes forward to be used (Open and Save panels are new each time).
    for (NSNotificationName name in @[ NSWindowDidBecomeKeyNotification, NSWindowDidBecomeMainNotification ])
        [[NSNotificationCenter defaultCenter] addObserverForName: name
                                                          object: nil
                                                           queue: [NSOperationQueue mainQueue]
                                                      usingBlock: ^(NSNotification* n) { place ((NSWindow*) [n object]); }];
}
} // namespace kal::ui
