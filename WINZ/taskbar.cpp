#include "stdafx.h"

_NT_BEGIN

#include "taskbar.h"

UINT s_uTaskbarRestart = RegisterWindowMessage(L"TaskbarCreated");

BOOL AddTaskbarIcon(HWND hwnd, UINT uID, UINT uCallbackMessage, PCWSTR szTip, PCWSTR idIcon)
{
	BOOL f = FALSE;

	NOTIFYICONDATA ni={sizeof(ni)};

	if (ni.hIcon = (HICON)LoadImage((HINSTANCE)&__ImageBase, idIcon, IMAGE_ICON, 16, 16, 0))
	{
		ni.hWnd = hwnd;
		ni.uID = uID;
		ni.uCallbackMessage = uCallbackMessage;
		ni.uFlags = NIF_MESSAGE|NIF_ICON|NIF_TIP;
		ni.uVersion = NOTIFYICON_VERSION_4;
		wcscpy(ni.szTip, szTip);
		f = Shell_NotifyIcon(NIM_ADD, &ni);
		DestroyIcon(ni.hIcon);
	}

	return f;
}

void DelTaskbarIcon(HWND hwnd, UINT uID)
{
	NOTIFYICONDATA ni={sizeof(ni)};
	ni.hWnd = hwnd;
	ni.uID = uID;
	Shell_NotifyIcon(NIM_DELETE, &ni);
}

_NT_END