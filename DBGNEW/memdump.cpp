#include "stdafx.h"

_NT_BEGIN

#include "DbgDoc.h"

#include "../inc/dump.h"
#include "dump_private.h"

C_ASSERT(FIELD_OFFSET(DUMP_HEADER64,Attributes)==0x1050);
C_ASSERT(FIELD_OFFSET(MEMORY_DUMP64,Bitmap)==0x2000);
C_ASSERT(FIELD_OFFSET(MEMORY_DUMP64,Bitmap.Bits)==0x2038);

C_ASSERT(FIELD_OFFSET(DUMP_HEADER32,ContextRecord)==0x320);

C_ASSERT(sizeof(DUMP_HEADER32)==0x1000);
C_ASSERT(sizeof(DUMP_HEADER64)==0x2000);

C_ASSERT(sizeof(BLOBDUMP) == 0x10);
C_ASSERT(sizeof(TAGBLOBHEADER) == 0x20);

ULONG NumberOfSetBitsUlong0(ULONG v)
{
	return __popcnt(v);
}

ULONG NumberOfSetBitsUlong1(ULONG v)
{
	v -= (v >> 1) & 0x55555555;
	v = (v & 0x33333333) + ((v >> 2) & 0x33333333);
	return ((((v >> 4) + v) & 0x0f0f0f0f) * 0x01010101) >> 24;
}

ULONG BitSetCount0(PULONG Bits, PULONG Nums, ULONG SizeOfBitMap)
{
	ULONG n = 0;

	if (SizeOfBitMap >>= 5)
	{
		do 
		{
			n += __popcnt(*Bits++);
			*Nums++ = n;
		} while (--SizeOfBitMap);
	}

	return n;
}

ULONG BitSetCount1(PULONG Bits, PULONG Nums, ULONG SizeOfBitMap)
{
	ULONG n = 0;

	if (SizeOfBitMap >>= 5)
	{
		do 
		{
			n += NumberOfSetBitsUlong1(*Bits++);
			*Nums++ = n;
		} while (--SizeOfBitMap);
	}

	return n;
}

ULONG (* BitSetCount)(PULONG Bits, PULONG Nums, ULONG SizeOfBitMap);
ULONG (* NumberOfSetBitsUlong)(ULONG v);

struct BitCountInit 
{
	BitCountInit()
	{
		int v[4];
		__cpuid(v, 1);

		if (_bittest((PLONG)v + 2, 23))
		{
			BitSetCount = BitSetCount0;
			NumberOfSetBitsUlong = NumberOfSetBitsUlong0;
		}
		else
		{
			BitSetCount = BitSetCount1;
			NumberOfSetBitsUlong = NumberOfSetBitsUlong1;
		}
	}
};

static BitCountInit _;

__forceinline BOOL IsKernelAddress32(LONG Addr)
{
	return Addr < 0;
}

__forceinline BOOL IsKernelPointerAddress32(LONG Addr)
{
	return Addr < 0 && !(Addr & 3);
}

//////////////////////////////////////////////////////////////////////////
//

void vlprintf(HWND hwnd, PCWSTR format, va_list args)
{
	WCHAR sz[1024];
	_vsnwprintf(sz, RTL_NUMBER_OF(sz) - 1, format, args);
	sz[RTL_NUMBER_OF(sz) - 1] = 0;
	SendMessage(hwnd, EM_SETSEL, MAXLONG, MAXLONG);
	SendMessage(hwnd, EM_REPLACESEL, 0, (LPARAM)sz);	
}	

void lprintf(HWND hwnd, PCWSTR format, ...)
{ 
	return vlprintf(hwnd, format, (va_list)(&format + 1)); 
}

//////////////////////////////////////////////////////////////////////////
// ZFileCache

ZFileCache::~ZFileCache()
{
	if (_Buffer)
	{
		SIZE_T RegionSize = 0;
		ZwFreeVirtualMemory(NtCurrentProcess(), &_Buffer, &RegionSize, MEM_RELEASE);
	}

	if (_hFile)
	{
		NtClose(_hFile);
	}
}

ZFileCache::ZFileCache()
{
	_Buffer = 0;
	_hFile = 0;
	_index = 0;
	__stosd(_iPages, MAXULONG, RTL_NUMBER_OF(_iPages));
}

NTSTATUS ZFileCache::Create()
{
	SIZE_T RegionSize = pages_count * PAGE_SIZE;

	return ZwAllocateVirtualMemory(NtCurrentProcess(), &_Buffer, 0, &RegionSize, MEM_COMMIT, PAGE_READWRITE);
}

NTSTATUS ZFileCache::ReadPage(ULONG iPage, PAGE* ppPage)
{
	if (MAXULONG == iPage)
	{
		return STATUS_ACCESS_VIOLATION;
	}

	PAGE Page;

	if (PULONG piPage = findDWORD(pages_count, _iPages, iPage))
	{
		Page = _Pages + (piPage - _iPages);
	}
	else
	{
		UCHAR index = _index++;

		IO_STATUS_BLOCK iosb;
		LARGE_INTEGER BytesOffset;
		BytesOffset.QuadPart = _PagesOffset + ((ULONG64)iPage << PAGE_SHIFT);
		NTSTATUS status = ZwReadFile(_hFile, 0, 0, 0, &iosb, Page = _Pages + index, PAGE_SIZE, &BytesOffset, 0);
		if (0 > status)
		{
			_iPages[index] = MAXULONG;

			return status;
		}
		_iPages[index] = iPage;
	}

	*ppPage = Page;

	return STATUS_SUCCESS;
}

NTSTATUS ZFileCache::ReadData(PLARGE_INTEGER offset, PVOID buf, ULONG cb, PULONG pcb)
{
	if (!cb) return STATUS_SUCCESS;

	IO_STATUS_BLOCK iosb;
	NTSTATUS status = ZwReadFile(_hFile, 0, 0, 0, &iosb, buf, cb, offset, 0);

	if (pcb)
	{
		*pcb = (ULONG)iosb.Information;
	}

	return status;
}

//////////////////////////////////////////////////////////////////////////
//

#ifdef _AMD64_

__forceinline BOOL IsKernelAddress(ULONG64 Addr)
{
	return Addr >= 0xFFFF800000000000ULL;
}

__forceinline BOOL IsKernelPointerAddress(ULONG64 Addr)
{
	return Addr >= 0xFFFF800000000000ULL && !(Addr & 7);
}

NTSTATUS ZMemoryDump64::VirtualToPhysical(ULONG_PTR Addr, PULONG pPfn)
{
	union {
		PAGE Page;
		_PTE* ppte;
	};

	ULONG Pfn = _DirectoryTablePfn;

	ULONG shift = 39;
	int i = 4;
	NTSTATUS status;

	do 
	{
		if (0 > (status = ReadPhysicalPage(Pfn, &Page)))
		{
			return status;
		}

		_PTE pte = ppte[(Addr >> shift) & 0x1ff];

		if (!pte.Valid)
		{
			return STATUS_ACCESS_VIOLATION;
		}

		Pfn = pte.PageFrameNumber;

		if (pte.LargePage)
		{
			Pfn += (Addr & ((1 << shift) - 1)) >> PAGE_SHIFT;
			break;
		}

	} while (shift -= 9, --i);

	*pPfn = Pfn;

	return STATUS_SUCCESS;
}

void ZMemoryDump64::_DumpContext(HWND hwndLog, CONTEXT* ctx)
{
	lprintf(hwndLog, 
		L"rax=%p rbx=%p\r\n"
		L"rcx=%p rdx=%p\r\n"
		L"rsi=%p rdi=%p\r\n"
		L" r8=%p  r9=%p\r\n"
		L"r10=%p r11=%p\r\n"
		L"r12=%p r13=%p\r\n"
		L"r14=%p r15=%p\r\n"
		L"rbp=%p rsp=%p\r\n"
		L"rip=%p efl=%08x\r\n"
		L"cs=%02x ss=%02x gs=%02x\r\n",
		ctx->Rax, ctx->Rbx, ctx->Rcx, ctx->Rdx, ctx->Rsi, ctx->Rdi,
		ctx->R8, ctx->R9, ctx->R10, ctx->R11, ctx->R12, ctx->R13, ctx->R14, ctx->R15,
		ctx->Rbp, ctx->Rsp, ctx->Rip, ctx->EFlags, ctx->SegCs, ctx->SegSs, ctx->SegGs
		);
}

union _PTE_PAE
{
	ULONGLONG Value;
	union
	{
		struct 
		{
			ULONG Valid : 01;//00
			ULONG Write : 01;//01
			ULONG Owner : 01;//02
			ULONG WriteThrough : 01;//03
			ULONG CacheDisable : 01;//04
			ULONG Accessed : 01;//05
			ULONG Dirty : 01;//06
			ULONG LargePage : 01;//07
			ULONG Global : 01;//08
			ULONG CopyOnWrite : 01;//09
			ULONG Prototype : 01;//10
			ULONG reserved0 : 01;//11
		};
		struct  
		{
			ULONGLONG Flags : 12;
			ULONGLONG PageFrameNumber : 26;//12
			ULONGLONG reserved1 : 26;//38
		};
	};
	struct  
	{
		struct
		{
			/*0000*/ULONG Valid : 01;//00
			/*0000*/ULONG PageFileLow : 04;//01
			/*0000*/ULONG Protection : 05;//05
			/*0000*/ULONG Prototype : 01;//10
			/*0000*/ULONG Transition : 01;//11
			/*0000*/ULONG Unused : 20;//12
		};
		ULONG PageFileHigh;
	};
};
#define PDI_SHIFT_X86    22
#define PDI_SHIFT_X86PAE 21

const ULONG PX_SELFMAP_PAE = 3;
const ULONG PTE_BASE_PAE = PX_SELFMAP_PAE << 30;
const ULONG PDE_BASE_PAE = PTE_BASE_PAE + (PX_SELFMAP_PAE << 21);
const ULONG PPE_BASE_PAE = PDE_BASE_PAE + (PX_SELFMAP_PAE << 12);

#define PTE_PAE(i, j, k) ((_PTE_PAE*)((PX_SELFMAP_PAE << 30) + ((ULONG)(i) << 21) + ((ULONG)(j) << 12) + ((ULONG)(k) << 3) ))
#define PDE_PAE(j, k) PTE_PAE(PX_SELFMAP_PAE, j, k)
#define PPE_PAE(k) PTE_PAE(PX_SELFMAP_PAE, PX_SELFMAP_PAE, k)

#define PTE_PAE_L(V) (&((_PTE_PAE*)PTE_BASE_PAE)[(DWORD)(V) >> 12])
#define PDE_PAE_L(V) (&((_PTE_PAE*)PDE_BASE_PAE)[(DWORD)(V) >> 21])
#define PPE_PAE_L(V) (&((_PTE_PAE*)PPE_BASE_PAE)[(DWORD)(V) >> 30])

union _PTE_X86
{
	ULONG Value;
	struct
	{
		ULONG Valid : 01;//00
		ULONG Write : 01;//01
		ULONG Owner : 01;//02
		ULONG WriteThrough : 01;//03
		ULONG CacheDisable : 01;//04
		ULONG Accessed : 01;//05
		ULONG Dirty : 01;//06
		ULONG LargePage : 01;//07
		ULONG Global : 01;//08
		ULONG CopyOnWrite : 01;//09
		ULONG Prototype : 01;//10
		ULONG reserved : 01;//11
		ULONG PageFrameNumber : 20;//12
	};
	struct
	{
		/*0000*/ULONG Valid : 01;//00
		/*0000*/ULONG PageFileLow : 04;//01
		/*0000*/ULONG Protection : 05;//05
		/*0000*/ULONG Prototype : 01;//10
		/*0000*/ULONG Transition : 01;//11
		/*0000*/ULONG PageFileHigh : 20;//12
	};
};

