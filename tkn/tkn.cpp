// tkn.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"

_NT_BEGIN

#include "tkn.h"

#ifdef _AMD64_
#include "../inc/amd64plat.h"
#endif

#ifdef _X86_
#include "../inc/x86plat.h"
#endif

PDEVICE_OBJECT g_ControlDevice;
USHORT g_OsVersion;

#ifdef _WIN64

BOOL IsAddressValid(ULONG_PTR p)
{
	_PTE* pte = PXE_X64_L(p);

	if (pte->Valid)
	{
		pte = PPE_X64_L(p);

		if (pte->Valid)
		{
			pte = PDE_X64_L(p);

			if (pte->Valid)
			{
				if (pte->LargePage)
				{
					return TRUE;
				}

				pte = PTE_X64_L(p);

				return pte->Valid || pte->Prototype || (pte->Protection && pte->Protection < MM_NOCACHE);
			}
		}
	}

	return FALSE;
}

#define LA(i, j, k, l) (0xFFFF000000000000 + \
	((ULONG_PTR)(i) << PXI_SHIFT) + \
	((ULONG_PTR)(j) << PPI_SHIFT) + \
	((ULONG_PTR)(k) << PDI_SHIFT) + \
	((ULONG_PTR)(l) << PTI_SHIFT))

BOOL findFirstNotValidVA(ULONG_PTR p, int d, ULONG_PTR& q)
{
	DWORD i = (p >> PXI_SHIFT) & PXI_MASK;
	DWORD j = (p >> PPI_SHIFT) & PPI_MASK;
	DWORD k = (p >> PDI_SHIFT) & PDI_MASK_AMD64;
	DWORD l = (p >> PTI_SHIFT) & PTI_MASK_AMD64;

	ULONG_PTR X = 0, Y = 0;

	BOOL bValid = FALSE;

	_PTE* pte = PXE(i);

	do 
	{
		if (!pte->Valid)
		{
			goto leave;
		}

		_PTE* pte = PPE(i, j &= PPI_MASK);

		do 
		{
			if (!pte->Valid)
			{
				goto leave;
			}

			_PTE* pte = PDE(i, j, k &= PDI_MASK_AMD64);

			do 
			{
				Y = LA(i, j, k, 0);

				if (!pte->Valid)
				{
					goto leave;
				}

				if (pte->LargePage)
				{
					X = Y;
					bValid = TRUE;
					continue;
				}

				_PTE* pte = PTE(i, j, k, l &= PTI_MASK_AMD64);

				do 
				{
					Y = LA(i, j, k, l);

					if (pte->Valid)
					{
						X = Y;
						bValid = TRUE;
					}
					else
					{
						if (pte->Prototype || (pte->Protection && pte->Protection < MM_NOCACHE))
						{
							X = Y;
							bValid = TRUE;
						}
						else
						{
							goto leave;
						}
					}

				} while (pte += d, (l += d) < PTE_PER_PAGE);

			} while (pte += d, (k += d) < PDE_PER_PAGE);

		} while (pte += d, (j += d) < PPE_PER_PAGE);

	} while (pte += d, (i += d) < PXE_PER_PAGE);

leave:

	if (bValid)
	{
		q = d < 0 ? X : Y;
		return TRUE;
	}

	return FALSE;
}

#else

ULONG PX_SELFMAP;

#define PPI_SHIFT 30

#define LA_PAE(i, j, k) (((ULONG_PTR)(i) << PPI_SHIFT) + ((ULONG_PTR)(j) << PDI_SHIFT_X86PAE) + ((ULONG_PTR)(k) << PTI_SHIFT))

BOOL findFirstNotValidVAPAE(ULONG_PTR p, int d, ULONG_PTR& q)
{
	DWORD i = (p >> PPI_SHIFT);
	DWORD j = (p >> PDI_SHIFT_X86PAE) & 0x1ff;
	DWORD k = (p >> PTI_SHIFT) & 0x1ff;

	ULONG_PTR X = 0, Y = 0;

	BOOL bValid = FALSE;

	_PTE_PAE* pte = PPE_PAE(i);

	do 
	{
		if (!pte->Valid)
		{
			goto leave;
		}

		_PTE_PAE* pte = PDE_PAE(i, j &= 0x1ff);

		do 
		{
			Y = LA_PAE(i, j, 0);

			if (!pte->Valid)
			{
				goto leave;
			}

			if (pte->LargePage)
			{
				X = Y;
				bValid = TRUE;
				continue;
			}

			_PTE_PAE* pte = PTE_PAE(i, j, k &= 0x1ff);

			do 
			{
				Y = LA_PAE(i, j, k);

				if (pte->Valid)
				{
					X = Y;
					bValid = TRUE;
				}
				else
				{
					if (pte->Prototype || (pte->Protection && pte->Protection < MM_NOCACHE))
					{
						X = Y;
						bValid = TRUE;
					}
					else
					{
						goto leave;
					}
				}

			} while (pte += d, (k += d) < 0x200);

		} while (pte += d, (j += d) < 0x200);

	} while (pte += d, (i += d) < 4);

leave:

	if (bValid)
	{
		q = d < 0 ? X : Y;
		return TRUE;
	}

	return FALSE;
}

