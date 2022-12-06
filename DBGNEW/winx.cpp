// winx.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
_NT_BEGIN
#include "../inc/initterm.h"
#include "../tkn/tkn.h"
#include "../winz/frame.h"
#include "../winz/mdi.h"
#include "../winz/scroll.h"
#include "../winz/TxtWnd.h"
#include "../winz/split.h"
#include "resource.h"
#include "zdlgs.h"
#include "adresswnd.h"
#include "fontdlg.h"
#include "common.h"
#include "types.h"

extern volatile const UCHAR guz = 0;
OBJECT_ATTRIBUTES zoa = { sizeof(zoa) };
HANDLE g_hDrv;
BYTE g_ThreadIndex, g_ProcessIndex, g_FileIndex;
ULONG g_dwGuiThreadId;

STATIC_UNICODE_STRING_(SOFC);

ZExceptionFC::ZExceptionFC()
{
	Reset();
}

void ZExceptionFC::Reset()
{
	RtlZeroMemory(this, sizeof(*this));
	_SINGLE_STEP = -1;
	_BREAKPOINT = -1;
}

void ZExceptionFC::Load()
{
	PKEY_VALUE_PARTIAL_INFORMATION pkvpi = (PKEY_VALUE_PARTIAL_INFORMATION)alloca(sizeof(ZExceptionFC) + sizeof(KEY_VALUE_PARTIAL_INFORMATION));

	DWORD DataLength;
	if (
		0 <= ZGLOBALS::getRegistry()->GetValue(&SOFC, KeyValuePartialInformation, pkvpi, sizeof(ZExceptionFC) + sizeof(KEY_VALUE_PARTIAL_INFORMATION), &pkvpi->TitleIndex) &&
		pkvpi->Type == REG_BINARY &&
		(DataLength = pkvpi->DataLength) &&
		!(DataLength & 3) &&
		DataLength <= sizeof(_bits) + sizeof(_status)
		)
	{
		memcpy(&_bits, pkvpi->Data, DataLength);
		_nExtra = (DataLength - sizeof(_bits)) >> 2;
	}
}

void ZExceptionFC::Save(LONG bits, DWORD nExtra, DWORD status[])
{
	_bits = bits;
	_nExtra = nExtra;
	if (nExtra) __movsd(_status, status, nExtra);
	ZGLOBALS::getRegistry()->SetValue(&SOFC, REG_BINARY, &_bits, sizeof(_bits) + (nExtra << 2));
}

LONG ZExceptionFC::get(PDWORD nExtra, DWORD status[])
{
	*nExtra = _nExtra;
	if (nExtra) __movsd(status, _status, _nExtra);
	return _bits;
}

BOOL ZExceptionFC::StopOnFC(NTSTATUS status)
{
	switch (status)
	{
	case STATUS_SINGLE_STEP:
		return _SINGLE_STEP;
	case STATUS_BREAKPOINT:
		return _BREAKPOINT;
	case STATUS_DATATYPE_MISALIGNMENT:
		return _DATATYPE_MISALIGNMENT;
	case STATUS_GUARD_PAGE_VIOLATION:
		return _GUARD_PAGE_VIOLATION;
	case STATUS_ACCESS_VIOLATION:
		return _ACCESS_VIOLATION;
	case STATUS_ILLEGAL_INSTRUCTION:
		return _ILLEGAL_INSTRUCTION;
	case STATUS_INTEGER_DIVIDE_BY_ZERO:
		return _INTEGER_DIVIDE_BY_ZERO;
	case STATUS_INTEGER_OVERFLOW:
		return _INTEGER_OVERFLOW;
	case STATUS_PRIVILEGED_INSTRUCTION:
		return _PRIVILEGED_INSTRUCTION;
	case STATUS_STACK_OVERFLOW:
		return _STACK_OVERFLOW;
	}

	if (_nExtra)
	{
		if (PDWORD pd = findDWORD(_nExtra, _status, status))
		{
			return _bittest(&_bits, 11 + (DWORD)(pd - _status));
		}
	}

	return _default;
}

HIMAGELIST g_himl16;
ZImageList g_IL20(5*GetSystemMetrics(SM_CXSMICON)>>2, 5*GetSystemMetrics(SM_CYSMICON)>>2);

#define _strnchr(a, b, c) strnchr(RtlPointerToOffset(a, b), a, c)

STATIC_UNICODE_STRING_(MainWnd);

