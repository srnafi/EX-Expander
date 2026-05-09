#pragma once
#include <string>
#include <dwrite.h>

// ---------------------------------------------------------------------------
// PopupLayout
//
// Measures text using DirectWrite and computes the required popup width
// based on the currently selected expansion item.
// ---------------------------------------------------------------------------

// Measure the display width of text in a given DirectWrite format.
// Returns 0.f if the factory or format is null, or if measurement fails.
float MeasureTextWidth(const std::wstring& text, IDWriteTextFormat* fmt);

// Compute the popup's required width based on the currently selected
// expansion's value. Width snaps instantly (no animation).
// Returns a value between UI::SpineWidth and UI::MaxWidth.
float ComputeTargetWidth();