#pragma once
#include <windows.h>

// ---------------------------------------------------------------------------
// Popup public API
// Call PopupInit once at startup, then use the Internal functions from
// PopupInput.h to drive content and navigation.
// ---------------------------------------------------------------------------

// Initialise the popup window and Direct2D resources
void PopupInit(HINSTANCE hInst);

// Returns true if the popup is currently visible and the user is navigating
// the suggestion list (i.e. UP/DOWN has been pressed at least once)
bool IsPopupNavigating();

// Enable or disable scroll animation.
// Disabled: instant navigation, zero CPU between keypresses.
// Enabled:  ~60fps spring animation, auto-stops after ~150ms per keypress.
void PopupSetAnimation(bool enabled);

// Returns true if the popup window is currently visible
bool IsPopupVisible();