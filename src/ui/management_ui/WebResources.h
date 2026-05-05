#pragma once
#include <string>

// Load embedded resource text
std::wstring LoadResourceText(int id);

// Build full HTML (inject CSS + JS)
std::wstring BuildHTML();