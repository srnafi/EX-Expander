// PopupState.cpp

#include "PopupState.h"
#include <cmath>

// ---------------------------------------------------------------------------
// UI CONSTANTS
// ---------------------------------------------------------------------------

namespace UI
{
    const float SpineWidth = 150.0f;
    const float MaxWidth = 420.0f;
    const float HorzPad = 18.0f;
    const int   ItemHeight = 42;
    const int   Padding = 6;

    const float OuterRadius = 12.0f;
    const float CardRadius = 8.0f;

    const float FontCenter = 20.0f;
    const float FontSide = 14.0f;
}

// ---------------------------------------------------------------------------
// ANIMATION CONSTANTS
// ---------------------------------------------------------------------------

namespace Anim
{
    const int   TimerID = 1;
    const int   Interval = 16;

    const float SStiffness = 600.0f;
    const float SDamping = 98.0f;

    const float OStiffness = 200.0f;
    const float ODamping = 28.0f;

    const float Dt = 0.016f;
    const float EpsS = 0.002f;
    const float EpsO = 0.004f;

    bool animEnabled = false;
}

// ---------------------------------------------------------------------------
// STATE VARIABLES
// ---------------------------------------------------------------------------

namespace State
{
    HWND hwnd = nullptr;

    int  centerIndex = 0;
    int  lastKey = 0;
    bool timerActive = false;

    Spring scrollOffset = {};
    Spring opacity = {};

    float currentWidth = UI::SpineWidth;
    float spineLeft = 0.f;

    bool  dragging = false;
    POINT dragStart = {};
    POINT winStart = {};
}

// ---------------------------------------------------------------------------
// GRAPHICS RESOURCES
// ---------------------------------------------------------------------------

namespace Gfx
{
    ComPtr<ID2D1Factory>        factory;
    ComPtr<IDWriteFactory>      writeFactory;
    ComPtr<ID2D1DCRenderTarget> dcRT;
    ComPtr<IDWriteTextFormat>   fmtCenter;
    ComPtr<IDWriteTextFormat>   fmtSide;
}

// ---------------------------------------------------------------------------
// GLOBAL FIXED POSITION
// ---------------------------------------------------------------------------

int g_PopupFixedX = 50;
int g_PopupFixedY = 50;

// ---------------------------------------------------------------------------
// SPRING IMPLEMENTATION
// ---------------------------------------------------------------------------

void State::Spring::snap(float t)
{
    value = t;
    target = t;
    velocity = 0.f;
}

void State::Spring::setTarget(float t)
{
    target = t;
}

bool State::Spring::step(float k, float b, float dt, float eps)
{
    float f = k * (target - value) - b * velocity;
    velocity += f * dt;
    value += velocity * dt;

    bool done = fabsf(target - value) <= eps && fabsf(velocity) <= eps;

    if (done)
    {
        value = target;
        velocity = 0.f;
    }

    return !done;
}