#include "PopupWindow.h"
#include "PopupState.h"
#include "PopupAnimation.h"
#include "AppLog.h"

#include <windows.h>

namespace
{
    // Cache cursor handle to avoid repeated LoadCursor calls
    HCURSOR g_arrowCursor = nullptr;
}

LRESULT CALLBACK PopupProc(HWND hwnd, UINT msg,
    WPARAM wParam, LPARAM lParam)
{
    using namespace State;

    switch (msg)
    {
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;

    case WM_SETCURSOR:
        if (!g_arrowCursor)
            g_arrowCursor = LoadCursor(nullptr, IDC_ARROW);
        SetCursor(g_arrowCursor);
        return TRUE;

    case WM_LBUTTONDOWN:
        dragging = true;
        GetCursorPos(&dragStart);
        {
            RECT r{};
            GetWindowRect(hwnd, &r);
            winStart = { r.left, r.top };
        }
        SetCapture(hwnd);
        return 0;

    case WM_MOUSEMOVE:
        if (dragging)
        {
            POINT cur{};
            GetCursorPos(&cur);

            int newX = winStart.x + (cur.x - dragStart.x);
            int newY = winStart.y + (cur.y - dragStart.y);

            spineLeft = static_cast<float>(newX);
            State::popupFixedX = newX;
            State::popupFixedY = newY;

            if (!SetWindowPos(hwnd, nullptr, newX, newY, 0, 0,
                SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE))
            {
                AppLog::Warn(L"PopupProc: SetWindowPos failed during drag");
            }
        }
        return 0;

    case WM_LBUTTONUP:
        if (dragging)
        {
            dragging = false;
            ReleaseCapture();
        }
        return 0;

    case WM_TIMER:
        if (wParam == Anim::TimerID)
            Tick();
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}