#define LA_X86(i, j) (((ULONG_PTR)(i) << PDI_SHIFT_X86) + ((ULONG_PTR)(j) << PTI_SHIFT))

BOOL findFirstNotValidVAX86(ULONG_PTR p, int d, ULONG_PTR& q)
{
	DWORD i = (p >> PDI_SHIFT_X86);
	DWORD j = (p >> PTI_SHIFT) & 0x3ff;

	ULONG_PTR X = 0, Y = 0;

	BOOL bValid = FALSE;

	_PTE_X86* pte = PDE_X86(i);

	do 
	{
		Y = LA_X86(i, 0);

		if (!pte->Valid)
		{
			goto leave;
		}

		if (pte->LargePage)
		{
			X = Y;
			bValid = TRUE;
			continue;
		}

		_PTE_X86* pte = PTE_X86(i, j &= 0x3ff);

		do 
		{
			Y = LA_X86(i, j);

			if (pte->Valid)
			{
				X = Y;
				bValid = TRUE;
			}
			else
			{
				if (pte->Prototype || (pte->Protection && pte->Protection < MM_NOCACHE))
				{
					X = Y;
					bValid = TRUE;
				}
				else
				{
					goto leave;
				}
			}

		} while (pte += d, (j += d) < 0x400);

	} while (pte += d, (i += d) < 0x400);

leave:

	if (bValid)
	{
		q = d < 0 ? X : Y;
		return TRUE;
	}

	return FALSE;
}

BOOL IsAddressValidPAE(ULONG_PTR p)
{
	_PTE_PAE* pte = PPE_PAE_L(p);

	if (pte->Valid)
	{
		pte = PDE_PAE_L(p);

		if (pte->Valid)
		{
			if (pte->LargePage)
			{
				return TRUE;
			}

			pte = PTE_PAE_L(p);

			return pte->Valid || pte->Prototype || (pte->Protection && pte->Protection < MM_NOCACHE);
		}
	}

	return FALSE;
}

BOOL IsAddressValidX86(ULONG_PTR p)
{
	_PTE_X86* pte = PDE_X86_L(p);

	if (pte->Valid)
	{
		if (pte->LargePage)
		{
			return TRUE;
		}

		pte = PTE_X86_L(p);

		return pte->Valid || pte->Prototype || (pte->Protection && pte->Protection < MM_NOCACHE);
	}

	return FALSE;
}

BOOL (* pIsAddressValid)(ULONG_PTR p);
BOOL (* pfindFirstNotValidVA)(ULONG_PTR p, int d, ULONG_PTR& q);

BOOL IsAddressValid(ULONG_PTR p)
{
	return pIsAddressValid(p);
}

BOOL findFirstNotValidVA(ULONG_PTR p, int d, ULONG_PTR& q)
{
	return pfindFirstNotValidVA(p, d, q);
}

#endif

NTSTATUS Read(PVOID InputBuffer, PVOID Buffer, ULONG Length, PULONG pcbCopyed)
{
	if (!PX_SELFMAP)
	{
		return STATUS_NOT_FOUND;
	}

	DWORD cbCopyed = 0;
	NTSTATUS status = STATUS_SUCCESS;

	if (Length) __try
	{
		ProbeForRead(InputBuffer, sizeof(PVOID), __alignof(PVOID));

		ULONG_PTR Address = *(PULONG_PTR)InputBuffer;
		
		DWORD cb = (DWORD)(((Address + PAGE_SIZE) & ~(PAGE_SIZE - 1)) - Address);

		do 
		{
			if (Length < cb) cb = Length;

			if (!IsAddressValid(Address))
			{
				status = STATUS_ACCESS_VIOLATION;
				break;
			}

			memcpy(Buffer, (PVOID)Address, cb);

			Buffer = RtlOffsetToPointer(Buffer, cb);
			Address += cb;
			cbCopyed += cb;
			Length -= cb;
			cb = PAGE_SIZE;

		} while (Length);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		status = GetExceptionCode();
	}
	
	*pcbCopyed = cbCopyed;

	return status;
}