extern ULONG PX_SELFMAP_X86, PTE_BASE_X86, PDE_BASE_X86;

#define INIT_PTE_CONSTS_X86(i) PX_SELFMAP_X86 = i;\
	PTE_BASE_X86 = PX_SELFMAP_X86 << 22;\
	PDE_BASE_X86 = PTE_BASE_X86 + (PX_SELFMAP_X86 << 12);

#define PTE_X86(i, j) ((_PTE_X86*)((PX_SELFMAP_X86 << 22) + ((ULONG)(i) << 12) + ((ULONG)(j) << 2) ))
#define PDE_X86(j) PTE_X86(PX_SELFMAP_X86, j)

#define PTE_X86_L(V) (&((_PTE_X86*)PTE_BASE_X86)[(DWORD)(V) >> 12])
#define PDE_X86_L(V) (&((_PTE_X86*)PDE_BASE_X86)[(DWORD)(V) >> 22])

#elif defined _X86_

#define WOW64_CONTEXT CONTEXT
#define IsKernelAddress IsKernelAddress32
#define IsKernelPointerAddress IsKernelPointerAddress32

#else
#error !_AMD64_ && !_X86_
#endif// _AMD64_ || _X86_

//////////////////////////////////////////////////////////////////////////
// ZMemoryDump32

NTSTATUS ZMemoryDump32::VirtualToPhysical(ULONG_PTR Addr, PULONG pPfn)
{
	union {
		PAGE Page;
		_PTE_PAE* ppte2;
		_PTE_X86* ppte1;
	};

	ULONG Pfn = _DirectoryTablePfn;

	ULONG shift, i, oPfn;
	NTSTATUS status;

	if (_PaeEnabled)
	{
		shift = 30, i = 3, oPfn = _oPfn;
		do 
		{
			if (0 > (status = ReadPhysicalPage(Pfn, &Page)))
			{
				return status;
			}

			_PTE_PAE pte = ppte2[oPfn + ((Addr >> shift) & 0x1ff)];

			if (!pte.Valid)
			{
				return STATUS_ACCESS_VIOLATION;
			}

			Pfn = pte.PageFrameNumber;

			if (pte.LargePage)
			{
				Pfn += (Addr & ((1 << shift) - 1)) >> PAGE_SHIFT;
				break;
			}

		} while (oPfn = 0, shift -= 9, --i);
	}
	else
	{
		shift = 22, i = 2;
		do 
		{
			if (0 > (status = ReadPhysicalPage(Pfn, &Page)))
			{
				return status;
			}

			_PTE_X86 pte = ppte1[(Addr >> shift) & 0x3ff];

			if (!pte.Valid)
			{
				return STATUS_ACCESS_VIOLATION;
			}

			Pfn = pte.PageFrameNumber;

			if (pte.LargePage)
			{
				Pfn += (Addr & ((1 << shift) - 1)) >> PAGE_SHIFT;
				break;
			}

		} while (shift -= 10, --i);
	}

	*pPfn = Pfn;

	return STATUS_SUCCESS;
}

void ZMemoryDump32::_DumpContext(HWND hwndLog, CONTEXT* ctx)
{
	lprintf(hwndLog, 
		L"eax=%08x ebx=%08x\r\n"
		L"ecx=%08x edx=%08x\r\n"
		L"esi=%08x edi=%08x\r\n"
		L"ebp=%08x esp=%08x\r\n"
		L"eip=%08x efl=%08x\r\n"
		L"cs=%02x ss=%02x fs=%02x\r\n",
#ifdef _WIN64
		(ULONG)ctx->Rax, (ULONG)ctx->Rbx, (ULONG)ctx->Rcx, (ULONG)ctx->Rdx, 
		(ULONG)ctx->Rsi, (ULONG)ctx->Rdi, (ULONG)ctx->Rbp, (ULONG)ctx->Rsp, 
		(ULONG)ctx->Rip, ctx->EFlags, ctx->SegCs, ctx->SegSs, ctx->SegFs
#else
		ctx->Eax, ctx->Ebx, ctx->Ecx, ctx->Edx, 
		ctx->Esi, ctx->Edi, ctx->Ebp, ctx->Esp, 
		ctx->Eip, ctx->EFlags, ctx->SegCs, ctx->SegSs, ctx->SegFs
#endif
		);
}

//////////////////////////////////////////////////////////////////////////
// ZMemoryDump

void IMemoryDump::_DumpExceptionRecord(HWND hwndLog, PEXCEPTION_RECORD Exception)
{
	ULONG NumberParameters = Exception->NumberParameters;

	lprintf(hwndLog, L"\r\nException %x at %x (%x, %x) %x [", 
		Exception->ExceptionCode, Exception->ExceptionAddress, 
		Exception->ExceptionFlags, Exception->ExceptionRecord, NumberParameters);

	if (NumberParameters)
	{
		if (NumberParameters > EXCEPTION_MAXIMUM_PARAMETERS) NumberParameters = EXCEPTION_MAXIMUM_PARAMETERS;
		
		PULONG_PTR ExceptionInformation = Exception->ExceptionInformation;
		do 
		{
			lprintf(hwndLog, L"\t%x\r\n", *ExceptionInformation++);
		} while (--NumberParameters);
	}
}

void ZMemoryDump::EnumTags(HWND hwndLog)
{
	if ((ULONG64)_DumpBlobSize.QuadPart > 0x1000000) //? >16Mb
	{
		lprintf(hwndLog, L"too big DumpBlobSize=%I64x\r\n", _DumpBlobSize.QuadPart);
		return ;
	}

	ULONG DumpBlobSize = _DumpBlobSize.LowPart;

	union {
		ULONG cb;
		ULONG64 cb64;
	};

	if (DumpBlobSize < sizeof(BLOBDUMP))
	{
		if (DumpBlobSize)
		{
			lprintf(hwndLog, L"invalid DumpBlobSize=%x\r\n", DumpBlobSize);
			return ;
		}

		lprintf(hwndLog, L"no blobs in dump\r\n");
		return ;
	}

	if (PBYTE buf = new BYTE[DumpBlobSize])
	{
		lprintf(hwndLog, L"*************************************************\r\n++ Begin Dump Blob\r\n");
		union {
			PBYTE pb;
			BLOBDUMP* pbd;
			TAGBLOBHEADER* pbh;
		};

		LARGE_INTEGER Offset = _DumpBlobOffset;

		if (0 > ReadData(&Offset, buf, DumpBlobSize, &cb) || cb != DumpBlobSize)
		{
			lprintf(hwndLog, L"fail read dump blob\r\n");
		}
		else
		{
			pb = buf, cb = pbd->cbHeader;

			if (cb < sizeof(BLOBDUMP) || pbd->Dump != 'pmuD' || pbd->Blob != 'bolB')
			{
				lprintf(hwndLog, L"invalid DumpBlob header\r\n");
			}
			else if (pb += cb, Offset.QuadPart += cb, DumpBlobSize -= cb)
			{
				do 
				{
					if (DumpBlobSize < sizeof(TAGBLOBHEADER) ||
						(cb64 = pbh->cbHeader) < sizeof(TAGBLOBHEADER) ||
						(cb64 += pbh->cbData + pbh->cbData1 + pbh->cbData2) > DumpBlobSize)
					{
						//lprintf(hwndLog, L"dump blob corrupted !!\r\n");
						break;
					}

					PBYTE data = pbh->tag.Data4;
					lprintf(hwndLog, L"{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x} size=%x ofs=%I64x (%x, %x)\r\n", 
						pbh->tag.Data1, pbh->tag.Data2, pbh->tag.Data3,
						data[0], data[1], data[2], data[3],
						data[4], data[5], data[6], data[7],
						pbh->cbData, Offset.QuadPart + pbh->cbHeader, pbh->cbData1, pbh->cbData2
						);

				} while (pb += cb, Offset.QuadPart += cb, DumpBlobSize -= cb);
			}
		}

		lprintf(hwndLog, L"*************************************************\r\n-- End Dump Blob\r\n");
		delete [] buf;
	}
}

//////////////////////////////////////////////////////////////////////////
// ZMemoryDump32

#ifdef _WIN64

void CopyWowContext(CONTEXT* ctx, WOW64_CONTEXT* wow)
{
	ctx->ContextFlags = wow->ContextFlags;

	ctx->Rax = wow->Eax;
	ctx->Rbx = wow->Ebx;
	ctx->Rcx = wow->Ecx;
	ctx->Rdx = wow->Edx;
	ctx->Rdi = wow->Edi;
	ctx->Rsi = wow->Esi;
	ctx->Rbp = wow->Ebp;
	ctx->Rsp = wow->Esp;
	ctx->Rip = wow->Eip;
	
	ctx->EFlags = wow->EFlags;

	ctx->Dr0 = wow->Dr0;
	ctx->Dr1 = wow->Dr1;
	ctx->Dr2 = wow->Dr2;
	ctx->Dr3 = wow->Dr3;
	ctx->Dr6 = wow->Dr6;
	ctx->Dr7 = wow->Dr7;

	ctx->SegSs = (USHORT)wow->SegSs;
	ctx->SegCs = (USHORT)wow->SegCs;
	ctx->SegDs = (USHORT)wow->SegDs;
	ctx->SegEs = (USHORT)wow->SegEs;
	ctx->SegFs = (USHORT)wow->SegFs;
	ctx->SegGs = (USHORT)wow->SegGs;

	memcpy(ctx->VectorRegister, &wow->FloatSave, sizeof(wow->FloatSave));
	memcpy(&ctx->FltSave, wow->ExtendedRegisters, sizeof(wow->ExtendedRegisters));
}

void CopyWowException(EXCEPTION_RECORD* exr, EXCEPTION_RECORD32* wow)
{
	ULONG NumberParameters;

	exr->ExceptionCode = wow->ExceptionCode;
	exr->ExceptionFlags = wow->ExceptionFlags;
	exr->NumberParameters = NumberParameters = wow->NumberParameters;
	exr->ExceptionAddress = (PVOID)(ULONG_PTR)wow->ExceptionAddress;
	exr->ExceptionRecord = (EXCEPTION_RECORD*)(ULONG_PTR)wow->ExceptionRecord;

	if (NumberParameters)
	{
		if (NumberParameters > EXCEPTION_MAXIMUM_PARAMETERS)
		{
			NumberParameters = EXCEPTION_MAXIMUM_PARAMETERS;
		}

		PULONG_PTR ExceptionInformation = exr->ExceptionInformation;
		PULONG wowExceptionInformation = wow->ExceptionInformation;

		do 
		{
			*ExceptionInformation++ = *wowExceptionInformation++;
		} while (--NumberParameters);
	}
}
#endif

