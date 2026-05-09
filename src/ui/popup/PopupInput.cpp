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

// ---------------------------------------------------------------------------
// PopupHandleKeyInternal
// Stores the key for this frame. PopupUpdateInternal reads and clears it.
// ---------------------------------------------------------------------------

void PopupHandleKeyInternal(int vkCode)
{
    State::lastKey = vkCode;
}

// ---------------------------------------------------------------------------
// PopupUpdateInternal
// ---------------------------------------------------------------------------

void PopupUpdateInternal()
{
    using namespace State;

    // -----------------------------------------------------------------------
    // HIDE: buffer was cleared (expansion fired or ESC pressed)
    // -----------------------------------------------------------------------
    if (!inTheBuffer)
    {
        StopTimer();
        ShowWindow(hwnd, SW_HIDE);
        centerIndex = 0;
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
        StopTimer();
        ShowWindow(hwnd, SW_HIDE);
        scrollOffset.snap(0.f);
        opacity.snap(0.f);
        return;
    }

    g_FilteredExpansions = DB_Search(token);

    const int total = static_cast<int>(g_FilteredExpansions.size());

    if (total == 0)
    {
        centerIndex = 0;
        StopTimer();
        ShowWindow(hwnd, SW_HIDE);
        return;
    }
    
    // Clamp once up front so all branches below work with a valid index
    ClampIndex(centerIndex, total);

    // -----------------------------------------------------------------------
    // PROCESS KEY
    // -----------------------------------------------------------------------
    const int key = lastKey;
    lastKey = 0;

    if (key == VK_DOWN || key == VK_UP)
    {
        // DOWN: next item slides UP into center from below.
        //   centerIndex advances, scrollOffset jumps to +1 (below center),
        //   spring pulls it back to 0 (items glide upward).
        //
        // UP: previous item slides DOWN into center from above.
        //   centerIndex retreats, scrollOffset jumps to -1 (above center),
        //   spring pulls it back to 0 (items glide downward).

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

        // Width snaps instantly to the new selection — no animation on width
        currentWidth = ComputeTargetWidth();
    }
    else if (key == VK_RETURN)
    {
        // Safety check before indexing (ClampIndex already ran above)
        if (centerIndex >= 0 && centerIndex < total)
        {
            AppLog::Info(L"Popup selected: " + g_FilteredExpansions[centerIndex].token);
            ReplaceWithExpansion(token, g_FilteredExpansions[centerIndex].value);
        }
        else
        {
            AppLog::Error(L"PopupUpdateInternal: centerIndex out of range on RETURN");
        }

        BufferReset();
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
    const int H = visible * UI::ItemHeight + UI::Padding * 2;

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

        spineLeft = static_cast<float>(posX);

        SetWindowPos(hwnd, nullptr,
            posX, posY,
            static_cast<int>(UI::MaxWidth), H,
            SWP_NOZORDER | SWP_NOACTIVATE);

        scrollOffset.snap(0.f);
        opacity.snap(0.f);
        currentWidth = ComputeTargetWidth();

        ShowWindow(hwnd, SW_SHOWNA);   // show without stealing focus
    }
    else
    {
        // Already visible – just resize height for new item count
        SetWindowPos(hwnd, nullptr,
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