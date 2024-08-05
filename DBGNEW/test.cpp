#include "StdAfx.h"

_NT_BEGIN

#include "common.h"
#include "Dll.h"
#include "../inc/rtf.h"

PRUNTIME_FUNCTION find(DWORD Va, PRUNTIME_FUNCTION firstRT, DWORD N)
{
	DWORD a = 0, o;
	do 
	{
		PRUNTIME_FUNCTION pRT = &firstRT[o = (a + N) >> 1];

		if (Va < pRT->BeginAddress)
		{
			N = o;
		}
		else if (pRT->EndAddress <= Va)
		{
			a = o + 1;
		}
		else
		{
			return pRT;
		}

	} while (a < N);

	return 0;
}

BOOL UnwindTest(HMODULE ImageBase, PRUNTIME_FUNCTION firstRT, DWORD N, DWORD RipRva)
{
	DWORD64 ib;
	PRUNTIME_FUNCTION pRT = RtlLookupFunctionEntry((ULONG_PTR)ImageBase+RipRva, &ib, 0);
	DbgPrint("[%p, %p)\n", ib+pRT->BeginAddress, ib+pRT->EndAddress);
	
	if (firstRT)
	{
		pRT = find(RipRva, firstRT, N);
		DbgPrint("[%p, %p)\n", ib+pRT->BeginAddress, ib+pRT->EndAddress);
	}

	if (!pRT) 
	{
		DbgBreak();
		return TRUE;
	}

	DWORD UnwindData = pRT->UnwindData;
	
	if (UnwindData & 1)
	{
		PRUNTIME_FUNCTION pRT = (PRUNTIME_FUNCTION)RtlOffsetToPointer(ImageBase, UnwindData-1);
		DbgPrint("----[%p, %p)\n", ib+pRT->BeginAddress, ib+pRT->EndAddress);
		
		UnwindData = pRT->UnwindData;
	}

	PUNWIND_INFO pui = (PUNWIND_INFO)RtlOffsetToPointer(ImageBase, UnwindData);

	BOOL RipInChain = TRUE;

	static PCSTR szRegs[] = {
		"rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
		"r8" , "r9" , "r10", "r11", "r12", "r13", "r14", "r15",
	};

	for (;;) 
	{
		BYTE CountOfCodes = pui->CountOfCodes, Version = pui->Version, FrameRegister = pui->FrameRegister;
		PUNWIND_CODE UnwindCode = pui->UnwindCode;

		if (FrameRegister)
		{
		}
		else
		{
			FrameRegister = 4;
		}

		DWORD BeginAddress = pRT->BeginAddress;

		while (CountOfCodes--)
		{
			BYTE i;
			DWORD cb;
			BOOL doUnwind = !RipInChain || (BeginAddress + UnwindCode->CodeOffset <= RipRva);

			//DbgPrint("0x%p>\t", ImageBase + BeginAddress + UnwindCode->CodeOffset);

			switch(UnwindCode->UnwindOp)
			{
			case UWOP_PUSH_NONVOL:

				if (doUnwind) 
				{
					//gr.grV[UnwindCode->OpInfo] = *Rsp++;
				}
				DbgPrint("pop %s\t\t\t\t;%u\n", szRegs[UnwindCode->OpInfo], doUnwind);

				break;

			case UWOP_ALLOC_LARGE:

				if (!CountOfCodes--) return TRUE;

				if (UnwindCode++->OpInfo)
				{
					if (!CountOfCodes--) return TRUE;
					cb = *(DWORD*)UnwindCode;
					UnwindCode++;
				}
				else
				{
					cb = *(WORD*)UnwindCode << 3;
				}

				if (doUnwind) 
				{
					//Rsp = (DWORD64*)RtlOffsetToPointer(Rsp, cb);
				}
				DbgPrint("add rsp,%x\t\t\t\t;%u\n", cb, doUnwind);
				break;

			case UWOP_ALLOC_SMALL:
				if (doUnwind) 
				{
					//Rsp += UnwindCode->OpInfo + 1;
				}
				DbgPrint("add rsp,%x\t\t\t\t;%u\n", (UnwindCode->OpInfo + 1) << 3, doUnwind);
				break;

			case UWOP_SET_FPREG:
				if (doUnwind) 
				{
					//Rsp = Frame - (pui->FrameOffset << 1);
				}
				DbgPrint("lea rsp,[%s - %x]\t\t\t\t\n", szRegs[FrameRegister], pui->FrameOffset << 4, doUnwind);
				break;

			case UWOP_SAVE_NONVOL:
				if (!CountOfCodes--) return TRUE;
				i = UnwindCode->OpInfo;
				cb = (++UnwindCode)->FrameOffset;
				if (doUnwind)
				{
					//gr.grV[i] = Rsp[cb];
				}
				DbgPrint("mov %s,[rsp + %x]\t\t\t\t;%u\n", szRegs[i], cb << 3, doUnwind);
				break;

			case UWOP_SAVE_NONVOL_FAR:
				if (!CountOfCodes-- || !CountOfCodes--) return TRUE;
				i = UnwindCode->OpInfo;
				cb = *(DWORD*)(UnwindCode + 1);
				if (doUnwind) 
				{
					//gr.grV[i] = *(DWORD64*)RtlOffsetToPointer(Rsp, cb);
				}
				DbgPrint("mov %s,[rsp + %x]\t\t\t\t;%u\n", szRegs[i], cb, doUnwind);
				UnwindCode += 2;
				break;

			case UWOP_SAVE_XMM:
				if (!CountOfCodes--) return TRUE;
				if (Version > 1)
				{
					++UnwindCode;
					DbgPrint("SAVE_XMM ??? %x\n", UnwindCode->FrameOffset);
				}
				else
				{
					i = UnwindCode->OpInfo;
					cb = (++UnwindCode)->FrameOffset;
					if (doUnwind) 
					{
						//XMM(i, Rsp + cb);
					}
					DbgPrint("mov xmm%u,[rsp + %x]\t\t\t\t;%u\n", i, cb << 3, doUnwind);
				}
				break;

			case UWOP_SAVE_XMM128:
				if (!CountOfCodes--) return TRUE;
				i = UnwindCode->OpInfo;
				cb = (++UnwindCode)->FrameOffset << 1;
				if (doUnwind) 
				{
					//XMM128(i, Rsp + cb);
				}
				DbgPrint("mov xmm%u,[rsp + %x]\t\t\t\t;%u\n", i, cb << 3, doUnwind);
				break;

			case UWOP_SAVE_XMM_FAR:
				if (!CountOfCodes-- || !CountOfCodes--) return TRUE;
				i = UnwindCode->OpInfo;
				cb = *(DWORD*)(UnwindCode + 1);
				UnwindCode += 2;
				if (doUnwind) 
				{
					//XMM(i, RtlOffsetToPointer(Rsp, cb));
				}
				DbgPrint("mov xmm%u,[rsp + %x]\t\t\t\t;%u\n", i, cb, doUnwind);
				break;

			case UWOP_SAVE_XMM128_FAR:
				if (!CountOfCodes-- || !CountOfCodes--) return TRUE;
				i = UnwindCode->OpInfo;
				cb = *(DWORD*)(UnwindCode + 1);
				UnwindCode += 2;
				if (doUnwind) 
				{
					//XMM128(i, RtlOffsetToPointer(Rsp, cb));
				}
				DbgPrint("mov xmm%u,[rsp + %x]\t\t\t\t;%u\n", i, cb, doUnwind);
				break;

			case UWOP_PUSH_MACHFRAME:
				DbgBreak();
				return TRUE;
			default:
				DbgBreak();
				return TRUE;
			}
			UnwindCode++;
		}

		if (!(pui->Flags & UNW_FLAG_CHAININFO))
		{
			break;
		}

		pRT = GetChainedFunctionEntry(pui);
		DbgPrint("+++[%p, %p)\n", ib+pRT->BeginAddress, ib+pRT->EndAddress);
		pui = (PUNWIND_INFO)RtlOffsetToPointer(ImageBase, pRT->UnwindData);
		RipInChain = FALSE;
	}

	return FALSE;
}

