#pragma once
#include <string>
#include <vector>

// Forward declare Expansion
struct Expansion;

// Build JSON string from expansions
std::wstring BuildJson(const std::vector<Expansion>& data);