void OnRemoteSectionMapped(DBGUI_WAIT_STATE_CHANGE& StateChange)
{
	PWSTR szDllDame = 0;

	if (HANDLE hFile = StateChange.LoadDll.FileHandle)
	{
		PVOID stack = alloca(sizeof(WCHAR));

		union {
			PVOID buf;
			POBJECT_NAME_INFORMATION poni;
		};
		
		NTSTATUS status;
		DWORD cb = 0, rcb = MAX_PATH * sizeof(WCHAR);

		do 
		{
			if (cb < rcb) cb = RtlPointerToOffset(buf = alloca(rcb - cb), stack);

			if (0 <= (status = NtQueryObject(hFile, ObjectNameInformation, buf, cb, &rcb)))
			{
				*(PWSTR)RtlOffsetToPointer(szDllDame = poni->Name.Buffer, poni->Name.Length) = 0;
			}

		} while (status == STATUS_BUFFER_OVERFLOW || status == STATUS_BUFFER_TOO_SMALL);

		NtClose(hFile);
	}

	WCHAR sz[128];
	swprintf(sz, L"remote section mapped by %x.%x at %p", 
		PtrToUlong(StateChange.AppClientId.UniqueProcess), 
		PtrToUlong(StateChange.AppClientId.UniqueThread), StateChange.LoadDll.BaseOfDll);

	HWND hwnd = ZGLOBALS::getMainHWND();
	SetForegroundWindow(hwnd);
	MessageBoxW(hwnd, sz, szDllDame, MB_ICONWARNING);
}

void OnRemoteSectionUnMapped(DBGUI_WAIT_STATE_CHANGE& StateChange)
{
	WCHAR sz[64];
	swprintf(sz, L"by %x.%x at %p", 
		PtrToUlong(StateChange.AppClientId.UniqueProcess), 
		PtrToUlong(StateChange.AppClientId.UniqueThread), StateChange.UnloadDll.BaseAddress);

	HWND hwnd = ZGLOBALS::getMainHWND();
	SetForegroundWindow(hwnd);
	MessageBoxW(hwnd, sz, L"remote section Unmapped", MB_ICONWARNING);
}

void OnUnWaited(DBGUI_WAIT_STATE_CHANGE& StateChange)
{
	WCHAR sz[128];
	swprintf(sz, L"code=%u pid=%x tid=%x", StateChange.NewState, 
		PtrToUlong(StateChange.AppClientId.UniqueProcess), 
		PtrToUlong(StateChange.AppClientId.UniqueThread));
	HWND hwnd = ZGLOBALS::getMainHWND();
	SetForegroundWindow(hwnd);
	MessageBoxW(hwnd, sz, L"Unwaited debug event !!", MB_ICONHAND);
}

void ZMyApp::OnSignal()
{
	DBGUI_WAIT_STATE_CHANGE StateChange;

	static LARGE_INTEGER timeout;

	while (!DbgUiWaitStateChange(&StateChange, &timeout))
	{
		PLIST_ENTRY head = &ZGLOBALS::get()->_docListHead, entry = head;

		BOOL bHandled = FALSE;
		ZDbgDoc* pDbg;

		while (!bHandled && (entry = entry->Flink) != head)
		{
			ZDocument* pDoc = static_cast<ZDocument*>(entry);

			if (!pDoc->QI(IID_PPV(pDbg)))
			{
				if (pDbg->getId() == PtrToUlong(StateChange.AppClientId.UniqueProcess))
				{
					bHandled = TRUE;
					pDbg->OnDebugEvent(StateChange);
				}
				pDbg->Release();
			}
		}

		if (!bHandled)
		{
			switch (StateChange.NewState)
			{
			case DbgCreateProcessStateChange:
				if (pDbg = new ZDbgDoc(TRUE))
				{
					pDbg->OnDebugEvent(StateChange);
					pDbg->Release();
					return;
				}
				break;
			case DbgLoadDllStateChange:
				OnRemoteSectionMapped(StateChange);
				break;
			case DbgUnloadDllStateChange:
				OnRemoteSectionUnMapped(StateChange);
				break;
			case DbgCreateThreadStateChange:
				if (StateChange.CreateThread.HandleToThread)
				{
					NtClose(StateChange.CreateThread.HandleToThread);
				}
			default:
				OnUnWaited(StateChange);
			}

			DbgUiContinue(&StateChange.AppClientId, DBG_CONTINUE);
		}
	}
}

BOOL ZMyApp::Init()
{
	if (0 > DbgUiConnectToDbg() )// 
	{
		return FALSE;
	}
	
	static DWORD flag = DEBUG_KILL_ON_CLOSE;
	NtSetInformationDebugObject(DbgUiGetThreadDebugObject(), DebugObjectFlags, &flag, sizeof(flag), 0);
	addWaitObject(this, DbgUiGetThreadDebugObject());
	return TRUE;
}

ZMyApp::~ZMyApp()
{
	delWaitObject(this);

	if (HANDLE hDebug = DbgUiGetThreadDebugObject())
	{
		DbgUiSetThreadDebugObject(0);
		NtClose(hDebug);
	}
}

class ZMainWnd : public ZMDIFrameWnd
{
	enum {
		ID_TASK_ICON = 0x8001,
		WM_TASKBAR = WM_USER + 0x100
	};
	ULONG _checkTime;
	BOOLEAN _bTopMost;

	virtual LRESULT WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	virtual BOOL CreateSB(HWND hwnd);
	virtual BOOL CreateTB(HWND hwnd);

	virtual PCUNICODE_STRING getPosName()
	{
		STATIC_UNICODE_STRING_(sMainWnd);
		return &sMainWnd;
	}

