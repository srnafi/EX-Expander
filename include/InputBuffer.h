#pragma once
#include <windows.h>
#include <string>

void         BufferHandleKey(DWORD vkCode);
void         BufferReset();
std::wstring BufferGetToken();
bool         IsTriggerKey(DWORD vkCode);   // true if vkCode matches g_TriggerChar

extern bool inTheBuffer;