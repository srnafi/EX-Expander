#pragma once
#include <string>

// Deletes the typed token and pastes the expansion value
// in its place using the clipboard.
void ReplaceWithExpansion(const std::wstring& token,
    const std::wstring& value);