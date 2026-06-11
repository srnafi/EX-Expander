#include "PopupRenderer.h"
#include "PopupState.h"
#include "PopupUtils.h"
#include "Globals.h"
#include "AppLog.h"
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>

#pragma comment(lib, "dwrite.lib")

using Microsoft::WRL::ComPtr;

// ===========================================================================
// CONSTANTS
// ===========================================================================
namespace RenderConstants
{
    constexpr float BgAlpha = 0.95f;
    constexpr float BorderAlpha = 0.30f;
    constexpr float SelCardAlpha = 0.16f;
    constexpr float TextAlphaFade = 0.72f;
    constexpr float CenterDistThresh = 0.5f;
}

// ===========================================================================
// INTERNAL HELPERS
// ===========================================================================
namespace
{
    int WrapIndex(int centerIdx, int offset, int total)
    {
        if (total <= 0) return 0;
        return ((centerIdx + offset) % total + total) % total;
    }

    bool CheckHR(HRESULT hr, const std::wstring& context)
    {
        if (FAILED(hr))
        {
            AppLog::Warn(L"PopupRenderer: " + context + L" failed");
            return false;
        }
        return true;
    }
}

// ===========================================================================
// D2D INIT
// ===========================================================================
bool InitD2D()
{
    using namespace Gfx;

    if (!CheckHR(D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        factory.GetAddressOf()),
        L"D2D1CreateFactory"))
        return false;

    D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(
            DXGI_FORMAT_B8G8R8A8_UNORM,
            D2D1_ALPHA_MODE_PREMULTIPLIED));

    if (!CheckHR(factory->CreateDCRenderTarget(
        &props,
        dcRT.GetAddressOf()),
        L"CreateDCRenderTarget"))
        return false;

    if (!CheckHR(DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(writeFactory.GetAddressOf())),
        L"DWriteCreateFactory"))
        return false;

    // Item text format
    if (!CheckHR(writeFactory->CreateTextFormat(
        L"Segoe UI Emoji",
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        UI::FontSize,
        L"",
        fmtItem.GetAddressOf()),
        L"CreateTextFormat(item)"))
        return false;

    fmtItem->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    fmtItem->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    // Header text format
    if (!CheckHR(writeFactory->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_ITALIC,
        DWRITE_FONT_STRETCH_NORMAL,
        13.0f,
        L"",
        fmtHeader.GetAddressOf()),
        L"CreateTextFormat(header)"))
        return false;

    fmtHeader->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    fmtHeader->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    AppLog::Info(L"Direct2D and DirectWrite initialised");
    return true;
}

// ===========================================================================
// GLOW EFFECT
// ===========================================================================
void DrawGlow(ID2D1DCRenderTarget* rt,
    const D2D1_ROUNDED_RECT& base,
    const D2D1::ColorF& c,
    float masterAlpha)
{
    if (!rt) return;

    constexpr float GlowLayerCount = 3.f;
    constexpr float GlowAlphaBase = 0.12f;
    constexpr float GlowExpand = 6.f;

    for (int i = 3; i >= 1; --i)
    {
        float t = static_cast<float>(i) / GlowLayerCount;
        float expand = GlowExpand * t;
        float alpha = t * t * GlowAlphaBase * masterAlpha;

        ComPtr<ID2D1SolidColorBrush> glowBrush;
        if (FAILED(rt->CreateSolidColorBrush(
            { c.r, c.g, c.b, alpha },
            glowBrush.GetAddressOf())))
            continue;

        D2D1_ROUNDED_RECT rr{
            D2D1::RectF(
                base.rect.left - expand,
                base.rect.top - expand,
                base.rect.right + expand,
                base.rect.bottom + expand),
            base.radiusX + expand * 0.3f,
            base.radiusY + expand * 0.3f
        };

        rt->FillRoundedRectangle(rr, glowBrush.Get());
    }
}

