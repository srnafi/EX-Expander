#include "KeyboardHook.h"
#include "InputBuffer.h"
#include "EmojiMatcher.h"
#include "EmojiReplacer.h"
#include "popup.h"
#include "Globals.h"
#include "Helper.h"
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#include <windows.h>
#include <PopupInput.h>

static HHOOK g_hook = nullptr;

// ---------------------------------------------------------------------------
// Exact-match helper
// ---------------------------------------------------------------------------

static const Expansion* FindExactMatch(const std::vector<Expansion>& matches,
    const std::wstring& token)
{
    for (const auto& m : matches)
        if (m.token == token) return &m;
    return nullptr;
}

// ---------------------------------------------------------------------------
// LOW LEVEL KEYBOARD HOOK
// ---------------------------------------------------------------------------

static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode < 0)
        return CallNextHookEx(g_hook, nCode, wParam, lParam);
    if (!IsAppAllowed())
        return CallNextHookEx(g_hook, nCode, wParam, lParam);
    // Key-down events only
    if (wParam != WM_KEYDOWN)
        return CallNextHookEx(g_hook, nCode, wParam, lParam);

    const KBDLLHOOKSTRUCT* kb = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);

    // Ignore injected input (prevents recursion from SendInput / paste)
    if (kb->flags & LLKHF_INJECTED)
        return CallNextHookEx(g_hook, nCode, wParam, lParam);

    const DWORD vk = kb->vkCode;

    // -----------------------------------------------------------------------
    // 1. CANCEL  (ESC / TAB / SPACE when popup is visible)
    //    Dismiss instantly and pass the key through to the target app.
    // -----------------------------------------------------------------------
    if (inTheBuffer && (vk == VK_ESCAPE || vk == VK_TAB))
    {
        BufferReset();   // clears inTheBuffer
        PopupUpdateInternal();   // hides popup
        return CallNextHookEx(g_hook, nCode, wParam, lParam);  // let key through
    }

    // -----------------------------------------------------------------------
    // 2. POPUP NAVIGATION  (UP / DOWN)
    // -----------------------------------------------------------------------
    if (inTheBuffer && (vk == VK_UP || vk == VK_DOWN))
    {
        PopupHandleKeyInternal(vk);
        PopupUpdateInternal();
        return 1;
    }

    // -----------------------------------------------------------------------
    // 3. CONFIRM SELECTION  (ENTER)
    // -----------------------------------------------------------------------
    if (inTheBuffer && vk == VK_RETURN && IsPopupNavigating())
    {
        PopupHandleKeyInternal(vk);
        PopupUpdateInternal();
        return 1;
    }

    // -----------------------------------------------------------------------
    // 2.5. SYMBOL-MODE COMPLETION
    //      When the user has chosen "symbol" as insert trigger, pressing the
    //      trigger character a second time (e.g. :smile:) confirms the match.
    // -----------------------------------------------------------------------
    if (!g_InsertOnSpace && inTheBuffer && IsTriggerKey(vk))
    {
        std::wstring token = BufferGetToken();

        if (!token.empty())
        {
            const auto matches = GetMatches(token);
            const Expansion* exact = FindExactMatch(matches, token);

            if (exact)
            {
                ReplaceWithEmoji(token, exact->value);
                BufferReset();
                PopupUpdateInternal();
                return 1;   // consume second trigger char
            }
        }

        // No exact match – reset buffer and pass the trigger char through
        BufferReset();
        return CallNextHookEx(g_hook, nCode, wParam, lParam);
    }

    // -----------------------------------------------------------------------
    // 4. SPACE TRIGGER  (replace on Space – only when space mode is active)
    // -----------------------------------------------------------------------
    if (vk == VK_SPACE)
    {
        if (g_InsertOnSpace && inTheBuffer)
        {
            std::wstring token = BufferGetToken();

            if (!token.empty())
            {
                const auto matches = GetMatches(token);
                const Expansion* exact = FindExactMatch(matches, token);

                if (exact)
                {
                    ReplaceWithEmoji(token, exact->value);
                    BufferReset();
                    PopupUpdateInternal();
                    return 1;   // consume space
                }
            }
        }

        // No match, space mode off, or symbol mode — reset + dismiss popup, let Space through
        BufferReset();
        PopupUpdateInternal();
        return CallNextHookEx(g_hook, nCode, wParam, lParam);
    }

    // -----------------------------------------------------------------------
    // 5. BUFFER UPDATE
    //    Feed key to buffer when: it's the trigger key, or buffer is active
    // -----------------------------------------------------------------------
    if (IsTriggerKey(vk) || inTheBuffer)
    {
        BufferHandleKey(vk);
    }

    // -----------------------------------------------------------------------
    // 6. POPUP UPDATE
    // -----------------------------------------------------------------------
    if (inTheBuffer)
    {
        PopupUpdateInternal();
    }

    return CallNextHookEx(g_hook, nCode, wParam, lParam);
}



// ---------------------------------------------------------------------------
// INSTALL / UNINSTALL
// ---------------------------------------------------------------------------

bool InstallHook()
{
    OutputDebugStringW(L"[EX-Expander] Installing keyboard hook...\n");

    g_hook = SetWindowsHookEx(
        WH_KEYBOARD_LL,
        LowLevelKeyboardProc,
        nullptr,
        0);

    if (!g_hook)
    {
        wchar_t msg[128];
        swprintf(msg, 128,
            L"[EX-Expander] Hook failed! Error: %lu\n", GetLastError());
        OutputDebugStringW(msg);
        return false;
    }

    OutputDebugStringW(L"[EX-Expander] Hook installed.\n");
    return true;
}

void UninstallHook()
{
    if (g_hook)
    {
        UnhookWindowsHookEx(g_hook);
        g_hook = nullptr;
    }
}