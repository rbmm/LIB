typedef unsigned char UCHAR;
typedef unsigned short USHORT;
typedef unsigned long ULONG;

#pragma warning(disable : 4073 4201)

#include "nt_ver.h"

extern "C"
__declspec(dllimport)
void
__stdcall
RtlGetNtVersionNumbers(
					   ULONG* pNtMajorVersion,
					   ULONG* pNtMinorVersion,
					   ULONG* pNtBuildNumber
					   );

NT_OS_VER::NT_OS_VER()
{
	ULONG M, m, b;
	RtlGetNtVersionNumbers(&M, &m, &b);
	Build = (USHORT)b;
	Minor = (UCHAR)m;
	Major = (UCHAR)M;
}

#pragma init_seg( lib )

NT_OS_VER g_nt_ver;