void RtfTest2(HMODULE hmod, DWORD Size, DWORD Va)
{
	PRUNTIME_FUNCTION firstRT = (PRUNTIME_FUNCTION)RtlOffsetToPointer(hmod, Va), pRT = firstRT, pRT1, pRT2;
	DWORD N = Size / sizeof(RUNTIME_FUNCTION), k = N;
	DWORD Address = 0, a = MAXDWORD, b = 0, UnwindData, v;

	do 
	{
		if (pRT->EndAddress <= pRT->BeginAddress || pRT->BeginAddress < Address) DbgBreak();

		UnwindData = pRT->UnwindData;

		if (UnwindData & 1)
		{
			v = UnwindData - 1 - Va;
			if (Size <= v || (v % sizeof(RUNTIME_FUNCTION))) DbgBreak();

		}
		else
		{
			if (UnwindData < a) a = UnwindData; 
			if (b < UnwindData) b = UnwindData;

			PUNWIND_INFO pui = (PUNWIND_INFO)RtlOffsetToPointer(hmod, UnwindData);

			if (pui->FrameRegister)
			{
				DbgPrint("%x[%x,%x) %x:%x\n", pui->Flags,pRT->BeginAddress, pRT->EndAddress, pui->FrameRegister, pui->FrameOffset);
			}

			if ((pui->Flags & ~(UNW_FLAG_EHANDLER|UNW_FLAG_UHANDLER|UNW_FLAG_CHAININFO))) DbgBreak();

			if (pui->Flags & UNW_FLAG_CHAININFO)
			{
				pRT1 = GetChainedFunctionEntry(pui);
				pRT2 = find(pRT1->BeginAddress, firstRT, N);

				if (!pRT2 || memcmp(pRT1, pRT2, sizeof(RUNTIME_FUNCTION)))
				{
					DbgBreak();
				}
			}
		}

		Address = pRT++->EndAddress;

	} while (--k);
}

