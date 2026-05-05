// ---------------------------------------------------------------------------
// SuggestionPopup.cpp
//
// Visual shape (⊢):  The "spine" (narrow column) stays fixed for all items.
// Only the selected (center) row protrudes rightward — width snaps instantly.
// Navigation animates a continuous scroll offset spring so items glide past
// the center slot like a smooth spinning wheel.
//
//   ┌───────┐
//   │ item  │  ← dims as it scrolls away from center
//   ├───────┼──────────────────────┐
//   │  SELECTED ITEM (expanded)    │  ← full size at scrollOffset == 0
//   ├───────┼──────────────────────┘
//   │ item  │
//   └───────┘
//
// ---------------------------------------------------------------------------
#include <windows.h>
#include "popup.h"
#include <PopupState.h>
#include <PopupAnimation.h>
#include <PopupRenderer.h>
#include "PopupWindow.h"

// ---------------------------------------------------------------------------
// PUBLIC API
// ---------------------------------------------------------------------------

void PopupInit(HINSTANCE hInst)
{
    WNDCLASS wc{};
    wc.lpfnWndProc = PopupProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"SuggestionPopupLayered";
    RegisterClass(&wc);

    // Window is always MaxWidth wide — only the rendered content varies.
    // This avoids any window-resize flicker during animation.
    State::hwnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE,
        wc.lpszClassName, L"",
        WS_POPUP,
        0, 0,
        (int)UI::MaxWidth, 200,
        nullptr, nullptr, hInst, nullptr);

    State::scrollOffset.snap(0.f);
    State::opacity.snap(0.f);
    State::currentWidth = UI::SpineWidth;

    InitD2D();
}

bool IsPopupNavigating()
{
    return true;
}

// Toggle smooth scroll animation on/off.
// When off: navigation is instant (zero CPU between keypresses). 
// When on:  ~60fps timer runs only while the spring is settling (~150ms/keypress).  ;la
void PopupSetAnimation(bool enabled)
{ 
    Anim::animEnabled = enabled;
    if (!enabled)
    {
        // Settle any in-progress animation immediately
        State::scrollOffset.snap(0.f);
        State::opacity.snap(State::opacity.target);
        StopTimer();
        RenderLayered();
    }
}