	virtual void OnIdle()
	{
		ULONG time = GetTickCount();
		if (_checkTime < time)
		{
			_checkTime = time + 2000;//2 sec

			PLIST_ENTRY head = &ZGLOBALS::get()->_docListHead, entry = head;

			BOOL ExistAttached = FALSE;
			ZDbgDoc* pDbg;

			while (!ExistAttached && (entry = entry->Flink) != head)
			{
				ZDocument* pDoc = static_cast<ZDocument*>(entry);

				if (!pDoc->QI(IID_PPV(pDbg)))
				{
					ExistAttached = pDbg->IsAttached();
					pDbg->Release();
				}
			}

			if (ExistAttached)
			{
				NTSTATUS status;
				ULONG cb = 0x80000;

				do 
				{
					status = STATUS_INSUFFICIENT_RESOURCES;
					if (PVOID buf = new UCHAR[cb])
					{
						if (0 <= (status = NtQuerySystemInformation(SystemExtendedProcessInformation, buf, cb, &cb)))
						{
							entry = head;

							while ((entry = entry->Flink) != head)
							{
								ZDocument* pDoc = static_cast<ZDocument*>(entry);

								if (!pDoc->QI(IID_PPV(pDbg)))
								{
									if (pDbg->IsAttached())
									{
										pDbg->UpdateThreads((PSYSTEM_PROCESS_INFORMATION)buf);
									}
									pDbg->Release();
								}
							}
						}
						delete [] buf;
					}
				} while (status == STATUS_INFO_LENGTH_MISMATCH);
			}
		}
		__super::OnIdle();
	}

	virtual DWORD getDocumentCmdId(WORD const** ppCmdId)
	{
		static const WORD cmdid[] = { 
			ID_KILL ,ID_DETACH, ID_MEMMAP, ID_BREAKPOINTS, ID_DBGFLAGS, ID_HANDLES, ID_VMOP, ID_MODULES, ID_BACK, ID_DISASM, ID_DBGTH
		};
		
		*ppCmdId = cmdid;

		return RTL_NUMBER_OF(cmdid);
	}
public:

	ZMainWnd()
	{
		DbgPrint("++ZMainWnd()\n");
		_bTopMost = FALSE, _checkTime = 0;
	}

	~ZMainWnd()
	{
		DbgPrint("--ZMainWnd()\n");
	}
};

BOOL ZMainWnd::CreateSB(HWND hwnd)
{
	//_bTopMost = TRUE;
	//SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOREDRAW|SWP_NOACTIVATE);

	if (hwnd = ZStatusBar::Create(hwnd))
	{
		static const int pp[] = { 
			256, 
			256 + 168, 
			256 + 168 + 168, 
			256 + 168 + 168 + 168, 
			256 + 168 + 168 + 168 + 256, 
#ifdef _WIN64
			256 + 168 + 168 + 168 + 256 + 256, 
#endif
			-1 
		};
		SetParts(pp, RTL_NUMBER_OF(pp));
		return TRUE;
	}
	return FALSE;
}

