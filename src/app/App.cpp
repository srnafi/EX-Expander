#include <windows.h>
#include <shellapi.h>
#include "Globals.h"
#include "KeyboardHook.h"
#include "Database.h"
#include "webUI.h"
#include <shellscalingapi.h>
#include "popup.h"
#pragma comment(lib, "Shcore.lib")
#include "InstalledApps.h"

// -----------------------------------------------------------------------
// Tray constants
// -----------------------------------------------------------------------
#define WM_TRAYICON         (WM_USER + 1)
#define ID_TRAY_MANAGE      1001
#define ID_TRAY_EXIT        1002
#define TRAY_UID            1

static NOTIFYICONDATAW  g_nid = {};
static HWND             g_hTray = nullptr;

// -----------------------------------------------------------------------
// Load settings from DB and apply to runtime globals
// Called once after DB_Open() to restore persisted settings.
// -----------------------------------------------------------------------
static void LoadSettingsFromDB()
{
    // Trigger character (':' or ';')
    std::wstring sym = DB_GetSetting(L"emojiSymbol", L":");
    g_TriggerChar = (!sym.empty() && sym[0] == L';') ? L';' : L':';

    // Max popup items (1-10)
    std::wstring maxStr = DB_GetSetting(L"maxPopup", L"5");
    int maxVal = _wtoi(maxStr.c_str());
    g_MaxPopupItems = max(1, min(10, (maxVal > 0 ? maxVal : 5)));

    // Insert trigger ("space" or "symbol")
    std::wstring trigger = DB_GetSetting(L"insertTrigger", L"space");
    g_InsertOnSpace = (trigger != L"symbol");
}

// -----------------------------------------------------------------------
// Tray icon helpers
// -----------------------------------------------------------------------
static void Tray_Add(HWND hWnd)
{
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = hWnd;
    g_nid.uID = TRAY_UID;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wcscpy_s(g_nid.szTip, L"EX-Expander — running");
    Shell_NotifyIconW(NIM_ADD, &g_nid);
}

static void Tray_Remove()
{
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
}

// -----------------------------------------------------------------------
// Right-click context menu
// -----------------------------------------------------------------------
static void Tray_ShowContextMenu(HWND hWnd)
{
    POINT pt;
    GetCursorPos(&pt);

    HMENU hMenu = CreatePopupMenu();
    InsertMenuW(hMenu, 0, MF_BYPOSITION | MF_STRING, ID_TRAY_MANAGE, L"Manage Expansions");
    InsertMenuW(hMenu, 1, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
    InsertMenuW(hMenu, 2, MF_BYPOSITION | MF_STRING, ID_TRAY_EXIT, L"Exit");

    SetForegroundWindow(hWnd);
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN,
        pt.x, pt.y, 0, hWnd, nullptr);
    DestroyMenu(hMenu);
}

// -----------------------------------------------------------------------
// Tray host window proc
// -----------------------------------------------------------------------
static LRESULT CALLBACK TrayWndProc(HWND hWnd, UINT msg,
    WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP)
            Tray_ShowContextMenu(hWnd);
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case ID_TRAY_MANAGE:
            // Open the management UI in its own thread (has its own message loop)
            CreateThread(
                nullptr, 0,
                [](LPVOID param) -> DWORD
                {
                    OpenWebUI((HINSTANCE)param);
                    return 0;
                },
                g_hInstance, 0, nullptr);
            break;

        case ID_TRAY_EXIT:
            PostQuitMessage(0);
            break;
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// -----------------------------------------------------------------------
// WinMain
// -----------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);

    // -- Database ---------------------------------------------------------
    if (!DB_Open())
    {
        MessageBoxW(nullptr, L"Failed to open database", L"EX-Expander", MB_OK);
        return 1;
    }

    // Seed default emoji data on first run
    if (DB_IsEmpty())
        SeedFromJson(L"emoji.json");
    // Restore persisted settings into runtime globals
    LoadSettingsFromDB();

    // -- Keyboard hook ----------------------------------------------------
    g_hInstance = hInstance;
   
    if (!InstallHook())
    {
        wchar_t msg[128];
        swprintf_s(msg, L"Failed to install hook. Error: %lu", GetLastError());
        MessageBoxW(nullptr, msg, L"EX-Expander", MB_OK);
        DB_Close();
        return 1;
    }

    PopupInit(hInstance);

    // -- Tray host window -------------------------------------------------
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = TrayWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"EasyEmojiTray";
    RegisterClassExW(&wc);

    g_hTray = CreateWindowExW(0, L"EasyEmojiTray", nullptr,
        0, 0, 0, 0, 0,
        HWND_MESSAGE, nullptr, hInstance, nullptr);

    Tray_Add(g_hTray);

    // -- Main message loop ------------------------------------------------
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // -- Cleanup ----------------------------------------------------------
    Tray_Remove();
    UninstallHook();
    DB_Close();
    return 0;
}