// ===========================================================================
// RENDER
// ===========================================================================
void RenderLayered()
{
    using namespace State;
    using namespace Gfx;
    using namespace RenderConstants;

    if (!dcRT.Get())
    {
        AppLog::Error(L"dcRT is null");
        return;
    }

    const int total = static_cast<int>(g_FilteredExpansions.size());
    if (total <= 0)
        return;

    ClampIndex(centerIndex, total);

    // -----------------------------------------------------------------------
    // GEOMETRY
    // -----------------------------------------------------------------------
    const int   W = static_cast<int>(UI::MaxWidth);
    const int   visible = (std::min)(total, g_MaxPopupItems);
    const int   listH = visible * UI::ItemHeight + UI::Padding * 2;
    const int   H = listH + static_cast<int>(UI::HeaderHeight);
    const float Wf = static_cast<float>(W);
    const float Hf = static_cast<float>(H);
    const float scroll = scrollOffset.value;
    const float alpha = std::clamp(opacity.value, 0.f, 1.f);
    const int   half = visible / 2;

    const D2D1::ColorF accent =
        GetAccent(g_FilteredExpansions[centerIndex].value);

    // -----------------------------------------------------------------------
    // SETUP: off-screen DIB
    // -----------------------------------------------------------------------
    HDC screenDC = GetDC(nullptr);
    if (!screenDC)
    {
        AppLog::Error(L"RenderLayered: GetDC(nullptr) failed");
        return;
    }

    HDC memDC = CreateCompatibleDC(screenDC);
    if (!memDC)
    {
        AppLog::Error(L"RenderLayered: CreateCompatibleDC failed");
        ReleaseDC(nullptr, screenDC);
        return;
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = W;
    bmi.bmiHeader.biHeight = -H;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;

    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(
        screenDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);

    if (!bmp)
    {
        AppLog::Error(L"RenderLayered: CreateDIBSection failed");
        DeleteDC(memDC);
        ReleaseDC(nullptr, screenDC);
        return;
    }

    SelectObject(memDC, bmp);
    RECT rcBind = { 0, 0, W, H };
    dcRT->BindDC(memDC, &rcBind);

    // -----------------------------------------------------------------------
    // BEGIN DRAWING
    // -----------------------------------------------------------------------
    dcRT->BeginDraw();
    dcRT->Clear({ 0, 0, 0, 0 });

    // -----------------------------------------------------------------------
    // BACKGROUND: simple rounded rect
    // -----------------------------------------------------------------------
    {
        ComPtr<ID2D1SolidColorBrush> bgBrush;
        ComPtr<ID2D1SolidColorBrush> borderBrush;

        if (SUCCEEDED(dcRT->CreateSolidColorBrush(
            { 0.10f, 0.10f, 0.13f, BgAlpha * alpha },
            bgBrush.GetAddressOf())) &&
            SUCCEEDED(dcRT->CreateSolidColorBrush(
                { accent.r, accent.g, accent.b, BorderAlpha * alpha },
                borderBrush.GetAddressOf())))
        {
            D2D1_ROUNDED_RECT rr{
                D2D1::RectF(0.f, 0.f, Wf, Hf),
                UI::OuterRadius,
                UI::OuterRadius
            };

            dcRT->FillRoundedRectangle(rr, bgBrush.Get());
            dcRT->DrawRoundedRectangle(rr, borderBrush.Get(), 1.0f);
        }
    }

    // -----------------------------------------------------------------------
    // HEADER BAR: shows current typed token
    // -----------------------------------------------------------------------
    {
        // Darker strip
        ComPtr<ID2D1SolidColorBrush> headerBgBrush;
        if (SUCCEEDED(dcRT->CreateSolidColorBrush(
            { 0.07f, 0.07f, 0.10f, BgAlpha * alpha },
            headerBgBrush.GetAddressOf())))
        {
            D2D1_ROUNDED_RECT headerRect{
                D2D1::RectF(0.f, 0.f, Wf, UI::HeaderHeight),
                UI::OuterRadius,
                UI::OuterRadius
            };
            dcRT->FillRoundedRectangle(headerRect, headerBgBrush.Get());

            // Fill bottom corners to make it flat at the bottom
            ComPtr<ID2D1SolidColorBrush> cornerFill;
            if (SUCCEEDED(dcRT->CreateSolidColorBrush(
                { 0.07f, 0.07f, 0.10f, BgAlpha * alpha },
                cornerFill.GetAddressOf())))
            {
                dcRT->FillRectangle(
                    D2D1::RectF(
                        0.f,
                        UI::HeaderHeight * 0.5f,
                        Wf,
                        UI::HeaderHeight),
                    cornerFill.Get());
            }
        }

        // Separator line
        ComPtr<ID2D1SolidColorBrush> sepBrush;
        if (SUCCEEDED(dcRT->CreateSolidColorBrush(
            { accent.r, accent.g, accent.b, 0.35f * alpha },
            sepBrush.GetAddressOf())))
        {
            dcRT->DrawLine(
                D2D1::Point2F(UI::HorzPad, UI::HeaderHeight - 0.5f),
                D2D1::Point2F(Wf - UI::HorzPad, UI::HeaderHeight - 0.5f),
                sepBrush.Get(),
                1.0f);
        }

        // Token text
        if (!State::currentToken.empty() && fmtHeader)
        {
            ComPtr<ID2D1SolidColorBrush> tokenBrush;
            if (SUCCEEDED(dcRT->CreateSolidColorBrush(
                { accent.r, accent.g, accent.b, 0.90f * alpha },
                tokenBrush.GetAddressOf())))
            {
                const float pad = UI::HorzPad * 0.6f;
                const std::wstring& tok = State::currentToken;

                dcRT->DrawTextW(
                    tok.c_str(),
                    static_cast<UINT32>(tok.size()),
                    fmtHeader.Get(),
                    D2D1::RectF(pad, 0.f, Wf - pad, UI::HeaderHeight),
                    tokenBrush.Get(),
                    D2D1_DRAW_TEXT_OPTIONS_NONE);
            }
        }
    }

    // -----------------------------------------------------------------------
    // ITEMS: scroll wheel, clipped to list area
    // -----------------------------------------------------------------------
    const float listTop = UI::HeaderHeight + static_cast<float>(UI::Padding);
    const float listBot = static_cast<float>(H - UI::Padding);

    dcRT->PushAxisAlignedClip(
        D2D1::RectF(0.f, listTop, Wf, listBot),
        D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    for (int s = -1; s <= visible; ++s)
    {
        const float slotY =
            UI::HeaderHeight +
            static_cast<float>(UI::Padding + s * UI::ItemHeight);

        const float rowTop =
            slotY + scroll * static_cast<float>(UI::ItemHeight);

        const float rowBot = rowTop + static_cast<float>(UI::ItemHeight);
        const float rowMid = rowTop + static_cast<float>(UI::ItemHeight) * 0.5f;

        // Skip out-of-clip items
        if (rowBot < listTop) continue;
        if (rowTop > listBot) continue;

        // Distance from center slot
        const float distFromCenter =
            static_cast<float>(s - half) + scroll;
        const float absDist = std::abs(distFromCenter);

        // Map to data
        const int idx = WrapIndex(centerIndex, s - half, total);
        if (idx < 0 || idx >= total) continue;

        // Fade/shrink off-center items
        const float t = (std::min)(1.f, absDist);
        const float smooth = t * t * (3.f - 2.f * t);
        const float tAlpha = (1.0f - smooth * TextAlphaFade) * alpha;
        if (tAlpha < 0.01f) continue;

        const bool isCenter = (absDist < CenterDistThresh);

        // ------------------------------------------------------------------
        // SELECTION HIGHLIGHT
        // ------------------------------------------------------------------
        if (isCenter)
        {
            ComPtr<ID2D1SolidColorBrush> selBrush;
            if (SUCCEEDED(dcRT->CreateSolidColorBrush(
                { accent.r, accent.g, accent.b, SelCardAlpha * alpha },
                selBrush.GetAddressOf())))
            {
                D2D1_ROUNDED_RECT selRect{
                    D2D1::RectF(
                        6.f,
                        rowTop + 3.f,
                        Wf - 6.f,
                        rowBot - 3.f),
                    UI::CardRadius,
                    UI::CardRadius
                };

                DrawGlow(dcRT.Get(), selRect, accent, alpha);
                dcRT->FillRoundedRectangle(selRect, selBrush.Get());

                ComPtr<ID2D1SolidColorBrush> selBorder;
                if (SUCCEEDED(dcRT->CreateSolidColorBrush(
                    { accent.r, accent.g, accent.b, 0.30f * alpha },
                    selBorder.GetAddressOf())))
                {
                    dcRT->DrawRoundedRectangle(selRect, selBorder.Get(), 1.0f);
                }
            }
        }

        // ------------------------------------------------------------------
        // ITEM TEXT
        // ------------------------------------------------------------------
        const auto& expansion = g_FilteredExpansions[idx];

        // Show description if available, otherwise truncate value
        std::wstring displayText;
        if (!expansion.description.empty())
        {
            displayText = expansion.description;
        }
        else if (expansion.value.length() > 60)
        {
            displayText = expansion.value.substr(0, 60) + L"\u2026";
        }
        else
        {
            displayText = expansion.value;
        }

        ComPtr<ID2D1SolidColorBrush> textBrush;
        if (FAILED(dcRT->CreateSolidColorBrush(
            { 1.f, 1.f, 1.f, tAlpha },
            textBrush.GetAddressOf())))
            continue;

        // Clip to row
        dcRT->PushAxisAlignedClip(
            D2D1::RectF(0.f, rowTop, Wf, rowBot),
            D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

        const float textW = Wf - UI::HorzPad * 2.f;
        const float ty = rowMid - UI::FontSize * 0.5f;

        ComPtr<IDWriteTextLayout> layout;
        if (writeFactory &&
            fmtItem &&
            SUCCEEDED(writeFactory->CreateTextLayout(
                displayText.c_str(),
                static_cast<UINT32>(displayText.size()),
                fmtItem.Get(),
                textW,
                UI::FontSize + 4.f,
                layout.GetAddressOf())))
        {
            ComPtr<IDWriteInlineObject> ellipsis;
            if (SUCCEEDED(writeFactory->CreateEllipsisTrimmingSign(
                fmtItem.Get(),
                ellipsis.GetAddressOf())))
            {
                DWRITE_TRIMMING trim{
                    DWRITE_TRIMMING_GRANULARITY_CHARACTER,
                    0,
                    0
                };
                layout->SetTrimming(&trim, ellipsis.Get());
            }

            dcRT->DrawTextLayout(
                D2D1::Point2F(UI::HorzPad, ty),
                layout.Get(),
                textBrush.Get(),
                D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
        }

        dcRT->PopAxisAlignedClip();
    }

    dcRT->PopAxisAlignedClip();

    // -----------------------------------------------------------------------
    // END DRAWING AND BLIT
    // -----------------------------------------------------------------------
    HRESULT hr = dcRT->EndDraw();
    if (FAILED(hr))
    {
        AppLog::Error(L"RenderLayered: EndDraw failed");
        DeleteObject(bmp);
        DeleteDC(memDC);
        ReleaseDC(nullptr, screenDC);
        return;
    }

    RECT wr{};
    if (!GetWindowRect(hwnd, &wr))
    {
        AppLog::Warn(L"RenderLayered: GetWindowRect failed");
        wr = { 0, 0, W, H };
    }

    POINT     dst = { wr.left, wr.top };
    POINT     src = { 0, 0 };
    SIZE      sz = { W, H };
    BLENDFUNCTION bf{};
    bf.BlendOp = AC_SRC_OVER;
    bf.SourceConstantAlpha = 255;
    bf.AlphaFormat = AC_SRC_ALPHA;

    UpdateLayeredWindow(
        hwnd, screenDC, &dst, &sz,
        memDC, &src, 0, &bf, ULW_ALPHA);

    // -----------------------------------------------------------------------
    // CLEANUP
    // -----------------------------------------------------------------------
    DeleteObject(bmp);
    DeleteDC(memDC);
    ReleaseDC(nullptr, screenDC);
}