#include <windows.h>
#include "PopupRenderer.h"
#include "popup.h"
#include "Globals.h"
#include "EmojiMatcher.h"
#include "EmojiReplacer.h"
#include "InputBuffer.h"
#include "PopupState.h"

#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <PopupUtils.h>
#pragma comment(lib, "dwrite")

using Microsoft::WRL::ComPtr;
ComPtr<ID2D1PathGeometry> BuildTPath(
    float narrowW, float expandW,
    float totalH,
    float extTop, float extBot,
    float r);
// ---------------------------------------------------------------------------
// D2D INIT
// ---------------------------------------------------------------------------

bool InitD2D()
{
    using namespace Gfx;

    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
        factory.GetAddressOf())))
        return false;

    D2D1_RENDER_TARGET_PROPERTIES props =
        D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                D2D1_ALPHA_MODE_PREMULTIPLIED));

    if (FAILED(factory->CreateDCRenderTarget(&props, dcRT.GetAddressOf())))
        return false;

    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(writeFactory.GetAddressOf()))))
        return false;

    writeFactory->CreateTextFormat(
        L"Segoe UI Emoji", nullptr,
        DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        UI::FontCenter, L"", fmtCenter.GetAddressOf());

    writeFactory->CreateTextFormat(
        L"Segoe UI Emoji", nullptr,
        DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        UI::FontSide, L"", fmtSide.GetAddressOf());

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

    return true;
}

// ---------------------------------------------------------------------------
// RENDER  (one frame)
//
// scrollOffset.value is in slot units.  A value of +0.3 means the list has
// scrolled 30% of one row downward — i.e. each item is rendered 0.3*ItemHeight
// pixels lower than its resting position, giving the wheel-spin illusion.
//
// For each visible slot i, the "fractional distance from center" is:
//   frac = i - half + scrollOffset.value     (can be non-integer mid-animation)
//
// Font size and opacity are interpolated based on |frac| so items visually
// grow/shrink as they pass through the focal point.
// ---------------------------------------------------------------------------