void DriverUnload(PDRIVER_OBJECT )
{
	IoDeleteDevice(g_ControlDevice);
}

NTSTATUS OnCloseCleanup(PDEVICE_OBJECT , PIRP Irp)
{
	Irp->IoStatus.Status = STATUS_SUCCESS;
	IofCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

NTSTATUS OnCreate(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	PFILE_OBJECT FileObject = IoGetCurrentIrpStackLocation(Irp)->FileObject;
	if (FileObject->FileName.Length || FileObject->RelatedFileObject)
	{
		Irp->IoStatus.Status = STATUS_OBJECT_NAME_INVALID;
		IofCompleteRequest(Irp, IO_NO_INCREMENT);
		return STATUS_OBJECT_NAME_INVALID;
	}
	return OnCloseCleanup(DeviceObject, Irp);
}

typedef NTSTATUS (*PsLookupXByXId)(HANDLE , void** );

NTSTATUS LookupById(
					void** ppvObject,
					PsLookupXByXId fn,
					PVOID InputBuffer ,
					ULONG InputBufferLength,
					ULONG OutputBufferLength
					)
{
	if (!OutputBufferLength && InputBufferLength == sizeof(HANDLE))
	{
		HANDLE Id;

		__try
		{
			ProbeForRead(InputBuffer, sizeof(HANDLE), __alignof(HANDLE));
			Id = *(HANDLE*)InputBuffer;
		}
		__except(EXCEPTION_EXECUTE_HANDLER)
		{
			return GetExceptionCode();
		}

		PVOID pvObject;
		NTSTATUS status = fn(Id, &pvObject);
		
		if (0 <= status)
		{
			*ppvObject = pvObject;
			ObfDereferenceObject(pvObject);
		}

		return status;
	}

	return STATUS_INVALID_PARAMETER;
}

NTSTATUS KQueryMemory(PULONG_PTR pp, PULONG_PTR pq)
{
	if (!PX_SELFMAP)
	{
		return STATUS_NOT_FOUND;
	}
	__try
	{
		ProbeForRead(pp, sizeof(ULONG_PTR), __alignof(ULONG_PTR));
		ULONG_PTR p = *pp, a, b;
		if (findFirstNotValidVA(p, -1, a) && findFirstNotValidVA(p, +1, b))
		{
			ProbeForWrite(pq, 2*sizeof(ULONG_PTR), __alignof(ULONG_PTR));
			pq[0] = a, pq[1] = b;
			return STATUS_SUCCESS;
		}

		return STATUS_MEMORY_NOT_ALLOCATED;
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		return GetExceptionCode();
	}
}

#define KERNEL_HANDLE_MASK ((ULONG_PTR)((LONG)0x80000000))

NTSTATUS KQueryHandles(FQH* pfqh, PVOID OutputBuffer, ULONG OutputBufferLength)
{
	NTSTATUS status;

	__try
	{
		ProbeForRead(pfqh, sizeof(HANDLE), __alignof(HANDLE));
		ULONG_PTR ProcessId = pfqh->ProcessId;
		UCHAR ThreadIndex = pfqh->ThreadIndex, ProcessIndex = pfqh->ProcessIndex, FileIndex = pfqh->FileIndex;

		if (g_OsVersion >= _WIN32_WINNT_WS03)
		{
			FileIndex = MAXUCHAR;
		}

		PEPROCESS Process;
		if (0 <= (status = PsLookupProcessByProcessId((HANDLE)ProcessId, &Process)))
		{
			status = STATUS_INSUFFICIENT_RESOURCES;

			if (PMDL Mdl = IoAllocateMdl(OutputBuffer, OutputBufferLength, FALSE, FALSE, 0))
			{
				MmProbeAndLockPages(Mdl, KernelMode, IoModifyAccess);
				if (PSYSTEM_HANDLE_INFORMATION_EX pshti = (PSYSTEM_HANDLE_INFORMATION_EX)MmGetSystemAddressForMdlSafe(Mdl, LowPagePriority))
				{
					DWORD rcb;
					if (0 <= (status = ZwQuerySystemInformation(SystemExtendedHandleInformation, OutputBuffer, OutputBufferLength, &rcb)))
					{
						if (ULONG_PTR NumberOfHandles = pshti->NumberOfHandles)
						{
							PSYSTEM_HANDLE_TABLE_ENTRY_INFO_EX Handles = pshti->Handles, p = Handles;
							DWORD n = 0;
							do 
							{
								if (Handles->UniqueProcessId == ProcessId)
								{
									Handles->CreatorBackTraceIndex = 0;
									Handles->Reserved = 0;
									*p++ = *Handles;
									n++;
								}
							} while (Handles++, --NumberOfHandles);

							if ((pshti->NumberOfHandles = n) && (OutputBufferLength -= RtlPointerToOffset(pshti, p)))
							{
								POBJECT_NAME_INFORMATION poni = (POBJECT_NAME_INFORMATION)p;
								PWSTR sz = (PWSTR)p;

								Handles = pshti->Handles;
								BOOL bKernel = Process == PsInitialSystemProcess;
								//if (bKernel) DbgBreak();
								KAPC_STATE as;
								if (!bKernel) KeStackAttachProcess(Process, &as);
								do 
								{
									POBJECT_TYPE ObjectType = 0;
									USHORT ObjectTypeIndex = Handles->ObjectTypeIndex;
									int c = 0;
									if (ObjectTypeIndex == ThreadIndex)
									{
										ObjectType = *PsThreadType, c = 1;
									}
									else if (ObjectTypeIndex == ProcessIndex)
									{
										ObjectType = *PsProcessType, c = 2;
									}
									else if (ObjectTypeIndex == FileIndex)
									{
										ObjectType = *IoFileObjectType, c = 3;
									}

									PVOID Object = 0;
									PUNICODE_STRING FileName;
									Handles->Reserved = RtlPointerToOffset(pshti, sz);
									PEPROCESS ThreadProcess;
									ULONG_PTR HandleValue = Handles->HandleValue;
									if (bKernel)
									{
										HandleValue |= KERNEL_HANDLE_MASK;
									}
									if (0 <= ObReferenceObjectByHandle((HANDLE)HandleValue, 0, ObjectType, 0, &Object, 0))
									{
										if (Object == Handles->Object) switch (c)
										{
										case 1:
											ThreadProcess = PsGetThreadProcess((PETHREAD)Object);
											rcb = swprintf(sz, L"%x.%x %p %S", 
												(DWORD)(ULONG_PTR)PsGetProcessId(ThreadProcess), 
												(DWORD)(ULONG_PTR)PsGetThreadId((PKTHREAD)Object), 
												PsGetThreadTeb((PETHREAD)Object), 
												PsGetProcessImageFileName(ThreadProcess)) << 1;
											goto __common;
										case 2:
											rcb = swprintf(sz, L"%x %S", (DWORD)(ULONG_PTR)PsGetProcessId((PEPROCESS)Object), PsGetProcessImageFileName((PEPROCESS)Object)) << 1;
__common:
											Handles->CreatorBackTraceIndex = (USHORT)rcb;
											sz = (PWSTR)RtlOffsetToPointer(sz, rcb);
											OutputBufferLength -= (rcb + (sizeof(PVOID) - 1))&~(sizeof(PVOID)-1);
											poni = (POBJECT_NAME_INFORMATION)(((ULONG_PTR)sz + (sizeof(PVOID) - 1))&~(sizeof(PVOID) - 1));
											break;
										case 3:// only for xp (for not hung in ObQueryNameString)
											FileName = &((PFILE_OBJECT)Object)->FileName;
											if (0 <= ObQueryNameString(((PFILE_OBJECT)Object)->DeviceObject, poni, OutputBufferLength, &rcb))
											{
												Handles->CreatorBackTraceIndex = (USHORT)(rcb = poni->Name.Length);
												memcpy(sz, poni->Name.Buffer, rcb);
												sz = (PWSTR)RtlOffsetToPointer(sz, rcb);
												OutputBufferLength -= (rcb + (sizeof(PVOID) - 1))&~(sizeof(PVOID)-1);
												poni = (POBJECT_NAME_INFORMATION)(((ULONG_PTR)sz + (sizeof(PVOID) - 1))&~(sizeof(PVOID) - 1));
											}

											if ((rcb = FileName->Length) <= OutputBufferLength)
											{
												Handles->CreatorBackTraceIndex += (USHORT)rcb;
												memcpy(sz, FileName->Buffer, rcb);
												sz = (PWSTR)RtlOffsetToPointer(sz, rcb);
												OutputBufferLength -= (rcb + (sizeof(PVOID) - 1))&~(sizeof(PVOID)-1);
												poni = (POBJECT_NAME_INFORMATION)(((ULONG_PTR)sz + (sizeof(PVOID) - 1))&~(sizeof(PVOID) - 1));
											}
											break;

										default:
											if (0 <= ObQueryNameString(Object, poni, OutputBufferLength, &rcb))
											{
												Handles->CreatorBackTraceIndex = (USHORT)(rcb = poni->Name.Length);
												memcpy(sz, poni->Name.Buffer, rcb);
												sz = (PWSTR)RtlOffsetToPointer(sz, rcb);
												OutputBufferLength -= (rcb + (sizeof(PVOID) - 1))&~(sizeof(PVOID)-1);
												poni = (POBJECT_NAME_INFORMATION)(((ULONG_PTR)sz + (sizeof(PVOID) - 1))&~(sizeof(PVOID) - 1));
											}
										}
										ObfDereferenceObject(Object);
									}
									
									Handles->Object = Object;

								} while (Handles++, --n && 1024 < OutputBufferLength);

								if (!bKernel) KeUnstackDetachProcess(&as);
							}
						}
					}
				}
				MmUnlockPages(Mdl);
				IoFreeMdl(Mdl);
			}

			ObfDereferenceObject(Process);
		}
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		return GetExceptionCode();
	}

	return status;
}

OBJECT_ATTRIBUTES zoa = { sizeof zoa };

NTSTATUS OpenProcessById(PHANDLE phProcess, PHANDLE pUniqueProcess)
{
	__try
	{
		ProbeForRead(pUniqueProcess, sizeof(HANDLE), __alignof(HANDLE));
		CLIENT_ID cid = { *pUniqueProcess };
		return ZwOpenProcess(phProcess, MAXIMUM_ALLOWED, &zoa, &cid);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		return GetExceptionCode();
	}	
}

NTSTATUS OpenTreadByCID(PHANDLE phThread, PCLIENT_ID pcid)
{
	__try
	{
		ProbeForRead(pcid, sizeof(CLIENT_ID), __alignof(CLIENT_ID));
		CLIENT_ID cid = *pcid;
		return ZwOpenThread(phThread, MAXIMUM_ALLOWED, &zoa, &cid);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		return GetExceptionCode();
	}	
}

ULONG g_protOffset, g_protMask, g_protMax;

NTSTATUS SetProtectedProcess(BOOL bSet)
{
	if (g_protMask)
	{
		PLONG pl = (PLONG)RtlOffsetToPointer(IoGetCurrentProcess(), g_protOffset);
		LONG oldValue, newValue;
		do 
		{
			oldValue = *pl;
			newValue = bSet ? ((oldValue | g_protMask) & g_protMax)  : (oldValue & ~g_protMask);
		} while (InterlockedCompareExchange(pl, newValue, oldValue) != oldValue);

		return STATUS_SUCCESS;
	}

	return STATUS_UNSUCCESSFUL;
}

BOOLEAN FastIoDeviceControl (
							 IN PFILE_OBJECT,
							 IN BOOLEAN ,
							 IN PVOID InputBuffer ,
							 IN ULONG InputBufferLength,
							 OUT PVOID OutputBuffer,
							 IN ULONG OutputBufferLength,
							 IN ULONG IoControlCode,
							 OUT PIO_STATUS_BLOCK IoStatus,
							 IN PDEVICE_OBJECT
							 )
{
	ULONG_PTR Information = 0;
	NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;

	switch (IoControlCode)
	{
	case IOCTL_LookupProcessByProcessId:
		status = LookupById((void**)&Information, (PsLookupXByXId)PsLookupProcessByProcessId, InputBuffer, InputBufferLength, OutputBufferLength);
		break;
	case IOCTL_LookupThreadByThreadId:
		status = LookupById((void**)&Information, (PsLookupXByXId)PsLookupThreadByThreadId, InputBuffer, InputBufferLength, OutputBufferLength);
		break;
	case IOCTL_ReadMemory:
		status = InputBufferLength == sizeof(PVOID) ? Read(InputBuffer, OutputBuffer, OutputBufferLength, (PULONG)&Information) : STATUS_INVALID_PARAMETER;
		break;
	case IOCTL_QueryMemory:
		status = InputBufferLength == sizeof(ULONG_PTR) && OutputBufferLength == 2*sizeof(ULONG_PTR) ? KQueryMemory((PULONG_PTR)InputBuffer, (PULONG_PTR)OutputBuffer) : STATUS_INVALID_PARAMETER;
		break;
	case IOCTL_QueryHandles:
		status = InputBufferLength == sizeof(FQH) && OutputBufferLength >= 0x100000 ? KQueryHandles((FQH*)InputBuffer, OutputBuffer, OutputBufferLength) : STATUS_INFO_LENGTH_MISMATCH;
		break;
	case IOCTL_OpenProcess:
		status = InputBufferLength == sizeof(PVOID) ? OpenProcessById((PHANDLE)&Information, (PHANDLE)InputBuffer) : STATUS_INVALID_PARAMETER;
		break;
	case IOCTL_OpenThread:
		status = InputBufferLength == sizeof(CLIENT_ID) ? OpenTreadByCID((PHANDLE)&Information, (PCLIENT_ID)InputBuffer) : STATUS_INVALID_PARAMETER;
		break;
	case IOCTL_SetProtectedProcess:
		status = SetProtectedProcess(TRUE);
		Information = g_protOffset;
		break;
	case IOCTL_DelProtectedProcess:
		status = SetProtectedProcess(FALSE);
		Information = g_protOffset;
		break;
	}

	IoStatus->Status = status;
	IoStatus->Information = Information;

	return TRUE;
}

extern "C"
{
	PSTR __fastcall strnstr(SIZE_T n1, const void* str1, SIZE_T n2, const void* str2);
}

#ifndef _WIN64

PVOID GetKernelBase()
{
	PVOID ImageBase = 0;
	NTSTATUS status;
	DWORD cb = 0x10000;
	do 
	{
		status = STATUS_INSUFFICIENT_RESOURCES;

		if (PRTL_PROCESS_MODULES buffer = (PRTL_PROCESS_MODULES)ExAllocatePool(PagedPool, cb))
		{
			if (0 <= (status = ZwQuerySystemInformation(SystemModuleInformation, buffer, cb, &cb)))
			{
				if (buffer->NumberOfModules)
				{
					ImageBase = buffer->Modules->ImageBase;
				}
			}

			ExFreePool(buffer);
		}

	} while (status == STATUS_INFO_LENGTH_MISMATCH);

	return ImageBase;
}
#endif

void FindProtectedBits(PVOID Process, BOOLEAN (CALLBACK * PsIsProtectedProcess)(PVOID Process))
{
	RtlZeroMemory(Process, PAGE_SIZE);
	PLONG pu = (PLONG)Process, qu, ru = 0;
	ULONG n = PAGE_SIZE >> 2, i;

	__try 
	{
		do 
		{
			*(qu = pu++) = ~0;

			if (PsIsProtectedProcess(Process))
			{
				if (ru)
				{
					return ;
				}
				ru = qu;
			}

			*qu = 0;

		} while (--n);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		return;
	}

	if (!ru)
	{
		return ;
	}

	i = 31;
	LONG mask = 0;
	do 
	{
		_bittestandset(ru, i);

		if (PsIsProtectedProcess(Process))
		{
			mask |= *ru;
			n++;
		}

		_bittestandreset(ru, i);

	} while (--i);

	g_protOffset = RtlPointerToOffset(Process, ru);

	switch (n)
	{
	case 1:
		g_protMask = mask;
		break;
	case 3:
		switch (mask)
		{
		case 0x07000000:
			g_protMask = 0xff000000;
			break;
		case 0x00070000:
			g_protMask = 0x00ff0000;
			break;
		case 0x00000700:
			g_protMask = 0x0000ff00;
			break;
		case 0x00000007:
			g_protMask = 0x000000ff;
			break;
		}
		break;
	}

	g_protMax = (*(PULONG)RtlOffsetToPointer(PsInitialSystemProcess, g_protOffset) & g_protMask) | ~g_protMask;
}

void FindProtectedBits()
{
	STATIC_UNICODE_STRING_(PsIsProtectedProcess);

	union {
		PVOID pv;
		BOOLEAN (CALLBACK * fn)(PVOID);
	};
	
	if (pv = MmGetSystemRoutineAddress(const_cast<PUNICODE_STRING>(&PsIsProtectedProcess)))
	{
		if (PVOID Process = ExAllocatePool(PagedPool, PAGE_SIZE))
		{
			FindProtectedBits(Process, fn);

			ExFreePool(Process);
		}
	}
}

#ifdef _WIN64

ULONGLONG PTE_BASE_X64, PDE_BASE_X64, PPE_BASE_X64, PXE_BASE_X64, PX_SELFMAP;

#define CR3_MASK 0x000FFFFFFFFFF000

BOOL FoundSelfMapIndex()
{
	ULONGLONG cr3 = __readcr3() & CR3_MASK;
	
	//DbgPrint("cr3=%I64x\n", cr3);

	int n = 0x100;
	
	PX_SELFMAP = PX_SELFMAP_MIN;

	ULONGLONG _PX_SELFMAP = 0;
	do 
	{
		_PTE* pte = PXE(PX_SELFMAP);
		
		if (MmIsAddressValid(pte))
		{
			//DbgPrint("%03x %p %I64x\n", PX_SELFMAP, pte, pte->Value);

			if ((pte->Value & CR3_MASK) == cr3)
			{
				if (_PX_SELFMAP)
				{
					PX_SELFMAP = 0;
					return FALSE;
				}
				else
				{
					_PX_SELFMAP = PX_SELFMAP;
				}
			}
		}

	} while (++PX_SELFMAP, --n);

	if (_PX_SELFMAP)
	{
		INIT_PTE_CONSTS(_PX_SELFMAP);

		//DbgPrint("%I64x\n%I64x\n%I64x\n%I64x\n%I64x\n", PX_SELFMAP,PTE_BASE_X64,PDE_BASE_X64,PPE_BASE_X64,PXE_BASE_X64);

		return TRUE;
	}

	return FALSE;
}
#else

ULONG PX_SELFMAP_X86, PTE_BASE_X86, PDE_BASE_X86;

#define CR3_MASK 0xFFFFF000

#if 0
BOOL FoundSelfMapIndexPAE()
{
	ULONG cr3 = __readcr3();// & CR3_MASK;

	DbgPrint("cr3=%08x\n", cr3);

	int n = 2;

	PX_SELFMAP_PAE = 2;

	ULONG _PX_SELFMAP = 0;
	do 
	{
		_PTE_PAE* pte = PPE_PAE(PX_SELFMAP_PAE);

		if (MmIsAddressValid(pte))
		{
			DbgPrint("%x %p %08x\n", PX_SELFMAP_PAE, pte, pte->Value);

			if ((pte->Value & CR3_MASK) == cr3)
			{
				if (_PX_SELFMAP)
				{
					PX_SELFMAP_PAE = 0;
					return FALSE;
				}
				else
				{
					_PX_SELFMAP = PX_SELFMAP_PAE;
				}
			}
		}

	} while (++PX_SELFMAP_PAE, --n);

	if (_PX_SELFMAP)
	{
		INIT_PTE_CONSTS_PAE(_PX_SELFMAP);

		DbgPrint("%x\n%x\n%x\n%x\n", PX_SELFMAP_PAE,PTE_BASE_PAE,PDE_BASE_PAE,PPE_BASE_PAE);

		PX_SELFMAP = PX_SELFMAP_PAE;

		return TRUE;
	}

	return FALSE;
}
#endif

BOOL FoundSelfMapIndexX86()
{
	ULONG cr3 = __readcr3() & CR3_MASK;

	//DbgPrint("cr3=%08x\n", __readcr3());

	int n = 0x200;

	PX_SELFMAP_X86 = 0x200;

	ULONG _PX_SELFMAP = 0;
	do 
	{
		_PTE_X86* pte = PDE_X86(PX_SELFMAP_X86);

		if (MmIsAddressValid(pte))
		{
			//DbgPrint("%x %p %08x\n", PX_SELFMAP_X86, pte, pte->Value);

			if ((pte->Value & CR3_MASK) == cr3)
			{
				if (_PX_SELFMAP)
				{
					PX_SELFMAP_X86 = 0;
					return FALSE;
				}
				else
				{
					_PX_SELFMAP = PX_SELFMAP_X86;
				}
			}
		}

	} while (++PX_SELFMAP_X86, --n);

	if (_PX_SELFMAP)
	{
		INIT_PTE_CONSTS_X86(_PX_SELFMAP);

		//DbgPrint("%x\n%x\n%x\n", PX_SELFMAP_X86,PTE_BASE_X86,PDE_BASE_X86);

		PX_SELFMAP = PX_SELFMAP_X86;

		return TRUE;
	}

	return FALSE;
}

#endif

EXTERN_C NTSTATUS NTAPI DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING )
{
	ULONG major, minor;
	PsGetVersion(&major, &minor, 0, 0);
	g_OsVersion = (USHORT)((major << 8) + minor);

#ifdef _WIN64
	FoundSelfMapIndex();
#else
	if (ExIsProcessorFeaturePresent(PF_PAE_ENABLED))
	{
		pIsAddressValid = IsAddressValidPAE;
		pfindFirstNotValidVA = findFirstNotValidVAPAE;
		PX_SELFMAP = PX_SELFMAP_PAE;
	}
	else
	{
		pIsAddressValid = IsAddressValidX86;
		pfindFirstNotValidVA = findFirstNotValidVAX86;
		FoundSelfMapIndexX86();
	}	

	if (g_OsVersion <= _WIN32_WINNT_WINXP)
	{
		if (PULONG pv = (PULONG)GetKernelBase())
		{
			if (PIMAGE_NT_HEADERS pinth = RtlImageNtHeader(pv))
			{
				if (pinth->FileHeader.NumberOfSections)
				{
					PIMAGE_SECTION_HEADER pish = IMAGE_FIRST_SECTION(pinth);
					if (major = pish->Misc.VirtualSize)
					{					
						static DWORD mask = 0x3E0DD7;
						if (pv = (PULONG)strnstr(major, RtlOffsetToPointer(pv, pish->VirtualAddress), sizeof(mask), &mask))
						{
							pv--;

							//DbgPrint("** %p\n", pv);

							if (PMDL mdl = IoAllocateMdl(pv, sizeof(DWORD), FALSE, FALSE, 0))
							{
								MmBuildMdlForNonPagedPool(mdl);
								CSHORT MdlFlags = mdl->MdlFlags;
								mdl->MdlFlags |= MDL_PAGES_LOCKED;
								mdl->MdlFlags &= ~MDL_SOURCE_IS_NONPAGED_POOL;
								if (pv = (PULONG)MmGetSystemAddressForMdlSafe(mdl, LowPagePriority))
								{
									*pv = 0x3F0DD7;
									MmUnmapLockedPages(pv, mdl);
								}
								mdl->MdlFlags = MdlFlags;

								IoFreeMdl(mdl);
							}
						}
					}
				}
			}
		}
	}
#endif

	FindProtectedBits();

	//DbgPrint("ProtectedBits(%08x %08x %08x)\n", g_protOffset, g_protMask, g_protMax);

	DriverObject->DriverUnload = DriverUnload;

	STATIC_UNICODE_STRING(DeviceName, "\\device\\69766781178D422cA183775611A8EE55");

	static const FAST_IO_DISPATCH s_fiod = { 
		sizeof FAST_IO_DISPATCH, 0, 0, 0, 0, 0, 0, 0, 0, 0, FastIoDeviceControl 
	};

	DriverObject->FastIoDispatch = (PFAST_IO_DISPATCH)&s_fiod;
	DriverObject->MajorFunction[IRP_MJ_CREATE] = OnCreate;
	DriverObject->MajorFunction[IRP_MJ_CLEANUP] = OnCloseCleanup;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = OnCloseCleanup;

	NTSTATUS status = IoCreateDevice(DriverObject, 0,
		const_cast<PUNICODE_STRING>(&DeviceName), FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE, 
		&g_ControlDevice);

	if (0 <= status)
	{
		// "D:P(A;;GA;;;SY)(A;;GA;;;BA)S:(ML;;NWNRNX;;;HI)"
		static const ULONG sd[] = {
			0x90140001, 0x00000000, 0x00000000, 0x00000014, 
			0x00000030, 0x001C0002, 0x00000001, 0x00140011, 
			0x00000007, 0x00000101, 0x10000000, 0x00003000, 
			0x00340002, 0x00000002, 0x00140000, 0x10000000, 
			0x00000101, 0x05000000, 0x00000012, 0x00180000, 
			0x10000000, 0x00000201, 0x05000000, 0x00000020, 
			0x00000220,
		};

		if (0 > (status = ObSetSecurityObjectByPointer(g_ControlDevice, DACL_SECURITY_INFORMATION|LABEL_SECURITY_INFORMATION, 
			const_cast<ULONG (*)[25]>(&sd))))
		{
			IoDeleteDevice(g_ControlDevice);
		}
		else
		{
			g_ControlDevice->Flags &= ~DO_DEVICE_INITIALIZING;
		}
	}

	return status;
}

_NT_END