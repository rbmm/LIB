#pragma once

//#define _PRINT_CPP_NAMES_
//#include "../inc/asmfunc.h"
#define ASM_FUNCTION

PVOID* __fastcall findPVOID(SIZE_T, PVOID*, PVOID)ASM_FUNCTION;
ULONG* __fastcall findDWORD(SIZE_T, ULONG*, ULONG)ASM_FUNCTION;
USHORT* __fastcall findWORD(SIZE_T, USHORT*, USHORT)ASM_FUNCTION;

PSTR __fastcall strnstr(SIZE_T n1, const void* str1, SIZE_T n2, const void* str2)ASM_FUNCTION;
PSTR __fastcall strnchr(SIZE_T n1, const void* str1, char c)ASM_FUNCTION;
PWSTR __fastcall wtrnstr(SIZE_T n1, const void* str1, SIZE_T n2, const void* str2)ASM_FUNCTION;
PWSTR __fastcall wtrnchr(SIZE_T n1, const void* str1, WCHAR c)ASM_FUNCTION;

#define _strnstr(a, b, x) strnstr(RtlPointerToOffset(a, b), a, sizeof(x) - 1, x)
#define _strnstrL(a, b, x) strnstr(RtlPointerToOffset(a, b), a, strlen(x), x)
#define _strnchr(a, b, c) strnchr(RtlPointerToOffset(a, b), a, c)
#define _strnstrS(a, b, s, x) strnstr(RtlPointerToOffset(a, b), a, s, x)

#define LP(str) RTL_NUMBER_OF(str) - 1, str

#undef ASM_FUNCTION
