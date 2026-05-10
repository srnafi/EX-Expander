#include "resource.h"
#include <WebView2.h>

#include "webUI.h"
#include "WebViewManager.h"

// ---------------------------------------------------------------------------
// OPEN WINDOW
// ---------------------------------------------------------------------------

void OpenWebUI(HINSTANCE hInstance)
{
    if (g_uiWindow) return;

    const wchar_t CLASS_NAME[] = L"EasyEmojiWebUI";

    WNDCLASS wc{};
    wc.lpfnWndProc = WebUIProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClass(&wc);

    g_uiWindow = CreateWindowEx(
        0, CLASS_NAME, L"EX-Expander Manager",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 700, 560,
        nullptr, nullptr, hInstance, nullptr);

    ShowWindow(g_uiWindow, SW_SHOW);
    UpdateWindow(g_uiWindow);

    InitWebView(g_uiWindow);


    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    PostQuitMessage(0);
}