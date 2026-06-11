#include "PopupState.h"
#include <cmath>

namespace UI
{
    const float MaxWidth = 420.0f;
    const float HorzPad = 18.0f;
    const int   ItemHeight = 42;
    const int   Padding = 6;
    const float OuterRadius = 12.0f;
    const float CardRadius = 8.0f;
    const float FontSize = 14.0f;
    const float HeaderHeight = 28.0f;
}

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
    bool        animEnabled = false;
}

namespace State
{
    HWND         hwnd = nullptr;
    int          centerIndex = 0;
    int          lastKey = 0;
    bool         timerActive = false;
    Spring       scrollOffset = {};
    Spring       opacity = {};
    bool         dragging = false;
    POINT        dragStart = {};
    POINT        winStart = {};
    int          popupFixedX = 50;
    int          popupFixedY = 50;
    std::wstring currentToken;
    float currentWidth = 0.f;
}

namespace Gfx
{
    ComPtr<ID2D1Factory>        factory;
    ComPtr<IDWriteFactory>      writeFactory;
    ComPtr<ID2D1DCRenderTarget> dcRT;
    ComPtr<IDWriteTextFormat>   fmtItem;
    ComPtr<IDWriteTextFormat>   fmtHeader;
}

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
    float force = k * (target - value) - b * velocity;
    velocity += force * dt;
    value += velocity * dt;

    bool done =
        std::abs(target - value) <= eps &&
        std::abs(velocity) <= eps;

    if (done)
    {
        value = target;
        velocity = 0.f;
    }

    return !done;
}