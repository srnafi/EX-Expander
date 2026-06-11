#include "PopupLayout.h"
#include "PopupState.h"
#include "PopupUtils.h"
#include "Globals.h"
#include <windows.h>
#include <wrl/client.h>
#include <algorithm>

using Microsoft::WRL::ComPtr;

float MeasureTextWidth(const std::wstring& text, IDWriteTextFormat* fmt)
{
    if (!Gfx::writeFactory || !fmt)
        return 0.f;

    if (text.empty())
        return 0.f;

    ComPtr<IDWriteTextLayout> layout;

    HRESULT hr = Gfx::writeFactory->CreateTextLayout(
        text.c_str(),
        static_cast<UINT32>(text.size()),
        fmt,
        8192.f,
        200.f,
        layout.GetAddressOf());

    if (FAILED(hr))
        return 0.f;

    DWRITE_TEXT_METRICS metrics{};
    layout->GetMetrics(&metrics);

    return metrics.widthIncludingTrailingWhitespace;
}

float ComputeTargetWidth()
{
    return UI::MaxWidth;
}