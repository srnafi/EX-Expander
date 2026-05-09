#include "KeyboardHook.h"
#include "AppLog.h"
#include "InputBuffer.h"
#include "Database.h"
#include "TextInsertion.h"
#include "Popup.h"
#include "PopupInput.h"
#include "Globals.h"
#include "Helper.h"

#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")

// ===========================================================================
// MODULE STATE
// ===========================================================================

namespace
{
    HHOOK g_hook = nullptr;

    // -----------------------------------------------------------------------
    // Find an expansion whose token matches exactly (case-sensitive)
    // -----------------------------------------------------------------------
    const Expansion* FindExactMatch(const std::vector<Expansion>& matches,
        const std::wstring& token)
    {
        for (const auto& m : matches)
            if (m.token == token)
                return &m;

        return nullptr;
    }

    // -----------------------------------------------------------------------
    // Try to expand the current buffer content and fire the replacement.
    // Returns true if an exact match was found and fired (caller should
    // consume the key), false otherwise.
    // -----------------------------------------------------------------------
    bool TryExpand()
    {
        std::wstring token = BufferGetToken();
        if (token.empty())
            return false;

        const auto matches = DB_Search(token);
        const Expansion* ex = FindExactMatch(matches, token);

        if (!ex)
            return false;

        ReplaceWithExpansion(token, ex->value);
        BufferReset();
        PopupUpdateInternal();

        return true;
    }

    // -----------------------------------------------------------------------
    // LOW-LEVEL KEYBOARD HOOK PROCEDURE
    //
    // Flow:
    //   ESC / TAB          → cancel buffer, pass key through
    //   UP / DOWN          → navigate popup, consume key
    //   ENTER (navigating) → confirm popup selection, consume key
    //   Trigger (symbol)   → attempt expand, consume key on match
    //   SPACE (space mode) → attempt expand, consume key on match
    //   Any other key      → feed to buffer, update popup, pass through
    // -----------------------------------------------------------------------
    LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
    {
        // Always pass to next hook when nCode < 0 (Windows requirement)
        if (nCode < 0)
            return CallNextHookEx(g_hook, nCode, wParam, lParam);

        // Per-app allow list: if the foreground app is excluded, do nothing
        if (!IsAppAllowed())
            return CallNextHookEx(g_hook, nCode, wParam, lParam);

        // Key-down events only
        if (wParam != WM_KEYDOWN)
            return CallNextHookEx(g_hook, nCode, wParam, lParam);

        const KBDLLHOOKSTRUCT* kb = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);

        // Ignore injected input – prevents recursion when we call SendInput
        if (kb->flags & LLKHF_INJECTED)
            return CallNextHookEx(g_hook, nCode, wParam, lParam);

        const DWORD vk = kb->vkCode;

        // -------------------------------------------------------------------
        // 1. CANCEL  –  ESC or TAB while buffer is active
        // -------------------------------------------------------------------
        if (inTheBuffer && (vk == VK_ESCAPE || vk == VK_TAB))
        {
            BufferReset();
            PopupUpdateInternal();
            return CallNextHookEx(g_hook, nCode, wParam, lParam);
        }

        // -------------------------------------------------------------------
        // 2. POPUP NAVIGATION  –  UP / DOWN arrows
        // -------------------------------------------------------------------
        if (inTheBuffer && (vk == VK_UP || vk == VK_DOWN))
        {
            PopupHandleKeyInternal(vk);
            PopupUpdateInternal();
            return 1;   // consume – do not type arrows into the target app
        }

        // -------------------------------------------------------------------
        // 3. POPUP CONFIRM  –  ENTER while navigating the popup list
        // -------------------------------------------------------------------
        if (inTheBuffer && vk == VK_RETURN && IsPopupNavigating())
        {
            PopupHandleKeyInternal(vk);
            PopupUpdateInternal();
            return 1;
        }

        // -------------------------------------------------------------------
        // 4. SYMBOL-MODE CONFIRM
        //    Second trigger character (e.g. :smile:) fires the expansion.
        //    If no exact match exists, reset and let the character through.
        // -------------------------------------------------------------------
        if (!g_InsertOnSpace && inTheBuffer && IsTriggerKey(vk))
        {
            if (TryExpand())
                return 1;   // match found – consume the closing trigger char

            // No match – reset buffer, pass the trigger character through
            BufferReset();
            return CallNextHookEx(g_hook, nCode, wParam, lParam);
        }

        // -------------------------------------------------------------------
        // 5. SPACE-MODE CONFIRM
        //    Space fires the expansion when space-mode is active.
        //    Always let Space through to the target app afterward.
        // -------------------------------------------------------------------
        if (vk == VK_SPACE)
        {
            if (g_InsertOnSpace && inTheBuffer)
                TryExpand();    // fires replacement if match found

            // Whether or not we matched, reset + dismiss and pass Space through
            BufferReset();
            PopupUpdateInternal();
            return CallNextHookEx(g_hook, nCode, wParam, lParam);
        }

        // -------------------------------------------------------------------
        // 6. BUFFER FEED
        //    Any other key: if we are in the buffer (or this IS the trigger
        //    key that starts the buffer), feed it and refresh the popup.
        // -------------------------------------------------------------------
        if (IsTriggerKey(vk) || inTheBuffer)
            BufferHandleKey(vk);

        if (inTheBuffer)
            PopupUpdateInternal();

        return CallNextHookEx(g_hook, nCode, wParam, lParam);
    }

} // namespace

// ===========================================================================
// PUBLIC API
// ===========================================================================

bool InstallHook()
{
    g_hook = SetWindowsHookEx(
        WH_KEYBOARD_LL,
        LowLevelKeyboardProc,
        nullptr,
        0);

    if (!g_hook)
    {
        AppLog::Error(L"Failed to install keyboard hook. Error: " +
            std::to_wstring(GetLastError()));
        return false;
    }

    AppLog::Info(L"Keyboard hook installed");
    return true;
}

void UninstallHook()
{
    if (g_hook)
    {
        UnhookWindowsHookEx(g_hook);
        g_hook = nullptr;
        AppLog::Info(L"Keyboard hook uninstalled");
    }
}