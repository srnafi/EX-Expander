#pragma once
#include <windows.h>
#include <string>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

namespace UI
{
    extern const float MaxWidth;
    extern const float HorzPad;
    extern const int   ItemHeight;
    extern const int   Padding;
    extern const float OuterRadius;
    extern const float CardRadius;
    extern const float FontSize;
    extern const float HeaderHeight;
}

namespace Anim
{
    extern const int   TimerID;
    extern const int   Interval;
    extern const float SStiffness;
    extern const float SDamping;
    extern const float OStiffness;
    extern const float ODamping;
    extern const float Dt;
    extern const float EpsS;
    extern const float EpsO;
    extern bool        animEnabled;
}

namespace State
{
    extern HWND hwnd;
    extern int  centerIndex;
    extern int  lastKey;
    extern bool timerActive;

    struct Spring
    {
        float value = 0.f;
        float velocity = 0.f;
        float target = 0.f;

        void snap(float t);
        void setTarget(float t);
        bool step(float k, float b, float dt, float eps);
    };

    extern Spring        scrollOffset;
    extern Spring        opacity;
    extern bool          dragging;
    extern POINT         dragStart;
    extern POINT         winStart;
    extern int           popupFixedX;
    extern int           popupFixedY;
    extern std::wstring  currentToken;
    extern float         currentWidth;
}

namespace Gfx
{
    extern ComPtr<ID2D1Factory>          factory;
    extern ComPtr<IDWriteFactory>        writeFactory;
    extern ComPtr<ID2D1DCRenderTarget>   dcRT;
    extern ComPtr<IDWriteTextFormat>     fmtItem;
    extern ComPtr<IDWriteTextFormat>     fmtHeader;
}