BOOL ZMainWnd::CreateTB(HWND hwnd)
{
	static const TBBUTTON g_btns[] = {
		{IMAGE_ENHMETAFILE, IDB_BITMAP1, TBSTATE_ENABLED, BTNS_AUTOSIZE, {}, (DWORD_PTR)L"Exceptions", -1},
#ifdef _WIN64
		{IMAGE_ENHMETAFILE, ID_PIPE, TBSTATE_ENABLED|TBSTATE_INDETERMINATE, BTNS_AUTOSIZE, {}, (DWORD_PTR)L"\\\\.\\pipe\\VmDbgPipe", -1},
#endif
		{IMAGE_ENHMETAFILE, IDB_BITMAP2, TBSTATE_ENABLED, BTNS_AUTOSIZE, {}, (DWORD_PTR)L"Debug", -1},
		{IMAGE_ENHMETAFILE, ID_PATH, TBSTATE_ENABLED, BTNS_AUTOSIZE, {}, (DWORD_PTR)L"Symbol File Path", -1 },
		{IMAGE_ENHMETAFILE, ID_BACK, 0, BTNS_AUTOSIZE, {}, (DWORD_PTR)L"Back", -1 },
		{IMAGE_ENHMETAFILE, ID_KILL, 0, BTNS_AUTOSIZE, {}, (DWORD_PTR)L"Terminate", -1 },
		{IMAGE_ENHMETAFILE, ID_DETACH, 0, BTNS_AUTOSIZE, {}, (DWORD_PTR)L"Detach", -1 },
		{IMAGE_ENHMETAFILE, ID_BREAKPOINTS, 0, BTNS_AUTOSIZE, {}, (DWORD_PTR)L"Breakpoints", -1 },
		{IMAGE_ENHMETAFILE, ID_VMOP, 0, BTNS_AUTOSIZE, {}, (DWORD_PTR)L"Vm Operations", -1 },
		{IMAGE_ENHMETAFILE, ID_DBGFLAGS, 0, BTNS_AUTOSIZE, {}, (DWORD_PTR)L"Debug Inherit", -1 },
		{IMAGE_ENHMETAFILE, ID_HANDLES, 0, BTNS_AUTOSIZE, {}, (DWORD_PTR)L"Handles", -1 },
		{IMAGE_ENHMETAFILE, ID_MEMMAP, 0, BTNS_AUTOSIZE, {}, (DWORD_PTR)L"Memory Map", -1 },
		{IMAGE_ENHMETAFILE, IDB_BITMAP12, TBSTATE_ENABLED, BTNS_AUTOSIZE, {}, (DWORD_PTR)L"Toggle TopMost", -1 },
		{IMAGE_ENHMETAFILE, IDB_BITMAP17, TBSTATE_ENABLED, BTNS_AUTOSIZE, {}, (DWORD_PTR)L"Choose Font", -1 },
		{IMAGE_ENHMETAFILE, ID_DISASM, 0, BTNS_AUTOSIZE|BTNS_DROPDOWN|BTNS_WHOLEDROPDOWN, {}, (DWORD_PTR)L"Select Assembler", -1 },
		{IMAGE_ENHMETAFILE, ID_MODULES, 0, BTNS_AUTOSIZE, {}, (DWORD_PTR)L"Modules", -1 },
		{IMAGE_ENHMETAFILE, ID_DBGTH, 0, BTNS_AUTOSIZE, {}, (DWORD_PTR)L"DbgWnds", -1 },
		{IMAGE_ENHMETAFILE, ID_PRFLT, TBSTATE_ENABLED, BTNS_AUTOSIZE, {}, (DWORD_PTR)L"Print Filter", -1 },
		{IMAGE_ENHMETAFILE, IDB_BITMAP20, TBSTATE_ENABLED, BTNS_AUTOSIZE, {}, (DWORD_PTR)L"Main Dialog", -1 },
		{IMAGE_ENHMETAFILE, ID_TOOLS, TBSTATE_ENABLED, BTNS_AUTOSIZE|BTNS_DROPDOWN|BTNS_WHOLEDROPDOWN, {}, (DWORD_PTR)L"Tools", -1 },
	};

	return ZToolBar::Create(hwnd, (HINSTANCE)&__ImageBase, 0, 0, 24, 24, g_btns, RTL_NUMBER_OF(g_btns), FALSE) != 0;
}

//////////////////////////////////////////////////////////////////////////

