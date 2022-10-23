#include "StdAfx.h"

_NT_BEGIN

#include "types.h"
#include "../tkn/tkn.h"

NTSTATUS DoIoControl(ULONG code);
NTSTATUS MyOpenProcess(PHANDLE ProcessHandle, ULONG DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes, PCLIENT_ID Cid);
NTSTATUS MyOpenThread(PHANDLE ThreadHandle, ULONG DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes, PCLIENT_ID Cid);

BOOLEAN IsExportSuppressionEnabled(HANDLE hProcess);
NTSTATUS SetExportValid(HANDLE hProcess, LPCVOID pv1, LPCVOID pv2);
NTSTATUS SetExportValid(HANDLE hProcess, LPCVOID pv);

extern volatile const UCHAR guz;
extern OBJECT_ATTRIBUTES zoa;

#ifdef _WIN64

#include "../wow/wow.h"

BEGIN_DLL_FUNCS(kernel32, 0)
	FUNC(LoadLibraryExW),
	FUNC(VirtualFree),
END_DLL_FUNCS();

BEGIN_DLL_FUNCS(ntdll, &kernel32)
	FUNC(LdrQueryProcessModuleInformation),
	FUNC(RtlExitUserThread),
	FUNC(LdrUnloadDll),
END_DLL_FUNCS();

#endif

typedef NTSTATUS (NTAPI * QUEUEAPC)(HANDLE hThread, PKNORMAL_ROUTINE ApcRoutine, PVOID ApcContext, PVOID Argument1, PVOID Argument2);

NTSTATUS ApcInjector(PCWSTR lpDllName, HANDLE hProcess, HANDLE hThread, QUEUEAPC QueueApc, PVOID pvLoad, PVOID pvFree)
{
	PVOID BaseAddress = 0;
	SIZE_T Size = (wcslen(lpDllName) + 1) << 1, Len = Size;

	NTSTATUS status = ZwAllocateVirtualMemory(hProcess, &BaseAddress, 0, &Size, MEM_COMMIT, PAGE_READWRITE);

	if (0 <= status)
	{
		if (0 <= (status = ZwWriteVirtualMemory(hProcess, BaseAddress, (PVOID)lpDllName, (ULONG)Len, 0)))
		{		
			if (0 <= (status = QueueApc(hThread, (PKNORMAL_ROUTINE)pvLoad, BaseAddress, 0, 0)))
			{
				QueueApc(hThread, (PKNORMAL_ROUTINE)pvFree, BaseAddress, 0, (PVOID)MEM_RELEASE);

				return STATUS_SUCCESS;
			}
		}

		ZwFreeVirtualMemory(hProcess, &BaseAddress, &Size, MEM_RELEASE);
	}

	return status;
}

NTSTATUS MyCreateUserThread(
							HANDLE hProcess,
							BOOLEAN CreateSuspended,
							PVOID	EntryPoint,
							const void*	Argument,
							PHANDLE	phThread
							)
{
	DoIoControl(IOCTL_SetProtectedProcess);
	NTSTATUS status = RtlCreateUserThread(hProcess, 0, CreateSuspended, 0, 0, 0, EntryPoint, Argument, phThread, 0);
	DoIoControl(IOCTL_DelProtectedProcess);
	return status;
}

