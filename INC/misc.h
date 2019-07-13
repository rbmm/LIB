#pragma once

#define _EXTERN_C_BEGIN extern "C" {
#define _EXTERN_C_END }

template<typename T> inline T* offset_ptr(T* ptr, int offset)
{
	return (T*)((LPBYTE)ptr + offset);
}

extern "C" {
	
extern IMAGE_DOS_HEADER __ImageBase;
	
}

typedef int (__cdecl * QSORTFN) (const void *, const void *);
typedef int (__cdecl * QSORTFN_S)(void *, const void *, const void *);

#ifndef _NTDRIVER_

NTDLL_(LONGLONG)
RtlInterlockedCompareExchange64 (
								 LONGLONG volatile *Destination,
								 LONGLONG Exchange,
								 LONGLONG Comperand
							  );

#define InterlockedPopEntrySList(Head) RtlInterlockedPopEntrySList(Head)
#define InterlockedPushEntrySList(Head, Entry) RtlInterlockedPushEntrySList(Head, Entry)
#define InterlockedFlushSList(Head) RtlInterlockedFlushSList(Head)
#define QueryDepthSList(Head) RtlQueryDepthSList(Head)
#define FirstEntrySList(Head) RtlFirstEntrySList(Head)

#ifndef _WIN64
#define InterlockedCompareExchange64(Destination, ExChange, Comperand) RtlInterlockedCompareExchange64(Destination, ExChange, Comperand)
#endif

#endif//_NTDRIVER_

#ifndef _WIN64
#define InterlockedCompareExchangePointer(Destination, ExChange, Comperand) \
	(PVOID)(LONG_PTR)InterlockedCompareExchange((PLONG)(Destination), (LONG)(LONG_PTR)(ExChange), (LONG)(LONG_PTR)(Comperand))

#define InterlockedExchangePointer(Destination, ExChange) \
	(PVOID)(LONG_PTR)InterlockedExchange((PLONG)(Destination), (LONG)(LONG_PTR)(ExChange))
#endif

#if 0//ndef _WIN64
#ifdef SetWindowLongPtrW
#undef SetWindowLongPtrW
#endif
#define SetWindowLongPtrW(hwnd, i, val)  ((LPARAM)SetWindowLongW(hwnd, i, (LONG)(LPARAM)(val)))
#ifdef GetWindowLongPtrW
#undef GetWindowLongPtrW
#endif
#define GetWindowLongPtrW(hwnd, i)  ((LPARAM)GetWindowLongW(hwnd, i))
#endif

#ifdef _WIN64
#define GetArbitraryUserPointer() (PVOID)__readgsqword(FIELD_OFFSET(NT_TIB, ArbitraryUserPointer))
#define SetArbitraryUserPointer(p) __writegsqword(FIELD_OFFSET(NT_TIB, ArbitraryUserPointer), (DWORD_PTR)(p))
#else
#define GetArbitraryUserPointer() (PVOID)__readfsdword(FIELD_OFFSET(NT_TIB, ArbitraryUserPointer))
#define SetArbitraryUserPointer(p) __writefsdword(FIELD_OFFSET(NT_TIB, ArbitraryUserPointer), (DWORD_PTR)(p))
#endif

/////////////////////////////////////////////////////////////
// error messages

#define FormatStatus(err, module, status) FormatMessage(\
	FORMAT_MESSAGE_IGNORE_INSERTS|FORMAT_MESSAGE_FROM_HMODULE,\