LRESULT ZMainWnd::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	union {
		LPARAM lp;
		NMHDR* hdr;
		NMTTDISPINFOW* lpnmtdi;
		LPNMTOOLBAR lpnmtb;
	};
	POINT pt;

	switch (uMsg)
	{
	case WM_NOTIFY:
		lp = lParam;

		switch (hdr->code)
		{
		case TBN_DROPDOWN:
			switch (lpnmtb->iItem)
			{
			case ID_TOOLS:
				pt.x = lpnmtb->rcButton.left;
				pt.y = lpnmtb->rcButton.bottom;

				if (ClientToScreen(lpnmtb->hdr.hwndFrom, &pt))
				{
					if (HMENU hmenu = LoadMenu((HMODULE)&__ImageBase, MAKEINTRESOURCE(IDR_MENU2)))
					{
						TrackPopupMenu(GetSubMenu(hmenu, 4), 0, pt.x, pt.y, 0, hwnd, 0);
						DestroyMenu(hmenu);
					}
				}
				return TBDDRET_DEFAULT;
			}
			break;

		case TTN_GETDISPINFO:
			hwnd = ZTabBar::getHWND();
			if (GetCursorPos(&pt) && ScreenToClient(hwnd, &pt))
			{
				if (DWORD n = TabCtrl_GetItemCount(hwnd))
				{
					RECT rc;
					do 
					{
						if (TabCtrl_GetItemRect(hwnd, --n, &rc))
						{
							if (PtInRect(&rc, pt))
							{
								TCITEM item = { TCIF_PARAM };
								if (TabCtrl_GetItem(hwnd, n, &item))
								{
									if (ZWnd* pWnd = FromHWND((HWND)item.lParam))
									{
										ZMDIChildFrame* pFrame;
										HRESULT hr = pWnd->QI(IID_PPV(pFrame));
										pWnd->Release();
										if (0 <= hr)
										{
											ZView* pView = pFrame->getView();
											pFrame->Release();
											if (pView)
											{
												if (ZDocument* pDocument = pView->getDocument())
												{
													ZDbgDoc* pDbgDoc;
													if (0 <= pDocument->QI(IID_PPV(pDbgDoc)))
													{
														if (ZSrcFile* pSrc = pDbgDoc->findSrc(pView))
														{
															lpnmtdi->lpszText = pSrc->Buffer;
															lpnmtdi->hinst = 0;
														}
														pDbgDoc->Release();
													}
												}
											}
										}
									}
								}
								break;
							}
						}
					} while (n);
				}
			}
			break;
		}
		break;

	case WM_DESTROY:
		if (GLOBALS_EX* globals = static_cast<GLOBALS_EX*>(ZGLOBALS::get()))
		{
			if (globals->_pipe)
			{
				StopDebugPipe(globals->_pipe);
				globals->_pipe = 0;
			}
		}
		break;

	case WM_COMMAND:
		switch (wParam)
		{
		case ID_4_Calc:
			if (CCalcDlg* pDlg = new CCalcDlg)
			{
				pDlg->Create((HINSTANCE)&__ImageBase, MAKEINTRESOURCE(IDD_DIALOG1), hwnd, 0);
				pDlg->Release();
			}
			return 0;

		case ID_4_FILEINUSE:
			if (CFileInUseDlg* pDlg = new CFileInUseDlg)
			{
				pDlg->Create((HINSTANCE)&__ImageBase, MAKEINTRESOURCE(IDD_DIALOG18), hwnd, 0);
				pDlg->Release();
			}
			return 0;

		case ID_4_RVATOOFS:
			if (CRvaToOfs* pDlg = new CRvaToOfs)
			{
				pDlg->Create((HINSTANCE)&__ImageBase, MAKEINTRESOURCE(IDD_DIALOG19), hwnd, 0);
				pDlg->Release();
			}
			return 0;

		case ID_4_SHOWTOKEN:
			if (CTokenDlg* pDlg = new CTokenDlg)
			{
				pDlg->Create((HINSTANCE)&__ImageBase, MAKEINTRESOURCE(IDD_DIALOG20), hwnd, 0);
				pDlg->Release();
			}
			return 0;

		case ID_FILE_EXIT:
			//if (IsListEmpty(&ZGLOBALS::get()->_docListHead)) __nop();
			PostMessageW(hwnd, WM_CLOSE, 0, 0);
			return 0;

		case IDB_BITMAP20:
			if (hwnd = CMainDlg_Create(hwnd))
			{
				if (ZDocument* pDoc = GetActiveDoc())
				{
					WCHAR sz[9];
					if (0 < swprintf_s(sz, _countof(sz), L"%x", static_cast<ZDbgDoc*>(pDoc)->getId()))
					{
						SetDlgItemTextW(hwnd, IDC_EDIT8, sz);
					}
					if (uMsg = static_cast<ZDbgDoc*>(pDoc)->getThreadId() )
					{
						if (0 < swprintf_s(sz, _countof(sz), L"%x", uMsg))
						{
							SetDlgItemTextW(hwnd, IDC_EDIT7, sz);
						}
					}
				}
			}
			return 0;

		case ID_PATH:
			if (CSymbolsDlg* pDlg = new CSymbolsDlg)
			{
				pDlg->Create((HINSTANCE)&__ImageBase, MAKEINTRESOURCE(IDD_DIALOG15), hwnd, 0);
				pDlg->Release();
			}
			return 0;

		case IDB_BITMAP12:
			SetWindowPos(hwnd, (_bTopMost = !_bTopMost) ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOREDRAW|SWP_NOACTIVATE);
			return 0;

		case IDB_BITMAP2:
			if (ZExecDlg* dlg = new ZExecDlg)
			{
				dlg->Create((HINSTANCE)&__ImageBase, MAKEINTRESOURCE(IDD_DIALOG8), hwnd, 0);
				dlg->Release();
			}
			return 0;
		
		case IDB_BITMAP1:
			if (ZExceptionDlg* p = new ZExceptionDlg)
			{
				p->Create((HINSTANCE)&__ImageBase, MAKEINTRESOURCE(IDD_DIALOG9), hwnd, 0);
				p->Release();
			}
			return 0;

		case IDB_BITMAP17:
			ChoseFont(hwnd);
			return 0;

		case ID_PRFLT:
			if (ZPrintFilter* p = new ZPrintFilter)
			{
				p->Create((HINSTANCE)&__ImageBase, MAKEINTRESOURCE(IDD_DIALOG11), hwnd, 0);
				p->Release();
			}
			return 0;

		case ID_PIPE:
			if (GLOBALS_EX* globals = static_cast<GLOBALS_EX*>(ZGLOBALS::get()))
			{
				if (globals->_pipe)
				{
					StopDebugPipe(globals->_pipe);
					globals->_pipe = 0;
					IndeterminateCmd(ID_PIPE, TRUE);
				}
				else if (StartDebugPipe(&globals->_pipe))
				{
					IndeterminateCmd(ID_PIPE, FALSE);
				}
			}
			return 0;
		}
		break;
	}
	return ZMDIFrameWnd::WindowProc(hwnd, uMsg, wParam, lParam);
}

extern "C" {
	GUID GUID_NULL;
}

void ChoseFont();

NTSTATUS AdjustPrivileges()
{
	HANDLE hToken;
	NTSTATUS status;
	if (0 <= (status = NtOpenProcessToken(NtCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken)))
	{
		BEGIN_PRIVILEGES(tp, 2)
			LAA(SE_DEBUG_PRIVILEGE),
			LAA(SE_LOAD_DRIVER_PRIVILEGE)
		END_PRIVILEGES	
		status = NtAdjustPrivilegesToken(hToken, FALSE, const_cast<PTOKEN_PRIVILEGES>(&tp), 0, 0, 0);
		NtClose(hToken);
	}
	return status;
}

