#pragma once
#include <windows.h>
#include <wrl.h>
#include <WebView2.h>

using Microsoft::WRL::ComPtr;

// Globals
extern HWND g_uiWindow;
extern ComPtr<ICoreWebView2Controller> g_controller;
extern ComPtr<ICoreWebView2> g_webview;

// Initialize WebView2 (async — returns immediately)
void InitWebView(HWND hwnd);

// Window procedure
LRESULT CALLBACK WebUIProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);