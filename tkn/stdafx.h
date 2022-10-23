#define _NTDRIVER_
#define _INC_SWPRINTF_INL_

#include "../inc/StdAfx.h"

NTDLL_(int) swprintf(wchar_t * _String, const wchar_t * _Format, ...);

#include <intrin.h>