GetModuleHandleW(L ## # module),status, 0, err, RTL_NUMBER_OF(err), 0)

#define FormatWin32Status(err, status) FormatStatus(err, kernel32.dll, status)
#define FormatNTStatus(err, status) FormatStatus(err, ntdll.dll, status)

#define FormatStatusEx(err, status) \
{ if (!FormatNTStatus(err, status)) FormatWin32Status(err, status); }

#define FormatLastNTStatus(err) FormatNTStatus(err, RtlGetLastNtStatus())

//////////////////////////////////////////////////////////////////////////

inline ULONG BOOL_TO_ERROR(BOOL f)
{
	return f ? NOERROR : GetLastError();
}

#ifdef _malloca
#undef _malloca
#endif
#ifdef _freea
#undef _freea
#endif

#define _malloca(size) ((size) < PAGE_SIZE ? alloca(size) : LocalAlloc(0, size))

inline void _freea(PVOID pv)
{
	PNT_TIB tib = (PNT_TIB)NtCurrentTeb();
	if (pv < tib->StackLimit || tib->StackBase <= pv) LocalFree(pv);
}

////////////////////////////////////////////////////////////////
// CID

struct CID : CLIENT_ID
{
	CID(HANDLE _UniqueProcess, HANDLE _UniqueThread = 0)
	{
		UniqueThread = _UniqueThread;
		UniqueProcess = _UniqueProcess;
	}
};

///////////////////////////////////////////////////////////////
// CUnicodeString

class CUnicodeString : public UNICODE_STRING
{
public:
	CUnicodeString(PCWSTR String)
	{
		RtlInitUnicodeString(this,String);
	}
};

///////////////////////////////////////////////////////////////
// CObjectAttributes

struct CObjectAttributes : public OBJECT_ATTRIBUTES
{
	CObjectAttributes(LPCWSTR _ObjectName,
		HANDLE _RootDirectory = 0,
		ULONG _Attributes = OBJ_CASE_INSENSITIVE,
		PVOID _SecurityDescriptor = 0,
		PVOID _SecurityQualityOfService = 0
		)
	{
		Length = sizeof OBJECT_ATTRIBUTES;
		RtlInitUnicodeString(ObjectName = &mus,_ObjectName);
		RootDirectory = _RootDirectory;
		Attributes = _Attributes;
		SecurityDescriptor = _SecurityDescriptor;
		SecurityQualityOfService = _SecurityQualityOfService;
	}
	CObjectAttributes(PCUNICODE_STRING _ObjectName,
		HANDLE _RootDirectory = 0,
		ULONG _Attributes = OBJ_CASE_INSENSITIVE,
		PVOID _SecurityDescriptor = 0,
		PVOID _SecurityQualityOfService = 0
		)
	{
		Length = sizeof OBJECT_ATTRIBUTES;
		ObjectName = (PUNICODE_STRING)_ObjectName;
		RootDirectory = _RootDirectory;
		Attributes = _Attributes;
		SecurityDescriptor = _SecurityDescriptor;
		SecurityQualityOfService = _SecurityQualityOfService;
	}
private:
	UNICODE_STRING mus;
};

#ifdef _OBJBASE_H_

struct SZ_GUID
{
	WCHAR sz[39];
	
	SZ_GUID(REFGUID rguid){ StringFromGUID2(rguid, sz, 39); }
};

#endif

#define zx_(i, t, v, s, e) __rcb = i; do if (__cb < __rcb) stack = alloca(__rcb - __cb), __cb = RtlPointerToOffset(stack, _stack), v = (t*)stack; while (s == (status = e))
#define zx(i, t, v, s, e) iv(t, v), zx_(i, t, v, s, e)
#define iv(t, v) t* v = 0; __cb = 0
#define iz() volatile static char label(z);iz_(label(z))
#define iz_(s) LPVOID _stack = alloca(s), stack = _stack;DWORD __cb, __rcb
#define ez() NTSTATUS status;iz()
#define ez_() NTSTATUS status;iz_()

#define DoAdjustBuffer(type, buf, cb, rcb) do { AdjustBuffer(type, buf, cb, rcb)

#define AdjustBuffer(type, buf, cb, rcb)\
	if ((int)cb < (int)rcb) buf = (type*)alloca(rcb - cb), cb = rcb

#define WhileBufferSmall(status) } while (STATUS_BUFFER_SMALL(status))
#define WhileErrorStatus(status, err) } while ((status) == (err))

#define STATUS_BUFFER_SMALL(status) \
((status == STATUS_BUFFER_OVERFLOW) ||\
(status == STATUS_BUFFER_TOO_SMALL) ||\
(status == STATUS_INFO_LENGTH_MISMATCH))

#include "mini_yvals.h"

#define _makeachar(x) #@x
#define makeachar(x) _makeachar(x)
#define _makewchar(x) L## #@x
#define makewchar(x) _makewchar(x)
#define echo(x) x
#define label(x) echo(x)##__LINE__
#define __C_ASSERT__ label(assert)
#define callmacro(macro, x) macro x
#define callmacro1(macro, x) macro(x)
#define callmacro2(macro, x, y) macro(x, y)
#define callmacro3(macro, x, y, z) macro(x, y, z)
#define showmacro(x) __pragma(message(__FILE__ _CRT_STRINGIZE((__LINE__): \nmacro\t)#x" expand to\n" _CRT_STRINGIZE(x)))
#define addlib(x) comment(linker, _CRT_STRINGIZE(/defaultlib:x.lib))
#define _showmacro(x) __pragma(message(__FILE__ _CRT_STRINGIZE((__LINE__): \nmacro\t)#x" expand to\n" x))

#define ALIASPROC(proc_name) extern "C" __declspec(naked) void __stdcall proc_name() {}
#define EXTERN_ALIASPROC(proc_name) extern "C" { void __stdcall proc_name(); }

#ifdef IsEqualIID
#undef IsEqualIID
#endif
#define IsEqualIID(rguid1, rguid2) (!memcmp(&(rguid1), &(rguid2), sizeof(IID)))

#define NEXT_ITEM_(type, item, offset) \
((type*)((LPBYTE)(item) + (INT_PTR)(offset)))

#define NEXT_ITEM(type, item, offset) \
(item = NEXT_ITEM_(type, item, offset))

#define lenof(str) (sizeof(str) - sizeof(str[0]))
#define symof(str) ((sizeof(str) / sizeof(str[0]) - 1))

#define RtlEqualStr(sz, csz) (RtlEqualMemory(sz, csz, sizeof(csz) - sizeof(csz[0])))

#define ZWNUM(func)  (*(PULONG)(PBYTE(func)+1))

#define SYSCALL(func) \
	_pKeServiceDescriptorTable->ntoskrnl.ServiceTable \
[*(PULONG)(PBYTE(func)+1)]

#define GetZwIndex(name) (*(PULONG)((char*)Zw##name+1))
#define ZwIndex(name) Zw##name##Index
#define DeclareZwIndex(name) ULONG ZwIndex(name)=GetZwIndex(name);

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) if (p) (p)->Release(), (p) = 0
#endif