NTSTATUS ApcInjector(PCWSTR lpDllName, PCLIENT_ID cid)
{
	NTSTATUS status;

	HANDLE hProcess, hThread;

	if (0 <= (status = MyOpenProcess(&hProcess, PROCESS_CREATE_THREAD|PROCESS_VM_OPERATION|PROCESS_VM_WRITE|PROCESS_QUERY_INFORMATION, &zoa, cid)))
	{
#ifdef _WIN64
		PVOID wow;
		if (0 <= (status = ZwQueryInformationProcess(hProcess, ProcessWow64Information, &wow, sizeof(wow), 0)))
#endif
		{
			PVOID funcs[3];
			QUEUEAPC QueueApc;

#ifdef _WIN64
			if (wow)
			{
				funcs[0] = ntdll.funcs[1].pv;
				funcs[1] = kernel32.funcs[0].pv;
				funcs[2] = kernel32.funcs[1].pv;

				if (!funcs[0] || !funcs[1] || !funcs[2])
				{
					status = STATUS_PROCEDURE_NOT_FOUND;
					goto __0;
				}

				QueueApc = RtlQueueApcWow64Thread;
			}
			else
#endif
			{
				funcs[0] = RtlExitUserThread;
				funcs[1] = LoadLibraryExW;
				funcs[2] = VirtualFree;
				QueueApc = ZwQueueApcThread;
			}

			if (IsExportSuppressionEnabled(hProcess))
			{
				if (0 > (status = SetExportValid(hProcess, funcs[0])) || 
					0 > (status = SetExportValid(hProcess, funcs[1], funcs[2])))
				{
					goto __0;
				}
			}

			if (0 <= (status = MyCreateUserThread(hProcess, TRUE, funcs[0], 0, &hThread)))
			{
				status = ApcInjector(lpDllName, hProcess, hThread, QueueApc, funcs[1], funcs[2]);

				ResumeThread(hThread);

				NtClose(hThread);
			}
		}
__0:
		NtClose(hProcess);
	}

	return status;
}

NTSTATUS RemoteUnload(HMODULE hmod, PCLIENT_ID cid)
{
	NTSTATUS status;

	HANDLE hProcess;

	if (0 <= (status = MyOpenProcess(&hProcess, PROCESS_VM_OPERATION|PROCESS_CREATE_THREAD|PROCESS_QUERY_INFORMATION, &zoa, cid)))
	{
#ifdef _WIN64
		PVOID wow;
		if (0 <= (status = ZwQueryInformationProcess(hProcess, ProcessWow64Information, &wow, sizeof(wow), 0)))
#endif
		{
			PVOID pfnUnload;

#ifdef _WIN64
			if (wow)
			{
				pfnUnload = ntdll.funcs[2].pv;
			}
			else
#endif
			{
				pfnUnload = LdrUnloadDll;
			}

			if (IsExportSuppressionEnabled(hProcess))
			{
				if (0 > (status = SetExportValid(hProcess, pfnUnload)))
				{
					goto __0;
				}
			}

			status = MyCreateUserThread(hProcess, 0, pfnUnload, hmod, 0);
		}
__0:
		NtClose(hProcess);
	}

	return status;
}

void Add32Modules(PRTL_PROCESS_MODULES mods, PRTL_PROCESS_MODULES32 mods32, ULONG Size)
{
	if (ULONG NumberOfModules = mods32->NumberOfModules)
	{
		if (Size == __builtin_offsetof(RTL_PROCESS_MODULES32, Modules) + NumberOfModules * sizeof(RTL_PROCESS_MODULE_INFORMATION32))
		{
			PRTL_PROCESS_MODULE_INFORMATION Modules = mods->Modules + mods->NumberOfModules;

			mods->NumberOfModules += NumberOfModules;

			PRTL_PROCESS_MODULE_INFORMATION32 Modules32 = mods32->Modules;
			do 
			{
				Modules->Flags = Modules32->Flags;
				Modules->ImageBase = (PVOID)(ULONG_PTR)Modules32->ImageBase;
				Modules->ImageSize = Modules32->ImageSize;
				Modules->InitOrderIndex = Modules32->InitOrderIndex;
				Modules->LoadCount = Modules32->LoadCount;
				Modules->LoadOrderIndex = Modules32->LoadOrderIndex;
				Modules->MappedBase = (PVOID)(ULONG_PTR)Modules32->MappedBase;
				Modules->OffsetToFileName = Modules32->OffsetToFileName;
				Modules->Section = (PVOID)(ULONG_PTR)Modules32->Section;

				strcpy(Modules++->FullPathName, Modules32++->FullPathName);

			} while (--NumberOfModules);
		}
	}
}

