#pragma once

#include <intrin.h>

#pragma code_seg(".text$zz")

#pragma intrinsic(strcmp, wcslen, strlen)

#define _EXTERN_C_BEGIN extern "C" {
#define _EXTERN_C_END }

_EXTERN_C_BEGIN

#pragma warning(disable : 4273)

// General
NTSTATUS NTAPI NtClose( _In_ HANDLE Handle );

// System
NTSTATUS NTAPI NtQuerySystemInformation ( IN SYSTEM_INFORMATION_CLASS SystemInformationClass, OUT PVOID SystemInformation, IN ULONG SystemInformationLength, OUT PULONG ReturnLength OPTIONAL );

// Section
NTSTATUS NTAPI NtOpenSection( _Out_ PHANDLE SectionHandle, _In_ ACCESS_MASK DesiredAccess, _In_ POBJECT_ATTRIBUTES ObjectAttributes );
NTSTATUS NTAPI NtQuerySection ( IN HANDLE SectionHandle, IN ULONG SectionInformationClass, OUT PVOID SectionInformation, IN ULONG SectionInformationLength, OUT PULONG ResultLength OPTIONAL );
NTSTATUS NTAPI NtUnmapViewOfSection( _In_ HANDLE ProcessHandle, _In_opt_ PVOID BaseAddress );

// Memory
NTSTATUS NTAPI NtAllocateVirtualMemory( _In_ HANDLE ProcessHandle, _Inout_ PVOID *BaseAddress, _In_ ULONG_PTR ZeroBits, _Inout_ PSIZE_T RegionSize, _In_ ULONG AllocationType, _In_ ULONG Protect );
NTSTATUS NTAPI NtQueryVirtualMemory ( IN HANDLE ProcessHandle, IN PVOID BaseAddres, IN MEMORY_INFORMATION_CLASS MemoryInformationClass, OUT PVOID MemoryInformation, IN SIZE_T MemoryInformationLength, OUT PSIZE_T ReturnLength OPTIONAL );
NTSTATUS NTAPI NtProtectVirtualMemory ( IN HANDLE ProcessHandle, IN OUT PVOID* BaseAddres, IN OUT PSIZE_T ProtectSize, IN ULONG NewProtect, OUT PULONG OldProtect );
NTSTATUS NTAPI NtWriteVirtualMemory(HANDLE ProcessHandle, PVOID BaseAddres, PVOID Buffer, SIZE_T BufferLength, PSIZE_T ReturnLength);
NTSTATUS NTAPI NtFreeVirtualMemory( _In_ HANDLE ProcessHandle, _Inout_ PVOID *BaseAddress, _Inout_ PSIZE_T RegionSize, _In_ ULONG FreeType );

// Process/Thread
NTSTATUS NTAPI NtOpenProcess(PHANDLE ProcessHandle, ULONG DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes, PCLIENT_ID Cid);
NTSTATUS NTAPI NtOpenThread(PHANDLE ThreadHandle, ULONG DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes, PCLIENT_ID Cid);
NTSTATUS NTAPI NtQueryInformationThread(HANDLE hThread, THREADINFOCLASS InformationClass, PVOID Information, ULONG InformationLength, PULONG ReturnLength );
NTSTATUS NTAPI NtQueueApcThread(HANDLE hThread, PKNORMAL_ROUTINE ApcRoutine, PVOID ApcContext, PVOID Argument1, PVOID Argument2);
NTSTATUS NTAPI NtSetContextThread ( IN HANDLE ThreadHandle, IN _CONTEXT* Context );

// Ldr
NTSTATUS NTAPI LdrUnloadDll(HMODULE DllBase);
NTSTATUS NTAPI LdrLoadDll ( PCWSTR SearchPaths, PULONG pFlags, PCUNICODE_STRING DllName, HMODULE* pDllBase );
NTSTATUS NTAPI LdrGetProcedureAddress ( HMODULE hModule, const ANSI_STRING* ProcedureName, ULONG Ordinal, void** pAddress );
NTSTATUS NTAPI LdrEnumerateLoadedModules ( int, PFNENUMERATEMODULES pfn, PVOID UserData );
NTSTATUS NTAPI LdrGetDllHandle(LPCWSTR szPath, int, PCUNICODE_STRING DllName, HMODULE* phmod);
PIMAGE_BASE_RELOCATION NTAPI LdrProcessRelocationBlock(PVOID VirtualAddress, ULONG RelocCount, PUSHORT TypeOffset, LONG_PTR Delta);

// RtlImage
PVOID NTAPI RtlAddressInSectionTable ( PIMAGE_NT_HEADERS NtHeaders, PVOID Base, ULONG Rva );
PVOID NTAPI RtlImageDirectoryEntryToData ( PVOID Base, BOOLEAN MappedAsImage, USHORT DirectoryEntry, PULONG Size );

// RtlStrings
BOOLEAN NTAPI RtlCreateUnicodeStringFromAsciiz ( OUT PUNICODE_STRING DestinationString, IN const char* SourceString );
BOOLEAN NTAPI RtlEqualUnicodeString(PCUNICODE_STRING String1, PCUNICODE_STRING String2, __in BOOLEAN CaseInSensitive );
VOID NTAPI RtlInitAnsiString( _Out_ PANSI_STRING DestinationString, _In_opt_z_ __drv_aliasesMem PCSZ SourceString );
VOID NTAPI RtlInitUnicodeString( _Out_ PUNICODE_STRING DestinationString, _In_opt_z_ __drv_aliasesMem PCWSTR SourceString );
VOID NTAPI RtlFreeUnicodeString( _Inout_ _At_(UnicodeString->Buffer, _Frees_ptr_opt_) PUNICODE_STRING UnicodeString );

// Vex
PVOID NTAPI RtlAddVectoredExceptionHandler( __in ULONG FirstHandler, __in PVECTORED_EXCEPTION_HANDLER VectoredHandler );
ULONG NTAPI RtlRemoveVectoredExceptionHandler( __in PVOID Handle );

// Frame
TEB_ACTIVE_FRAME* NTAPI RtlGetFrame();
VOID NTAPI RtlPushFrame(TEB_ACTIVE_FRAME* Frame);
VOID NTAPI RtlPopFrame(TEB_ACTIVE_FRAME* Frame);

// runtime
wchar_t * __cdecl wcsrchr(__in_z wchar_t *_Str, __in wchar_t _Ch);
int __cdecl wcscmp(const wchar_t *, const wchar_t *);
void* __cdecl memcpy(PVOID Destination, PVOID Source, ULONG Length );
void * __cdecl memset(void *dest, int c, size_t count );
ULONG
__cdecl
DbgPrint (
		  _In_z_ _Printf_format_string_ PCSTR Format,
		  ...
		  );

#pragma warning(default : 4273)

_EXTERN_C_END

PVOID __fastcall get_hmod(PCWSTR lpModuleName);
PVOID __fastcall GetFuncAddressEx(PIMAGE_DOS_HEADER pidh, PCSTR lpsz);
