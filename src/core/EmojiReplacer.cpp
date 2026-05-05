#include "EmojiReplacer.h"

#include <windows.h>
#include <vector>
#include <thread>
#include <string>

// ---------------------------------------------------------------------------
// CLIPBOARD UTILITIES
// ---------------------------------------------------------------------------

// RAII helper to ensure clipboard is always closed properly
class ClipboardGuard
{
public:
    ClipboardGuard() : opened(OpenClipboardWithRetry()) {}
    ~ClipboardGuard()
    {
        if (opened)
            CloseClipboard();
    }

    bool IsOpen() const { return opened; }

private:
    static bool OpenClipboardWithRetry(int maxRetries = 10)
    {
        for (int i = 0; i < maxRetries; ++i)
        {
            if (OpenClipboard(nullptr))
                return true;

            Sleep(2); // small delay before retry
        }
        return false;
    }

    bool opened;
};

// ---------------------------------------------------------------------------
// GET CLIPBOARD TEXT
// ---------------------------------------------------------------------------

static std::wstring GetClipboardText()
{
    std::wstring result;

    ClipboardGuard guard;
    if (!guard.IsOpen())
        return result;

    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (!hData)
        return result;

    const wchar_t* text = static_cast<const wchar_t*>(GlobalLock(hData));
    if (!text)
        return result;

    result = text;

    GlobalUnlock(hData);
    return result;
}

// ---------------------------------------------------------------------------
// SET CLIPBOARD TEXT
// ---------------------------------------------------------------------------

static void SetClipboardTextSafe(const std::wstring& text)
{
    ClipboardGuard guard;
    if (!guard.IsOpen())
        return;

    // Clear existing clipboard contents
    EmptyClipboard();

    const size_t sizeBytes = (text.size() + 1) * sizeof(wchar_t);

    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, sizeBytes);
    if (!hMem)
        return;

    void* ptr = GlobalLock(hMem);
    if (!ptr)
    {
        GlobalFree(hMem);
        return;
    }

    memcpy(ptr, text.c_str(), sizeBytes);
    GlobalUnlock(hMem);

    // IMPORTANT:
    // After SetClipboardData succeeds, system owns the memory
    if (!SetClipboardData(CF_UNICODETEXT, hMem))
    {
        GlobalFree(hMem); // only free on failure
    }
}

// ---------------------------------------------------------------------------
// ASYNC CLIPBOARD RESTORE
// ---------------------------------------------------------------------------

// Restores previous clipboard content after paste completes.
// Delay is required because target apps read clipboard asynchronously.
static void RestoreClipboardAsync(std::wstring oldClipboard)
{
    std::thread([oldClipboard = std::move(oldClipboard)]()
        {
            // NOTE:
            // 300ms–500ms is usually enough.
            // 3000ms is very safe but slightly overkill.
            Sleep(3000);

            SetClipboardTextSafe(oldClipboard);
        }).detach();
}

// ---------------------------------------------------------------------------
// INPUT HELPERS
// ---------------------------------------------------------------------------

// Adds a key press + release to input list
static void AddKey(std::vector<INPUT>& inputs, WORD vk, bool keyUp = false)
{
    INPUT in{};
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = vk;
    if (keyUp)
        in.ki.dwFlags = KEYEVENTF_KEYUP;

    inputs.push_back(in);
}

// ---------------------------------------------------------------------------
// MAIN FUNCTION
// ---------------------------------------------------------------------------

void ReplaceWithEmoji(const std::wstring& token, const std::wstring& emoji)
{
    // -----------------------------------------------------------------------
    // STEP 1: DELETE EXISTING TOKEN (":token")
    // -----------------------------------------------------------------------

    const int deleteCount = static_cast<int>(token.size()) + 1;

    wchar_t dbg[128];
    swprintf(dbg, 128, L"[Replace] deleteCount: %d\n", deleteCount);
    OutputDebugStringW(dbg);

    std::vector<INPUT> inputs;
    inputs.reserve(deleteCount * 2);

    for (int i = 0; i < deleteCount; ++i)
    {
        AddKey(inputs, VK_BACK, false); // key down
        AddKey(inputs, VK_BACK, true);  // key up
    }

    SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));

    // -----------------------------------------------------------------------
    // STEP 2: BACKUP CLIPBOARD
    // -----------------------------------------------------------------------

    std::wstring oldClipboard = GetClipboardText();

    // -----------------------------------------------------------------------
    // STEP 3: SET EMOJI TO CLIPBOARD
    // -----------------------------------------------------------------------

    SetClipboardTextSafe(emoji);

    // -----------------------------------------------------------------------
    // STEP 4: SIMULATE CTRL + V (PASTE)
    // -----------------------------------------------------------------------

    INPUT paste[4]{};

    paste[0].type = INPUT_KEYBOARD;
    paste[0].ki.wVk = VK_CONTROL;

    paste[1].type = INPUT_KEYBOARD;
    paste[1].ki.wVk = 'V';

    paste[2].type = INPUT_KEYBOARD;
    paste[2].ki.wVk = 'V';
    paste[2].ki.dwFlags = KEYEVENTF_KEYUP;

    paste[3].type = INPUT_KEYBOARD;
    paste[3].ki.wVk = VK_CONTROL;
    paste[3].ki.dwFlags = KEYEVENTF_KEYUP;

    SendInput(4, paste, sizeof(INPUT));

    // -----------------------------------------------------------------------
    // STEP 5: RESTORE ORIGINAL CLIPBOARD (ASYNC)
    // -----------------------------------------------------------------------

    RestoreClipboardAsync(std::move(oldClipboard));
}