void ZMemoryDump32::UpdateContext()
{
	union {
		ULONG_PTR up;
		PVOID pCtx;
	};

	pCtx = 0;

	switch (_BugCheckCode)
	{
	case SYSTEM_THREAD_EXCEPTION_NOT_HANDLED:
	case SYSTEM_THREAD_EXCEPTION_NOT_HANDLED_M:
		up = _BugCheckParameter[3];
		break;
	case NTFS_FILE_SYSTEM:
	case FAT_FILE_SYSTEM:
	case CDFS_FILE_SYSTEM:
		up = _BugCheckParameter[2];
		break;//KTRAP_FRAME;
	}

	WOW64_CONTEXT ctx;

	if (pCtx)
	{
		if (0 <= ReadVirtual(pCtx, &ctx, sizeof(ctx)))
		{
#ifdef _WIN64
			CopyWowContext(this, &ctx);
#else
			memcpy(static_cast<CONTEXT*>(this), &ctx, sizeof(ctx));
#endif
		}
	}

}
void ZMemoryDump32::Init(HANDLE hFile, DUMP_HEADER32& md)
{
	_hFile = hFile;

	_Is64Bit = FALSE;

	SetDirectoryTableBase(md.DirectoryTableBase);

#ifdef _WIN64
	CopyWowContext(this, (WOW64_CONTEXT*)md.ContextRecord);
	CopyWowException(this, &md.Exception);

	PULONG_PTR BugCheckParameter = _BugCheckParameter;
	PULONG mdBugCheckParameter = md.BugCheckParameter;
	int i = RTL_NUMBER_OF(md.BugCheckParameter);
	do 
	{
		*BugCheckParameter++ = *mdBugCheckParameter++;
	} while (--i);
#else
	memcpy(static_cast<CONTEXT*>(this), md.ContextRecord, sizeof(CONTEXT));
	memcpy(static_cast<EXCEPTION_RECORD*>(this), &md.Exception, sizeof(EXCEPTION_RECORD));
	memcpy(_BugCheckParameter, md.BugCheckParameter, sizeof(md.BugCheckParameter));
#endif

	_PsLoadedModuleList = md.PsLoadedModuleList;
	_PsActiveProcessHead = md.PsActiveProcessHead;
	_KdDebuggerDataBlock = md.KdDebuggerDataBlock;

	_SystemTime = md.SystemTime;
	_SystemUpTime = md.SystemUpTime;

	_BugCheckCode = md.BugCheckCode;

	_dwBuildNumber = md.dwBuildNumber;

	_MachineImageType = md.MachineImageType;
	_NumberProcessors = md.NumberProcessors;

	_PaeEnabled = md.PaeEnabled;
}

//////////////////////////////////////////////////////////////////////////
// ZMemoryDump64
#ifdef _WIN64

void ZMemoryDump64::UpdateContext()
{
	union {
		ULONG_PTR up;
		PVOID pCtx;
	};

	pCtx = 0;

	switch (_BugCheckCode)
	{
	case SYSTEM_THREAD_EXCEPTION_NOT_HANDLED:
	case SYSTEM_THREAD_EXCEPTION_NOT_HANDLED_M:
		up = _BugCheckParameter[3];
		break;
	case NTFS_FILE_SYSTEM:
	case FAT_FILE_SYSTEM:
	case CDFS_FILE_SYSTEM:
		up = _BugCheckParameter[2];
		break;
	}

	CONTEXT ctx;

	if (pCtx)
	{
		if (0 <= ReadVirtual(pCtx, &ctx, sizeof(ctx)))
		{
			memcpy(static_cast<CONTEXT*>(this), &ctx, sizeof(ctx));
		}
	}
}

void ZMemoryDump64::Init(HANDLE hFile, DUMP_HEADER64& md)
{
	_hFile = hFile;

	_Is64Bit = TRUE;

	SetDirectoryTableBase(md.DirectoryTableBase);

	memcpy(static_cast<CONTEXT*>(this), md.ContextRecord, sizeof(CONTEXT));
	memcpy(static_cast<EXCEPTION_RECORD*>(this), &md.Exception, sizeof(EXCEPTION_RECORD));

	memcpy(_BugCheckParameter, md.BugCheckParameter, sizeof(md.BugCheckParameter));

	_PsLoadedModuleList = md.PsLoadedModuleList;
	_PsActiveProcessHead = md.PsActiveProcessHead;
	_KdDebuggerDataBlock = md.KdDebuggerDataBlock;

	_SystemTime = md.SystemTime;
	_SystemUpTime = md.SystemUpTime;

	_BugCheckCode = md.BugCheckCode;

	_dwBuildNumber = md.dwBuildNumber;

	_MachineImageType = md.MachineImageType;
	_NumberProcessors = md.NumberProcessors;
}
#endif
//////////////////////////////////////////////////////////////////////////
// IMemoryDump

NTSTATUS IMemoryDump::ReadVirtual(PVOID Addr, PVOID buf, SIZE_T cb, PSIZE_T pcb)
{
	SIZE_T s = 0;
	if (!pcb) pcb = &s;

	NTSTATUS status = STATUS_ACCESS_VIOLATION;

	if (cb)
	{
		ULONG_PTR ptr = (ULONG_PTR)(Addr) & ~(PAGE_SIZE - 1);
		ULONG Pfn, ofs = RtlPointerToOffset(ptr, Addr);
		SIZE_T len = PAGE_SIZE - ofs;
		PAGE Page;

		do 
		{
			if (0 > VirtualToPhysical(ptr, &Pfn) || 0 > ReadPhysicalPage(Pfn, &Page))
			{
				break;
			}

			if (cb < len)
			{
				len = cb;
			}

			memcpy(buf, RtlOffsetToPointer(Page, ofs), len);

			cb -= len;
			s += len;
			ptr += PAGE_SIZE;
			buf = RtlOffsetToPointer(buf, len);
			len = PAGE_SIZE, ofs = 0, status = STATUS_PARTIAL_COPY;

		} while (cb);
	}

	*pcb = s;

	return cb ? status : STATUS_SUCCESS;
}

void _DumpDebuggerData(HWND hwndLog, IMemoryDump* pDump, KDDEBUGGER_DATA64& KdDebuggerDataBlock);

void IMemoryDump::DumpDebuggerData(HWND hwndLog)
{
	KDDEBUGGER_DATA64 DebuggerDataBlock;

	PVOID PsActiveProcessHeadAddr = PsActiveProcessHead();
	PVOID PsLoadedModuleListAddr = PsLoadedModuleList();

	if (0 > ReadVirtual(KdDebuggerDataBlock(), &DebuggerDataBlock, sizeof(DebuggerDataBlock)))
	{
		lprintf(hwndLog, L"Fail Read KdDebuggerDataBlock !!\r\n");
	}
	else
	{
		if (PsActiveProcessHeadAddr != (PVOID)(ULONG_PTR)DebuggerDataBlock.PsActiveProcessHead)
		{
			lprintf(hwndLog, L"PsActiveProcessHead != KdDebuggerDataBlock.PsActiveProcessHead (%p)\r\n", 
				DebuggerDataBlock.PsActiveProcessHead);
		}

		if (PsLoadedModuleListAddr != (PVOID)(ULONG_PTR)DebuggerDataBlock.PsLoadedModuleList)
		{
			lprintf(hwndLog, L"PsLoadedModuleList != KdDebuggerDataBlock.PsLoadedModuleList (%p)\r\n", 
				DebuggerDataBlock.PsLoadedModuleList);
		}

		_DumpDebuggerData(hwndLog, this, DebuggerDataBlock);
	}
}
//////////////////////////////////////////////////////////////////////////
// CBitmapDump

ULONG CBitmapDump::PhysicalPageToIndex(ULONG Pfn)
{
	if (Pfn >= SizeOfBitMap || !_bittest((PLONG)Buffer, Pfn))//RtlTestBit(this, Pfn)
	{
		return MAXULONG;
	}

	ULONG i = Pfn >> 5, index = 0;

	if (i)
	{
		index = _NumBitsSet[i - 1];
	}

	i = Buffer[i] & (1 << (Pfn & 31)) - 1;

	return index + NumberOfSetBitsUlong(i);
}

NTSTATUS CBitmapDump::Validate(HWND hwndLog, HANDLE hFile, COMMON_BITMAP_DUMP& bd, ZMemoryDump* pDump)
{
	if (bd.ValidDump != DUMP_SUMMARY_VALID || bd.WaitSignature != bd.Signature)
	{
		lprintf(hwndLog, L"Invalid Dump!\r\n");
		return STATUS_INVALID_IMAGE_FORMAT;
	}

	switch (bd.Signature)
	{
	case DUMP_SUMMARY_SIGNATURE:
		lprintf(hwndLog, L"summary bitmap\r\n");
		break;
	case FULL_SUMMARY_SIGNATURE:
		lprintf(hwndLog, L"full bitmap\r\n");
		break;
	default:
		lprintf(hwndLog, L"Invalid Dump signature! [%.4s]\r\n", &bd.Signature);
		return STATUS_INVALID_IMAGE_FORMAT;
	}

	ULONG64 BitmapSize = bd.BitmapSize, HeaderSize = bd.HeaderSize, Pages = bd.Pages, sds;

	if (BitmapSize > MAXULONG)
	{
		lprintf(hwndLog, L"BitmapSize (%I64u) too big\r\n", BitmapSize);
		return STATUS_NOT_SUPPORTED;
	}

	ULONG BitmapSizeInBytes = (ULONG)(BitmapSize + 7) >> 3, BitmapSizeIn4 = (BitmapSizeInBytes + 3) & ~3;

	if (!Pages)
	{
		lprintf(hwndLog, L"no Pages in dump!\r\n");
		return STATUS_INVALID_IMAGE_FORMAT;
	}

	if (Pages > BitmapSize)
	{
		lprintf(hwndLog, L"Pages Count (%I64u) > BitmapSize (%I64u)\r\n", Pages, BitmapSize);
		return STATUS_INVALID_IMAGE_FORMAT;
	}

	LARGE_INTEGER ByteOffset = { bd.BitsOffset };

	if (HeaderSize < (ULONG64)ByteOffset.QuadPart + BitmapSizeInBytes)
	{
		lprintf(hwndLog, L"HeaderSize (%I64u) too small\r\n", HeaderSize);
		return STATUS_INVALID_IMAGE_FORMAT;
	}

	sds = (Pages << PAGE_SHIFT) + HeaderSize;

	if (sds > bd.RequiredDumpSpace)
	{
		lprintf(hwndLog, L"DumpSpace < HeaderSize + PagesSize\r\n");
		return STATUS_INVALID_IMAGE_FORMAT;
	}

	pDump->set_DumpBlobOffset(sds);

	if (sds = bd.RequiredDumpSpace - sds)
	{
		lprintf(hwndLog, L"SECONDARY_DUMP_DATA = %x bytes\r\n", sds);
	}

	pDump->set_DumpBlobSize(sds);

	if (!(Buffer = (PULONG)new UCHAR[BitmapSizeIn4 << 1]))
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	IO_STATUS_BLOCK iosb;
	NTSTATUS status = ZwReadFile(hFile, 0, 0, 0, &iosb, Buffer, BitmapSizeInBytes, &ByteOffset, 0);

	if (0 > status)
	{
		return status;
	}

	if (iosb.Information != BitmapSizeInBytes)
	{
		return STATUS_INVALID_IMAGE_FORMAT;
	}

	_NumBitsSet = (PULONG)RtlOffsetToPointer(Buffer, BitmapSizeIn4);

	if (ULONG m = BitmapSize & 31)
	{
		// zero init unused bits
		*(_NumBitsSet - 1) &= (1 << m) - 1;
	}

	SizeOfBitMap = ((ULONG)BitmapSize + 31) & ~31;

	if (BitSetCount(Buffer, _NumBitsSet, SizeOfBitMap) != Pages)
	{
		return STATUS_INVALID_IMAGE_FORMAT;
	}

	pDump->set_PagesOffset((ULONG)HeaderSize);

	return STATUS_SUCCESS;
}

