// StringUtils.h
#pragma once
#include <string>
#include <vector>

std::string          WideToUtf8(const std::wstring& wide);
std::wstring         Utf8ToWide(const char* utf8);
std::wstring         JsonEscape(const std::wstring& s);
std::vector<std::wstring> SplitW(const std::wstring& str, wchar_t delim, int maxParts = -1);
std::vector<std::wstring> ParseTags(const std::wstring& csv);

// Simple JSON field extractors (currently duplicated in webUI.cpp)
bool         JsonBoolW(const std::wstring& json, const std::wstring& key);
std::wstring JsonStringW(const std::wstring& json, const std::wstring& key);
int          JsonIntW(const std::wstring& json, const std::wstring& key);