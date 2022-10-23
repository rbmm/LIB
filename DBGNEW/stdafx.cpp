// stdafx.cpp : source file that includes just the standard includes
// winx.pch will be the pre-compiled header
// stdafx.obj will contain the pre-compiled type information

#include "stdafx.h"
#ifdef _X86_
extern "C"
{
	uintptr_t __security_cookie;

	void __fastcall __security_check_cookie(__in uintptr_t _StackCookie)
	{
		if (__security_cookie != _StackCookie)
		{
			__debugbreak();
		}
	}

	BOOL __cdecl _ValidateImageBase(PVOID pImageBase)
	{
		return NT::RtlImageNtHeader(pImageBase) != 0;
	}

	PIMAGE_SECTION_HEADER __cdecl _FindPESection(PVOID pImageBase, ULONG rva)
	{
		return NT::RtlImageRvaToSection(NT::RtlImageNtHeader(pImageBase), pImageBase, rva);
	}

#pragma comment(linker, "/include:@__security_check_cookie@4")
#pragma comment(linker, "/include:__ValidateImageBase")
#pragma comment(linker, "/include:__FindPESection")

}

#endif