#ifdef _AMD64_

NTSTATUS CBitmapDump64::Validate(HWND hwndLog, HANDLE hFile, MEMORY_DUMP64& md, COMMON_BITMAP_DUMP& bd)
{
	ULONG64 DirectoryTablePfn = md.DirectoryTableBase >> PAGE_SHIFT;

	if (DirectoryTablePfn >= bd.BitmapSize)
	{
		lprintf(hwndLog, L"DirectoryTablePfn (%I64u) > BitmapSize (%I64u)\r\n", DirectoryTablePfn, bd.BitmapSize);
		return STATUS_INVALID_IMAGE_FORMAT;
	}

	NTSTATUS status = CBitmapDump::Validate(hwndLog, hFile, bd, this);

	if (0 > status)
	{
		return status;
	}

	return Create();
}

NTSTATUS OpenDump64(HWND hwndLog, HANDLE hFile, LARGE_INTEGER& EndOfFile, MEMORY_DUMP64& md, IMemoryDump** ppDump)
{
	lprintf(hwndLog, L"DUMP64\r\nDumpSpace  (%016I64u)\r\nSizeOfFile (%016I64u)\r\n", md.RequiredDumpSpace.QuadPart, EndOfFile.QuadPart);

	if ((ULONGLONG)md.RequiredDumpSpace.QuadPart > (ULONGLONG)EndOfFile.QuadPart)
	{
		lprintf(hwndLog, L"DumpSpace (%016I64u) > SizeOfFile (%016I64u)\r\n");
		return STATUS_INVALID_IMAGE_FORMAT;
	}

	lprintf(hwndLog, L"NtBuildNumber = %u\r\nNumberProcessors = %x\r\nBugCheckCode = %x {\r\n\t%p,\r\n\t%p,\r\n\t%p,\r\n\t%p\r\n\t}\r\n", 
		md.dwBuildNumber, md.NumberProcessors, md.BugCheckCode,
		md.BugCheckParameter[0],
		md.BugCheckParameter[1],
		md.BugCheckParameter[2],
		md.BugCheckParameter[3]
	);

	EXCEPTION_RECORD64& Exception = md.Exception;
	ULONG NumberParameters = Exception.NumberParameters;

	lprintf(hwndLog, L"%x at %p (%x, %p) %x [", Exception.ExceptionCode, (PVOID)(ULONG_PTR)Exception.ExceptionAddress, 
		Exception.ExceptionFlags, (PVOID)(ULONG_PTR)Exception.ExceptionRecord, NumberParameters);

	if (NumberParameters)
	{
		if (NumberParameters > EXCEPTION_MAXIMUM_PARAMETERS)
		{
			NumberParameters = EXCEPTION_MAXIMUM_PARAMETERS;
		}

		PVOID* ExceptionInformation = (PVOID*)Exception.ExceptionInformation;
		do 
		{
			lprintf(hwndLog, L"\t%p\r\n", *ExceptionInformation++);
		} while (--NumberParameters);
	}

	lprintf(hwndLog, L"\t]\r\n");

	lprintf(hwndLog, L"PfnDataBase = %p\r\nPsLoadedModuleList = %p\r\nPsActiveProcessHead = %p\r\nKdDebuggerDataBlock = %p\r\n", 
		md.PfnDataBase, md.PsLoadedModuleList, md.PsActiveProcessHead, md.KdDebuggerDataBlock);

	if (!IsKernelPointerAddress(md.PfnDataBase))
	{
		lprintf(hwndLog, L"PfnDataBase - invalid !!!\r\n");
		md.PfnDataBase = 0;
	}

	if (!IsKernelPointerAddress(md.PsLoadedModuleList))
	{
		lprintf(hwndLog, L"PsLoadedModuleList - invalid\r\n");
		md.PsLoadedModuleList = 0;
	}

	if (!IsKernelPointerAddress(md.PsActiveProcessHead))
	{
		lprintf(hwndLog, L"PsActiveProcessHead - invalid !!!\r\n");
		md.PsActiveProcessHead = 0;
	}

	if (!IsKernelPointerAddress(md.KdDebuggerDataBlock))
	{
		lprintf(hwndLog, L"KdDebuggerDataBlock - invalid !!!\r\n");
		md.KdDebuggerDataBlock = 0;
	}

	if (md.DirectoryTableBase & (PAGE_SIZE - 1))
	{
		DbgPrint("Invalid DirectoryTableBase=%x\r\n", md.DirectoryTableBase);
		return STATUS_INVALID_IMAGE_FORMAT;
	}

	TIME_FIELDS tf;
	RtlTimeToTimeFields(&md.SystemTime, &tf);
	lprintf(hwndLog, L"SystemTime   = %u-%02u-%02u %02u:%02u:%02u\r\n", tf.Year, tf.Month, tf.Day, tf.Hour, tf.Minute, tf.Second);
	RtlTimeToTimeFields(&md.SystemUpTime, &tf);
	tf.Year -= 1601, tf.Day -= 1, tf.Month -= 1;
	lprintf(hwndLog, L"SystemUpTime = %u-%02u-%02u %02u:%02u:%02u\r\n", tf.Year, tf.Month, tf.Day, tf.Hour, tf.Minute, tf.Second);

	lprintf(hwndLog, L"MachineImageType = ");

	switch (md.MachineImageType)
	{
	case IMAGE_FILE_MACHINE_AMD64:
		lprintf(hwndLog, L"IMAGE_FILE_MACHINE_AMD64\r\n");
		break;
	default:
		lprintf(hwndLog, L"%x - unsupported\r\n", md.MachineImageType);
		return STATUS_INVALID_IMAGE_FORMAT;
	}

	NTSTATUS status;

	PCWSTR szSpace = L"Kernel";

	COMMON_BITMAP_DUMP cbd;

	cbd.Signature = md.Summary.Signature;//==md.Bitmap.Signature
	cbd.ValidDump = md.Summary.ValidDump;//==md.Bitmap.ValidDump
	cbd.WaitSignature = DUMP_SUMMARY_SIGNATURE;
	cbd.RequiredDumpSpace = md.RequiredDumpSpace.QuadPart;

	switch (md.DumpType)
	{
	case DUMP_TYPE_FULL:
		{
			ULONG NumberOfRuns = md.NumberOfRuns;

			if (!NumberOfRuns || NumberOfRuns > RTL_NUMBER_OF(md.Run))
			{
				lprintf(hwndLog, L"invalid NumberOfRuns = 0x%x\r\n", NumberOfRuns);
				return STATUS_INVALID_IMAGE_FORMAT;
			}
			if (CFullDump64* p = new(NumberOfRuns) CFullDump64)
			{
				status = p->Validate(hwndLog, md);

				if (0 <= status)
				{
					*ppDump = p;
					return status;
				}
				p->Release();
			}
			else
			{
				return STATUS_INSUFFICIENT_RESOURCES;
			}
		}
		break;

	case DUMP_TYPE_SUMMARY:
		lprintf(hwndLog, L"64-bit Kernel Summary Dump\r\n");
		if (md.Summary.BitmapSize != md.Summary.SizeOfBitMap)
		{
			lprintf(hwndLog, L"BitmapSize(%x) != SizeOfBitMap(%x)\r\n", md.Summary.BitmapSize, md.Summary.SizeOfBitMap);
			return STATUS_INVALID_IMAGE_FORMAT;
		}
		cbd.HeaderSize = md.Summary.HeaderSize;
		cbd.Pages = md.Summary.Pages;
		cbd.BitmapSize = md.Summary.BitmapSize;
		cbd.BitsOffset = FIELD_OFFSET(MEMORY_DUMP64, Summary.Bits);
		goto __common_bitmap;

	case DUMP_TYPE_BITMAP_FULL:
		szSpace = L"full";
		cbd.WaitSignature = FULL_SUMMARY_SIGNATURE;
	case DUMP_TYPE_BITMAP_KERNEL:
		lprintf(hwndLog, L"64-bit Kernel Bitmap Dump - %s address space is available\r\n", szSpace);

		cbd.HeaderSize = md.Bitmap.HeaderSize;
		cbd.Pages = md.Bitmap.Pages;
		cbd.BitmapSize = md.Bitmap.BitmapSize;
		cbd.BitsOffset = FIELD_OFFSET(MEMORY_DUMP64, Bitmap.Bits);
__common_bitmap:
		if (CBitmapDump64* p = new CBitmapDump64)
		{
			status = p->Validate(hwndLog, hFile, md, cbd);

			if (0 <= status)
			{
				*ppDump = p;
				return status;
			}
			p->Release();
		}
		else
		{
			return STATUS_INSUFFICIENT_RESOURCES;
		}
		break;
	default: 
		lprintf(hwndLog, L"DumpType = %x not implemented\r\n", md.DumpType);
		return STATUS_NOT_IMPLEMENTED;
	}

	return status;
}

//////////////////////////////////////////////////////////////////////////
// CFullDump64

ULONG CFullDump64::PhysicalPageToIndex(ULONG Pfn)
{
	PHYSICAL_MEMORY_RUN64* Run = _Run;
	ULONG NumberOfRuns = _NumberOfRuns;
	ULONG64 i = 0, PageCount, BasePage, m;

	if (Pfn < Run->BasePage)
	{
		return MAXULONG;
	}

	do 
	{
		BasePage = Run->BasePage;

		if (BasePage <= Pfn)
		{
			PageCount = Run->PageCount, m = Pfn - BasePage;
			if (m < PageCount)
			{
				return (ULONG)(i + m);
			}

			i += PageCount;
		}
	} while (Run++, --NumberOfRuns);

	return MAXULONG;
}

NTSTATUS CFullDump64::Validate(HWND hwndLog, MEMORY_DUMP64& md)
{
	ULONG NumberOfRuns = md.NumberOfRuns;

	_NumberOfRuns = NumberOfRuns;

	PHYSICAL_MEMORY_RUN64* Run = md.Run;
	memcpy(_Run, Run, sizeof(PHYSICAL_MEMORY_RUN64) * NumberOfRuns);

	ULONG64 NumberOfPages = 0, PageCount, BasePage, EndPage = 0;
	do 
	{
		BasePage = Run->BasePage;
		if (BasePage < EndPage)
		{
			lprintf(hwndLog, L"invalid memory Runs\r\n");
			return STATUS_INVALID_IMAGE_FORMAT;
		}
		PageCount = Run->PageCount;
		EndPage = BasePage + PageCount;
		if (EndPage <= BasePage)
		{
			lprintf(hwndLog, L"invalid memory Runs\r\n");
			return STATUS_INVALID_IMAGE_FORMAT;
		}
		NumberOfPages += PageCount;

	} while (Run++, --NumberOfRuns);

	if (NumberOfPages != md.NumberOfPages)
	{
		lprintf(hwndLog, L"NumberOfPages(%x) != DUMP_HEADER32.NumberOfPages(%x)\r\n", NumberOfPages, md.NumberOfPages);
		return STATUS_INVALID_IMAGE_FORMAT;
	}

	set_PagesOffset(FIELD_OFFSET(MEMORY_DUMP64, Full.Memory));

	ULONG64 size = FIELD_OFFSET(MEMORY_DUMP64, Full.Memory) + ((ULONG64)NumberOfPages << PAGE_SHIFT);

	if ((ULONG64)md.RequiredDumpSpace.QuadPart < size)
	{
		lprintf(hwndLog, L"DumpSpace(%I64x) < HeaderSize(%x) + PagesSize(%I64x)\r\n", 
			md.RequiredDumpSpace.QuadPart, 
			FIELD_OFFSET(MEMORY_DUMP64, Full.Memory), ((ULONG64)NumberOfPages << PAGE_SHIFT));

		return STATUS_INVALID_IMAGE_FORMAT;
	}

	set_DumpBlobOffset(size);

	if (size = (ULONG64)md.RequiredDumpSpace.QuadPart - size)
	{
		lprintf(hwndLog, L"SECONDARY_DUMP_DATA = %x bytes\r\n", size);
	}

	set_DumpBlobSize(size);

	return Create();
}


