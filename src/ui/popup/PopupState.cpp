#include "PopupState.h"
#include <cmath>

// ===========================================================================
// UI CONSTANTS  –  layout dimensions and typography
// ===========================================================================

namespace UI
{
    const float SpineWidth = 150.0f;     // narrow column for off-center items
    const float MaxWidth = 420.0f;     // total window width when expanded
    const float HorzPad = 18.0f;      // horizontal text padding
    const int   ItemHeight = 42;         // pixels per item row
    const int   Padding = 6;          // top/bottom padding inside window

    const float OuterRadius = 12.0f;      // popup frame corner radius
    const float CardRadius = 8.0f;       // selected-item card corner radius

    const float FontCenter = 20.0f;      // selected item font size
    const float FontSide = 14.0f;      // off-center item font size
}

// ===========================================================================
// ANIMATION CONSTANTS  –  spring physics and timer
//
// Stiffness (k): higher = faster snap toward target
// Damping (b):  higher = less oscillation
// For critical damping: b = 2 * sqrt(k * mass)  (mass = 1 here)
// ===========================================================================

namespace Anim
{
    const int   TimerID = 1;
    const int   Interval = 16;            // ~60 fps

    const float SStiffness = 600.0f;        // scroll spring – snappy
    const float SDamping = 98.0f;

    const float OStiffness = 200.0f;        // opacity spring – gentle
    const float ODamping = 28.0f;

    const float Dt = 0.016f;        // physics step (matches timer)
    const float EpsS = 0.002f;        // scroll settle threshold
    const float EpsO = 0.004f;        // opacity settle threshold

    bool animEnabled = true;                // animations on by default
}

// ===========================================================================
// STATE VARIABLES  –  mutable runtime state
// ===========================================================================

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

    int popupFixedX = 50;
    int popupFixedY = 50;
}

// ===========================================================================
// GRAPHICS RESOURCES
// ===========================================================================

namespace Gfx
{
    ComPtr<ID2D1Factory>        factory;
    ComPtr<IDWriteFactory>      writeFactory;
    ComPtr<ID2D1DCRenderTarget> dcRT;
    ComPtr<IDWriteTextFormat>   fmtCenter;
    ComPtr<IDWriteTextFormat>   fmtSide;
}

// ===========================================================================
// SPRING IMPLEMENTATION  –  critically-damped spring physics
// ===========================================================================

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
    // Hooke's law with damping:  F = k(target - value) - b * velocity
    float force = k * (target - value) - b * velocity;

    velocity += force * dt;
    value += velocity * dt;

    // Spring is "done" when both value and velocity are within epsilon
    bool done = std::abs(target - value) <= eps && std::abs(velocity) <= eps;

    if (done)
    {
        value = target;
        velocity = 0.f;
    }

    return !done;
}