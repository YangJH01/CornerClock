#pragma once
#include <windows.h>
#include <d2d1.h>

// Additional digital overlays share the main thread, timer and glyph cache.
void SecondaryInit(HWND primary, ID2D1Factory* factory, int sizeDips);
void SecondaryRefresh(int sizeDips);
void SecondaryRebuild();
void SecondarySync();
void SecondaryDraw();
void SecondaryDestroy();

bool FullscreenCoversClock(HMONITOR monitor, const RECT& screen,
                          HWND taskbar, bool barExpanded);