#endif

//////////////////////////////////////////////////////////////////////////
// CBitmapDump32

NTSTATUS CBitmapDump32::Validate(HWND hwndLog, HANDLE hFile, MEMORY_DUMP32& md, COMMON_BITMAP_DUMP& bd)
{
	ULONG64 DirectoryTablePfn = md.DirectoryTableBase >> PAGE_SHIFT;

	if (DirectoryTablePfn >= bd.BitmapSize)
	{
		lprintf(hwndLog, L"DirectoryTablePfn (%I64u) > BitmapSize (%I64u)\r\n", DirectoryTablePfn, bd.BitmapSize);
		return STATUS_INVALID_IMAGE_FORMAT;
	}

	NTSTATUS status = CBitmapDump::Validate(hwndLog, hFile, bd, this);

	if (0 > status)
	{
		return status;
	}

	return Create();
}

//////////////////////////////////////////////////////////////////////////
// CFullDump32

ULONG CFullDump32::PhysicalPageToIndex(ULONG Pfn)
{
	PHYSICAL_MEMORY_RUN32* Run = _Run;
	ULONG NumberOfRuns = _NumberOfRuns;
	ULONG i = 0, PageCount, BasePage, m;

	if (Pfn < Run->BasePage)
	{
		return MAXULONG;
	}

	do 
	{
		BasePage = Run->BasePage;

		if (BasePage <= Pfn)
		{
			PageCount = Run->PageCount, m = Pfn - BasePage;
			if (m < PageCount)
			{
				return i + m;
			}

			i += PageCount;
		}
	} while (Run++, --NumberOfRuns);

	return MAXULONG;
}

NTSTATUS CFullDump32::Validate(HWND hwndLog, MEMORY_DUMP32& md)
{
	ULONG NumberOfRuns = md.NumberOfRuns;

	_NumberOfRuns = NumberOfRuns;

	PHYSICAL_MEMORY_RUN32* Run = md.Run;
	memcpy(_Run, Run, sizeof(PHYSICAL_MEMORY_RUN32) * NumberOfRuns);

	ULONG NumberOfPages = 0, PageCount, BasePage, EndPage = 0;
	do 
	{
		BasePage = Run->BasePage;
		if (BasePage < EndPage)
		{
			lprintf(hwndLog, L"invalid memory Runs\r\n");
			return STATUS_INVALID_IMAGE_FORMAT;
		}
		PageCount = Run->PageCount;
		EndPage = BasePage + PageCount;
		if (EndPage <= BasePage)
		{
			lprintf(hwndLog, L"invalid memory Runs\r\n");
			return STATUS_INVALID_IMAGE_FORMAT;
		}
		NumberOfPages += PageCount;

	} while (Run++, --NumberOfRuns);

	if (NumberOfPages != md.NumberOfPages)
	{
		lprintf(hwndLog, L"NumberOfPages(%x) != DUMP_HEADER32.NumberOfPages(%x)\r\n", NumberOfPages, md.NumberOfPages);
		return STATUS_INVALID_IMAGE_FORMAT;
	}

	set_PagesOffset(FIELD_OFFSET(MEMORY_DUMP32, Full.Memory));

	ULONG64 size = FIELD_OFFSET(MEMORY_DUMP32, Full.Memory) + ((ULONG64)NumberOfPages << PAGE_SHIFT);

	if ((ULONG64)md.RequiredDumpSpace.QuadPart < size)
	{
		lprintf(hwndLog, L"DumpSpace(%I64x) < HeaderSize(%x) + PagesSize(%I64x)\r\n", 
			md.RequiredDumpSpace.QuadPart, 
			FIELD_OFFSET(MEMORY_DUMP32, Full.Memory), ((ULONG64)NumberOfPages << PAGE_SHIFT));

		return STATUS_INVALID_IMAGE_FORMAT;
	}

	set_DumpBlobOffset(size);

	if (size = (ULONG64)md.RequiredDumpSpace.QuadPart - size)
	{
		lprintf(hwndLog, L"SECONDARY_DUMP_DATA = %x bytes\r\n", size);
	}

	set_DumpBlobSize(size);

	return Create();
}

//////////////////////////////////////////////////////////////////////////
// Open
NTSTATUS OpenDump32(HWND hwndLog, HANDLE hFile, LARGE_INTEGER& EndOfFile, MEMORY_DUMP32& md, IMemoryDump** ppDump)
{
	lprintf(hwndLog, L"DUMP32\r\nDumpSpace  (%016I64u)\r\nSizeOfFile (%016I64u)\r\n", md.RequiredDumpSpace.QuadPart, EndOfFile.QuadPart);

	if ((ULONGLONG)md.RequiredDumpSpace.QuadPart > (ULONGLONG)EndOfFile.QuadPart)
	{
		lprintf(hwndLog, L"DumpSpace (%016I64u) > SizeOfFile (%016I64u)\r\n");
		return STATUS_INVALID_IMAGE_FORMAT;
	}

	lprintf(hwndLog, L"NtBuildNumber = %u\r\nNumberProcessors = %x\r\nBugCheckCode = %x {\r\n\t%x,\r\n\t%x,\r\n\t%x,\r\n\t%x\r\n\t}\r\n", 
		md.dwBuildNumber, md.NumberProcessors, md.BugCheckCode,
		md.BugCheckParameter[0],
		md.BugCheckParameter[1],
		md.BugCheckParameter[2],
		md.BugCheckParameter[3]
	);

	EXCEPTION_RECORD32& Exception = md.Exception;
	ULONG NumberParameters = Exception.NumberParameters;

	lprintf(hwndLog, L"Exception %x at %x (%x, %x) %x [", Exception.ExceptionCode, Exception.ExceptionAddress, 
		Exception.ExceptionFlags, Exception.ExceptionRecord, NumberParameters);

	if (NumberParameters)
	{
		if (NumberParameters > EXCEPTION_MAXIMUM_PARAMETERS)
		{
			NumberParameters = EXCEPTION_MAXIMUM_PARAMETERS;
		}

		ULONG* ExceptionInformation = Exception.ExceptionInformation;
		do 
		{
			lprintf(hwndLog, L"\t%x\r\n", *ExceptionInformation++);
		} while (--NumberParameters);
	}

	lprintf(hwndLog, L"\t]\r\n");

	lprintf(hwndLog, L"PfnDataBase = %x\r\nPsLoadedModuleList = %x\r\nPsActiveProcessHead = %x\r\nKdDebuggerDataBlock = %x\r\n", 
		md.PfnDataBase, md.PsLoadedModuleList, md.PsActiveProcessHead, md.KdDebuggerDataBlock);

	if (!IsKernelPointerAddress32(md.PfnDataBase))
	{
		lprintf(hwndLog, L"PfnDataBase - invalid !!!\r\n");
		md.PfnDataBase = 0;
	}

	if (!IsKernelPointerAddress32(md.PsLoadedModuleList))
	{
		lprintf(hwndLog, L"PsLoadedModuleList - invalid\r\n");
		md.PsLoadedModuleList = 0;
	}

	if (!IsKernelPointerAddress32(md.PsActiveProcessHead))
	{
		lprintf(hwndLog, L"PsActiveProcessHead - invalid !!!\r\n");
		md.PsActiveProcessHead = 0;
	}

	if (!IsKernelPointerAddress32(md.KdDebuggerDataBlock))
	{
		lprintf(hwndLog, L"KdDebuggerDataBlock - invalid !!!\r\n");
		md.KdDebuggerDataBlock = 0;
	}

	if (md.PaeEnabled)
	{
		if (
			( md.DirectoryTableBase & (__alignof(_PTE_PAE) - 1) ) || 
			( (md.DirectoryTableBase & (PAGE_SIZE - 1)) > (PAGE_SIZE - 4 * sizeof(_PTE_PAE)) ) 
			)
		{
			goto __err;
		}
	}
	else if (md.DirectoryTableBase & (PAGE_SIZE - 1))
	{
__err:
		DbgPrint("Invalid DirectoryTableBase=%x\r\n", md.DirectoryTableBase);
		return STATUS_INVALID_IMAGE_FORMAT;
	}

	TIME_FIELDS tf;
	RtlTimeToTimeFields(&md.SystemTime, &tf);
	lprintf(hwndLog, L"SystemTime   = %u-%02u-%02u %02u:%02u:%02u\r\n", tf.Year, tf.Month, tf.Day, tf.Hour, tf.Minute, tf.Second);
	RtlTimeToTimeFields(&md.SystemUpTime, &tf);
	tf.Year -= 1601, tf.Day -= 1, tf.Month -= 1;
	lprintf(hwndLog, L"SystemUpTime = %u-%02u-%02u %02u:%02u:%02u\r\n", tf.Year, tf.Month, tf.Day, tf.Hour, tf.Minute, tf.Second);

	lprintf(hwndLog, L"MachineImageType = ");

	switch (md.MachineImageType)
	{
	case IMAGE_FILE_MACHINE_I386:
		lprintf(hwndLog, L"IMAGE_FILE_MACHINE_I386\r\n");
		break;
	default:
		lprintf(hwndLog, L"%x - unsupported\r\n", md.MachineImageType);
		return STATUS_INVALID_IMAGE_FORMAT;
	}

	NTSTATUS status;

	PCWSTR szSpace = L"Kernel";
	COMMON_BITMAP_DUMP cbd;

	cbd.Signature = md.Summary.Signature;//==md.Bitmap.Signature
	cbd.ValidDump = md.Summary.ValidDump;//==md.Bitmap.ValidDump
	cbd.WaitSignature = DUMP_SUMMARY_SIGNATURE;
	cbd.RequiredDumpSpace = md.RequiredDumpSpace.QuadPart;

	switch (md.DumpType)
	{
	case DUMP_TYPE_FULL:
		{
			ULONG NumberOfRuns = md.NumberOfRuns;

			if (!NumberOfRuns || NumberOfRuns > RTL_NUMBER_OF(md.Run))
			{
				lprintf(hwndLog, L"invalid NumberOfRuns = 0x%x\r\n", NumberOfRuns);
				return STATUS_INVALID_IMAGE_FORMAT;
			}
			if (CFullDump32* p = new(NumberOfRuns) CFullDump32)
			{
				status = p->Validate(hwndLog, md);

				if (0 <= status)
				{
					*ppDump = p;
					return status;
				}
				p->Release();
			}
			else
			{
				return STATUS_INSUFFICIENT_RESOURCES;
			}
		}
		break;

	case DUMP_TYPE_SUMMARY:
		lprintf(hwndLog, L"32-bit Kernel Summary Dump\r\n");
		if (md.Summary.BitmapSize != md.Summary.SizeOfBitMap)
		{
			lprintf(hwndLog, L"BitmapSize(%x) != SizeOfBitMap(%x)\r\n", md.Summary.BitmapSize, md.Summary.SizeOfBitMap);
			return STATUS_INVALID_IMAGE_FORMAT;
		}
		cbd.HeaderSize = md.Summary.HeaderSize;
		cbd.Pages = md.Summary.Pages;
		cbd.BitmapSize = md.Summary.BitmapSize;
		cbd.BitsOffset = FIELD_OFFSET(MEMORY_DUMP32, Summary.Bits);
		goto __common_bitmap;

	case DUMP_TYPE_BITMAP_FULL:
		szSpace = L"full";
		cbd.WaitSignature = FULL_SUMMARY_SIGNATURE;
	case DUMP_TYPE_BITMAP_KERNEL:
		lprintf(hwndLog, L"32-bit Kernel Bitmap Dump - %s address space is available\r\n", szSpace);

		cbd.HeaderSize = md.Bitmap.HeaderSize;
		cbd.Pages = md.Bitmap.Pages;
		cbd.BitmapSize = md.Bitmap.BitmapSize;
		cbd.BitsOffset = FIELD_OFFSET(MEMORY_DUMP32, Bitmap.Bits);
__common_bitmap:
		if (CBitmapDump32* p = new CBitmapDump32)
		{
			status = p->Validate(hwndLog, hFile, md, cbd);

			if (0 <= status)
			{
				*ppDump = p;
				return status;
			}
			p->Release();
		}
		else
		{
			return STATUS_INSUFFICIENT_RESOURCES;
		}
		break;
	default: 
		lprintf(hwndLog, L"DumpType = %x not implemented\r\n", md.DumpType);
		return STATUS_NOT_IMPLEMENTED;
	}

	return status;
}

