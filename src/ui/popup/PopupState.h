#pragma once
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

// ---------------------------------------------------------------------------
// PopupState
//
// Central hub for all popup-related globals:
//   UI    – layout constants (sizes, fonts, colors)
//   Anim  – spring physics constants and animation toggle
//   State – runtime mutable state (window handle, springs, selection)
//   Gfx   – Direct2D / DirectWrite resource pointers
// ---------------------------------------------------------------------------

namespace UI
{
    extern const float SpineWidth;
    extern const float MaxWidth;
    extern const float HorzPad;
    extern const int   ItemHeight;
    extern const int   Padding;
    extern const float OuterRadius;
    extern const float CardRadius;
    extern const float FontCenter;
    extern const float FontSide;
}

namespace Anim
{
    extern const int   TimerID;
    extern const int   Interval;       // milliseconds (~60 fps)
    extern const float SStiffness;     // scroll spring stiffness
    extern const float SDamping;       // scroll spring damping
    extern const float OStiffness;     // opacity spring stiffness
    extern const float ODamping;       // opacity spring damping
    extern const float Dt;             // physics timestep (seconds)
    extern const float EpsS;           // scroll settle threshold
    extern const float EpsO;           // opacity settle threshold

    extern bool animEnabled;           // master animation on/off
}

namespace State
{
    extern HWND hwnd;
    extern int  centerIndex;
    extern int  lastKey;
    extern bool timerActive;

    // ---------------------------------------------------------------------------
    // Critically-damped spring for smooth animation.
    //
    //   value     – current position
    //   target    – where the spring is pulling toward
    //   velocity  – current rate of change
    //
    // Direct field access is intentional — Spring is a POD-like struct.
    // ---------------------------------------------------------------------------
    struct Spring
    {
        float value = 0.f;
        float velocity = 0.f;
        float target = 0.f;

        // Set value and target instantly, killing velocity
        void snap(float t);

        // Set target only — value will spring toward it over time
        void setTarget(float t);

        // Advance one physics step. Returns true if still moving.
        //
        //   k   – stiffness
        //   b   – damping
        //   dt  – timestep
        //   eps – settle threshold
        bool step(float k, float b, float dt, float eps);
    };

    extern Spring scrollOffset;
    extern Spring opacity;

    extern float currentWidth;
    extern float spineLeft;

    extern bool  dragging;
    extern POINT dragStart;
    extern POINT winStart;

    // Fixed popup position (when not in cursor-follow mode)
    extern int popupFixedX;
    extern int popupFixedY;
}

namespace Gfx
{
    extern ComPtr<ID2D1Factory>        factory;
    extern ComPtr<IDWriteFactory>      writeFactory;
    extern ComPtr<ID2D1DCRenderTarget> dcRT;
    extern ComPtr<IDWriteTextFormat>   fmtCenter;
    extern ComPtr<IDWriteTextFormat>   fmtSide;
}