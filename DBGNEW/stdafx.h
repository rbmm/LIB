
#include "../inc/StdAfx.h"

#include <winuser.h>
//#include <afxres.h>
#include <commctrl.h>
#include <intrin.h>
#include <WINDOWSX.H>
#include "../inc/msdis170.h"
#include <Objsafe.h>
#include <ActivScp.h>
#include <commoncontrols.h>
#include <DbgHelp.h>
#include <DbgEng.h>
#include <ws2ipdef.h>
#include <mstcpip.h>
#include <Uxtheme.h>
#include <shobjidl_core.h >
#define NOEXTAPI
#include <WDBGEXTS.H>
//////
#if defined(_X86_)
#include "../inc/x86plat.h"
#elif defined(_AMD64_)
#include "../inc/amd64plat.h"
#endif

#define PROCESS_ALL_ACCESS_XP (SYNCHRONIZE | 0xFFF)

#define DbgPrint /##/