NTSTATUS OpenDump(HWND hwndLog, POBJECT_ATTRIBUTES poa, IMemoryDump** ppDump)
{
	IO_STATUS_BLOCK iosb;
	HANDLE hFile;
	NTSTATUS status = ZwOpenFile(&hFile, FILE_GENERIC_READ, poa, &iosb, FILE_SHARE_READ, FILE_SYNCHRONOUS_IO_NONALERT);

	lprintf(hwndLog, L"OpenDump(%wZ) = %x\r\n", poa->ObjectName, status);

	if (0 <= status)
	{
		FILE_STANDARD_INFORMATION fsi;

		if (0 <= (status = ZwQueryInformationFile(hFile, &iosb, &fsi, sizeof(fsi), FileStandardInformation)))
		{
			union {
				DUMP_HEADER md;
				MEMORY_DUMP32 md32;
				MEMORY_DUMP64 md64;
				UCHAR u[max(sizeof(MEMORY_DUMP32),sizeof(MEMORY_DUMP64))];
			};

			if (fsi.EndOfFile.QuadPart >= sizeof(u))
			{
				if (0 <= (status = ZwReadFile(hFile, 0, 0, 0, &iosb, u, sizeof(u), 0, 0)))
				{
					status = STATUS_INVALID_IMAGE_FORMAT;

					if (iosb.Information == sizeof(u))
					{
						if (md.Signature != DUMP_SIGNATURE)
						{
							lprintf(hwndLog, L"Wrong Dump Signature\r\n");
						}
						else
						{
							switch (md.ValidDump)
							{
							case DUMP_VALID_DUMP64:
#ifdef _WIN64
								if (0 <= (status = OpenDump64(hwndLog, hFile, fsi.EndOfFile, md64, ppDump)))
								{
									static_cast<ZMemoryDump64*>(*ppDump)->Init(hFile, md64);
								}
#else
								lprintf(hwndLog, L"this is 64-bit dump. use 64-bit debugger !\r\n");
								status = STATUS_NOT_SUPPORTED;
#endif
								break;
							case DUMP_VALID_DUMP32:
								if (0 <= (status = OpenDump32(hwndLog, hFile, fsi.EndOfFile, md32, ppDump)))
								{
									static_cast<ZMemoryDump32*>(*ppDump)->Init(hFile, md32);
								}
								break;
							default: 
								status = STATUS_INVALID_IMAGE_FORMAT;
								lprintf(hwndLog, L"Not a ValidDump\r\n");
							}
						}
					}
				}
			}
			else
			{
				status = STATUS_INVALID_IMAGE_FORMAT;
				lprintf(hwndLog, L"Dump File too small (%I64u bytes)\r\n", fsi.EndOfFile.QuadPart);
			}
		}

		if (0 > status) NtClose(hFile);

	}

	return status;
}

union U32_64 {
	PVOID pv;
	ULONG32 *p32;
#ifdef _WIN64
	ULONG64 *p64;
#endif
};