#define IID_PPV(pItf) __uuidof(*pItf),(void**)&pItf

#define CUNISTR(x) CUnicodeString(L ## # x)

#define ZERO_INIT_FROM_FILD(x) \
RtlZeroMemory(&x,sizeof *this - ((PUCHAR)&x - (PUCHAR)this));

#define ZERO_INIT_FILDS(fild1,fild2) \
RtlZeroMemory(&fild1,(PUCHAR)(&fild2+1)-(PUCHAR)&fild1);

#ifdef ROUND_TO_SIZE
#undef ROUND_TO_SIZE
#endif

#define _ROUND_TO_SIZE(length, align_1) ((((ULONG_PTR)(length)) + align_1) & ~align_1)
#define ROUND_TO_SIZE(length, align) _ROUND_TO_SIZE(length, ((ULONG_PTR)(align) - 1))
#define ROUND_TO_TYPE(n, type) ROUND_TO_SIZE((n) * sizeof(type), __alignof(type))
#define TYPE_ALIGN(Buffer, type) ((type*)ROUND_TO_SIZE(Buffer, __alignof(type)))

#define RectToPointSize(rc) rc.left, (rc).top, (rc).right - (rc).left, (rc).bottom - (rc).top
#define PointSizeToRect(rc, x, y, cx, cy) \
((rc).left = x, (rc).top = y, (rc).right = (x) + (cx), (rc).bottom = (y) + (cy))

