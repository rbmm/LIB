#pragma once

#include "winZ.h"

BOOL WINZ_API AddTaskbarIcon(HWND hwnd, UINT uID, UINT uCallbackMessage, PCWSTR szTip, PCWSTR idIcon);
void WINZ_API DelTaskbarIcon(HWND hwnd, UINT uID);

extern WINZ_API UINT s_uTaskbarRestart;