NTSTATUS StartQuery(
					_In_ HANDLE hProcess,
					_In_ PVOID RemoteBaseAddress,
					_In_ ULONG Size,
					_In_ BOOLEAN ExportSuppression,
#ifdef _WIN64
					_In_ BOOL wow,
#endif
					_Out_ PHANDLE phThread
					)
{
	PVOID pvLdrQueryProcessModuleInformation;
	PVOID pvRtlExitUserThread;
	NTSTATUS (NTAPI *QueueApcThread)(HANDLE hThread, PKNORMAL_ROUTINE , PVOID , PVOID , PVOID );

#ifdef _WIN64
	if (wow)
	{
		pvLdrQueryProcessModuleInformation = ntdll.funcs[0].pv;
		pvRtlExitUserThread = ntdll.funcs[1].pv;
		QueueApcThread = RtlQueueApcWow64Thread;
	}
	else
#endif
	{
		pvLdrQueryProcessModuleInformation = LdrQueryProcessModuleInformation;
		pvRtlExitUserThread = RtlExitUserThread;
		QueueApcThread = ZwQueueApcThread;
	}

	NTSTATUS status;

	if (ExportSuppression)
	{
		if (0 > (status = SetExportValid(hProcess, pvLdrQueryProcessModuleInformation, pvRtlExitUserThread)))
		{
			return status;
		}
	}

	HANDLE hThread;
	if (0 <= (status = RtlCreateUserThread(hProcess, 0, TRUE, 0, 0, 0, pvRtlExitUserThread, 0, &hThread, 0)))
	{
		if (0 <= (status = QueueApcThread(hThread, 
			(PKNORMAL_ROUTINE)pvLdrQueryProcessModuleInformation, 
			RemoteBaseAddress, 
			(PVOID)(ULONG_PTR)Size, 
			(PBYTE)RemoteBaseAddress + Size)))
		{
			NtSetInformationThread(hThread, ThreadHideFromDebugger, 0, 0);

			if (0 <= (status = ZwResumeThread(hThread, 0)))
			{
				*phThread = hThread;

				return STATUS_SUCCESS;
			}
		}

		ZwTerminateThread(hThread, 0);
		NtClose(hThread);
	}

	return status;
}

void FreePM(_In_ PRTL_PROCESS_MODULES mods)
{
	VirtualFree(mods, 0, MEM_RELEASE);
}

