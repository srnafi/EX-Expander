#pragma once
#include <string>
#include <d2d1.h>

// ---------------------------------------------------------------------------
// PopupUtils
//
// Small utility functions used across the popup system:
//   - ClampIndex      : safe index clamping
//   - DecodeCodepoint : UTF-16 → Unicode codepoint conversion
//   - GetAccent       : emoji-based accent color selection
// ---------------------------------------------------------------------------

// Clamp index into [0, size). Sets index to 0 if size is non-positive.
void ClampIndex(int& index, int size);

// Decode the first codepoint from a UTF-16 string.
// Handles surrogate pairs correctly for emoji and other supplementary
// plane characters. Returns 0 if the string is empty.
unsigned int DecodeCodepoint(const std::wstring& text);

// Get an accent color based on the first emoji codepoint.
// Maps Unicode emoji blocks to themed colors:
//   - Smileys (U+1F600–U+1F64F)            → yellow
//   - Misc Symbols & Pictographs (U+1F300–U+1F5FF) → teal-green
//   - Transport & Map (U+1F680–U+1F6FF)    → blue
//   - Supplemental Symbols (U+1F900–U+1F9FF) → orange
//   - Misc Symbols (U+2600–U+27BF)         → gold
//   - Default                              → blue
D2D1::ColorF GetAccent(const std::wstring& emoji);