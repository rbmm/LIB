#pragma once
#include "../winz/window.h"
#include "common.h"
#include "resource.h"

class CTokenDlg : public ZDlg 
{
	void OnOk(HWND hwndDlg);
	virtual INT_PTR DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

class CCalcDlg : public ZDlg, public CUILayot 
{
	void OnChange(HWND hwnd, HWND hwndRes);
	virtual INT_PTR DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

class CFileInUseDlg : public ZDlg, public CUILayot 
{
	virtual INT_PTR DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

class CRvaToOfs : public ZDlg, public CUILayot 
{
	PULONG _pofs = 0;
	ULONG _cxMin;
	BOOLEAN _bRvaToOfs;

	void Convert(HWND hwndDlg);
	virtual INT_PTR DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

	void ShowOfsForModule(HWND hwndCbModules, HWND hwndOfs);
	void OnPaste(HWND hwndCB, HWND hwndOfs, PWSTR psz, PWSTR end);
	void OnJmp(HWND hwndDlg);
};

class CSymbolsDlg : public ZDlg, public CUILayot 
{
	BOOL OnOK(HWND hwnd);
	virtual INT_PTR DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

class ZDbgDoc;
class ZDll;
struct ZBreakPoint;

class ZPDBPathDlg : public ZDlg
{
	HWND _getPdbDlg;
	PHANDLE _hFile;
	PCWSTR _NtSymbolPath;
	PCWSTR _PdbFileName;
	PGUID _Signature;
	ULONG _Age;
	BOOL OnOk(HWND hwndDlg);
	virtual INT_PTR DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

public:
	ZPDBPathDlg(PHANDLE hFile, PCWSTR NtSymbolPath, PCWSTR PdbFileName, PGUID Signature, ULONG Age) : 
	  _hFile(hFile), _NtSymbolPath(NtSymbolPath), _PdbFileName(PdbFileName), _Signature(Signature), _Age(Age)
	{
	}
};

class ZSrcPathDlg : public ZDlg
{
public:

	struct ZZ {
		PCWSTR FileName;
		PCWSTR DllName;
		PWSTR path;
		BOOL bNotLoad;
	};
private:
	ZZ* _pq;
	PCWSTR _filename;
	BOOL OnOk(HWND hwndDlg);
	virtual INT_PTR DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

class ZBpExp : public ZDlg, ZDetachNotify
{
	ZDbgDoc* _pDoc;
	PVOID _Va;

	void OnInitDialog(HWND hwndDlg);
	BOOL OnOk(HWND hwnd);

	virtual void OnDetach();

	virtual INT_PTR DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

	~ZBpExp();

public:

	HWND Create(PCWSTR exp);

	ZBpExp(ZDbgDoc* pDoc, PVOID Va);
};

class ZBPDlg : public ZDlg
{
	ZDbgDoc* _pDoc;
	ZBreakPoint** _pV;
	DWORD _n;

	void OnInitDialog(HWND hwndDlg);
	
	virtual INT_PTR DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

	~ZBPDlg();

	void CreateBPV(HWND hwndLV);

	void EnableAllBps(HWND hwndDlg, BOOL bEnable);

public:
	
	enum { WM_BPENBDIS = WM_USER + 0x100, WM_BPADDDEL };

	HWND Create();
	
	ZBPDlg(ZDbgDoc* pDoc);
};

class ZModulesDlg : public ZDlg, public CUILayot 
{
	ZDbgDoc* _pDoc;
	ZDll** _ppDll;
	DWORD _nDllCount;
	LONG _SortOrder;

	void OnInitDialog(HWND hwndDlg);

	void OnDestroy(HWND hwnd);

	virtual INT_PTR DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

	~ZModulesDlg();
public:
	HWND Create(DWORD nDllCount, PLIST_ENTRY head);
	ZModulesDlg(ZDbgDoc* pDoc);
};

class ZHandlesDlg : public ZDlg, public CUILayot 
{
	PSYSTEM_HANDLE_INFORMATION_EX _pshti;
	PULONG _ItoI;
	ULONG _nItems;
	bool _bTypeCanceled, _bDropDown;

	SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX* Item(ULONG i)
	{
		if (i < _nItems)
		{
			if (_ItoI)
			{
				i = _ItoI[i];
			}

			return &_pshti->Handles[i];
		}

		return 0;
	}

	NTSTATUS BuildList(HWND hwndDlg);
	BOOL OnInitDialog(HWND hwndDlg);

	void Apply(HWND hwndDlg);

	virtual INT_PTR DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
	~ZHandlesDlg();
public:
	ZHandlesDlg();

	enum { Pid, Han, Obj, Access, Attr, TN, IDD = IDD_DIALOG21 };
};

class ZSymbolsDlg : public ZDlg, public CUILayot, ZDetachNotify 
{
	ZDbgDoc* _pDoc;
	ZDll* _pDll;
	PDWORD _pIndexes;
	DWORD _nItems;
	LONG _SortOrder;

	virtual void OnDetach();

	void OnInitDialog(HWND hwndDlg);

	virtual INT_PTR DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

	~ZSymbolsDlg();
public:
	HWND Create();
	ZSymbolsDlg(ZDbgDoc* pDoc, ZDll* pDll);
};

class ZForwardDlg : public ZDlg, public CUILayot, ZDetachNotify 
{
	ZDbgDoc* _pDoc;
	ZDll* _pDll;
	DWORD _nItems;
	LONG _SortOrder;

	virtual void OnDetach();

	void OnInitDialog(HWND hwndDlg);

	virtual INT_PTR DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

	~ZForwardDlg();
public:
	HWND Create();
	ZForwardDlg(ZDbgDoc* pDoc, ZDll* pDll);
};

class ZExecDlg : public ZDlg
{
	HANDLE _hSysToken;
	BOOLEAN _bDump;
	void OnInitDialog(HWND hwndDlg);
	BOOL OnOk(HWND hwndDlg);
	virtual INT_PTR DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
public:
	ZExecDlg() : _hSysToken(0) {}
};

class ZExceptionDlg : public ZDlg
{
public:
	ZExceptionDlg();
protected:
private:
	DWORD _N, _iSel;
	LONG _bits;
	DWORD _status[31];
	BOOLEAN _wait_1, _bCanDel;
	void OnInitDialog(HWND hwnd);
	virtual INT_PTR DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
	~ZExceptionDlg();
};

class ZVmDialog : public ZDlg
{
	HANDLE _hProcess;
	int _f;
public:
	virtual INT_PTR DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
	ZVmDialog();
	~ZVmDialog();
	static void Create(HANDLE UniqueProcess);
};

class ZPrintFilter : public ZDlg
{
	virtual INT_PTR DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
public:
	ZPrintFilter();
	~ZPrintFilter();
};

class ZBreakDll : public ZDlg, ZDetachNotify
{
	ZDbgDoc* _pDoc;
	PVOID _ep;
	virtual void OnDetach();

	virtual INT_PTR DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
public:
	
	ZBreakDll(PVOID ep, ZDbgDoc* pDoc);
	
	~ZBreakDll();

	static void create(PVOID ep, ZDbgDoc* pDoc, PCWSTR name);
};

class ZXrefs : public ZDlg, ZDetachNotify
{
	ZDbgDoc* _pDoc;
	virtual void OnDetach();

	virtual INT_PTR DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
public:

	ZXrefs(ZDbgDoc* pDoc);

	~ZXrefs();

	static void create(ZDbgDoc* pDoc, DWORD N, PLONG_PTR xrefs, LONG_PTR Va);
};

