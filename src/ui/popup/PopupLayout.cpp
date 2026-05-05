#include "PopupLayout.h"
#include "popup.h"
#include "Globals.h"
#include "EmojiMatcher.h"
#include "EmojiReplacer.h"
#include "InputBuffer.h"
#include <wrl/client.h>
#include <PopupState.h>
#include <PopupUtils.h>
// ---------------------------------------------------------------------------
// TEXT MEASUREMENT
// ---------------------------------------------------------------------------

float MeasureTextWidth(const std::wstring& text, IDWriteTextFormat* fmt)
{
    if (!Gfx::writeFactory || !fmt || text.empty()) return 0.f;

    ComPtr<IDWriteTextLayout> layout;
    if (FAILED(Gfx::writeFactory->CreateTextLayout(
        text.c_str(), (UINT32)text.size(),
        fmt, 8192.f, 200.f, layout.GetAddressOf())))
        return 0.f;

    DWRITE_TEXT_METRICS m{};
    layout->GetMetrics(&m);
    return m.widthIncludingTrailingWhitespace;
}

// Compute display width needed for the currently selected item (instant snap)
float ComputeTargetWidth()
{
    int total = (int)g_FilteredExpansions.size();
    if (total == 0) return UI::SpineWidth;

    ClampIndex(State::centerIndex, total);
    const auto& text = g_FilteredExpansions[State::centerIndex].value;

    float textW = MeasureTextWidth(text, Gfx::fmtCenter.Get());
    float needed = textW + UI::HorzPad * 2.f;

    return min(max(needed, UI::SpineWidth), UI::MaxWidth);
}