NTSTATUS QueryPM(_In_ HANDLE dwProcessId, _Out_ PRTL_PROCESS_MODULES* pmods)
{
	HANDLE hProcess;

	CLIENT_ID cid = { dwProcessId };
	NTSTATUS status = MyOpenProcess(&hProcess, 
		PROCESS_VM_OPERATION|PROCESS_CREATE_THREAD|PROCESS_QUERY_INFORMATION|PROCESS_SET_INFORMATION, &zoa, &cid);
	
	if (0 <= status)
	{
		PROCESS_EXTENDED_BASIC_INFORMATION pebi = {sizeof(pebi)};

		if (0 <= (status = NtQueryInformationProcess(hProcess, ProcessBasicInformation, &pebi, sizeof(pebi), 0)))
		{
			if (pebi.IsProcessDeleting)
			{
				status = STATUS_PROCESS_IS_TERMINATING;
			}
			else if (pebi.IsFrozen && pebi.IsStronglyNamed)
			{
				status = STATUS_INVALID_DEVICE_STATE;
			}
			else
			{
				enum { secshift = 17, secsize = (1 << secshift) };

#ifdef _WIN64
				if (!ntdll.funcs[0].pv || !ntdll.funcs[1].pv)
#endif
				{
					pebi.IsWow64Process = 0;
				}

				LARGE_INTEGER SectionSize = { (pebi.IsWow64Process  ? 2 : 1) << secshift };

				HANDLE hSection;

				if (0 <= (status = NtCreateSection(&hSection, SECTION_ALL_ACCESS, 0, &SectionSize, PAGE_READWRITE, SEC_COMMIT, 0)))
				{
					struct QueryBuf 
					{
						ULONG NumberOfModules;
						UCHAR buf[secsize - 2 * sizeof(ULONG)];
						ULONG ReturnedSize;
					};

					union {
						PVOID BaseAddress = 0;
						PRTL_PROCESS_MODULES mods;
						QueryBuf* pQb;
					};

					SIZE_T ViewSize = 0;

					if (0 <= (status = ZwMapViewOfSection(hSection, NtCurrentProcess(), 
						&BaseAddress, 0, 0, 0, &ViewSize, ViewUnmap, 0, PAGE_READWRITE)))
					{
						BOOLEAN ExportSuppression = IsExportSuppressionEnabled(hProcess);

						PVOID RemoteBaseAddress = 0;

						if (0 <= (status = ZwMapViewOfSection(hSection, hProcess, &RemoteBaseAddress, 0, 
							0, 0, &(ViewSize = 0), ViewUnmap, 0, PAGE_READWRITE)))
						{
							DoIoControl(IOCTL_SetProtectedProcess);

							HANDLE hThreads[2]{};

							if (0 <= (status = StartQuery(hProcess, RemoteBaseAddress, 
								secsize - sizeof(ULONG), ExportSuppression, 
#ifdef _WIN64
								FALSE, 
#endif
								hThreads)))
							{
								ULONG HandleCount = 1;

#ifdef _WIN64
								if (pebi.IsWow64Process && 0 <= (status = StartQuery(
									hProcess, RtlOffsetToPointer(RemoteBaseAddress, secsize), 
									secsize - sizeof(ULONG), 
									ExportSuppression, TRUE, hThreads + 1)))
								{
									HandleCount = 2;
								}
#endif

								LARGE_INTEGER Timeout = { (ULONG)-10000000, -1 };
								
								status = ZwWaitForMultipleObjects(HandleCount, hThreads, WaitAll, TRUE, &Timeout);

								if (status)
								{
									ULONG i = HandleCount;
									do 
									{
										ZwTerminateThread(hThreads[--i], 0);
									} while (i);
								}

								status = STATUS_UNSUCCESSFUL;
								
								if (ULONG NumberOfModules = mods->NumberOfModules)
								{
									if (pQb->ReturnedSize == __builtin_offsetof(RTL_PROCESS_MODULES, Modules) + NumberOfModules * sizeof(RTL_PROCESS_MODULE_INFORMATION))
									{
#ifdef _WIN64
										if (HandleCount == 2)
										{
											union {
												PRTL_PROCESS_MODULES32 mods32;
												QueryBuf* pQb32;
											};

											pQb32 = pQb + 1;

											Add32Modules(mods, mods32, pQb32->ReturnedSize);
										}
#endif

										*pmods = mods, BaseAddress = 0, status = STATUS_SUCCESS;
									}
								}

								do 
								{
									NtClose(hThreads[--HandleCount]);
								} while (HandleCount);

							}

							DoIoControl(IOCTL_DelProtectedProcess);

							ZwUnmapViewOfSection(hProcess, RemoteBaseAddress);
						}

						if (BaseAddress) ZwUnmapViewOfSection(NtCurrentProcess(), BaseAddress);
					}

					NtClose(hSection);
				}
			}
		}

		NtClose(hProcess);
	}

	return status;
}

NTSTATUS SetLowLevel(HANDLE hToken)
{
	TOKEN_MANDATORY_LABEL tml = { { alloca(GetSidLengthRequired(1)), SE_GROUP_INTEGRITY|SE_GROUP_INTEGRITY_ENABLED } };
	static SID_IDENTIFIER_AUTHORITY LabelAuthority = SECURITY_MANDATORY_LABEL_AUTHORITY;
	InitializeSid(tml.Label.Sid, &LabelAuthority, 1);
	*GetSidSubAuthority(tml.Label.Sid, 0) = SECURITY_MANDATORY_LOW_RID;
	return ZwSetInformationToken(hToken, TokenIntegrityLevel, &tml, sizeof(tml));
}

NTSTATUS GetLastNtStatus(BOOL fOk)
{
	if (fOk) return STATUS_SUCCESS;
	NTSTATUS status = RtlGetLastNtStatus();
	ULONG dwError = GetLastError();
	return RtlNtStatusToDosErrorNoTeb(status) == dwError ? status : 0x80000000|(FACILITY_WIN32 << 16)|dwError;
}

