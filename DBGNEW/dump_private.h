#pragma once

#include "memdump.h"

//#undef DbgPrint

class ZFileCache
{
	enum{
		pages_count = MAXUCHAR + 1
	};
protected:
	HANDLE _hFile;
	ULONG _PagesOffset;
private:
	union
	{
		PVOID _Buffer;
		PAGE _Pages;
	};
	ULONG _iPages[pages_count];
	UCHAR _index;

protected:
	virtual ~ZFileCache();
	ZFileCache();

public:

	void set_PagesOffset(ULONG PagesOffset)
	{
		_PagesOffset = PagesOffset;
	}

	NTSTATUS Create();
	NTSTATUS ReadPage(ULONG iPage, PAGE* ppPage);
	NTSTATUS ReadData(PLARGE_INTEGER offset, PVOID buf, ULONG cb, PULONG pcb = 0);
};

class __declspec(novtable) ZMemoryDump : public IMemoryDump, public ZFileCache
{
	virtual void EnumTags(HWND hwndLog);
};

class __declspec(novtable) ZMemoryDump32 : public ZMemoryDump
{
	virtual NTSTATUS VirtualToPhysical(ULONG_PTR Addr, PULONG pPfn);
	virtual void UpdateContext();

public:

	static void _DumpContext(HWND hwndLog, CONTEXT* ctx);

public:

	void Init(HANDLE hFile, DUMP_HEADER32& md);

	virtual void DumpContext(HWND hwndLog)
	{
		_DumpContext(hwndLog, this);
	}

};

#ifdef _WIN64

class __declspec(novtable) ZMemoryDump64 : public ZMemoryDump
{
	virtual NTSTATUS VirtualToPhysical(ULONG_PTR Addr, PULONG pPfn);
	virtual void UpdateContext();

public:

	static void _DumpContext(HWND hwndLog, CONTEXT* ctx);

	void Init(HANDLE hFile, DUMP_HEADER64& md);

	virtual void DumpContext(HWND hwndLog)
	{
		_DumpContext(hwndLog, this);
	}
};
#endif

struct __declspec(novtable) CBitmapDump : RTL_BITMAP 
{
	PULONG _NumBitsSet;

	CBitmapDump()
	{
		Buffer = 0;
		_NumBitsSet = 0;
	}

	virtual ~CBitmapDump()
	{
		if (Buffer) delete Buffer;
	}

	ULONG PhysicalPageToIndex(ULONG Pfn);

	NTSTATUS Validate(HWND hwndLog, HANDLE hFile, COMMON_BITMAP_DUMP& bd, ZMemoryDump* pDump);
};

#ifdef _AMD64_

struct CBitmapDump64 : public CBitmapDump, public ZMemoryDump64
{
	virtual NTSTATUS ReadPhysicalPage(ULONG Pfn, PAGE* pPage)
	{
		return ReadPage(PhysicalPageToIndex(Pfn), pPage);
	}

	NTSTATUS Validate(HWND hwndLog, HANDLE hFile, MEMORY_DUMP64& md, COMMON_BITMAP_DUMP& bd);
};

struct CFullDump64 : public ZMemoryDump64
{
	ULONG _NumberOfRuns;
	PHYSICAL_MEMORY_RUN64 _Run[];

	ULONG PhysicalPageToIndex(ULONG Pfn);

	virtual NTSTATUS ReadPhysicalPage(ULONG Pfn, PAGE* pPage)
	{
		return ReadPage(PhysicalPageToIndex(Pfn), pPage);
	}

	NTSTATUS Validate(HWND hwndLog, MEMORY_DUMP64& md);
public:
	void* operator new(size_t , ULONG NumberOfRuns)
	{
		return ::operator new(FIELD_OFFSET(CFullDump64, _Run[NumberOfRuns]));
	}

	void operator delete(PVOID pv)
	{
		::operator delete(pv);
	}
};

#endif

struct CBitmapDump32 : public CBitmapDump, public ZMemoryDump32
{
	virtual NTSTATUS ReadPhysicalPage(ULONG Pfn, PAGE* pPage)
	{
		return ReadPage(PhysicalPageToIndex(Pfn), pPage);
	}

	NTSTATUS Validate(HWND hwndLog, HANDLE hFile, MEMORY_DUMP32& md, COMMON_BITMAP_DUMP& bd);
};

struct CFullDump32 : public ZMemoryDump32
{
	ULONG _NumberOfRuns;
	PHYSICAL_MEMORY_RUN32 _Run[];

	ULONG PhysicalPageToIndex(ULONG Pfn);

	virtual NTSTATUS ReadPhysicalPage(ULONG Pfn, PAGE* pPage)
	{
		return ReadPage(PhysicalPageToIndex(Pfn), pPage);
	}

	NTSTATUS Validate(HWND hwndLog, MEMORY_DUMP32& md);
public:
	void* operator new(size_t , ULONG NumberOfRuns)
	{
		return ::operator new(FIELD_OFFSET(CFullDump32, _Run[NumberOfRuns]));
	}

	void operator delete(PVOID pv)
	{
		::operator delete(pv);
	}
};
