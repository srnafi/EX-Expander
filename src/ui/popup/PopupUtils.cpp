#include "PopupUtils.h"

// ===========================================================================
// EMOJI ACCENT COLORS
// ===========================================================================

namespace AccentColors
{
    const D2D1::ColorF Smileys{ 1.00f, 0.80f, 0.10f };  // yellow
    const D2D1::ColorF Pictogram{ 0.25f, 0.75f, 0.55f };  // teal-green
    const D2D1::ColorF Transport{ 0.20f, 0.60f, 1.00f };  // blue
    const D2D1::ColorF Symbols{ 0.90f, 0.45f, 0.15f };  // orange
    const D2D1::ColorF MiscGold{ 1.00f, 0.85f, 0.10f };  // gold
    const D2D1::ColorF Default{ 0.25f, 0.55f, 1.00f };  // soft blue
}

// ===========================================================================
// EMOJI UNICODE BLOCK RANGES
// ===========================================================================

namespace EmojiRanges
{
    constexpr unsigned int SmileysStart = 0x1F600;
    constexpr unsigned int SmileysEnd = 0x1F64F;

    constexpr unsigned int PictogramStart = 0x1F300;
    constexpr unsigned int PictogramEnd = 0x1F5FF;

    constexpr unsigned int TransportStart = 0x1F680;
    constexpr unsigned int TransportEnd = 0x1F6FF;

    constexpr unsigned int SymbolsStart = 0x1F900;
    constexpr unsigned int SymbolsEnd = 0x1F9FF;

    constexpr unsigned int MiscGoldStart = 0x2600;
    constexpr unsigned int MiscGoldEnd = 0x27BF;
}

// ===========================================================================
// PUBLIC API
// ===========================================================================

void ClampIndex(int& index, int size)
{
    if (size <= 0)
    {
        index = 0;
        return;
    }

    if (index >= size) index = size - 1;
    if (index < 0)     index = 0;
}

unsigned int DecodeCodepoint(const std::wstring& text)
{
    if (text.empty())
        return 0;

    // Single-unit (BMP) characters — direct cast
    unsigned int cp = static_cast<unsigned int>(static_cast<unsigned short>(text[0]));

    // Check for surrogate pair (UTF-16 supplementary plane)
    // High surrogate range: 0xD800–0xDBFF
    // Low  surrogate range: 0xDC00–0xDFFF
    constexpr unsigned int HighSurrogateStart = 0xD800;
    constexpr unsigned int HighSurrogateEnd = 0xDBFF;
    constexpr unsigned int LowSurrogateStart = 0xDC00;
    constexpr unsigned int SupplementaryBase = 0x10000;

    if (text.size() >= 2 &&
        text[0] >= HighSurrogateStart &&
        text[0] <= HighSurrogateEnd)
    {
        // Decode supplementary codepoint:
        //   cp = 0x10000 + ((high - 0xD800) << 10) + (low - 0xDC00)
        cp = SupplementaryBase
            + (static_cast<unsigned int>(text[0] - HighSurrogateStart) << 10)
            + static_cast<unsigned int>(text[1] - LowSurrogateStart);
    }

    return cp;
}

D2D1::ColorF GetAccent(const std::wstring& emoji)
{
    using namespace EmojiRanges;
    using namespace AccentColors;

    const unsigned int cp = DecodeCodepoint(emoji);

    if (cp >= SmileysStart && cp <= SmileysEnd)   return Smileys;
    if (cp >= PictogramStart && cp <= PictogramEnd) return Pictogram;
    if (cp >= TransportStart && cp <= TransportEnd) return Transport;
    if (cp >= SymbolsStart && cp <= SymbolsEnd)   return Symbols;
    if (cp >= MiscGoldStart && cp <= MiscGoldEnd)  return MiscGold;

    return Default;
}