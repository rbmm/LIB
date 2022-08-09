#define SECURITY_WIN32

#include "../inc/StdAfx.h"

#include <iphlpapi.h>
#include <WinDNS.h>
#include <ws2ipdef.h>
#include <mstcpip.h>
_NT_BEGIN
#define SCHANNEL_USE_BLACKLISTS
#include <schannel.h>
#include <tdi.h>

_NT_END

#pragma init_seg(lib)