NTSTATUS CreateProcessEx(HANDLE hProcess, 
						 PCWSTR lpApplicationName, 
						 PWSTR lpCommandLine, 
						 DWORD dwCreationFlags, 
						 PCWSTR lpCurrentDirectory,
						 STARTUPINFOW* si, 
						 PROCESS_INFORMATION* pi)
{
	HANDLE hToken;
	NTSTATUS status = NtOpenProcessToken(hProcess, TOKEN_QUERY|TOKEN_DUPLICATE|TOKEN_ASSIGN_PRIMARY, &hToken);

	if (0 <= status)
	{
		status = GetLastNtStatus(CreateProcessAsUserW(hToken, lpApplicationName, 
			lpCommandLine, 0, 0, 0, dwCreationFlags, 0, lpCurrentDirectory, si, pi));

		NtClose(hToken);
	}

	return status;
}

NTSTATUS CreateProcessExLow(HANDLE hProcess, 
						 PCWSTR lpApplicationName, 
						 PWSTR lpCommandLine, 
						 DWORD dwCreationFlags, 
						 PCWSTR lpCurrentDirectory,
						 STARTUPINFOW* si, 
						 PROCESS_INFORMATION* pi)
{
	HANDLE hToken, hNewToken;

	NTSTATUS status = NtOpenProcessToken(hProcess, TOKEN_DUPLICATE, &hToken);

	if (0 <= status)
	{
		status = NtDuplicateToken(hToken, TOKEN_QUERY|TOKEN_DUPLICATE|TOKEN_ASSIGN_PRIMARY|
			TOKEN_ADJUST_DEFAULT, 0, FALSE, TokenPrimary, &hNewToken);

		NtClose(hToken);

		if (0 <= status)
		{
			// need TOKEN_ADJUST_DEFAULT
			if (0 <= (status = SetLowLevel(hNewToken)))
			{
				// need TOKEN_QUERY|TOKEN_DUPLICATE|TOKEN_ASSIGN_PRIMARY
				status = GetLastNtStatus(CreateProcessAsUser(hNewToken, lpApplicationName, 
					lpCommandLine, 0, 0, 0, dwCreationFlags, 
					0, lpCurrentDirectory, si, pi));
			}
			NtClose(hNewToken);
		}
	}

	return status;
}

NTSTATUS CreateProcessEx(PCLIENT_ID cid, 
						 BOOLEAN bLow,
						 PCWSTR lpApplicationName, 
						 PWSTR lpCommandLine, 
						 DWORD dwCreationFlags, 
						 PCWSTR lpCurrentDirectory,
						 STARTUPINFOEXW* si, 
						 PROCESS_INFORMATION* pi)
{
	PVOID stack = alloca(guz);
	SIZE_T size = 0x30;
	ULONG cb = 0;

	do 
	{
		if (cb < size)
		{
			size = cb = RtlPointerToOffset(si->lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)alloca(size - cb), stack);
		}

		if (InitializeProcThreadAttributeList(si->lpAttributeList, 1, 0, &size))
		{
			HANDLE hProcess;

			NTSTATUS status = MyOpenProcess(&hProcess, READ_CONTROL|PROCESS_CREATE_PROCESS|PROCESS_QUERY_LIMITED_INFORMATION, &zoa, cid);

			if (0 <= status)
			{
				// hProcess need PROCESS_CREATE_PROCESS
				if (UpdateProcThreadAttribute(si->lpAttributeList, 0, 
					PROC_THREAD_ATTRIBUTE_PARENT_PROCESS, &hProcess, sizeof(hProcess), 0, 0))
				{
					status = bLow ? 
						CreateProcessExLow(
						hProcess, 
						lpApplicationName, 
						lpCommandLine, 
						dwCreationFlags, 
						lpCurrentDirectory,
						&si->StartupInfo, 
						pi) : 
					CreateProcessEx(
						hProcess, 
						lpApplicationName, 
						lpCommandLine, 
						dwCreationFlags, 
						lpCurrentDirectory,
						&si->StartupInfo, 
						pi);
				}
				else
				{
					status = STATUS_UNSUCCESSFUL;
				}

				NtClose(hProcess);
			}

			DeleteProcThreadAttributeList(si->lpAttributeList);

			return status;
		}

	} while (GetLastError() == ERROR_INSUFFICIENT_BUFFER);

	return STATUS_UNSUCCESSFUL;
}

_NT_END