void RtfTest1()
{
	HMODULE hmod = GetModuleHandle(L"ntdll");
	PIMAGE_NT_HEADERS pinth = RtlImageNtHeader(hmod);

	DWORD NumberOfSections = pinth->FileHeader.NumberOfSections;
	PIMAGE_SECTION_HEADER pish = IMAGE_FIRST_SECTION(pinth);
	do 
	{
		if (!strcmp((PCSTR)pish->Name, ".pdata"))
		{
			ULONG Size = pish->Misc.VirtualSize;

			if (!Size || (Size % sizeof(RUNTIME_FUNCTION))) DbgBreak() ;

			DWORD VirtualAddress = pish->VirtualAddress;

			UnwindTest((HMODULE)&__ImageBase, 0, 0, 66+RtlPointerToOffset(&__ImageBase,RtfTest1));

			static DWORD ofs[] = {
				0x19580,
				0x50440,
				0x5096f,
				0x99d11,
				0xb7e20,
				0xd4960
			};
			DWORD i = RTL_NUMBER_OF(ofs);
			PRUNTIME_FUNCTION prtf = (PRUNTIME_FUNCTION)RtlOffsetToPointer(hmod, VirtualAddress);
			DWORD N = Size / sizeof(RUNTIME_FUNCTION);
			do 
			{
				UnwindTest(hmod, prtf, N, ofs[--i]);
			} while (i);

			RtfTest2(hmod, Size, VirtualAddress);
		}
	} while (pish++, --NumberOfSections);
}

#include "OATI.h"

void typetest()
{
	if (DWORD NumberOfTypes = g_AOTI)
	{
		STATIC_UNICODE_STRING_(PcwObject);
		if (OBJECT_TYPE_INFORMATION* poti = g_AOTI[&PcwObject])
		{
			DWORD ObjectTypeIndex = g_AOTI[poti];

			NTSTATUS status;
			PVOID buf = 0, stack = alloca(guz);
			DWORD cb = 0, rcb = 0x10000;

			do 
			{
				if (cb < rcb) cb = RtlPointerToOffset(buf = alloca(rcb - cb), stack);

				if (0 <= (status = ZwQuerySystemInformation(SystemExtendedHanfleInformation, buf, cb, &rcb)))
				{
					PSYSTEM_HANDLE_INFORMATION_EX pshti = (PSYSTEM_HANDLE_INFORMATION_EX)buf;

					if (ULONG NumberOfHandles = (ULONG)pshti->NumberOfHandles)
					{
						PSYSTEM_HANDLE_TABLE_ENTRY_INFO_EX Handles = pshti->Handles;

						do 
						{
							if (Handles->ObjectTypeIndex == ObjectTypeIndex)
							{
								DbgPrint("%p %p %p\n", Handles->UniqueProcessId, Handles->HandleValue, Handles->Object);
							}

						} while (Handles++, --NumberOfHandles);
					}
				}

			} while (STATUS_INFO_LENGTH_MISMATCH == status);

		}
	}
}

BOOL LoadDrv();

NTSTATUS CreateThreadStack(HANDLE hProcess, SIZE_T StackReserve, SIZE_T StackCommit, USER_STACK& us)
{
	NTSTATUS status = ZwAllocateVirtualMemory(hProcess, &us.ExpandableStackBottom, 0, &StackReserve, MEM_RESERVE, PAGE_EXECUTE_READWRITE);

	if (0 <= status)
	{
		us.ExpandableStackBase = (PBYTE)us.ExpandableStackBottom + StackReserve;

		DWORD o;

		if (
			0 > (status = ZwAllocateVirtualMemory(hProcess, &(us.ExpandableStackLimit = (PBYTE)us.ExpandableStackBase - StackCommit), 0, &StackCommit, MEM_COMMIT, PAGE_EXECUTE_READWRITE))
			||
			0 > (status = ZwProtectVirtualMemory(hProcess, &us.ExpandableStackLimit, &(StackCommit = PAGE_SIZE), PAGE_EXECUTE_READWRITE|PAGE_GUARD, &o))
			)
		{
			ZwFreeVirtualMemory(hProcess, &us.ExpandableStackBottom, &StackReserve, MEM_RELEASE);
		}
	}
	
	return status;
}

_NT_END