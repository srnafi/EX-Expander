#include "PopupInput.h"
#include "PopupState.h"
#include "InputBuffer.h"
#include "PopupAnimation.h"
#include "Database.h"
#include "PopupLayout.h"
#include "PopupRenderer.h"
#include "TextInsertion.h"
#include "PopupUtils.h"
#include "Globals.h"
#include "AppLog.h"
#include <windows.h>
#include <algorithm>

void PopupHandleKeyInternal(int vkCode)
{
    State::lastKey = vkCode;
}

void PopupUpdateInternal()
{
    using namespace State;

    // -----------------------------------------------------------------------
    // HIDE: buffer cleared
    // -----------------------------------------------------------------------
    if (!inTheBuffer)
    {
        StopTimer();
        ShowWindow(hwnd, SW_HIDE);
        centerIndex = 0;
        State::currentToken.clear();
        scrollOffset.snap(0.f);
        opacity.snap(0.f);
        return;
    }

    // -----------------------------------------------------------------------
    // QUERY
    // -----------------------------------------------------------------------
    std::wstring token = BufferGetToken();

    if (token.empty())
    {
        centerIndex = 0;
        State::currentToken.clear();
        StopTimer();
        ShowWindow(hwnd, SW_HIDE);
        scrollOffset.snap(0.f);
        opacity.snap(0.f);
        return;
    }

    State::currentToken = token;

    g_FilteredExpansions = DB_Search(token);

    const int total = static_cast<int>(g_FilteredExpansions.size());

    if (total == 0)
    {
        centerIndex = 0;
        StopTimer();
        ShowWindow(hwnd, SW_HIDE);
        return;
    }

    ClampIndex(centerIndex, total);

    // -----------------------------------------------------------------------
    // PROCESS KEY
    // -----------------------------------------------------------------------
    const int key = lastKey;
    lastKey = 0;

    if (key == VK_DOWN || key == VK_UP)
    {
        if (key == VK_DOWN)
        {
            centerIndex = (centerIndex + 1) % total;
            scrollOffset.value += 1.f;
        }
        else
        {
            centerIndex = (centerIndex - 1 + total) % total;
            scrollOffset.value -= 1.f;
        }

        scrollOffset.setTarget(0.f);
    }
    else if (key == VK_RETURN)
    {
        if (centerIndex >= 0 && centerIndex < total)
        {
            AppLog::Info(
                L"Popup selected: " +
                g_FilteredExpansions[centerIndex].token);

            ReplaceWithExpansion(
                token,
                g_FilteredExpansions[centerIndex].value);
        }
        else
        {
            AppLog::Error(
                L"PopupUpdateInternal: centerIndex out of range on RETURN");
        }

        BufferReset();
        State::currentToken.clear();
        StopTimer();
        ShowWindow(hwnd, SW_HIDE);
        scrollOffset.snap(0.f);
        opacity.snap(0.f);
        return;
    }

    // -----------------------------------------------------------------------
    // POSITION / RESIZE WINDOW
    // -----------------------------------------------------------------------
    const int visible = (std::min)(total, g_MaxPopupItems);
    const int H =
        visible * UI::ItemHeight +
        UI::Padding * 2 +
        static_cast<int>(UI::HeaderHeight);

    const bool wasHidden = !IsWindowVisible(hwnd);

    if (wasHidden)
    {
        int posX = 0;
        int posY = 0;

        if (g_PopupPosition == L"cursor")
        {
            POINT cur{};
            GetCursorPos(&cur);
            posX = cur.x;
            posY = cur.y - H - 8;
        }
        else
        {
            posX = State::popupFixedX;
            posY = State::popupFixedY;
        }

        SetWindowPos(
            hwnd, nullptr,
            posX, posY,
            static_cast<int>(UI::MaxWidth), H,
            SWP_NOZORDER | SWP_NOACTIVATE);

        scrollOffset.snap(0.f);
        opacity.snap(0.f);

        ShowWindow(hwnd, SW_SHOWNA);
    }
    else
    {
        SetWindowPos(
            hwnd, nullptr,
            0, 0,
            static_cast<int>(UI::MaxWidth), H,
            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    // -----------------------------------------------------------------------
    // ANIMATE / RENDER
    // -----------------------------------------------------------------------
    opacity.setTarget(1.f);

    if (!Anim::animEnabled)
    {
        scrollOffset.snap(0.f);
        opacity.snap(1.f);
        StopTimer();
        RenderLayered();
        return;
    }

    StartTimer();
    Tick();
}