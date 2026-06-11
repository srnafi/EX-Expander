// ---------------------------------------------------------------------------
// popup.cpp
//
// Public entry point for the suggestion popup system.
//
// Visual shape:
//
//   ┌───────┐
//   │ item  │  ← dims as it scrolls away from center
//   ├───────┼──────────────────────┐
//   │  SELECTED ITEM (expanded)    │  ← full width, center slot
//   ├───────┼──────────────────────┘
//   │ item  │
//   └───────┘
//
// The spine (narrow left column) stays fixed for all items.
// Only the selected center row protrudes rightward.
// Navigation animates a spring-based scroll offset so items glide
// past the center slot like a spinning wheel.
//
// Sub-modules:
//   PopupState      – all mutable state (items, selection, springs)
//   PopupAnimation  – spring math and timer
//   PopupRenderer   – Direct2D draw calls
//   PopupWindow     – WNDPROC and message handling
// ---------------------------------------------------------------------------

#include "popup.h"
#include "PopupState.h"
#include "PopupAnimation.h"
#include "PopupRenderer.h"
#include "PopupWindow.h"
#include "AppLog.h"

// ===========================================================================
// PUBLIC API
// ===========================================================================

void PopupInit(HINSTANCE hInst)
{
    WNDCLASS wc{};
    wc.lpfnWndProc = PopupProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"SuggestionPopupLayered";

    if (!RegisterClass(&wc))
    {
        // ERROR_CLASS_ALREADY_EXISTS is acceptable (e.g. called twice)
        DWORD err = GetLastError();
        if (err != ERROR_CLASS_ALREADY_EXISTS)
        {
            AppLog::Error(L"PopupInit: RegisterClass failed, error "
                + std::to_wstring(err));
            return;
        }
    }

    // Window is always MaxWidth wide so the window never needs resizing
    // during animation – only the rendered content changes each frame.
    State::hwnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE,
        wc.lpszClassName,
        L"",
        WS_POPUP,
        0, 0,
        static_cast<int>(UI::MaxWidth), 200,
        nullptr, nullptr, hInst, nullptr);

    if (!State::hwnd)
    {
        AppLog::Error(L"PopupInit: CreateWindowEx failed, error "
            + std::to_wstring(GetLastError()));
        return;
    }

    State::scrollOffset.snap(0.f);
    State::opacity.snap(0.f);

    InitD2D();

    AppLog::Info(L"Popup initialised");
}

bool IsPopupVisible()
{
    return State::hwnd && IsWindowVisible(State::hwnd);
}

bool IsPopupNavigating()
{
    // Popup must be visible and the user must have moved off the first item
    // (selectedIndex > 0 means UP/DOWN was pressed at least once)
    return IsPopupVisible() && State::centerIndex > 0;
}

void PopupSetAnimation(bool enabled)
{
    Anim::animEnabled = enabled;

    if (!enabled)
    {
        // Settle any in-progress spring immediately so the next render
        // does not try to resume from a partial animation state
        State::scrollOffset.snap(0.f);
        State::opacity.snap(State::opacity.target);
        StopTimer();
        RenderLayered();
    }
}