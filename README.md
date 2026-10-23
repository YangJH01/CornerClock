# CornerClock

A small Windows clock for an automatically hidden taskbar. Based on
[DeskTick](https://github.com/riyasy/DeskTick), with independent naming and icon.
This is an unofficial modification, not an RYF Tools release.

The default is a 24-hour digital clock in the primary monitor's bottom-right
corner, eight scaled pixels from each edge. When the primary taskbar expands,
the widget hides; when it retracts, the widget returns with the current time.

Taskbar tracking uses WinEvent notifications restricted to the taskbar thread.
It adds no polling timer, global mouse hook, service, or helper process. With
seconds disabled, the existing clock timer wakes once per minute. Memory usage
must distinguish total working set from private memory; measure on the target PC.

Right-click the clock or its tray icon for appearance, size, startup and exit.
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

Original source copyright RYF Tools. CornerClock adds taskbar-aware visibility,
content-based corner placement, minute-only digital defaults, UTF-8 compilation,
and independent application/configuration identity. Original source license and
notices are preserved in [LICENSE](LICENSE). That file declares GPL-3.0 for the
source and separate restrictions for binaries and trademarks. This repository
contains source, not a binary release.
