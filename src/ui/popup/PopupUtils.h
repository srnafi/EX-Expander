#pragma once
#include <string>
#include <d2d1.h>

// Clamp index into valid range
void ClampIndex(int& idx, int size);

// Decode Unicode codepoint
unsigned int DecodeCodepoint(const std::wstring& t);

// Get accent color based on emoji
D2D1::ColorF GetAccent(const std::wstring& v);