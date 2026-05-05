#include "PopupUtils.h"

// ---------------------------------------------------------------------------
// HELPERS
// ---------------------------------------------------------------------------

void ClampIndex(int& idx, int size)
{
    if (size <= 0) { idx = 0; return; }
    if (idx >= size) idx = size - 1;
    if (idx < 0)    idx = 0;
}

// ---------------------------------------------------------------------------
// ACCENT
// ---------------------------------------------------------------------------

UINT32 DecodeCodepoint(const std::wstring& t)
{
    if (t.empty()) return 0;
    UINT32 cp = (UINT32)(unsigned short)t[0];
    if (t.size() >= 2 && t[0] >= 0xD800 && t[0] <= 0xDBFF)
        cp = 0x10000u + ((UINT32)(t[0] - 0xD800u) << 10) + (UINT32)(t[1] - 0xDC00u);
    return cp;
}

D2D1::ColorF GetAccent(const std::wstring& v)
{
    UINT32 cp = DecodeCodepoint(v);
    if (cp >= 0x1F600 && cp <= 0x1F64F) return { 1.0f, 0.80f, 0.10f };
    if (cp >= 0x1F300 && cp <= 0x1F5FF) return { 0.25f, 0.75f, 0.55f };
    if (cp >= 0x1F680 && cp <= 0x1F6FF) return { 0.20f, 0.60f, 1.0f };
    if (cp >= 0x1F900 && cp <= 0x1F9FF) return { 0.90f, 0.45f, 0.15f };
    if (cp >= 0x2600 && cp <= 0x27BF)  return { 1.0f,  0.85f, 0.10f };
    return { 0.25f, 0.55f, 1.0f };
}