#define StringEnd(sz) (&sz[RTL_NUMBER_OF(sz)])
#define RemainingLen(sz, lpsz) (StringEnd(sz) - (lpsz))

#define RTL_CONSTANT_STRINGA(s) { sizeof( s ) - sizeof( (s)[0] ), sizeof( s ), const_cast<PSTR>(s) }
#define RTL_CONSTANT_STRINGW_(s) { sizeof( s ) - sizeof( (s)[0] ), sizeof( s ), const_cast<PWSTR>(s) }
#define RTL_CONSTANT_STRINGW(s) RTL_CONSTANT_STRINGW_(echo(L)echo(s))

#define STATIC_UNICODE_STRING(name, str) \
static const WCHAR label(__)[] = echo(L)str;\
static const UNICODE_STRING name = RTL_CONSTANT_STRINGW_(label(__))

#define STATIC_ANSI_STRING(name, str) \
static const CHAR label(__)[] = str;\
static const ANSI_STRING name = RTL_CONSTANT_STRINGA(label(__))

#define STATIC_ASTRING(name, str) static const CHAR name[] = str
#define STATIC_WSTRING(name, str) static const WCHAR name[] = echo(L)str

#define STATIC_UNICODE_STRING_(name) STATIC_UNICODE_STRING(name, #name)
#define STATIC_WSTRING_(name) STATIC_WSTRING(name, #name)
#define STATIC_ANSI_STRING_(name) STATIC_ANSI_STRING(name, #name)
#define STATIC_ASTRING_(name) STATIC_ASTRING(name, #name)

#define STATIC_OBJECT_ATTRIBUTES(oa, name)\
	STATIC_UNICODE_STRING(label(m), name);\
	static OBJECT_ATTRIBUTES oa = { sizeof(oa), 0, const_cast<PUNICODE_STRING>(&label(m)), OBJ_CASE_INSENSITIVE }

#define STATIC_OBJECT_ATTRIBUTES_EX(oa, name, a, sd, sqs)\
	STATIC_UNICODE_STRING(label(m), name);\
	static OBJECT_ATTRIBUTES oa = { sizeof(oa), 0, const_cast<PUNICODE_STRING>(&label(m)), a, sd, sqs }

#define DISTANCE(p, q) (LPBYTE(q) - LPBYTE(p))

#define ADRIVER_KEY(x) echo("\\Registry\\Machine\\System\\CurrentControlSet\\Services\\"L)x

#define STATIC_DRIVER_KEY(u, x) STATIC_UNICODE_STRING(u, ADRIVER_KEY(x))
#define LOADDRIVER(x) ZwLoadDriver(&CUnicodeString(echo(L)ADRIVER_KEY(x)))
#define UNLOADDRIVER(x) ZwUnloadDriver(&CUnicodeString(echo(L)ADRIVER_KEY(x)))

#define OPENPORTS() \
ZwSetInformationProcess(NtCurrentProcess(), ProcessUserModeIOPL, 0, 0);

#define RtlCreateThreadEx(hProcess,bSuspend,pfn,arg,hThread,cid)\
	RtlCreateUserThread(hProcess,0,bSuspend,0,\
	KERNEL_STACK_SIZE,0,pfn,(void*)(arg),hThread,cid)

#define RtlCreateThread(pfn,p,h) \
	RtlCreateThreadEx(NtCurrentProcess(),0,pfn,p,h,0)

//////////////////////////////////////////////////////////////
// Inrange

#define INRANGE(base, address, size) ((DWORD_PTR)(address) - (DWORD_PTR)(base) < (DWORD_PTR)(size))

#define INRANGE_SIZE(lpA, lpB, sizeB, sizeA) \
	(((DWORD_PTR)(lpB) - (DWORD_PTR)(lpA) < (DWORD_PTR)(sizeA)) &&\
	((DWORD_PTR)(sizeB) <= (DWORD_PTR)(sizeA) - (DWORD_PTR)(lpB) + (DWORD_PTR)(lpA)))


#pragma warning(default : 4005)
