#pragma once

// ---------------------------------------------------------------------------
// PopupInput
//
// Two functions drive the popup from the keyboard hook:
//
//   PopupHandleKeyInternal  – call first, stores the key for this frame
//   PopupUpdateInternal     – call second, processes state and redraws
//
// Always call them in that order from KeyboardHook.cpp.
// ---------------------------------------------------------------------------

// Store the key pressed this frame (VK_UP, VK_DOWN, VK_RETURN, or 0).
// Processing is deferred to PopupUpdateInternal so that state and
// rendering stay in one place.
void PopupHandleKeyInternal(int vkCode);

// Re-query the database, update selection, reposition the window,
// and trigger a render / animation tick.
void PopupUpdateInternal();