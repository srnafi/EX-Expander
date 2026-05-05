#pragma once
#include <windows.h>

// ENTRYPOINT API

// Initialize popup system
void PopupInit(HINSTANCE hInst);

// Whether popup is navigating
bool IsPopupNavigating();

// Enable / disable animation
void PopupSetAnimation(bool enabled);