NTSTATUS GetSystemToken(PBYTE buf, PHANDLE phSysToken)
{
	NTSTATUS status;

	union {
		PBYTE pb;
		PSYSTEM_PROCESS_INFORMATION pspi;
	};

	pb = buf;
	ULONG NextEntryOffset = 0;

	do 
	{
		pb += NextEntryOffset;

		HANDLE hProcess, hToken, hNewToken;

		if (pspi->InheritedFromUniqueProcessId && pspi->UniqueProcessId)
		{
			static SECURITY_QUALITY_OF_SERVICE sqos = {
				sizeof sqos, SecurityImpersonation, SECURITY_DYNAMIC_TRACKING, FALSE
			};

			static OBJECT_ATTRIBUTES soa = { sizeof(soa), 0, 0, 0, 0, &sqos };

			if (0 <= NtOpenProcess(&hProcess, PROCESS_QUERY_LIMITED_INFORMATION, &zoa, &pspi->TH->ClientId))
			{
				status = NtOpenProcessToken(hProcess, TOKEN_DUPLICATE, &hToken);

				NtClose(hProcess);

				if (0 <= status)
				{
					status = NtDuplicateToken(hToken, TOKEN_ADJUST_PRIVILEGES|TOKEN_IMPERSONATE, 
						&soa, FALSE, TokenImpersonation, &hNewToken);

					NtClose(hToken);

					if (0 <= status)
					{
						BEGIN_PRIVILEGES(tp, 4)
							LAA(SE_TCB_PRIVILEGE),
							LAA(SE_DEBUG_PRIVILEGE),
							LAA(SE_INCREASE_QUOTA_PRIVILEGE),
							LAA(SE_ASSIGNPRIMARYTOKEN_PRIVILEGE),
						END_PRIVILEGES	

						if (STATUS_SUCCESS == NtAdjustPrivilegesToken(hNewToken, FALSE, (PTOKEN_PRIVILEGES)&tp, 0, 0, 0))	
						{
							*phSysToken = hNewToken;
							return STATUS_SUCCESS;
						}

						NtClose(hNewToken);
					}
				}
			}
		}

	} while (NextEntryOffset = pspi->NextEntryOffset);

	return STATUS_UNSUCCESSFUL;
}

NTSTATUS ImpersonateSystemToken(PBYTE buf)
{
	HANDLE hToken;

	NTSTATUS status = GetSystemToken(buf, &hToken);

	if (0 <= status)
	{
		status = NtSetInformationThread(NtCurrentThread(), ThreadImpersonationToken, &hToken, sizeof(hToken));
		NtClose(hToken);
	}
	return status;
}

GLOBALS_EX::GLOBALS_EX()
{
	_NtSymbolPath = 0;
	_pScript = 0;
	_pipe = 0;
}

GLOBALS_EX::~GLOBALS_EX()
{
	if (_pipe)
	{
		//_pipe->Release();
	}

	if (_pScript)
	{
		_pScript->Stop();
		_pScript->Release();
		CoUninitialize();
	}

	if (_NtSymbolPath)
	{
		delete _NtSymbolPath;
	}
}

STATIC_UNICODE_STRING_(SymbolPath);

BOOL GLOBALS_EX::Init()
{
	ULONG cb = 0, rcb = 256;
	PVOID stack = alloca(guz);
	union {
		PKEY_VALUE_PARTIAL_INFORMATION pkvpi;
		PVOID buf;
	};

	NTSTATUS status;
	PWSTR path;
	do 
	{
		if (cb < rcb)
		{
			cb = RtlPointerToOffset(buf = alloca(rcb - cb), stack);
		}

		if (0 <= (status = Reg->GetValue(&SymbolPath, KeyValuePartialInformation, pkvpi, cb, &rcb)))
		{
			if (ifRegSz(pkvpi))
			{
				memcpy(path = (PWSTR)pkvpi->Data - 4, L"\\??\\", 4*sizeof(WCHAR));
				IO_STATUS_BLOCK iosb;
				HANDLE hFile;
				UNICODE_STRING ObjectName;
				OBJECT_ATTRIBUTES oa = { sizeof(oa), 0, &ObjectName};
				RtlInitUnicodeString(&ObjectName, path);
				if (0 <= ZwOpenFile(&hFile, SYNCHRONIZE, &oa, &iosb, FILE_SHARE_VALID_FLAGS, FILE_DIRECTORY_FILE))
				{
					NtClose(hFile);
					return SetPathNoReg((PWSTR)pkvpi->Data);
				}
			}
		}
	} while (status == STATUS_BUFFER_OVERFLOW);

	PCWSTR NtSystemRoot = USER_SHARED_DATA->NtSystemRoot;
	path = (PWSTR)alloca((wcslen(NtSystemRoot) << 1) + sizeof(L"\\Symbols"));
	swprintf(path, L"%s\\Symbols", NtSystemRoot);
	return SetPathNoReg(path);
}

