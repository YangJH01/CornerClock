# CornerClock

A small Windows clock for an automatically hidden taskbar. Based on
[DeskTick](https://github.com/riyasy/DeskTick), with independent naming and icon.
This is an unofficial modification, not an RYF Tools release.

The default is a transparent 24-hour digital clock with seconds in the primary
monitor's bottom-right corner, eight scaled pixels from each edge. As the
primary taskbar slides up or down, the clock follows its current top edge and
stays visible above it. The clock's position is locked and mouse clicks pass
through it to the application underneath.

On the clock's monitor, a foreground borderless fullscreen app hides the clock
while it covers the taskbar. Returning to a normal window restores the clock;
revealing the taskbar above the fullscreen app also reveals the clock. Normal
captioned maximized windows are not treated as fullscreen.

The Digital face offers Clean (default), Bold and Mono typography presets in
the appearance dialog. All use aligned, fixed-width digit cells for hours,
minutes and seconds, with quieter separators and a small text-only shadow.

Taskbar tracking uses WinEvent notifications restricted to the taskbar thread.
Foreground changes use a foreground event hook and a geometry hook restricted
to the active window's thread, with an early HWND filter.
It adds no polling timer, global mouse hook, service, or helper process. The
clock redraws once per second (once per minute if seconds are disabled). Movement
repositions the cached clock window without redrawing its text. Memory usage
must distinguish total working set from private memory; measure on the target PC.

Right-click the tray icon for appearance, startup and exit. Resize and hiding
the tray icon are disabled while the overlay is locked. To change its size,
exit and edit `Size` in the `[Window]` INI section (physical pixels; default 192).
Settings live in `%LOCALAPPDATA%\CornerClock.ini`; the startup entry is named
`CornerClock`. To restore freely draggable placement, exit, set `Corner=0` in
the `[Window]` INI section, and restart. Corner mode targets the primary taskbar;
secondary monitors do not receive additional widgets.

## Build

Use Visual Studio 2022 C++ desktop tools and a Windows SDK:

```powershell
msbuild src\DeskTick\DeskTick.vcxproj /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143
```

Output: `src\DeskTick\x64\Release\CornerClock.exe`. The historical source
folder names remain to keep the upstream diff small. The desktop executable is
the supported build; the inherited upstream MSIX project is not a CornerClock
package and should not be published.

## Source and license

Original source copyright RYF Tools. CornerClock adds taskbar animation tracking,
locked corner placement, transparent digital defaults with seconds, UTF-8 compilation,
and independent application/configuration identity. Original source license and
notices are preserved in [LICENSE](LICENSE). That file declares GPL-3.0 for the
source and separate restrictions for binaries and trademarks. This repository
contains source, not a binary release.
