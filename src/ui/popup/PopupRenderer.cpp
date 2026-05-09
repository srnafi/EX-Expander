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
    constexpr float CardAlpha = 0.18f;
    constexpr float CardPadding = 4.f;
    constexpr float CenterDistThresh = 0.5f;
    constexpr float TextAlphaFade = 0.72f;
    constexpr float GlowLayerCount = 3.f;
    constexpr float GlowAlphaBase = 0.12f;
    constexpr float GlowExpand = 6.f;
}

// ===========================================================================
// INTERNAL HELPERS
// ===========================================================================

namespace
{
    // Circular index wrapping: converts (centerIndex + offset) to a valid index
    int WrapIndex(int centerIdx, int offset, int total)
    {
        if (total <= 0) return 0;
        return ((centerIdx + offset) % total + total) % total;
    }

    // HRESULT check with logging
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

    if (!CheckHR(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
        factory.GetAddressOf()),
        L"D2D1CreateFactory"))
        return false;

    D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
            D2D1_ALPHA_MODE_PREMULTIPLIED));

    if (!CheckHR(factory->CreateDCRenderTarget(&props, dcRT.GetAddressOf()),
        L"CreateDCRenderTarget"))
        return false;

    if (!CheckHR(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(writeFactory.GetAddressOf())),
        L"DWriteCreateFactory"))
        return false;

    // Create text formats
    if (!CheckHR(writeFactory->CreateTextFormat(
        L"Segoe UI Emoji", nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        UI::FontCenter, L"",
        fmtCenter.GetAddressOf()),
        L"CreateTextFormat(center)"))
        return false;

    if (!CheckHR(writeFactory->CreateTextFormat(
        L"Segoe UI Emoji", nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        UI::FontSide, L"",
        fmtSide.GetAddressOf()),
        L"CreateTextFormat(side)"))
        return false;

    // Configure alignment
    if (fmtCenter)
    {
        fmtCenter->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        fmtCenter->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }

    if (fmtSide)
    {
        fmtSide->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        fmtSide->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }

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

    for (int i = 3; i >= 1; --i)
    {
        float t = static_cast<float>(i) / RenderConstants::GlowLayerCount;
        float expand = RenderConstants::GlowExpand * t;
        float alpha = t * t * RenderConstants::GlowAlphaBase * masterAlpha;

        ComPtr<ID2D1SolidColorBrush> glowBrush;
        if (FAILED(rt->CreateSolidColorBrush(
            { c.r, c.g, c.b, alpha },
            glowBrush.GetAddressOf())))
            continue;

        D2D1_ROUNDED_RECT rr{
            D2D1::RectF(base.rect.left - expand,
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
// T-SHAPE GEOMETRY (⊢ shape)
//
// Parameters:
//   narrowW  – spine width (fixed)
//   expandW  – total width including protrusion
//   totalH   – full window height
//   extTop   – Y where selected row begins
//   extBot   – Y where selected row ends
//   r        – outer corner radius
//
// All outer corners are rounded. Inner junctions (spine ↔ protrusion) 
// are sharp for visual clarity.
// ===========================================================================

ComPtr<ID2D1PathGeometry> BuildTPath(float narrowW, float expandW,
    float totalH,
    float extTop, float extBot,
    float r)
{
    ComPtr<ID2D1PathGeometry> path;

    if (FAILED(Gfx::factory->CreatePathGeometry(path.GetAddressOf())))
        return nullptr;

    ComPtr<ID2D1GeometrySink> sink;
    if (FAILED(path->Open(sink.GetAddressOf())))
        return nullptr;

    sink->SetFillMode(D2D1_FILL_MODE_WINDING);

    auto Pt = [](float x, float y) { return D2D1::Point2F(x, y); };

    auto Arc = [&](float ex, float ey)
        {
            D2D1_ARC_SEGMENT arc{};
            arc.point = Pt(ex, ey);
            arc.size = { r, r };
            arc.rotationAngle = 0.f;
            arc.sweepDirection = D2D1_SWEEP_DIRECTION_CLOCKWISE;
            arc.arcSize = D2D1_ARC_SIZE_SMALL;
            sink->AddArc(arc);
        };

    const bool hasProtrusion = (expandW > narrowW + 1.f);

    if (!hasProtrusion)
    {
        // Plain rounded rect (spine only)
        sink->BeginFigure(Pt(r, 0), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(Pt(narrowW - r, 0));
        Arc(narrowW, r);
        sink->AddLine(Pt(narrowW, totalH - r));
        Arc(narrowW - r, totalH);
        sink->AddLine(Pt(r, totalH));
        Arc(0, totalH - r);
        sink->AddLine(Pt(0, r));
        Arc(r, 0);
    }
    else
    {
        // T-shape with protrusion (clockwise from top-left)
        sink->BeginFigure(Pt(r, 0), D2D1_FIGURE_BEGIN_FILLED);

        // Top of spine
        sink->AddLine(Pt(narrowW - r, 0));
        Arc(narrowW, r);

        // Down spine to protrusion top (sharp inner corner)
        sink->AddLine(Pt(narrowW, extTop));
        sink->AddLine(Pt(expandW - r, extTop));
        Arc(expandW, extTop + r);

        // Down right side of protrusion
        sink->AddLine(Pt(expandW, extBot - r));
        Arc(expandW - r, extBot);

        // Bottom of protrusion to spine (sharp inner corner)
        sink->AddLine(Pt(narrowW, extBot));
        sink->AddLine(Pt(narrowW, totalH - r));
        Arc(narrowW - r, totalH);

        // Bottom of spine
        sink->AddLine(Pt(r, totalH));
        Arc(0, totalH - r);

        // Left side
        sink->AddLine(Pt(0, r));
        Arc(r, 0);
    }

    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    sink->Close();

    return path;
}

// ===========================================================================
// RENDER  (one frame)
//
// The scroll wheel effect works by:
//   - scrollOffset.value is in slot units (fractional during animation)
//   - Each visible slot is drawn at Y = slotRestY + scrollOffset.value * itemHeight
//   - Items fade and shrink as they move away from the center slot
//   - The center slot is the focal point (full size, full opacity)
// ===========================================================================

void RenderLayered()
{
    using namespace State;
    using namespace Gfx;
    using namespace RenderConstants;
    // Diagnostic: force intellisense to recognize dcRT
    auto test = dcRT.Get();
    if (!test)
    {
        AppLog::Error(L"dcRT is null");
        return;
    }
    const int total = static_cast<int>(g_FilteredExpansions.size());
    if (total <= 0)
        return;

    ClampIndex(centerIndex, total);

    const int W = static_cast<int>(UI::MaxWidth);
    const int visible = (std::min)(total, g_MaxPopupItems);
    const int H = visible * UI::ItemHeight + UI::Padding * 2;

    const float scroll = scrollOffset.value;
    const float alpha = std::clamp(opacity.value, 0.f, 1.f);
    const float bumpW = currentWidth;

    const int half = visible / 2;
    const float extTop = static_cast<float>(UI::Padding + half * UI::ItemHeight);
    const float extBot = extTop + static_cast<float>(UI::ItemHeight);

    const D2D1::ColorF accent = GetAccent(g_FilteredExpansions[centerIndex].value);

    // -----------------------------------------------------------------------
    // SETUP: off-screen DIB and rendering context
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
    bmi.bmiHeader.biHeight = -H;       // negative = top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;

    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(screenDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);

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
    // BACKGROUND: T-shape with border
    // -----------------------------------------------------------------------
    ComPtr<ID2D1PathGeometry> tPath = BuildTPath(
        UI::SpineWidth, bumpW, static_cast<float>(H),
        extTop, extBot, UI::OuterRadius);

    if (tPath)
    {
        ComPtr<ID2D1SolidColorBrush> bgBrush, borderBrush;

        if (SUCCEEDED(dcRT->CreateSolidColorBrush(
            { 0.10f, 0.10f, 0.13f, BgAlpha * alpha },
            bgBrush.GetAddressOf())) &&
            SUCCEEDED(dcRT->CreateSolidColorBrush(
                { accent.r, accent.g, accent.b, BorderAlpha * alpha },
                borderBrush.GetAddressOf())))
        {
            dcRT->FillGeometry(tPath.Get(), bgBrush.Get());
            dcRT->DrawGeometry(tPath.Get(), borderBrush.Get(), 1.0f);
        }
    }

    // -----------------------------------------------------------------------
    // SELECTED ITEM HIGHLIGHT CARD
    // -----------------------------------------------------------------------
    {
        ComPtr<ID2D1SolidColorBrush> cardBrush;

        if (SUCCEEDED(dcRT->CreateSolidColorBrush(
            { accent.r, accent.g, accent.b, CardAlpha * alpha },
            cardBrush.GetAddressOf())))
        {
            D2D1_ROUNDED_RECT card{
                D2D1::RectF(UI::HorzPad * 0.5f,
                            extTop + CardPadding,
                            bumpW - UI::HorzPad * 0.5f,
                            extBot - CardPadding),
                UI::CardRadius, UI::CardRadius
            };

            DrawGlow(dcRT.Get(), card, accent, alpha);
            dcRT->FillRoundedRectangle(card, cardBrush.Get());

            ComPtr<ID2D1SolidColorBrush> borderBrush;
            if (SUCCEEDED(dcRT->CreateSolidColorBrush(
                { accent.r, accent.g, accent.b, BorderAlpha * alpha },
                borderBrush.GetAddressOf())))
            {
                dcRT->DrawRoundedRectangle(card, borderBrush.Get(), 1.0f);
            }
        }
    }

    // -----------------------------------------------------------------------
    // DRAW ITEMS (scroll wheel effect)
    // -----------------------------------------------------------------------
    dcRT->PushAxisAlignedClip(
        D2D1::RectF(0, static_cast<float>(UI::Padding),
            static_cast<float>(W),
            static_cast<float>(H - UI::Padding)),
        D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    for (int s = -1; s <= visible; ++s)
    {
        const float slotY = static_cast<float>(UI::Padding + s * UI::ItemHeight);
        const float rowTop = slotY + scroll * static_cast<float>(UI::ItemHeight);
        const float rowMid = rowTop + static_cast<float>(UI::ItemHeight) * 0.5f;

        // Skip entirely out-of-clip items
        if (rowTop + static_cast<float>(UI::ItemHeight) < static_cast<float>(UI::Padding))
            continue;
        if (rowTop > static_cast<float>(H - UI::Padding))
            continue;

        // Distance from center slot (fractional during animation)
        const float distFromCenter = static_cast<float>(s - half) + scroll;
        const float absDist = std::abs(distFromCenter);

        // Map to data item via circular wrapping
        const int idx = WrapIndex(centerIndex, s - half, total);

        if (idx < 0 || idx >= total)
            continue;

        // Visual interpolation: items fade/shrink as they move away
        const float t = (std::min)(1.f, absDist);
        const float smooth = t * t * (3.f - 2.f * t);    // smoothstep

        const float fontSize = UI::FontCenter + (UI::FontSide - UI::FontCenter) * smooth;
        const float textAlpha = (1.0f - smooth * TextAlphaFade) * alpha;

        if (textAlpha < 0.01f)
            continue;

        const auto& text = g_FilteredExpansions[idx].value;
        const bool isCenter = (absDist < CenterDistThresh);

        ComPtr<ID2D1SolidColorBrush> textBrush;
        if (FAILED(dcRT->CreateSolidColorBrush(
            { 1.f, 1.f, 1.f, textAlpha },
            textBrush.GetAddressOf())))
            continue;

        if (isCenter)
        {
            // Center item: full width in protrusion zone
            const float ty = rowMid - fontSize * 0.5f;
            dcRT->DrawTextW(
                text.c_str(), static_cast<UINT32>(text.size()),
                fmtCenter.Get(),
                D2D1::RectF(UI::HorzPad, ty, bumpW - UI::HorzPad, ty + fontSize),
                textBrush.Get(),
                D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
        }
        else
        {
            // Off-center items: clipped to spine with ellipsis
            dcRT->PushAxisAlignedClip(
                D2D1::RectF(0, rowTop,
                    UI::SpineWidth,
                    rowTop + static_cast<float>(UI::ItemHeight)),
                D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

            const float textW = UI::SpineWidth - UI::HorzPad * 2.f;
            const float ty = rowMid - fontSize * 0.5f;

            ComPtr<IDWriteTextLayout> layout;
            if (writeFactory && fmtSide &&
                SUCCEEDED(writeFactory->CreateTextLayout(
                    text.c_str(), static_cast<UINT32>(text.size()),
                    fmtSide.Get(), textW, fontSize + 4.f,
                    layout.GetAddressOf())))
            {
                ComPtr<IDWriteInlineObject> ellipsis;
                if (SUCCEEDED(writeFactory->CreateEllipsisTrimmingSign(
                    fmtSide.Get(), ellipsis.GetAddressOf())))
                {
                    DWRITE_TRIMMING trim{ DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
                    layout->SetTrimming(&trim, ellipsis.Get());
                }

                dcRT->DrawTextLayout(
                    D2D1::Point2F(UI::HorzPad, ty),
                    layout.Get(), textBrush.Get(),
                    D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
            }

            dcRT->PopAxisAlignedClip();
        }
    }

    dcRT->PopAxisAlignedClip();

    // -----------------------------------------------------------------------
    // END DRAWING AND BLIT
    // -----------------------------------------------------------------------
    HRESULT hr = dcRT->EndDraw();
    if (FAILED(hr))
    {
        AppLog::Error(L"RenderLayered: BeginDraw failed");
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

    POINT dst = { wr.left, wr.top };
    POINT src = { 0, 0 };
    SIZE sz = { W, H };

    BLENDFUNCTION bf{};
    bf.BlendOp = AC_SRC_OVER;
    bf.SourceConstantAlpha = 255;
    bf.AlphaFormat = AC_SRC_ALPHA;

    UpdateLayeredWindow(hwnd, screenDC, &dst, &sz, memDC, &src, 0, &bf, ULW_ALPHA);

    // -----------------------------------------------------------------------
    // CLEANUP
    // -----------------------------------------------------------------------
    DeleteObject(bmp);
    DeleteDC(memDC);
    ReleaseDC(nullptr, screenDC);
}