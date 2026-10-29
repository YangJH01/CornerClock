// Larger digital clocks on non-primary monitors. No extra process or timer.
#include "secondary.h"
#include "faces.h"
#include "app.h"
#include <new>

namespace {
struct Clock {
    Clock* next = nullptr;
    HMONITOR monitor = nullptr;
    HWND window = nullptr, taskbar = nullptr;
    HWINEVENTHOOK hook = nullptr;
    UINT dpi = 96;
    int px = 0;
    HDC dc = nullptr;
    HBITMAP dib = nullptr;
    ID2D1DCRenderTarget* target = nullptr;
    ID2D1SolidColorBrush* brush = nullptr;
};
Clock* s_clocks;
HWND s_primary;
ID2D1Factory* s_factory; // borrowed; destroyed before the main factory
int s_sizeDips;
bool s_pending;
constexpr UINT SYNC_MESSAGE = WM_APP + 3; // main window's WM_TASKBAR_CHANGED

void RequestSync()
{
    if (!s_pending && s_primary)
        s_pending = PostMessageW(s_primary, SYNC_MESSAGE, 0, 0) != FALSE;
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wp, LPARAM lp)
{
    switch (message) {
    case WM_DPICHANGED:
        RequestSync();
        return 0; // position and scale are derived from this monitor in Sync
    case WM_WINDOWPOSCHANGED:
        if (IsWindowVisible(window) &&
            !(GetWindowLongPtrW(window, GWL_EXSTYLE) & WS_EX_TOPMOST)) RequestSync();
        break;
    case WM_NCHITTEST: return HTTRANSPARENT;
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    }
    return DefWindowProcW(window, message, wp, lp);
}

void ReleaseGraphics(Clock& clock)
{
    SafeRelease(clock.brush);
    SafeRelease(clock.target);
    if (clock.dc) { DeleteDC(clock.dc); clock.dc = nullptr; }
    if (clock.dib) { DeleteObject(clock.dib); clock.dib = nullptr; }
}

bool CreateGraphics(Clock& clock, UINT dpi)
{
    ReleaseGraphics(clock);
    clock.dpi = dpi;
    clock.px = MulDiv(MulDiv(s_sizeDips, ConfigSecondaryScale(), 100), dpi, 96);
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = clock.px;
    info.bmiHeader.biHeight = -clock.px;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    clock.dib = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    clock.dc = CreateCompatibleDC(nullptr);
    if (!clock.dib || !clock.dc) return false;
    SelectObject(clock.dc, clock.dib);
    float renderDpi = clock.px * 96.0f / DIAL;
    auto props = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        renderDpi, renderDpi);
    if (FAILED(s_factory->CreateDCRenderTarget(&props, &clock.target))) return false;
    RECT rect = { 0, 0, clock.px, clock.px };
    return SUCCEEDED(clock.target->BindDC(clock.dc, &rect)) &&
        SUCCEEDED(clock.target->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1), &clock.brush));
}

void Draw(Clock& clock, bool beforeShow = false)
{
    if (!clock.target || !clock.brush) return;
    if (!beforeShow && (!IsWindowVisible(clock.window) || IsIconic(clock.window))) return;
    SYSTEMTIME time;
    GetLocalTime(&time);
    auto identity = D2D1::Matrix3x2F::Identity();
    clock.target->BeginDraw();
    clock.target->SetTransform(identity);
    clock.target->Clear(D2D1::ColorF(0, 0.0f));
    // The digital dial is empty or a single optional panel. Font geometries
    // are shared with the primary digital face, not rebuilt for each window.
    g_faceDigital->DrawDial(clock.target, clock.brush);
    g_faceDigital->DrawHands(clock.target, clock.brush, time, ShowSeconds(), identity);
    clock.target->EndDraw();
    SIZE size = { clock.px, clock.px };
    POINT source = {};
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    UpdateLayeredWindow(clock.window, nullptr, nullptr, &size,
                        clock.dc, &source, 0, &blend, ULW_ALPHA);
}

BOOL CALLBACK FindTaskbar(HWND window, LPARAM param)
{
    Clock& clock = *(Clock*)param;
    WCHAR name[64];
    GetClassNameW(window, name, _countof(name));
    if (!lstrcmpW(name, L"Shell_SecondaryTrayWnd") &&
        MonitorFromWindow(window, MONITOR_DEFAULTTONULL) == clock.monitor) {
        clock.taskbar = window;
        return FALSE;
    }
    return TRUE;
}

void CALLBACK TaskbarEvent(HWINEVENTHOOK, DWORD, HWND window,
                          LONG object, LONG child, DWORD, DWORD)
{
    if (object != OBJID_WINDOW || child != CHILDID_SELF) return;
    for (Clock* clock = s_clocks; clock; clock = clock->next)
        if (clock->taskbar == window) { RequestSync(); return; }
}

