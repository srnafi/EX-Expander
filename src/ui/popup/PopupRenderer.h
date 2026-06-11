#pragma once
#include <d2d1.h>
#include <string>

// ---------------------------------------------------------------------------
// PopupRenderer
//
// Renders the popup UI using Direct2D to a layered window.
// The popup is composed of:
//   - A glow effect behind the selected item
//   - A list of items scrolling past the center slot (wheel effect)
// ---------------------------------------------------------------------------

// Initialise Direct2D, DirectWrite, and related resources
bool InitD2D();

// Render one frame to the layered window
void RenderLayered();

// Draw a glow effect (multi-layer expanding rounded rect)
void DrawGlow(ID2D1DCRenderTarget* rt,
    const D2D1_ROUNDED_RECT& base,
    const D2D1::ColorF& c,
    float masterAlpha);
