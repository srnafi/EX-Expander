#include "InputBuffer.h"
#include "Globals.h"
#include <windows.h>
#include <cwctype>

// ---------------------------------------------------------------------------
// INTERNAL STATE
// ---------------------------------------------------------------------------

static std::wstring g_buffer;
bool inTheBuffer = false;

// ---------------------------------------------------------------------------
// TRIGGER KEY CHECK
// Returns true when the pressed key produces g_TriggerChar on the current
// keyboard layout.  Only VK_OEM_1 is considered (handles US ';'/':').
// ---------------------------------------------------------------------------

bool IsTriggerKey(DWORD vkCode)
{
    if (vkCode != VK_OEM_1) return false;

    bool shiftDown = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

    if (g_TriggerChar == L':') return shiftDown;   // Shift+; = :
    if (g_TriggerChar == L';') return !shiftDown;  // bare ;

    return false;
}

// ---------------------------------------------------------------------------
// RESET
// ---------------------------------------------------------------------------

void BufferReset()
{
    g_buffer.clear();
    inTheBuffer = false;
}

// ---------------------------------------------------------------------------
// KEY HANDLING
// ---------------------------------------------------------------------------

void BufferHandleKey(DWORD vkCode)
{
    // -----------------------------------------------------------------------
    // Hard-reset keys
    // -----------------------------------------------------------------------
    if (vkCode == VK_SPACE ||
        vkCode == VK_ESCAPE ||
        vkCode == VK_RETURN ||
        vkCode == VK_TAB)
    {
        BufferReset();
        return;
    }

    // -----------------------------------------------------------------------
    // Backspace
    // -----------------------------------------------------------------------
    if (vkCode == VK_BACK && inTheBuffer)
    {
        if (!g_buffer.empty())
            g_buffer.pop_back();

        // Exit if trigger char is gone
        if (g_buffer.empty() || g_buffer.find(g_TriggerChar) == std::wstring::npos)
            BufferReset();

        return;
    }

    // -----------------------------------------------------------------------
    // Trigger character – start/continue buffer mode
    // -----------------------------------------------------------------------
    if (IsTriggerKey(vkCode))
    {
        g_buffer += g_TriggerChar;
        inTheBuffer = true;
        return;
    }

    // -----------------------------------------------------------------------
    // Character input (only while in buffer mode)
    // -----------------------------------------------------------------------
    if (!inTheBuffer)
        return;

    BYTE keyboardState[256];
    if (!GetKeyboardState(keyboardState))
        return;

    const bool shiftPressed = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    keyboardState[VK_SHIFT] = shiftPressed ? 0x80 : 0;
    keyboardState[VK_LSHIFT] = keyboardState[VK_SHIFT];
    keyboardState[VK_RSHIFT] = keyboardState[VK_SHIFT];

    WCHAR unicodeBuffer[4] = { 0 };

    int result = ToUnicode(
        vkCode,
        MapVirtualKeyW(vkCode, MAPVK_VK_TO_VSC),
        keyboardState,
        unicodeBuffer,
        4,
        0);

    if (result > 0)
    {
        for (int i = 0; i < result; ++i)
        {
            wchar_t ch = towlower(unicodeBuffer[i]);
            g_buffer += ch;
        }

        // Cap buffer size to 64 characters
        constexpr size_t kMaxBufferSize = 64;
        if (g_buffer.size() > kMaxBufferSize)
            g_buffer.erase(0, g_buffer.size() - kMaxBufferSize);
    }
}

// ---------------------------------------------------------------------------
// TOKEN EXTRACTION  (text after the last g_TriggerChar)
// ---------------------------------------------------------------------------

std::wstring BufferGetToken()
{
    for (int i = (int)g_buffer.size() - 1; i >= 0; --i)
    {
        wchar_t c = g_buffer[i];

        if (c == g_TriggerChar)
            return g_buffer.substr(i + 1);

        // Abort on invalid character
        if (!(iswalnum(c) ||
            c == L'_' || c == L'-' || c == L'.' ||
            c == L'/' || c == L'\\' ||
            c == L'(' || c == L')' ||
            c == L'[' || c == L']'))
        {
            return L"";
        }
    }

    return L"";
}