void BindTaskbar(Clock& clock)
{
    if (clock.hook) UnhookWinEvent(clock.hook);
    clock.hook = nullptr;
    clock.taskbar = nullptr;
    EnumWindows(FindTaskbar, (LPARAM)&clock);
    if (!clock.taskbar) return;
    DWORD process = 0;
    DWORD thread = GetWindowThreadProcessId(clock.taskbar, &process);
    if (thread) clock.hook = SetWinEventHook(EVENT_OBJECT_SHOW, EVENT_OBJECT_LOCATIONCHANGE,
        nullptr, TaskbarEvent, process, thread, WINEVENT_OUTOFCONTEXT);
}

void Clear()
{
    while (s_clocks) {
        Clock* clock = s_clocks;
        s_clocks = clock->next;
        if (clock->hook) UnhookWinEvent(clock->hook);
        if (clock->window) DestroyWindow(clock->window);
        ReleaseGraphics(*clock);
        delete clock;
    }
}

BOOL CALLBACK AddMonitor(HMONITOR monitor, HDC, LPRECT, LPARAM)
{
    MONITORINFO info = { sizeof(info) };
    if (!GetMonitorInfoW(monitor, &info) || (info.dwFlags & MONITORINFOF_PRIMARY)) return TRUE;
    Clock* clock = new (std::nothrow) Clock;
    if (!clock) return FALSE;
    clock->monitor = monitor;
    clock->window = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW |
        WS_EX_NOACTIVATE | WS_EX_TRANSPARENT, L"CornerClockSecondary", L"CornerClock Secondary",
        WS_POPUP, info.rcMonitor.left, info.rcMonitor.top, 1, 1,
        nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!clock->window) { delete clock; return TRUE; }
    clock->next = s_clocks;
    s_clocks = clock;
    BindTaskbar(*clock);
    return TRUE;
}
}

void SecondaryInit(HWND primary, ID2D1Factory* factory, int sizeDips)
{
    s_primary = primary;
    s_factory = factory;
    s_sizeDips = sizeDips;
    WNDCLASSW cls = {};
    cls.lpfnWndProc = WindowProc;
    cls.hInstance = GetModuleHandleW(nullptr);
    cls.lpszClassName = L"CornerClockSecondary";
    RegisterClassW(&cls);
    SecondaryRebuild();
}

void SecondaryRebuild()
{
    if (!s_primary) return;
    Clear();
    EnumDisplayMonitors(nullptr, nullptr, AddMonitor, 0);
    SecondarySync();
}

void SecondaryRefresh(int sizeDips)
{
    s_sizeDips = sizeDips;
    for (Clock* clock = s_clocks; clock; clock = clock->next) {
        ReleaseGraphics(*clock);
        // Also causes Sync to paint fresh settings before making it visible.
        ShowWindow(clock->window, SW_HIDE);
    }
    SecondarySync();
}

void SecondarySync()
{
    s_pending = false;
    for (Clock* clock = s_clocks; clock; clock = clock->next) {
        MONITORINFO info = { sizeof(info) };
        if (!GetMonitorInfoW(clock->monitor, &info)) {
            ShowWindow(clock->window, SW_HIDE);
            continue;
        }
        if (clock->taskbar && !IsWindow(clock->taskbar)) BindTaskbar(*clock);
        UINT dpi = GetDpiForWindow(clock->window);
        bool fresh = !clock->target || !clock->brush || clock->dpi != dpi;
        if (fresh && !CreateGraphics(*clock, dpi)) {
            ShowWindow(clock->window, SW_HIDE);
            continue;
        }
        RECT bar = {}, visible = {};
        bool expanded = clock->taskbar && IsWindowVisible(clock->taskbar) &&
            GetWindowRect(clock->taskbar, &bar) && IntersectRect(&visible, &bar, &info.rcMonitor) &&
            visible.bottom - visible.top > MulDiv(3, dpi, 96);
        if (FullscreenCoversClock(clock->monitor, info.rcMonitor, clock->taskbar, expanded)) {
            if (IsWindowVisible(clock->window)) ShowWindow(clock->window, SW_HIDE);
            continue;
        }
        auto bounds = DigitalBounds();
        int margin = MulDiv(8, dpi, 96);
        int x = info.rcMonitor.right - margin - (int)(bounds.right * clock->px / DIAL);
        int bottom = expanded ? visible.top : info.rcMonitor.bottom;
        int y = bottom - margin - (int)(bounds.bottom * clock->px / DIAL);
        RECT current = {};
        GetWindowRect(clock->window, &current);
        if (current.left != x || current.top != y || fresh)
            SetWindowPos(clock->window, nullptr, x, y, clock->px, clock->px,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        if (fresh || !IsWindowVisible(clock->window) || IsIconic(clock->window)) {
            Draw(*clock, true);
            ShowWindow(clock->window, SW_SHOWNOACTIVATE);
        }
        if (!(GetWindowLongPtrW(clock->window, GWL_EXSTYLE) & WS_EX_TOPMOST))
            SetWindowPos(clock->window, HWND_TOPMOST, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

void SecondaryDraw()
{
    for (Clock* clock = s_clocks; clock; clock = clock->next) Draw(*clock);
}

void SecondaryDestroy()
{
    s_primary = nullptr;
    Clear();
    s_factory = nullptr;
}
