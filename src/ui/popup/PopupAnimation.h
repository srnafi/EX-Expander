#pragma once

// ---------------------------------------------------------------------------
// PopupAnimation
//
// Drives the two springs that animate the popup:
//   - State::scrollOffset   (items gliding past the center slot)
//   - State::opacity        (fade in / fade out)
//
// The timer runs at ~60fps ONLY while at least one spring is still moving.
// It auto-stops when both springs settle, so CPU cost between keypresses
// is zero.
// ---------------------------------------------------------------------------

// Start the animation timer (no-op if already running)
void StartTimer();

// Stop the animation timer (no-op if already stopped)
void StopTimer();

// Advance both springs by one frame and re-render.
// Called by WM_TIMER in PopupWindow. Stops the timer automatically
// when both springs have settled.
void Tick();