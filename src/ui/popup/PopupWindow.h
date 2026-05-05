#pragma once
#include <windows.h>

// Window procedure
LRESULT CALLBACK PopupProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Initialize popup window
void PopupInitWindow(HINSTANCE hInst);