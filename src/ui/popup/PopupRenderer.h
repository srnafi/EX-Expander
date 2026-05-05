#pragma once
#include <d2d1.h>
#include <string>

// Init D2D resources
bool InitD2D();

// Render one frame
void RenderLayered();

// Glow effect
void DrawGlow(ID2D1DCRenderTarget* rt,
    const D2D1_ROUNDED_RECT& base,
    const D2D1::ColorF& c,
    float masterAlpha);

//// Build T-shape geometry
//ID2D1PathGeometry* BuildTPath(
//    float narrowW,
//    float expandW,
//    float totalH,
//    float extTop,
//    float extBot,
//    float r
//);