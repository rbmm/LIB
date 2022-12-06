#pragma once

#include "../winZ/object.h"

class ZMemoryCache;
class ZDbgDoc;
class ZModulesDlg;
class ZSymbolsDlg;
class ZForwardDlg;
class PdbReader;
struct CV_DebugSSubsectionHeader_t;
struct SC;
struct SYM_HEADER;
struct ANNOTATIONSYM;
struct PUBSYM32;
struct DbiModuleInfo;
struct DbiSecCon;
struct FileNamesHelper;
struct CV_DebugSFile;

union SYM {
	PVOID pv;
	PBYTE pb;
	SYM_HEADER* psh;
	ANNOTATIONSYM* pas;
	PUBSYM32* pbs;

	SYM(PVOID p)
	{
		pv = p;
	}
};

#define FLAG_FORWARD	0x80000000
#define FLAG_ORDINAL	0x80000000
#define FLAG_NOEXPORT	0x40000000
#define FLAG_RVAEXPORT	0x20000000
#define NANE_OFS(ofs)	((ofs) & 0x0fffffff)
#define NAME_FLAGS(ofs) ((ofs) & 0x60000000)

struct RVAOFS
{
	DWORD rva, ofs;
	
	static int __cdecl compare(RVAOFS& a, RVAOFS& b);
	static int __cdecl compare_nf(RVAOFS& a, RVAOFS& b);
};

struct MD;

struct LINE_INFO
{
	ULONG Rva, line : 20, len : 12;

	static int __cdecl compare(LINE_INFO& a, LINE_INFO& b);
};

class ZDll : public ZObject, public LIST_ENTRY
{
	friend ZDbgDoc;
	friend ZModulesDlg;
	friend ZSymbolsDlg;
	friend ZForwardDlg;

	PVOID _EntryPoint;
	PVOID _BaseOfDll;
	PWSTR _ImageName;
	PWSTR _ImagePath;
	PSTR _pFileNames;
	
	union {
		RVAOFS *_pSymbols;
		PSTR _szSymbols;
	};
	
	union {
		RVAOFS *_pForwards;
		PSTR _szForwards;
	};

	CV_DebugSSubsectionHeader_t** _pMI;// [_nMI]
	SC* _pSC; //[_nSC] { [offset, offset + size) -> _pMI[module] -> CV_DebugSSubsectionHeader_t*(CV_DebugSLinesHeader_t) }

#ifdef _WIN64
	union{
		PRUNTIME_FUNCTION _prtf;
		DWORD _tmpRtfRVA;
	};
	union{
		DWORD _nrtf;
		DWORD _tmpRtfSize;
	};
#endif

	union{
		DWORD _nForwards;
		DWORD _tmpExpSize;
	};

	union{
		DWORD _nSymbols;
		DWORD _tmpExpRVA;
	};

	union {
		ULONG _nMI;
		ULONG _tmpIddRva;
	};

	union {
		ULONG _nSC;
		ULONG _tmpIddSize;
	};

	DWORD _SizeOfImage;
	DWORD _index;
	DWORD _dllId;

	union
	{
		LONG _Flags;
		struct  
		{
			ULONG _IsParsed : 1;
			ULONG _NotAtBase : 1;
			ULONG _Is64Bit : 1;
			ULONG _RtfPresent : 1;
			ULONG _RtfParced : 1;
			ULONG _IsUnloaded : 1;
			ULONG _IsSystem : 1;
			ULONG _DontSearchSrc : 1;
			ULONG _SpareBits : 24;
		};
	};
	
	NTSTATUS LoadPublicSymbols(PdbReader* pdb, SHORT symrecStream, ULONG expLen);
	
	NTSTATUS LoadPublicSymbols(PdbReader* pdb, PVOID stream, ULONG size, ULONG expLen);
	
	ULONG LoadSymbols(PdbReader* pdb,
		PULONG pstr_len,
		PVOID stream, 
		ULONG size, 
		MD& md, 
		RVAOFS* pSymbols, 
		ULONG nSymbols, 
		ULONG nSpace, 
		BOOL bSecondLoop);
	
	NTSTATUS LoadModuleInfo(PdbReader* pdb);
	
	NTSTATUS LoadSC(PdbReader* pdb);
	
	ULONG GetLineByRVA(ULONG rva, CV_DebugSSubsectionHeader_t* pHeader, CV_DebugSFile** ppFile);

	void ProcessExport(ZDbgDoc* pDoc, PVOID BaseOfDll, PULONG pLen);

	void ProcessExport(PIMAGE_EXPORT_DIRECTORY pied, DWORD ExpSize, DWORD ExpRVA, PULONG pLen);

	void DeleteLineInfo();

	int IsRvaExported(ULONG rva, PCSTR name);

	void Parse(ZDbgDoc* pDoc);

public:

	BOOL get_DontSearchSrc() { return _DontSearchSrc; }

	void set_DontSearchSrc(BOOL b) { _DontSearchSrc = b != 0; }

	PCWSTR path() { return _ImagePath; }
	
	PCWSTR name() { return _ImageName; }

	BOOL Is64Bit() { return _Is64Bit; }
	
	BOOL IsUnloaded() { return _IsUnloaded; }

	void Unload();

	BOOL Load(ZDbgDoc* pDoc, PDBGKM_LOAD_DLL LoadDll);

	BOOL Load2(ZDbgDoc* pDoc, DWORD hash);

	PCSTR GetFileName(CV_DebugSFile* pFile);

	ULONG GetLineByVA(PVOID Va, CV_DebugSFile** ppFile);

	LINE_INFO* GetSrcLines(CV_DebugSFile* pFile, PULONG pN);

	PVOID getVaByName(PCSTR name, ZDbgDoc* pDoc);

	PCSTR getNameByVa(PVOID Va, PULONG pFlags);
	
	PCSTR getNameByVaEx(PVOID Va, PINT_PTR pVa, PULONG Index);

	PCSTR getNameByVa2(PVOID Va, PINT_PTR pVa);

	PCSTR getNameByIndex(ULONG Index, PINT_PTR pVa, PULONG pFlags);

	BOOL InRange(PVOID Va) { return _IsUnloaded ? FALSE : RtlPointerToOffset(_BaseOfDll, Va) < _SizeOfImage; }
	
	BOOL GetRange(void** pLo, void** pHi) { *pLo = _BaseOfDll, *pHi = RtlOffsetToPointer(_BaseOfDll, _SizeOfImage); return !_IsUnloaded; }

	BOOL VaInImage(PVOID Va) { return (ULONG_PTR)Va - (ULONG_PTR)_BaseOfDll < _SizeOfImage; }

	DWORD getID() { return _dllId; }

	PVOID getBase(){ return _BaseOfDll; }

	DWORD getSize(){ return _SizeOfImage; }

#ifdef _WIN64
	void CreateRTF(ZDbgDoc* pDoc);
	void CreateRTF(ZDbgDoc* pDoc, PRUNTIME_FUNCTION pRT, DWORD Size, DWORD Va);
	BOOL DoUnwind(INT_PTR Va, CONTEXT& ctx, ZDbgDoc* pDoc);
#endif

	ZDll(DWORD index);

	~ZDll();

	static int Compare(ZDll* p, ZDll* q, int iSubItem, int sortOrder);

	static inline HMODULE _hmod_nt = GetModuleHandleW(L"ntdll");
	static inline PVOID LdrpDispatchUserCallTarget = 0;
	static inline PVOID LdrpDispatchUserCallTargetES = 0;
};