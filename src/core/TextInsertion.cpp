#include "TextInsertion.h"
#include "AppLog.h"

#include <windows.h>
#include <vector>
#include <string>
#include <thread>

// ===========================================================================
// CLIPBOARD GUARD  –  RAII wrapper, ensures clipboard is always closed
// ===========================================================================

class ClipboardGuard
{
public:
    ClipboardGuard() : m_open(TryOpen()) {}
    ~ClipboardGuard() { if (m_open) CloseClipboard(); }

    bool IsOpen() const { return m_open; }

    // Non-copyable
    ClipboardGuard(const ClipboardGuard&) = delete;
    ClipboardGuard& operator=(const ClipboardGuard&) = delete;

private:
    bool m_open;

    static bool TryOpen(int retries = 10)
    {
        for (int i = 0; i < retries; ++i)
        {
            if (OpenClipboard(nullptr))
                return true;

            Sleep(5);
        }

        AppLog::Error(L"Failed to open clipboard after retries");
        return false;
    }
};

// ===========================================================================
// INTERNAL HELPERS  –  anonymous namespace, not visible outside this file
// ===========================================================================

namespace
{
    // -----------------------------------------------------------------------
    // Read current clipboard text
    // -----------------------------------------------------------------------
    std::wstring GetClipboardText()
    {
        ClipboardGuard guard;
        if (!guard.IsOpen())
            return {};

        HANDLE hData = GetClipboardData(CF_UNICODETEXT);
        if (!hData)
            return {};

        const wchar_t* text = static_cast<const wchar_t*>(GlobalLock(hData));
        if (!text)
            return {};

        std::wstring result(text);
        GlobalUnlock(hData);

        return result;
    }

    // -----------------------------------------------------------------------
    // Write text to clipboard
    // -----------------------------------------------------------------------
    bool SetClipboardText(const std::wstring& text)
    {
        ClipboardGuard guard;
        if (!guard.IsOpen())
            return false;

        EmptyClipboard();

        const size_t bytes = (text.size() + 1) * sizeof(wchar_t);

        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (!hMem)
        {
            AppLog::Error(L"GlobalAlloc failed when setting clipboard");
            return false;
        }

        void* ptr = GlobalLock(hMem);
        if (!ptr)
        {
            GlobalFree(hMem);
            AppLog::Error(L"GlobalLock failed when setting clipboard");
            return false;
        }

        memcpy(ptr, text.c_str(), bytes);
        GlobalUnlock(hMem);

        // System takes ownership of hMem on success – only free on failure
        if (!SetClipboardData(CF_UNICODETEXT, hMem))
        {
            GlobalFree(hMem);
            AppLog::Error(L"SetClipboardData failed");
            return false;
        }

        return true;
    }

    // -----------------------------------------------------------------------
    // Restore previous clipboard content after a short delay.
    //
    // Why async?  The target application reads the clipboard after receiving
    // the WM_PASTE / Ctrl+V message, which happens on its own thread/message
    // loop. We must not overwrite the clipboard before it has had a chance
    // to read it. 600ms covers virtually every real-world app without the
    // 3-second penalty that users would notice.
    // -----------------------------------------------------------------------
    void RestoreClipboardAsync(std::wstring previous)
    {
        std::thread([prev = std::move(previous)]()
            {
                Sleep(600);
                SetClipboardText(prev);
            }).detach();
    }

    // -----------------------------------------------------------------------
    // Append a key-down or key-up event to an input list
    // -----------------------------------------------------------------------
    void PushKey(std::vector<INPUT>& inputs, WORD vk, bool keyUp = false)
    {
        INPUT in{};
        in.type = INPUT_KEYBOARD;
        in.ki.wVk = vk;
        in.ki.dwFlags = keyUp ? KEYEVENTF_KEYUP : 0;
        inputs.push_back(in);
    }

    // -----------------------------------------------------------------------
    // Send backspace presses to erase the trigger token.
    //
    // We erase (token.size() + 1) characters:
    //   +1 accounts for the trigger character the user typed (e.g. Space or
    //   Enter) that fired the expansion, if your hook fires on that character.
    //   If your hook fires before that character is committed, change +1 to 0.
    // -----------------------------------------------------------------------
    void EraseToken(const std::wstring& token)
    {
        const int count = static_cast<int>(token.size()) + 1;

        // Each backspace needs a key-down + key-up = 2 INPUT structs
        std::vector<INPUT> inputs;
        inputs.reserve(static_cast<size_t>(count) * 2);

        for (int i = 0; i < count; ++i)
        {
            PushKey(inputs, VK_BACK, false);    // key down
            PushKey(inputs, VK_BACK, true);     // key up
        }

        UINT sent = SendInput(static_cast<UINT>(inputs.size()),
            inputs.data(), sizeof(INPUT));

        if (sent != inputs.size())
            AppLog::Warn(L"SendInput: not all backspace events were sent");
    }

    // -----------------------------------------------------------------------
    // Paste via Ctrl+V
    // -----------------------------------------------------------------------
    void SendPaste()
    {
        INPUT keys[4]{};

        // Ctrl down
        keys[0].type = INPUT_KEYBOARD;
        keys[0].ki.wVk = VK_CONTROL;

        // V down
        keys[1].type = INPUT_KEYBOARD;
        keys[1].ki.wVk = 'V';

        // V up
        keys[2].type = INPUT_KEYBOARD;
        keys[2].ki.wVk = 'V';
        keys[2].ki.dwFlags = KEYEVENTF_KEYUP;

        // Ctrl up
        keys[3].type = INPUT_KEYBOARD;
        keys[3].ki.wVk = VK_CONTROL;
        keys[3].ki.dwFlags = KEYEVENTF_KEYUP;

        UINT sent = SendInput(4, keys, sizeof(INPUT));
        if (sent != 4)
            AppLog::Warn(L"SendInput: Ctrl+V paste may not have been sent fully");
    }

} // namespace

// ===========================================================================
// PUBLIC API
// ===========================================================================

void ReplaceWithExpansion(const std::wstring& token, const std::wstring& value)
{
    // Back up whatever is currently on the clipboard
    std::wstring previousClipboard = GetClipboardText();

    // Erase the token the user typed
    EraseToken(token);

    // Put the expansion value on the clipboard
    if (!SetClipboardText(value))
    {
        AppLog::Error(L"ReplaceWithExpansion: could not set clipboard, aborting");
        return;
    }

    // Paste it into the active application
    SendPaste();

    AppLog::Info(L"Expanded: " + token);

    // Put the user's original clipboard content back
    RestoreClipboardAsync(std::move(previousClipboard));
}