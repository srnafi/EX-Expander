#include "PopupLayout.h"
#include "PopupState.h"
#include "PopupUtils.h"
#include "Globals.h"

#include <windows.h>
#include <wrl/client.h>
#include <algorithm>

using Microsoft::WRL::ComPtr;

// ===========================================================================
// TEXT MEASUREMENT
// ===========================================================================

float MeasureTextWidth(const std::wstring& text, IDWriteTextFormat* fmt)
{
    // Guard against null pointers or empty input
    if (!Gfx::writeFactory || !fmt)
    {
        // Silently return 0 for invalid inputs (don't spam logs)
        return 0.f;
    }

    if (text.empty())
        return 0.f;

    ComPtr<IDWriteTextLayout> layout;

    // Create a temporary text layout to measure the width.
    // Parameters:
    //   8192.f  – max layout width (effectively "infinite" for measurement)
    //   200.f   – max layout height (line height)
    HRESULT hr = Gfx::writeFactory->CreateTextLayout(
        text.c_str(),
        static_cast<UINT32>(text.size()),
        fmt,
        8192.f,     // maxWidth
        200.f,      // maxHeight
        layout.GetAddressOf());

    if (FAILED(hr))
    {
        // Measurement failed – likely a bad format or out of memory
        return 0.f;
    }

    DWRITE_TEXT_METRICS metrics{};
    layout->GetMetrics(&metrics);

    return metrics.widthIncludingTrailingWhitespace;
}

// ===========================================================================
// WIDTH COMPUTATION
// ===========================================================================

float ComputeTargetWidth()
{
    const int total = static_cast<int>(g_FilteredExpansions.size());

    if (total == 0)
        return UI::SpineWidth;

    // Ensure the selected index is valid before accessing the vector
    ClampIndex(State::centerIndex, total);

    // Double-check bounds before access (defensive)
    if (State::centerIndex < 0 || State::centerIndex >= total)
    {
        return UI::SpineWidth;
    }

    const auto& selectedItem = g_FilteredExpansions[State::centerIndex];
    float textWidth = MeasureTextWidth(selectedItem.value, Gfx::fmtCenter.Get());

    // Add horizontal padding on both sides
    float needed = textWidth + UI::HorzPad * 2.f;

    // Clamp to [SpineWidth, MaxWidth]
    return (std::min)((std::max)(needed, UI::SpineWidth), UI::MaxWidth);
}