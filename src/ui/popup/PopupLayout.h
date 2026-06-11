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

// Returns UI::MaxWidth.
float ComputeTargetWidth();