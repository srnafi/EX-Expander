#include "PopupAnimation.h"
#include <PopupState.h>
#include <PopupRenderer.h>

void StartTimer()
{
    if (!State::timerActive)
    {
        SetTimer(State::hwnd, Anim::TimerID, Anim::Interval, nullptr);
        State::timerActive = true;
    }
}

void StopTimer()
{
    if (State::timerActive)
    {
        KillTimer(State::hwnd, Anim::TimerID);
        State::timerActive = false;
    }
}

// ---------------------------------------------------------------------------
// TICK  (one animation step, 16ms)
// ---------------------------------------------------------------------------

void Tick()
{
    bool sMoving = State::scrollOffset.step(Anim::SStiffness, Anim::SDamping,
        Anim::Dt, Anim::EpsS);
    bool oMoving = State::opacity.step(Anim::OStiffness, Anim::ODamping,
        Anim::Dt, Anim::EpsO);
    RenderLayered();
    if (!sMoving && !oMoving) StopTimer();
}