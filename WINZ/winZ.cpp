// winZ.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"

_NT_BEGIN

#include "window.h"
#include "taskbar.h"
#define MAX_DESTRUCTOR_COUNT 4
#include "../inc/initterm.h"

#pragma comment(linker, "/export:findPVOID")
#pragma comment(linker, "/export:findDWORD")
#pragma comment(linker, "/export:findWORD")
#pragma comment(linker, "/export:wtrnstr")
#pragma comment(linker, "/export:wtrnchr")
#pragma comment(linker, "/export:strnstr")
#pragma comment(linker, "/export:strnchr")

BOOLEAN APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       PVOID /*lpReserved*/
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(hModule);
		s_uTaskbarRestart = RegisterWindowMessage(L"TaskbarCreated");
		return ZWnd::_InitClass() != 0;

	case DLL_PROCESS_DETACH:
		ZWnd::_DoneClass();
		break;
	}
    return TRUE;
}

_NT_END