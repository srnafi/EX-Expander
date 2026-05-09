#pragma once
#include <windows.h>

// ---------------------------------------------------------------------------
// webUI
//
// Creates and manages the WebView2-based settings/editor window.
// The window runs its own message loop while visible.
//
// Call OpenWebUI() to show the window. It blocks until the user closes it.
// ---------------------------------------------------------------------------

// Create and display the Web UI settings window.
// Blocks the calling thread in a message loop until the window is closed.
// Returns true if successful.
void OpenWebUI(HINSTANCE hInstance);

// Window procedure (called by Windows message loop)
LRESULT CALLBACK WebUIProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);