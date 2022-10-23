#pragma once

typedef BYTE (*PAGE)[PAGE_SIZE];

#ifdef _WIN64

struct LDR_DATA_TABLE_ENTRY32 {
	/*0000*/ LIST_ENTRY32 InLoadOrderLinks;
	/*0010*/ LIST_ENTRY32 InMemoryOrderLinks;
	/*0020*/ LIST_ENTRY32 InInitializationOrderLinks;
	/*0030*/ ULONG DllBase;
	/*0038*/ ULONG EntryPoint;
	/*0040*/ ULONG SizeOfImage;
	/*0048*/ UNICODE_STRING32 FullDllName;
	/*0058*/ UNICODE_STRING32 BaseDllName;
	/*0068*/ ULONG Flags;
};

#else
#define LDR_DATA_TABLE_ENTRY32 _LDR_DATA_TABLE_ENTRY
#endif

class __declspec(novtable) IMemoryDump : protected CONTEXT, protected EXCEPTION_RECORD
{
protected:
	virtual NTSTATUS VirtualToPhysical(ULONG_PTR Addr, PULONG pPfn) = 0;
	virtual NTSTATUS ReadPhysicalPage(ULONG Pfn, PAGE* pPage) = 0;

	LARGE_INTEGER _SystemTime;
	LARGE_INTEGER _SystemUpTime;

	LARGE_INTEGER _DumpBlobOffset;
	LARGE_INTEGER _DumpBlobSize;

	PVOID _KernBase;
	PVOID _Process;
	PVOID _Thread;

	ULONG_PTR _BugCheckParameter[4];
	ULONG_PTR _PsLoadedModuleList;
	ULONG_PTR _PsActiveProcessHead;
	ULONG_PTR _KdDebuggerDataBlock;

	LONG _dwRef;

	ULONG _dwBuildNumber;
	ULONG _DirectoryTablePfn;
	ULONG _BugCheckCode;

	ULONG _MachineImageType;
	ULONG _NumberProcessors;

	USHORT _oPfn;
	BOOLEAN _PaeEnabled;
	BOOLEAN _Is64Bit;

	IMemoryDump()
	{
		_dwRef = 1;
		_KernBase = 0;
		_Process = 0;
		_Thread = 0;
	}

	virtual ~IMemoryDump() {}

public:

	void AddRef()
	{
		InterlockedIncrement(&_dwRef);
	}

	void Release()
	{
		if (!InterlockedDecrement(&_dwRef)) delete this;
	}

	ULONG get_BugCheckCode()
	{
		return _BugCheckCode;
	}

	ULONG_PTR BugCheckParameter(ULONG i)
	{
		return _BugCheckParameter[i];
	}

	PVOID PsLoadedModuleList()
	{
		return (PVOID)_PsLoadedModuleList;
	}

	PVOID PsActiveProcessHead()
	{
		return (PVOID)_PsActiveProcessHead;
	}

	PVOID KdDebuggerDataBlock()
	{
		return (PVOID)_KdDebuggerDataBlock;
	}

	CONTEXT* GetContextRecord()
	{
		return this;
	}

	EXCEPTION_RECORD* GetExceptionRecord()
	{
		return this;
	}

	void DumpExceptionRecord(HWND hwndLog)
	{
		_DumpExceptionRecord(hwndLog, this);
	}

	static void _DumpExceptionRecord(HWND hwndLog, PEXCEPTION_RECORD per);

	BOOLEAN Is64Bit()
	{
		return _Is64Bit;
	}

	virtual void UpdateContext() = 0;

	virtual void DumpContext(HWND hwndLog) = 0;

	virtual void EnumTags(HWND hwndLog) = 0;

	NTSTATUS ReadVirtual(PVOID Addr, PVOID buf, SIZE_T cb, PSIZE_T pcb = 0);

	void DumpDebuggerData(HWND hwndLog);

	void SetDirectoryTableBase(ULONG64 DirectoryTableBase)
	{
		_DirectoryTablePfn = (ULONG)(DirectoryTableBase >> PAGE_SHIFT);
		_oPfn = ((ULONG)DirectoryTableBase & (PAGE_SIZE - 1)) >> 3;
	}

	ULONG get_MachineImageType()
	{
		return _MachineImageType;
	}

	ULONG get_NumberProcessors()
	{
		return _NumberProcessors;
	}

	PVOID get_KernBase()
	{
		return _KernBase;
	}

	void set_KernBase(PVOID KernBase)
	{
		_KernBase = KernBase;
	}

	PVOID get_Process()
	{
		return _Process;
	}

	void set_Process(PVOID Process)
	{
		_Process = Process;
	}

	PVOID get_Thread()
	{
		return _Thread;
	}

	void set_Thread(PVOID Thread)
	{
		_Thread = Thread;
	}

	void set_DumpBlobOffset(ULONG64 DumpBlobOffset)
	{
		_DumpBlobOffset.QuadPart = DumpBlobOffset;
	}

	void set_DumpBlobSize(ULONG64 DumpBlobSize)
	{
		_DumpBlobSize.QuadPart = DumpBlobSize;
	}

	ULONG64 get_DumpBlobSize()
	{
		return _DumpBlobSize.QuadPart;
	}
};

NTSTATUS OpenDump(HWND hwndLog, POBJECT_ATTRIBUTES poa, IMemoryDump** ppDump);

void lprintf(HWND hwnd, PCWSTR format, ...);