BOOL DumpDebuggerDataEx(HWND hwndLog, 
						IMemoryDump* pDump, 
						KDDEBUGGER_DATA64& KdDebuggerDataBlock,
						U32_64 KiProcessorBlock,
						ULONG NumberProcessors
						)
{
	ULONG Size = KdDebuggerDataBlock.Header.Size;
	
	if (Size < FIELD_OFFSET(KDDEBUGGER_DATA64, OffsetPrcbPcrPage)) return FALSE;

#ifdef _WIN64
	BOOL Is64Bit = pDump->Is64Bit();

	ULONG_PTR ptr_size, ptr_align;

	if (Is64Bit)
	{
		ptr_size = 8;
		ptr_align = 7UL;
	}
	else
	{
		ptr_size = 4;
		ptr_align = 3UL;
	}
#else
	enum : ULONG { ptr_size = 4, ptr_align = 3UL };
#endif

	ULONG SizePrcb = KdDebuggerDataBlock.SizePrcb, SizePcr = KdDebuggerDataBlock.SizePcr;
	ULONG OffsetPcrContainedPrcb = KdDebuggerDataBlock.OffsetPcrContainedPrcb;

	if (SizePcr < SizePrcb + OffsetPcrContainedPrcb || (OffsetPcrContainedPrcb & ptr_align))
	{
		lprintf(hwndLog, L"Error: SizePcr(%x) != SizePrcb(%x) + OffsetPcrContainedPrcb !!\r\n", SizePcr, SizePrcb, OffsetPcrContainedPrcb);

		return FALSE;
	}

	LONG ValidFlags = 0;

	enum {
		evKThreadTeb,
		evKThreadKernelStack,
		evKThreadInitialStack,
		evKThreadApcProcess,
		evKThreadState,

		evEprocessPeb,
		evEprocessParentCID,
		evEprocessDirectoryTableBase,

		evSelfPcr,
		evCurrentPrcb,
		evContainedPrcb,
		evInitialStack,
		evStackLimit,
		evbDpcRoutine,
		evbCurrentThread,
		evbMhz,
		evbCpuType,
		evbVendorString,
		evbNumber,
		evbContext,
		evbIdleThread,
		evbDpcStack,
		evbIsrStack,
	};

	// Server 2003 addition

	ULONG SizeEThread = KdDebuggerDataBlock.SizeEThread;
	ULONG OffsetKThreadTeb = KdDebuggerDataBlock.OffsetKThreadTeb;
	ULONG OffsetKThreadKernelStack = KdDebuggerDataBlock.OffsetKThreadKernelStack;
	ULONG OffsetKThreadInitialStack = KdDebuggerDataBlock.OffsetKThreadInitialStack;
	ULONG OffsetKThreadApcProcess = KdDebuggerDataBlock.OffsetKThreadApcProcess;
	ULONG OffsetKThreadState = KdDebuggerDataBlock.OffsetKThreadState;

	ULONG SizeEProcess = KdDebuggerDataBlock.SizeEProcess;
	ULONG OffsetEprocessPeb = KdDebuggerDataBlock.OffsetEprocessPeb;
	ULONG OffsetEprocessParentCID = KdDebuggerDataBlock.OffsetEprocessParentCID;
	ULONG OffsetEprocessDirectoryTableBase = KdDebuggerDataBlock.OffsetEprocessDirectoryTableBase;

	ULONG OffsetPrcbDpcRoutineActive = KdDebuggerDataBlock.OffsetPrcbDpcRoutine;
	ULONG OffsetPrcbCurrentThread = KdDebuggerDataBlock.OffsetPrcbCurrentThread;
	ULONG OffsetPrcbMhz = KdDebuggerDataBlock.OffsetPrcbMhz;

	ULONG OffsetPrcbCpuType = KdDebuggerDataBlock.OffsetPrcbCpuType;
	ULONG OffsetPrcbVendorString = KdDebuggerDataBlock.OffsetPrcbVendorString;
	ULONG OffsetPrcbNumber = KdDebuggerDataBlock.OffsetPrcbNumber;

	ULONG OffsetPcrSelfPcr = KdDebuggerDataBlock.OffsetPcrSelfPcr;
	ULONG OffsetPcrCurrentPrcb = KdDebuggerDataBlock.OffsetPcrCurrentPrcb;
	ULONG OffsetPcrInitialStack = KdDebuggerDataBlock.OffsetPcrInitialStack;
	ULONG OffsetPcrStackLimit = KdDebuggerDataBlock.OffsetPcrStackLimit;

	// Windows 7 addition

	ULONG OffsetPrcbContext = KdDebuggerDataBlock.OffsetPrcbContext;

	// Windows 8 addition

	ULONG OffsetPrcbIdleThread = KdDebuggerDataBlock.OffsetPrcbIdleThread;
	ULONG OffsetPrcbDpcStack = KdDebuggerDataBlock.OffsetPrcbDpcStack;
	ULONG OffsetPrcbIsrStack = KdDebuggerDataBlock.OffsetPrcbIsrStack;

	if (
		!(OffsetKThreadTeb & ptr_align) &&
		(OffsetKThreadTeb + ptr_size <= SizeEThread)
		)
	{
		_bittestandset(&ValidFlags, evKThreadTeb);
	}

	if (
		!(OffsetKThreadKernelStack & ptr_align) &&
		(OffsetKThreadKernelStack + ptr_size <= SizeEThread)
		)
	{
		_bittestandset(&ValidFlags, evKThreadKernelStack);
	}

	if (
		!(OffsetKThreadInitialStack & ptr_align) &&
		(OffsetKThreadInitialStack + ptr_size <= SizeEThread)
		)
	{
		_bittestandset(&ValidFlags, evKThreadInitialStack);
	}

	if (
		!(OffsetKThreadApcProcess & ptr_align) &&
		(OffsetKThreadApcProcess + ptr_size <= SizeEThread)
		)
	{
		_bittestandset(&ValidFlags, evKThreadApcProcess);
	}

	if (OffsetKThreadState + sizeof(UCHAR) <= SizeEThread)
	{
		_bittestandset(&ValidFlags, evKThreadState);
	}

	if (
		!(OffsetEprocessDirectoryTableBase & ptr_align) &&
		(OffsetEprocessDirectoryTableBase + ptr_size <= SizeEProcess)
		)
	{
		_bittestandset(&ValidFlags, evEprocessDirectoryTableBase);
	}

	if (
		!(OffsetEprocessPeb & ptr_align) &&
		(OffsetEprocessPeb + ptr_size <= SizeEProcess)
		)
	{
		_bittestandset(&ValidFlags, evEprocessPeb);
	}

	if (
		!(OffsetEprocessParentCID & ptr_align) &&
		(OffsetEprocessParentCID + ptr_size <= SizeEProcess)
		)
	{
		_bittestandset(&ValidFlags, evEprocessParentCID);
	}

	if (OffsetPrcbDpcRoutineActive + sizeof(UCHAR) <= SizePrcb)
	{
		_bittestandset(&ValidFlags, evbDpcRoutine);
	}

	if (
		!(OffsetPrcbCurrentThread & ptr_align) && 
		(OffsetPrcbCurrentThread + ptr_size <= SizePrcb)
		)
	{
		_bittestandset(&ValidFlags, evbCurrentThread);
	}

	if (
		!(OffsetPrcbMhz & (__alignof(ULONG) - 1)) && 
		(OffsetPrcbMhz + sizeof(ULONG) <= SizePrcb)
		)
	{
		_bittestandset(&ValidFlags, evbMhz);
	}

	if (OffsetPrcbCpuType + sizeof(char) <= SizePrcb)
	{
		_bittestandset(&ValidFlags, evbCpuType);
	}

	if (
		(OffsetPrcbVendorString + 0xd <= SizePrcb) && 
		(OffsetPrcbVendorString < OffsetPrcbVendorString + 0xd)
		)
	{
		_bittestandset(&ValidFlags, evbVendorString);
	}

	if (
		!(OffsetPrcbNumber & (__alignof(ULONG) - 1)) && 
		(OffsetPrcbNumber + sizeof(ULONG) <= SizePrcb)
		)
	{
		_bittestandset(&ValidFlags, evbNumber);
	}

	if (
		!(OffsetPcrSelfPcr & ptr_align) && 
		(OffsetPcrSelfPcr + ptr_size <= SizePcr)
		)
	{
		_bittestandset(&ValidFlags, evSelfPcr);
	}

	if (
		!(OffsetPcrCurrentPrcb & ptr_align) && 
		(OffsetPcrCurrentPrcb + ptr_size <= SizePcr)
		)
	{
		_bittestandset(&ValidFlags, evCurrentPrcb);
	}

	if (
		!(OffsetPcrInitialStack & ptr_align) && 
		(OffsetPcrInitialStack + ptr_size <= SizePcr)
		)
	{
		_bittestandset(&ValidFlags, evInitialStack);
	}

	if (
		!(OffsetPcrStackLimit & ptr_align) && 
		(OffsetPcrStackLimit + ptr_size <= SizePcr)
		)
	{
		_bittestandset(&ValidFlags, evStackLimit);
	}

	// Windows 7 addition

	if (Size >= FIELD_OFFSET(KDDEBUGGER_DATA64, OffsetPrcbMaxBreakpoints))
	{
		if (
			!(OffsetPrcbContext & ptr_align) && 
			(OffsetPrcbContext + ptr_size <= SizePrcb)
			)
		{
			_bittestandset(&ValidFlags, evbContext);
		}

	}
	// Windows 8.1 Addition

	if (Size >= FIELD_OFFSET(KDDEBUGGER_DATA64, OffsetKPriQueueThreadListHead))
	{
		if (
			!(OffsetPrcbIdleThread & ptr_align) && 
			(OffsetPrcbIdleThread + ptr_size <= SizePrcb)
			)
		{
			_bittestandset(&ValidFlags, evbIdleThread);
		}

		if (
			!(OffsetPrcbDpcStack & ptr_align) && 
			(OffsetPrcbDpcStack + ptr_size <= SizePrcb)
			)
		{
			_bittestandset(&ValidFlags, evbDpcStack);
		}

		if (
			!(OffsetPrcbIsrStack & ptr_align) && 
			(OffsetPrcbIsrStack + ptr_size <= SizePrcb)
			)
		{
			_bittestandset(&ValidFlags, evbIsrStack);
		}
	}

	PKPCR Pcr = (PKPCR)alloca(SizePcr);
	PVOID Pcrb = RtlOffsetToPointer(Pcr, OffsetPcrContainedPrcb);

	SIZE_T rcb;

	do 
	{
		ULONG_PTR PcrbAddr = 
#ifdef _WIN64
			Is64Bit ? *KiProcessorBlock.p64++ : 
#endif
			*KiProcessorBlock.p32++;

		ULONG_PTR PcrAddr = PcrbAddr - OffsetPcrContainedPrcb;

		lprintf(hwndLog, L"\r\n========== KPCR at %p KPRCB at %p\r\n", PcrAddr, PcrbAddr);

		if (0 <= pDump->ReadVirtual((void*)PcrAddr, Pcr, SizePcr, &rcb))
		{
			if (_bittest(&ValidFlags, evbNumber))
			{
				lprintf(hwndLog, L"#%x\r\n", *(ULONG**)RtlOffsetToPointer(Pcrb, OffsetPrcbNumber));
			}

			if (_bittest(&ValidFlags, evbCpuType))
			{
				lprintf(hwndLog, L"%x\r\n", *(CHAR**)RtlOffsetToPointer(Pcrb, OffsetPrcbCpuType));
			}

			if (_bittest(&ValidFlags, evbMhz))
			{
				lprintf(hwndLog, L"%u Mhz\r\n", *(ULONG**)RtlOffsetToPointer(Pcrb, OffsetPrcbMhz));
			}

			if (_bittest(&ValidFlags, evbVendorString))
			{
				lprintf(hwndLog, L"%.13S\r\n", RtlOffsetToPointer(Pcrb, OffsetPrcbVendorString));
			}

			if (_bittest(&ValidFlags, evbDpcRoutine))
			{
				lprintf(hwndLog, L"DpcRoutineActive = %x\r\n", *(UCHAR**)RtlOffsetToPointer(Pcrb, OffsetPrcbDpcRoutineActive));
			}

			ULONG_PTR CurrentThread = 0, IdleThread = 0;

			CONTEXT ctx, *pCtx = 0;

#ifdef _WIN64
			if (Is64Bit)
			{
#endif
				if (_bittest(&ValidFlags, evbContext))
				{
					if (pCtx = *(CONTEXT**)RtlOffsetToPointer(Pcrb, OffsetPrcbContext))
					{
						if (0 > pDump->ReadVirtual(pCtx, &ctx, sizeof(ctx), 0))
						{
							pCtx = 0;
						}
					}
				}

				if (_bittest(&ValidFlags, evSelfPcr))
				{
					lprintf(hwndLog, L"SelfPcr = %p\r\n", *(void**)RtlOffsetToPointer(Pcr, OffsetPcrSelfPcr));
				}

				if (_bittest(&ValidFlags, evCurrentPrcb))
				{
					lprintf(hwndLog, L"CurrentPrcb = %p\r\n", *(void**)RtlOffsetToPointer(Pcr, OffsetPcrCurrentPrcb));
				}

				if (_bittest(&ValidFlags, evbIdleThread))
				{
					lprintf(hwndLog, L"IdleThread = %p\r\n", IdleThread = *(ULONG_PTR*)RtlOffsetToPointer(Pcrb, OffsetPrcbIdleThread));

					if (!IsKernelPointerAddress(IdleThread ))
					{
						IdleThread = 0;
					}
				}

				if (_bittest(&ValidFlags, evbCurrentThread))
				{
					lprintf(hwndLog, L"CurrentThread = %p\r\n", CurrentThread = *(ULONG_PTR*)RtlOffsetToPointer(Pcrb, OffsetPrcbCurrentThread));

					if (!IsKernelPointerAddress(CurrentThread))
					{
						CurrentThread = 0;
					}
				}

				if (_bittest(&ValidFlags, evInitialStack))
				{
					lprintf(hwndLog, L"InitialStack = %p\r\n", *(void**)RtlOffsetToPointer(Pcr, OffsetPcrInitialStack));
				}

				if (_bittest(&ValidFlags, evStackLimit))
				{
					lprintf(hwndLog, L"StackLimit = %p\r\n", *(void**)RtlOffsetToPointer(Pcr, OffsetPcrStackLimit));
				}

				if (_bittest(&ValidFlags, evbDpcStack))
				{
					lprintf(hwndLog, L"DpcStack = %p\r\n", *(void**)RtlOffsetToPointer(Pcrb, OffsetPrcbDpcStack));
				}

				if (_bittest(&ValidFlags, evbIsrStack))
				{
					lprintf(hwndLog, L"IsrStack = %p\r\n", *(void**)RtlOffsetToPointer(Pcrb, OffsetPrcbIsrStack));
				}

				if (pCtx)
				{
					if (!memcmp(pDump->GetContextRecord(), &ctx, sizeof(CONTEXT)))
					{
						if (CurrentThread)
						{
							pDump->set_Thread((PVOID)CurrentThread);

							lprintf(hwndLog, L"==== bug on Thread = %p !!\r\n", CurrentThread);

							PVOID Thread = alloca(SizeEThread);

							if (0 <= pDump->ReadVirtual((PVOID)CurrentThread, Thread, SizeEThread, 0))
							{
								if (_bittest(&ValidFlags, evKThreadApcProcess))
								{
									ULONG_PTR CurrentProcess = *(ULONG_PTR*)RtlOffsetToPointer(Thread, OffsetKThreadApcProcess);

									if (CurrentProcess >= 0xFFFF800000000000 && !(CurrentProcess & 7))
									{
										pDump->set_Process((void*)CurrentProcess);

										lprintf(hwndLog, L"EPROCESS = %p\n", CurrentProcess);

										PVOID Process = alloca(SizeEProcess);

										if (0 <= pDump->ReadVirtual((PVOID)CurrentProcess, Process, SizeEProcess, 0))
										{
											if (_bittest(&ValidFlags, evEprocessParentCID))
											{
												PCLIENT_ID cid = (PCLIENT_ID)RtlOffsetToPointer(Process, OffsetEprocessParentCID);
												lprintf(hwndLog, L"pid = %p\r\n", cid->UniqueProcess);
											}
										}
									}
								}
							}
						}
					}

#ifdef _WIN64
					ZMemoryDump64
#else
					ZMemoryDump32 
#endif

						::_DumpContext(hwndLog, &ctx);
				}
			}
#ifdef _WIN64
			else
			{
				WOW64_CONTEXT* pWowCtx, wowctx;

				if (_bittest(&ValidFlags, evbContext))
				{
					if (pWowCtx = (WOW64_CONTEXT*)*(ULONG*)RtlOffsetToPointer(Pcrb, OffsetPrcbContext))
					{
						if (0 <= pDump->ReadVirtual(pWowCtx, &wowctx, sizeof(wowctx), 0))
						{
							CopyWowContext(pCtx = &ctx, &wowctx);
						}
					}
				}

				if (_bittest(&ValidFlags, evSelfPcr))
				{
					lprintf(hwndLog, L"SelfPcr = %x\r\n", *(ULONG**)RtlOffsetToPointer(Pcr, OffsetPcrSelfPcr));
				}

				if (_bittest(&ValidFlags, evCurrentPrcb))
				{
					lprintf(hwndLog, L"CurrentPrcb = %x\r\n", *(ULONG**)RtlOffsetToPointer(Pcr, OffsetPcrCurrentPrcb));
				}

				if (_bittest(&ValidFlags, evbIdleThread))
				{
					lprintf(hwndLog, L"IdleThread = %x\r\n", IdleThread = *(ULONG*)RtlOffsetToPointer(Pcrb, OffsetPrcbIdleThread));

					if (!IsKernelPointerAddress32((ULONG)IdleThread))
					{
						IdleThread = 0;
					}
				}

				if (_bittest(&ValidFlags, evbCurrentThread))
				{
					lprintf(hwndLog, L"CurrentThread = %x\r\n", CurrentThread = *(ULONG*)RtlOffsetToPointer(Pcrb, OffsetPrcbCurrentThread));

					if (!IsKernelPointerAddress32((ULONG)CurrentThread))
					{
						CurrentThread = 0;
					}
				}

				if (_bittest(&ValidFlags, evInitialStack))
				{
					lprintf(hwndLog, L"InitialStack = %x\r\n", *(ULONG**)RtlOffsetToPointer(Pcr, OffsetPcrInitialStack));
				}

				if (_bittest(&ValidFlags, evStackLimit))
				{
					lprintf(hwndLog, L"StackLimit = %x\r\n", *(ULONG**)RtlOffsetToPointer(Pcr, OffsetPcrStackLimit));
				}

				if (_bittest(&ValidFlags, evbDpcStack))
				{
					lprintf(hwndLog, L"DpcStack = %x\r\n", *(ULONG**)RtlOffsetToPointer(Pcrb, OffsetPrcbDpcStack));
				}

				if (_bittest(&ValidFlags, evbIsrStack))
				{
					lprintf(hwndLog, L"IsrStack = %x\r\n", *(ULONG**)RtlOffsetToPointer(Pcrb, OffsetPrcbIsrStack));
				}

				if (pCtx)
				{
					if (!memcmp(pDump->GetContextRecord(), &ctx, sizeof(CONTEXT)))
					{
						if (CurrentThread)
						{
							pDump->set_Thread((PVOID)CurrentThread);

							lprintf(hwndLog, L"==== bug on Thread = %x !!\r\n", CurrentThread);

							PVOID Thread = alloca(SizeEThread);

							if (0 <= pDump->ReadVirtual((PVOID)CurrentThread, Thread, SizeEThread, 0))
							{
								if (_bittest(&ValidFlags, evKThreadApcProcess))
								{
									ULONG CurrentProcess = *(ULONG*)RtlOffsetToPointer(Thread, OffsetKThreadApcProcess);

									if (IsKernelPointerAddress32(CurrentProcess))
									{
										pDump->set_Process((void*)(ULONG_PTR)CurrentProcess);

										lprintf(hwndLog, L"EPROCESS = %x\n", CurrentProcess);

										PVOID Process = alloca(SizeEProcess);

										if (0 <= pDump->ReadVirtual((PVOID)(ULONG_PTR)CurrentProcess, Process, SizeEProcess, 0))
										{
											if (_bittest(&ValidFlags, evEprocessParentCID))
											{
												struct CLIENT_ID32 {
													ULONG UniqueProcess, UniqueThread;
												};
												CLIENT_ID32* cid = (CLIENT_ID32*)RtlOffsetToPointer(Process, OffsetEprocessParentCID);
												lprintf(hwndLog, L"pid = %x\r\n", cid->UniqueProcess);
											}
										}
									}
								}
							}
						}
					}
					ZMemoryDump32::_DumpContext(hwndLog, &ctx);
				}
			}
		}
#endif
	} while (--NumberProcessors);

	return TRUE;
}

