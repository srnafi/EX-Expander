#pragma once
#include <string>
#include <dwrite.h>

// Measure text width
float MeasureTextWidth(const std::wstring& text, IDWriteTextFormat* fmt);

// Compute popup width based on selected item
float ComputeTargetWidth();