void RenderLayered()
{
    using namespace State;
    using namespace Gfx;

    int total = (int)g_FilteredExpansions.size();
    if (total == 0) return;

    ClampIndex(centerIndex, total);

    const int   W = (int)UI::MaxWidth;
    int         visible = min(total, g_MaxPopupItems);
    const int   H = visible * UI::ItemHeight + UI::Padding * 2;

    float scroll = State::scrollOffset.value;   // fractional slot offset
    float alpha = min(1.f, max(0.f, opacity.value));
    float bumpW = State::currentWidth;          // already snapped, no animation

    int half = visible / 2;

    // Y extents of the center slot (fixed, never moves)
    float extTop = (float)(UI::Padding + half * UI::ItemHeight);
    float extBot = extTop + (float)UI::ItemHeight;

    // Accent from the center index
    D2D1::ColorF accent = GetAccent(g_FilteredExpansions[centerIndex].value);

    // ---- Off-screen DIB ----
    HDC screenDC = GetDC(nullptr);
    HDC memDC = CreateCompatibleDC(screenDC);

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = W;
    bmi.bmiHeader.biHeight = -H;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;

    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(screenDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bmp) { DeleteDC(memDC); ReleaseDC(nullptr, screenDC); return; }
    SelectObject(memDC, bmp);

    RECT rcBind = { 0, 0, W, H };
    dcRT->BindDC(memDC, &rcBind);

    // ---- Brushes ----
    ComPtr<ID2D1SolidColorBrush> bgBrush, borderBrush, cardBrush, brightBrush;
    dcRT->CreateSolidColorBrush({ 0.10f, 0.10f, 0.13f, 0.95f * alpha }, bgBrush.GetAddressOf());
    dcRT->CreateSolidColorBrush({ accent.r, accent.g, accent.b, 0.30f * alpha }, borderBrush.GetAddressOf());
    dcRT->CreateSolidColorBrush({ accent.r, accent.g, accent.b, 0.18f * alpha }, cardBrush.GetAddressOf());
    dcRT->CreateSolidColorBrush({ 1.f, 1.f, 1.f, alpha }, brightBrush.GetAddressOf());

    dcRT->BeginDraw();
    dcRT->Clear({ 0, 0 });

    // ---- T-shape background (static, based on center slot) ----
    auto tPath = BuildTPath(UI::SpineWidth, bumpW, (float)H,
        extTop, extBot, UI::OuterRadius);
    if (tPath)
    {
        dcRT->FillGeometry(tPath.Get(), bgBrush.Get());
        dcRT->DrawGeometry(tPath.Get(), borderBrush.Get(), 1.0f);
    }

    // ---- Draw selected card highlight behind items ----
    {
        D2D1_ROUNDED_RECT card = {
            D2D1::RectF(UI::HorzPad * 0.5f,
                        extTop + 4.f,
                        bumpW - UI::HorzPad * 0.5f,
                        extBot - 4.f),
            UI::CardRadius, UI::CardRadius
        };
        DrawGlow(dcRT.Get(), card, accent, alpha);
        dcRT->FillRoundedRectangle(card, cardBrush.Get());
        dcRT->DrawRoundedRectangle(card, borderBrush.Get(), 1.0f);
    }

    // ---- Draw each visible row — scroll offset shifts Y continuously ----
    //
    // scroll is in slot units. scroll = -1 means every item is shifted UP by
    // one row height (the new selection has just moved into center from below).
    // We draw [visible+2] slots — one extra above and below — so items sliding
    // in from outside the clip region are already rendered when they enter.
    //
    // slot index s ranges from -1 to visible (inclusive).
    // s=0..visible-1 are the normal slots; s=-1 and s=visible are the overflow
    // slots that are only visible mid-animation.
    dcRT->PushAxisAlignedClip(
        D2D1::RectF(0, (float)UI::Padding, (float)W, (float)(H - UI::Padding)),
        D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    for (int s = -1; s <= visible; ++s)
    {
        // Resting Y for this slot (no scroll)
        float slotY = (float)(UI::Padding + s * UI::ItemHeight);

        // Apply scroll: positive scroll shifts items DOWN (press UP = items come down)
        //               negative scroll shifts items UP  (press DOWN = items go up)
        float rowTop = slotY + scroll * (float)UI::ItemHeight;
        float rowMid = rowTop + (float)UI::ItemHeight * 0.5f;

        // Skip if entirely outside clip region (no point drawing)
        if (rowTop + (float)UI::ItemHeight < (float)UI::Padding) continue;
        if (rowTop > (float)(H - UI::Padding))                   continue;

        // Distance from center slot in slots (fractional during animation)
        float distFromCenter = (float)(s - half) + scroll;
        float absDist = fabsf(distFromCenter);

        // Which data item lives in this slot?
        // Slot relative to center: (s - half). Wrap into [0, total).
        int rel = s - half;
        int idx = ((centerIndex + rel) % total + total) % total;

        // Visual interpolation — smoothstep on distance clamped to [0,1]
        float t = min(1.f, absDist);
        float smooth = t * t * (3.f - 2.f * t);

        float fontSize = UI::FontCenter + (UI::FontSide - UI::FontCenter) * smooth;
        float textAlpha = (1.0f - smooth * 0.72f) * alpha;

        if (textAlpha < 0.01f) continue;

        const auto& text = g_FilteredExpansions[idx].value;
        bool isCenter = (absDist < 0.5f);

        if (isCenter)
        {
            // Center item: full-width protrusion zone
            ComPtr<ID2D1SolidColorBrush> textBrush;
            dcRT->CreateSolidColorBrush({ 1.f, 1.f, 1.f, textAlpha }, textBrush.GetAddressOf());

            float ty = rowMid - fontSize * 0.5f;
            dcRT->DrawTextW(text.c_str(), (UINT32)text.size(),
                fmtCenter.Get(),
                D2D1::RectF(UI::HorzPad, ty, bumpW - UI::HorzPad, ty + fontSize),
                textBrush.Get(),
                D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
        }
        else
        {
            // Non-center: clipped to spine, ellipsis-trimmed
            ComPtr<ID2D1SolidColorBrush> dimBrush;
            dcRT->CreateSolidColorBrush({ 1.f, 1.f, 1.f, textAlpha }, dimBrush.GetAddressOf());

            dcRT->PushAxisAlignedClip(
                D2D1::RectF(0, rowTop, UI::SpineWidth, rowTop + (float)UI::ItemHeight),
                D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

            float textW = UI::SpineWidth - UI::HorzPad * 2.f;
            float ty = rowMid - fontSize * 0.5f;

            ComPtr<IDWriteTextLayout> layout;
            if (writeFactory && fmtSide &&
                SUCCEEDED(writeFactory->CreateTextLayout(
                    text.c_str(), (UINT32)text.size(),
                    fmtSide.Get(), textW, fontSize + 4.f, layout.GetAddressOf())))
            {
                ComPtr<IDWriteInlineObject> ellipsis;
                writeFactory->CreateEllipsisTrimmingSign(fmtSide.Get(), ellipsis.GetAddressOf());
                DWRITE_TRIMMING trim{ DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
                layout->SetTrimming(&trim, ellipsis.Get());

                dcRT->DrawTextLayout(
                    D2D1::Point2F(UI::HorzPad, ty),
                    layout.Get(), dimBrush.Get(),
                    D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
            }

            dcRT->PopAxisAlignedClip();
        }
    }

    dcRT->PopAxisAlignedClip(); // item area clip

    dcRT->EndDraw();

    // ---- Blit to screen ----
    RECT wr{};
    GetWindowRect(hwnd, &wr);

    POINT dst = { wr.left, wr.top };
    POINT src = { 0, 0 };
    SIZE  sz = { W, H };

    BLENDFUNCTION bf{};
    bf.BlendOp = AC_SRC_OVER;
    bf.SourceConstantAlpha = 255;
    bf.AlphaFormat = AC_SRC_ALPHA;

    UpdateLayeredWindow(hwnd, screenDC, &dst, &sz, memDC, &src, 0, &bf, ULW_ALPHA);

    DeleteObject(bmp);
    DeleteDC(memDC);
    ReleaseDC(nullptr, screenDC);
}

// ---------------------------------------------------------------------------
// GLOW  (drawn behind selected card)
// ---------------------------------------------------------------------------
void DrawGlow(ID2D1DCRenderTarget* rt,
    const D2D1_ROUNDED_RECT& base,
    const D2D1::ColorF& c,
    float masterAlpha)
{
    for (int i = 3; i >= 1; --i)
    {
        float t = (float)i / 3.f;
        float expand = 6.f * t;
        float alpha = t * t * 0.12f * masterAlpha;

        ComPtr<ID2D1SolidColorBrush> b;
        if (FAILED(rt->CreateSolidColorBrush({ c.r, c.g, c.b, alpha }, b.GetAddressOf())))
            continue;

        D2D1_ROUNDED_RECT rr = {
            D2D1::RectF(base.rect.left - expand,
                        base.rect.top - expand,
                        base.rect.right + expand,
                        base.rect.bottom + expand),
            base.radiusX + expand * 0.3f,
            base.radiusY + expand * 0.3f
        };
        rt->FillRoundedRectangle(rr, b.Get());
    }
}

// ---------------------------------------------------------------------------
// T-SHAPE PATH GEOMETRY  (⊢ shape)
//
//  narrowW  = spine width  (SpineWidth, fixed)
//  expandW  = total window width  (spine + protrusion)
//  totalH   = full window height
//  extTop   = Y where selected row begins
//  extBot   = Y where selected row ends
//  r        = outer corner radius
//
//  All outer corners are rounded.  The two inner junction corners
//  (where protrusion meets spine) are left sharp for clarity.
// ---------------------------------------------------------------------------

ComPtr<ID2D1PathGeometry> BuildTPath(
    float narrowW, float expandW,
    float totalH,
    float extTop, float extBot,
    float r)
{
    ComPtr<ID2D1PathGeometry> path;
    if (FAILED(Gfx::factory->CreatePathGeometry(path.GetAddressOf())))
        return path;

    ComPtr<ID2D1GeometrySink> sink;
    if (FAILED(path->Open(sink.GetAddressOf())))
        return path;

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

    bool hasProtrusion = (expandW > narrowW + 1.f);

    if (!hasProtrusion)
    {
        // Plain rounded rect (spine only)
        sink->BeginFigure(Pt(r, 0), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(Pt(narrowW - r, 0));   Arc(narrowW, r);
        sink->AddLine(Pt(narrowW, totalH - r)); Arc(narrowW - r, totalH);
        sink->AddLine(Pt(r, totalH));          Arc(0, totalH - r);
        sink->AddLine(Pt(0, r));               Arc(r, 0);
    }
    else
    {
        // ⊢ shape (clockwise from top-left):
        //
        //   (r,0) ──── (narrowW-r, 0)
        //   arc ──> (narrowW, r)
        //   │ down to (narrowW, extTop)          ← sharp inner corner
        //   ──── (expandW-r, extTop)
        //   arc ──> (expandW, extTop+r)
        //   │ down to (expandW, extBot-r)
        //   arc ──> (expandW-r, extBot)
        //   ──── (narrowW, extBot)               ← sharp inner corner
        //   │ down to (narrowW, totalH-r)
        //   arc ──> (narrowW-r, totalH)
        //   ──── (r, totalH)
        //   arc ──> (0, totalH-r)
        //   │ up to (0, r)
        //   arc ──> (r, 0)

        sink->BeginFigure(Pt(r, 0), D2D1_FIGURE_BEGIN_FILLED);

        sink->AddLine(Pt(narrowW - r, 0));
        Arc(narrowW, r);                            // top-right of spine

        sink->AddLine(Pt(narrowW, extTop));         // down to bump top  (sharp)
        sink->AddLine(Pt(expandW - r, extTop));     // across bump top
        Arc(expandW, extTop + r);                   // top-right of bump

        sink->AddLine(Pt(expandW, extBot - r));     // down bump right side
        Arc(expandW - r, extBot);                   // bottom-right of bump

        sink->AddLine(Pt(narrowW, extBot));         // across bump bottom  (sharp)
        sink->AddLine(Pt(narrowW, totalH - r));     // down to bottom of spine
        Arc(narrowW - r, totalH);                   // bottom-right of spine

        sink->AddLine(Pt(r, totalH));
        Arc(0, totalH - r);                         // bottom-left
        sink->AddLine(Pt(0, r));
        Arc(r, 0);                                  // top-left
    }

    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    sink->Close();
    return path;
}