BOOL GLOBALS_EX::SetPathNoReg(PCWSTR NtSymbolPath)
{
	ULONG len = (ULONG)wcslen(NtSymbolPath) + 1;

	if (PWSTR sz = new WCHAR[len])
	{
		memcpy(sz, NtSymbolPath, len << 1);

		if (_NtSymbolPath)
		{
			delete _NtSymbolPath;
		}

		_NtSymbolPath = sz;

		return TRUE;
	}

	return FALSE;
}

BOOL GLOBALS_EX::SetPath(PCWSTR NtSymbolPath)
{
	Reg->SetValue(&SymbolPath, REG_SZ, (void*)NtSymbolPath, ((ULONG)wcslen(NtSymbolPath) + 1) << 1);
	return SetPathNoReg(NtSymbolPath);
}

PCWSTR GLOBALS_EX::getPath()
{
	if (_NtSymbolPath)
	{
		return _NtSymbolPath;
	}
	return L"";
}

JsScript* GLOBALS_EX::getScript()
{
	if (!_pScript)
	{
		if (JsScript* pScript = new JsScript)
		{
			if (pScript->CreateScriptEngine())
			{
				_pScript = pScript;
			}
			else
			{
				_pScript->Stop();
				_pScript->Release();
				pScript = 0;
			}
		}
	}

	return _pScript;
}

JsScript* GLOBALS_EX::_getScript()
{
	return static_cast<GLOBALS_EX*>(ZGLOBALS::get())->getScript();
}

BOOL LoadDrv();
void InitPrMask();
void SymbolsThread(PCWSTR NtSymbolPath);

void zmain()
{
	g_dwGuiThreadId = GetCurrentThreadId();

	GLOBALS_EX globals;
	ZMyApp app;
	ZRegistry reg;
	ZFont font(TRUE);
	
	if (app.Init() && 0 <= reg.Create(L"Software\\{913F69B4-6707-4860-934D-B47A9591B6B9}") && font.Init() && globals.Init())
	{
#ifdef _UDT_
		RtlCreateUserThread(NtCurrentProcess(), 0, 0, 0, 0, 0, SymbolsThread, globals.getPath(), 0, 0);
#endif

		InitPrMask();
		app.Load();

		HWND hwnd = 0;

		if (ZMainWnd* p = new ZMainWnd)
		{
			hwnd = p->ZSDIFrameWnd::Create(L"Z-Dbg", (HINSTANCE)&__ImageBase, MAKEINTRESOURCE(IDR_MENU1));//

			p->Release();
		}

		if (hwnd)
		{
			app.Run();
		}
	}
}

void UDTEntry(PCWSTR lpsz);
#include "../winz/wic.h"

HIMAGELIST ImageList_LoadImageV(_In_ PCWSTR pszName, ULONG cx, ULONG cy, ULONG n)
{
	HIMAGELIST himl = 0;
	ULONG cbTile = cx*cy<<2;
	LIC lic { new UCHAR[cbTile*n], cx, cy*n };

	if (PBYTE pbBits = (PBYTE)lic._pvBits)
	{
		if (0 <= lic.CreateBMPFromPNG(pszName))
		{
			if (himl = ImageList_Create(cx, cy, ILC_COLOR32, n, 0))
			{
				BITMAPINFO bi = { {sizeof(BITMAPINFOHEADER), cx, -(int)cy, 1, 32 } };

				PVOID Bits;
				if (HBITMAP hbmp = CreateDIBSection(0, &bi, DIB_RGB_COLORS, &Bits, 0, 0))
				{
					pbBits += cbTile*n;
					do 
					{
						memcpy(Bits, pbBits -= cbTile, cbTile);
						if (0 > ImageList_Add(himl, hbmp, 0))
						{
							break;
						}
					} while (--n);

					DeleteObject(hbmp);
				}

				if (n) 
				{
					ImageList_Destroy(himl), himl = 0;
				}
			}
		}

		delete [] lic._pvBits;
	}

	return himl;
}

void initIndexes()
{
	//UDTEntry(L"c:\\windows\\symbols\\exe\\ntkrnlmp.pdb");

	g_himl16 = ImageList_LoadImageV(MAKEINTRESOURCE(4), 16, 16, 17);
		//ImageList_LoadImageV(&__ImageBase, aa, 3);
	g_IL20.LoadFromPNG(5, MAKEINTRESOURCE(5));

	STATIC_UNICODE_STRING_(File);
	STATIC_UNICODE_STRING_(Thread);
	STATIC_UNICODE_STRING_(Process);
	const OBJECT_TYPE_INFORMATION* poti;
	if (poti = g_AOTI[&File])
	{
		g_FileIndex = poti->TypeIndex;
	}
	if (poti = g_AOTI[&Process])
	{
		g_ProcessIndex = poti->TypeIndex;
	}
	if (poti = g_AOTI[&Thread])
	{
		g_ThreadIndex = poti->TypeIndex;
	}
}

