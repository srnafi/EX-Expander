#include "PopupInput.h"
#include <PopupState.h>
#include <InputBuffer.h>
#include <PopupAnimation.h>
#include <EmojiMatcher.h>
#include <PopupLayout.h>
#include <PopupRenderer.h>
#include <EmojiReplacer.h>
#include <PopupUtils.h>

void PopupHandleKeyInternal(int vkCode)
{
    State::lastKey = vkCode;
}
void PopupUpdateInternal()
{
    using namespace State;

    if (!inTheBuffer)
    {
        StopTimer();
        ShowWindow(hwnd, SW_HIDE);
        centerIndex = 0;
        scrollOffset.snap(0.f);
        opacity.snap(0.f);
        return;
    }

    std::wstring token = BufferGetToken();
    g_FilteredExpansions = GetMatches(token);

    int total = (int)g_FilteredExpansions.size();
    if (total == 0)
    {
        centerIndex = 0;
        StopTimer();
        ShowWindow(hwnd, SW_HIDE);
        return;
    }

    int key = lastKey;
    lastKey = 0;

    if (key == VK_DOWN || key == VK_UP)
    {
        // Press DOWN: next item slides UP into center from below.
        //   → centerIndex advances, scrollOffset snaps to +1 (one slot below),
        //     spring pulls it back to 0 (items visually glide upward).
        // Press UP: previous item slides DOWN into center from above.
        //   → centerIndex retreats, scrollOffset snaps to -1, spring → 0.
        if (key == VK_DOWN)
        {
            centerIndex = (centerIndex + 1) % total;
            scrollOffset.value += 1.f;   // start one slot below, animate up to 0
        }
        else
        {
            centerIndex = (centerIndex - 1 + total) % total;
            scrollOffset.value -= 1.f;   // start one slot above, animate down to 0
        }
        scrollOffset.setTarget(0.f);

        // Width snaps immediately to the new selection — no animation
        currentWidth = ComputeTargetWidth();
    }
    else if (key == VK_RETURN)
    {
        StopTimer();
        ClampIndex(centerIndex, total);
        ReplaceWithEmoji(token, g_FilteredExpansions[centerIndex].value);
        BufferReset();
        ShowWindow(hwnd, SW_HIDE);
        scrollOffset.snap(0.f);
        opacity.snap(0.f);
        return;
    }

    ClampIndex(centerIndex, total);

    // First appearance
    bool wasHidden = !IsWindowVisible(hwnd);
    if (wasHidden)
    {
        int posX, posY;

        int visible = min(total, g_MaxPopupItems);
        int H = visible * UI::ItemHeight + UI::Padding * 2;
        if (g_PopupPosition == L"cursor")
        {
            POINT cur;
            GetCursorPos(&cur);

            posX = cur.x;
            posY = cur.y - H - 8;
        }
        else
        {
            posX = g_PopupFixedX;
            posY = g_PopupFixedY;
        }


        spineLeft = (float)posX;
        SetWindowPos(hwnd, nullptr,
            posX, posY,
            (int)UI::MaxWidth, H,
            SWP_NOZORDER | SWP_NOACTIVATE);

        scrollOffset.snap(0.f);
        opacity.snap(0.f);
        currentWidth = ComputeTargetWidth();
        ShowWindow(hwnd, SW_SHOWNA);
    }
    else
    {
        int visible = min(total, g_MaxPopupItems);
        int H = visible * UI::ItemHeight + UI::Padding * 2;
        SetWindowPos(hwnd, nullptr, 0, 0,
            (int)UI::MaxWidth, H,
            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    opacity.setTarget(1.f);

    if (!Anim::animEnabled)
    {
        // No animation: snap everything instantly and render one frame
        scrollOffset.snap(0.f);
        opacity.snap(1.f);
        StopTimer();
        RenderLayered();
        return;
    }

    StartTimer();
    Tick();
}