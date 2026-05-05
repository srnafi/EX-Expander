#include "PopupWindow.h"
#include <PopupState.h>
#include <PopupAnimation.h>

// ---------------------------------------------------------------------------
// WINDOW PROC
// ---------------------------------------------------------------------------

LRESULT CALLBACK PopupProc(HWND hwnd, UINT msg,
    WPARAM wParam, LPARAM lParam)
{
    using namespace State;

    switch (msg)
    {
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;

    case WM_SETCURSOR:
        SetCursor(LoadCursor(nullptr, IDC_ARROW));
        return TRUE;

    case WM_LBUTTONDOWN:
        dragging = true;
        GetCursorPos(&dragStart);
        { RECT r; GetWindowRect(hwnd, &r); winStart = { r.left, r.top }; }
        SetCapture(hwnd);
        return 0;

    case WM_MOUSEMOVE:
        if (dragging)
        {
            POINT cur; GetCursorPos(&cur);

            int newX = winStart.x + (cur.x - dragStart.x);
            int newY = winStart.y + (cur.y - dragStart.y);
            spineLeft = (float)newX;

            g_PopupFixedX = newX;
            g_PopupFixedY = newY;

            SetWindowPos(hwnd, nullptr, newX, newY, 0, 0,
                SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
        return 0;

    case WM_LBUTTONUP:
        if (dragging) { dragging = false; ReleaseCapture(); }
        return 0;

    case WM_TIMER:
        if (wParam == Anim::TimerID) Tick();
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}