NTSTATUS MyOpenProcess(PHANDLE ProcessHandle, ULONG DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes, PCLIENT_ID Cid)
{
	if (g_hDrv)
	{
		IO_STATUS_BLOCK iosb;
		NTSTATUS status = ZwDeviceIoControlFile(g_hDrv, 0, 0, 0, &iosb, IOCTL_OpenProcess, &Cid->UniqueProcess, sizeof(HANDLE), 0, 0);
		*ProcessHandle = (HANDLE)iosb.Information;
		return status;
	}
	return ZwOpenProcess(ProcessHandle, DesiredAccess, ObjectAttributes, Cid);
}

NTSTATUS MyOpenThread(PHANDLE ThreadHandle, ULONG DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes, PCLIENT_ID Cid)
{
	if (g_hDrv)
	{
		IO_STATUS_BLOCK iosb;
		NTSTATUS status = ZwDeviceIoControlFile(g_hDrv, 0, 0, 0, &iosb, IOCTL_OpenThread, Cid, sizeof(CLIENT_ID), 0, 0);
		*ThreadHandle = (HANDLE)iosb.Information;
		return status;
	}
	return ZwOpenThread(ThreadHandle, DesiredAccess, ObjectAttributes, Cid);
}

LONG g_printMask = MAXDWORD;

void InitShowCmdLine();

//#define _HEAP_CHECK_

#ifdef _HEAP_CHECK_
HANDLE ghHeap;

HANDLE WINAPI GetMyHeap(  )
{
	return ghHeap;
}

void CheckHeap()
{
	if (HeapLock(ghHeap))
	{
		PROCESS_HEAP_ENTRY Entry = {};

		while (HeapWalk(ghHeap, &Entry))
		{
			if (Entry.wFlags & PROCESS_HEAP_ENTRY_BUSY)
			{
				__debugbreak();
			}
		}

		HeapUnlock(ghHeap);
	}
}

extern "C"
{
	extern PVOID __imp_GetProcessHeap;
}

void RedirectHeap()
{
	ULONG op;
	if (VirtualProtect(&__imp_GetProcessHeap, sizeof(__imp_GetProcessHeap), PAGE_READWRITE, &op))
	{
		__imp_GetProcessHeap = GetMyHeap;
		if (op != PAGE_READWRITE)
		{
			VirtualProtect(&__imp_GetProcessHeap, sizeof(__imp_GetProcessHeap), op, &op);
		}
	}
}
#endif//_HEAP_CHECK_

#include "../inc/rundown.h"
#include "../asio/io.h"

void IO_RUNDOWN::RundownCompleted()
{
	DbgPrint("RundownCompleted\n");

	if (g_hDrv)
	{
		NtClose(g_hDrv);
	}

	destroyterm();

#ifdef _HEAP_CHECK_
	CheckHeap();
	HeapDestroy(ghHeap);
#endif
	ExitProcess(0);
}

VOID WINAPI FiberProc(PVOID lpFiber)
{
	zmain();
	SwitchToFiber(lpFiber);
}

#include "../wow/wow.h"
extern DLL_LIST_0 ntdll;

void ep(_PEB*)
{
#ifdef _HEAP_CHECK_
	ghHeap = HeapCreate(0, 0x100000, 0);
	RedirectHeap();
#endif

#ifndef _WIN64
	PVOID wow;
	if (0 > ZwQueryInformationProcess(NtCurrentProcess(), ProcessWow64Information, &wow, sizeof(wow), 0) || wow)
	{
		MessageBox(0, L"The 32-bit version of this program is not compatible with the 64-bit Windows you're running.", 
			L"Machine Type Mismatch", MB_ICONWARNING);
		ExitProcess(0);
	}
#else
	DLL_LIST_0::Process(&ntdll);
#endif
	if (AdjustPrivileges())
	{
		MessageBox(0, L"Fail Get Debug Privilege", 0, MB_ICONHAND);
		ExitProcess(0);
	}

	if (!LoadDrv())
	{
		if (MessageBox(0, L"Fail load driver. continue work ?", 0, MB_ICONWARNING|MB_YESNO) != IDYES)
		{
			ExitProcess(0);
		}
	}

	initterm();

	g_AOTI.Init();

	InitShowCmdLine();

	if (0 <= CoInitializeEx(0, COINIT_DISABLE_OLE1DDE|COINIT_APARTMENTTHREADED))
	{
		initIndexes();

		if (PVOID MainFiber = ConvertThreadToFiber(0))
		{
			if (PVOID Fiber = CreateFiberEx(0x100000*sizeof(PVOID), 0x200000*sizeof(PVOID), 0, FiberProc, MainFiber))
			{
				SwitchToFiber(Fiber);
				DeleteFiber(Fiber);
			}

			ConvertFiberToThread();
		}

		CoUninitialize();
	}

	IO_RUNDOWN::g_IoRundown.BeginRundown();
}

_NT_END