void _DumpDebuggerData(HWND hwndLog, IMemoryDump* pDump, KDDEBUGGER_DATA64& KdDebuggerDataBlock)
{
	lprintf(hwndLog, L"Kernel Base = %p\r\n", KdDebuggerDataBlock.KernBase);

	pDump->set_KernBase((PVOID)(ULONG_PTR)KdDebuggerDataBlock.KernBase);

	CHAR sz[256];
	if (0 <= pDump->ReadVirtual((PVOID)KdDebuggerDataBlock.NtBuildLab, sz, sizeof(sz), 0))
	{
		sz[255] = 0;
		lprintf(hwndLog, L"Built by: %S\r\n", sz);
	}

	PVOID KiProcessorBlockAddr = (PVOID)(ULONG_PTR)KdDebuggerDataBlock.KiProcessorBlock;
	
	lprintf(hwndLog, L"KiProcessorBlock at %p\r\n", KiProcessorBlockAddr);

	if (ULONG NumberProcessors = pDump->get_NumberProcessors())
	{
		if (NumberProcessors > 1024)
		{
			NumberProcessors = 1024;
		}

#ifdef _WIN64
		BOOL Is64Bit = pDump->Is64Bit();
#endif

		ULONG size = NumberProcessors * (
#ifdef _WIN64
			Is64Bit ? 8 :
#endif
			4);

		U32_64 KiProcessorBlock = { alloca(size) };

		SIZE_T rcb;

		if (0 <= pDump->ReadVirtual(KiProcessorBlockAddr, KiProcessorBlock.pv, size, &rcb))
		{
			if (!DumpDebuggerDataEx(hwndLog, pDump, KdDebuggerDataBlock, KiProcessorBlock, NumberProcessors))
			{
				int i = 0;
				do 
				{
					ULONG_PTR kpcrb = 
#ifdef _WIN64
						Is64Bit ? *KiProcessorBlock.p64++ : 
#endif
						*KiProcessorBlock.p32++;
					lprintf(hwndLog, L"========== KPRCB #%u at %p\r\n", i++, kpcrb);
				} while (--NumberProcessors);
			}
		}
	}
}

struct ZType;

BOOL GetStruct(PVOID UdtCtx, PCSTR TypeName, ZType** ppti, ULONG* pSize);
BOOL GetFieldOffset(ZType* pti, PCSTR FieldName, ULONG& ofs);

PWSTR BuildProcessListWow(PWSTR lpsz, 
						  IMemoryDump* pDump, 
						  PVOID PsActiveProcessHeadAddr,
						  ULONG SizeEProcess, 
						  ULONG UniqueProcessId, 
						  ULONG ActiveProcessLinks, 
						  ULONG ImageFileName,
						  ULONG InheritedFromUniqueProcessId)
{
	if (
		UniqueProcessId + sizeof(DWORD) >= SizeEProcess ||
		InheritedFromUniqueProcessId + sizeof(DWORD) >= SizeEProcess ||
		ActiveProcessLinks + sizeof(LIST_ENTRY32) >= SizeEProcess
		)
	{
		return lpsz;
	}

	ULONG max = 256;
	PVOID Process = alloca(SizeEProcess), ProcessAddr;
	PLIST_ENTRY32 pActiveProcessLinks = (PLIST_ENTRY32)RtlOffsetToPointer(Process, ActiveProcessLinks);
	ULONG ProcessCount = 0;

	if (0 <= pDump->ReadVirtual(PsActiveProcessHeadAddr, pActiveProcessLinks, sizeof(LIST_ENTRY32)))
	{
		max = 256;

		PVOID EntryAddr, BlinkAddr = PsActiveProcessHeadAddr;

		while ((EntryAddr = ULongToPtr(pActiveProcessLinks->Flink)) != PsActiveProcessHeadAddr)
		{
			ProcessAddr = (PBYTE)EntryAddr - ActiveProcessLinks;

			if (0 > pDump->ReadVirtual(ProcessAddr, Process, SizeEProcess))
			{
				break;
			}

			if (ULongToPtr(pActiveProcessLinks->Blink) != BlinkAddr)
			{
				break;
			}

			BlinkAddr = EntryAddr;

			lpsz += swprintf(lpsz, L"%02u %x %x(%x) %S\r\n", ProcessCount++, 
				PtrToUlong(ProcessAddr), 
				*(ULONG*)RtlOffsetToPointer(Process, UniqueProcessId),
				*(ULONG*)RtlOffsetToPointer(Process, InheritedFromUniqueProcessId),
				RtlOffsetToPointer(Process, ImageFileName)
				);

			if (!max--)
			{
				break;
			}
		}
	}

	return lpsz;
}

PWSTR BuildProcessList(PWSTR lpsz, IMemoryDump* pDump, PVOID UdtCtx)
{
	struct ZType* pti;
	ULONG SizeEProcess, UniqueProcessId, ActiveProcessLinks, ImageFileName, InheritedFromUniqueProcessId;
	PVOID PsActiveProcessHeadAddr = pDump->PsActiveProcessHead();

	if (
		!PsActiveProcessHeadAddr ||
		!GetStruct(UdtCtx, "_EPROCESS", &pti, &SizeEProcess) ||
		SizeEProcess >= MAXUSHORT ||
		!GetFieldOffset(pti, "UniqueProcessId", UniqueProcessId) ||
		!GetFieldOffset(pti, "ActiveProcessLinks", ActiveProcessLinks) ||
		!GetFieldOffset(pti, "ImageFileName", ImageFileName) ||
		!GetFieldOffset(pti, "InheritedFromUniqueProcessId", InheritedFromUniqueProcessId) ||
		ImageFileName + 16 >= SizeEProcess
		)
	{
		return lpsz;
	}

	if (!pDump->Is64Bit())
	{
		return BuildProcessListWow(lpsz, pDump, PsActiveProcessHeadAddr,
			SizeEProcess, 
			UniqueProcessId, 
			ActiveProcessLinks, 
			ImageFileName,
			InheritedFromUniqueProcessId);
	}

	if (
		UniqueProcessId + sizeof(PVOID) >= SizeEProcess ||
		InheritedFromUniqueProcessId + sizeof(PVOID) >= SizeEProcess ||
		ActiveProcessLinks + sizeof(LIST_ENTRY) >= SizeEProcess
		)
	{
		return lpsz;
	}

	ULONG max = 256;
	PVOID Process = alloca(SizeEProcess), ProcessAddr;
	PLIST_ENTRY pActiveProcessLinks = (PLIST_ENTRY)RtlOffsetToPointer(Process, ActiveProcessLinks);
	ULONG ProcessCount = 0;

	if (0 <= pDump->ReadVirtual(PsActiveProcessHeadAddr, pActiveProcessLinks, sizeof(LIST_ENTRY)))
	{
		max = 256;

		PVOID EntryAddr, BlinkAddr = PsActiveProcessHeadAddr;

		while ((EntryAddr = pActiveProcessLinks->Flink) != PsActiveProcessHeadAddr)
		{
			ProcessAddr = (PBYTE)EntryAddr - ActiveProcessLinks;

			if (0 > pDump->ReadVirtual(ProcessAddr, Process, SizeEProcess))
			{
				break;
			}

			if (pActiveProcessLinks->Blink != BlinkAddr)
			{
				break;
			}

			BlinkAddr = EntryAddr;

			lpsz += swprintf(lpsz, L"%02u %p %I64x(%I64x) %S\r\n", ProcessCount++, ProcessAddr, 
				*(ULONG64*)RtlOffsetToPointer(Process, UniqueProcessId),
				*(ULONG64*)RtlOffsetToPointer(Process, InheritedFromUniqueProcessId),
				RtlOffsetToPointer(Process, ImageFileName)
				);

			if (!max--)
			{
				break;
			}
		}
	}

	return lpsz;
}

_NT_END