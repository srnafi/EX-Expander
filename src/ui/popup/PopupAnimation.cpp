#include <windows.h>
#include "PopupAnimation.h"
#include "PopupState.h"
#include "PopupRenderer.h"
#include "AppLog.h"


// ---------------------------------------------------------------------------
// TIMER CONTROL
// ---------------------------------------------------------------------------

void StartTimer()
{
    if (State::timerActive)
        return;

    UINT_PTR result = SetTimer(State::hwnd, Anim::TimerID, Anim::Interval, nullptr);
    if (!result)
    {
        AppLog::Error(L"PopupAnimation: SetTimer failed – animation will not run");
        return;
    }

    State::timerActive = true;
}

void StopTimer()
{
    if (!State::timerActive)
        return;

    KillTimer(State::hwnd, Anim::TimerID);
    State::timerActive = false;
}

// ---------------------------------------------------------------------------
// TICK  –  one animation frame (~16ms at 60fps)
// ---------------------------------------------------------------------------

void Tick()
{
    // If animation was disabled mid-flight, stop immediately
    if (!Anim::animEnabled)
    {
        StopTimer();
        return;
    }

    bool scrollMoving = State::scrollOffset.step(
        Anim::SStiffness, Anim::SDamping, Anim::Dt, Anim::EpsS);

    bool opacityMoving = State::opacity.step(
        Anim::OStiffness, Anim::ODamping, Anim::Dt, Anim::EpsO);

    RenderLayered();

    // Both springs settled – no need to keep the timer alive
    if (!scrollMoving && !opacityMoving)
        StopTimer();
}