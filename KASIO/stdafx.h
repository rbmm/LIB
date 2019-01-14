// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#define NOWINBASEINTERLOCK
#define _NTOS_
#define _NTDRIVER_
#include "../inc/stdafx.h"

_NT_BEGIN
#include <tdikrnl.h>
_NT_END
#pragma init_seg(lib)

// TODO: reference additional headers your program requires here
