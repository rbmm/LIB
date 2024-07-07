#include "StdAfx.h"

#include "resource.h"
#include "../inc/idcres.h"
#include "..\NtVer\nt_ver.h"

_NT_BEGIN
#pragma warning(disable : 4477 )
#include "common.h"
#include "types.h"

NTSTATUS ApcInjector(PCWSTR lpDllName, PCLIENT_ID cid);
NTSTATUS RemoteUnload(HMODULE hmod, PCLIENT_ID cid);
void ShowToken(HANDLE hToken, ULONG id, PCWSTR caption);
void ShowObjectSecurity(HANDLE hObject, ULONG id, PCWSTR caption);
void EnumAllWins(HWND hwnd);
void PrintWindows();
void SetEditText(HWND hwnd, PVOID txt);
void FreePM(_In_ PRTL_PROCESS_MODULES mods);
NTSTATUS QueryPM(_In_ HANDLE dwProcessId, _Out_ PRTL_PROCESS_MODULES* pmods);

HRESULT OnBrowse(_In_ HWND hwndDlg, 
				 _In_ UINT cFileTypes, 
				 _In_ const COMDLG_FILTERSPEC *rgFilterSpec, 
				 _Out_ PWSTR* ppszFilePath, 
				 _In_ UINT iFileType = 0,
				 _In_ const CLSID* pclsid = &__uuidof(FileOpenDialog),
				 _In_ PCWSTR pszDefaultExtension = 0);

void OnBrowse(HWND hwndDlg, 
			  UINT nIDDlgItem, 
			  UINT cFileTypes, 
			  const COMDLG_FILTERSPEC *rgFilterSpec, 
			  _In_ UINT iFileType = 0,
			  _In_ const CLSID* pclsid = &__uuidof(FileOpenDialog),
			  _In_ PCWSTR pszDefaultExtension = 0);

void RtlRevertToSelf()
{
	HANDLE hToken = 0;
	NtSetInformationThread(NtCurrentThread(), ThreadImpersonationToken, &hToken, sizeof(HANDLE));
}

NTSTATUS GetSystemToken(PBYTE buf, PHANDLE phSysToken);

NTSTATUS ImpersonateSystemToken(PBYTE buf);

NTSTATUS GetProcessList(PUCHAR* pbbuf)
{
	NTSTATUS status;
	DWORD cb = 0x40000;
	do 
	{
		if (PUCHAR buf = new UCHAR[cb += PAGE_SIZE])
		{
			if (0 <= (status = NtQuerySystemInformation(SystemProcessInformation, buf, cb, &cb)))
			{
				*pbbuf = buf;

				return STATUS_SUCCESS;
			}

			delete [] buf;
		}
		else
		{
			return STATUS_NO_MEMORY;
		}

	} while (status == STATUS_INFO_LENGTH_MISMATCH);

	return status;
}

NTSTATUS ImpersonateSystemToken()
{
	PBYTE buf;
	NTSTATUS status = GetProcessList(&buf);
	if (0 <= status)
	{
		status = ImpersonateSystemToken(buf);
		delete [] buf;
	}

	return status;
}

class CInjectDlg : public ZDlg
{
	HANDLE m_dwProcessId;

	void OnOk(HWND hwndDlg)
	{
		HWND hwnd = GetDlgItem(hwndDlg, IDC_EDIT1);

		if (ULONG len = GetWindowTextLengthW(hwnd))
		{
			if (PWSTR pszDll = (PWSTR)_malloca(++len * sizeof(WCHAR)))
			{
				if (GetWindowTextW(hwnd, pszDll, len))
				{
					CLIENT_ID cid = { m_dwProcessId };
					NTSTATUS status = ApcInjector(pszDll, &cid);

					if (0 > status)
					{
						ShowNTStatus(hwndDlg, status, L"ApcInjector");
					}
					else
					{
						MessageBox(0, pszDll, L"Inject Ok", 0);
						EndDialog(hwndDlg, 0);
					}
				}
				_freea(pszDll);
			}
		}
	}

	void OnInitDialog(HWND hwndDlg)
	{
		WCHAR sz[32];
		swprintf_s(sz, _countof(sz), L"Inject to %x", (ULONG)(ULONG_PTR)m_dwProcessId);
		SetWindowTextW(hwndDlg, sz);
	}

	virtual INT_PTR DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		static const COMDLG_FILTERSPEC rgSpec[] =
		{ 
			{ L"Dll files", L"*.dll" },
			{ L"All files", L"*" },
		};

		switch (uMsg)
		{
		case WM_INITDIALOG:
			OnInitDialog(hwndDlg);
			break;

		case WM_COMMAND:
			switch (wParam)
			{
			case IDCANCEL:
				EndDialog(hwndDlg, 0);
				return 0;

			case IDC_BUTTON1:
				OnBrowse(hwndDlg, IDC_EDIT1,_countof(rgSpec), rgSpec);
				return 0;

			case IDOK:
				OnOk(hwndDlg);
				return 0;
			}
			break;
		}
		return __super::DialogProc(hwndDlg, uMsg, wParam, lParam);
	}
public:
	CInjectDlg(HANDLE dwProcessId) : m_dwProcessId(dwProcessId){}
};

#define ComboBox_AddStringEx(hwndCtl, sz, Data) ComboBox_SetItemData(hwndCtl, ComboBox_AddString(hwndCtl, sz), Data)

struct _SORTMODE 
{
	int mode;
};

#include "../inc/rtlframe.h"

typedef RTL_FRAME<_SORTMODE> SORTMODE;

int __cdecl KModCompare(PRTL_PROCESS_MODULE_INFORMATION& mod1, PRTL_PROCESS_MODULE_INFORMATION& mod2)
{
	if (_SORTMODE* prf = SORTMODE::get())
	{
		switch(prf->mode) 
		{
		case 0: 
			if (mod1->ImageBase < mod2->ImageBase) return -1;
			if (mod1->ImageBase > mod2->ImageBase) return +1;
			return 0;
		case 1: return _stricmp(
					mod1->FullPathName + mod1->OffsetToFileName,
					mod2->FullPathName + mod2->OffsetToFileName);
		case -1: return _stricmp(mod1->FullPathName, mod2->FullPathName);
		default:__assume(false);
		}
	}

	return 0;
}

PCSTR GetThreadWaitReason(KWAIT_REASON wr)
{
	static PCSTR wrNames[] = 
	{
		"Executive",
		"FreePage",
		"PageIn",
		"PoolAllocation",
		"DelayExecution",
		"Suspended",
		"UserRequest",
		"WrExecutive",
		"WrFreePage",
		"WrPageIn",
		"WrPoolAllocation",
		"WrDelayExecution",
		"WrSuspended",
		"WrUserRequest",
		"WrEventPair",
		"WrQueue",
		"WrLpcReceive",
		"WrLpcReply",
		"WrVirtualMemory",
		"WrPageOut",
		"WrRendezvous",
		"WrKeyedEvent",
		"WrTerminated",
		"WrProcessInSwap",
		"WrCpuRateControl",
		"WrCalloutStack",
		"WrKernel",
		"WrResource",
		"WrPushLock",
		"WrMutex",
		"WrQuantumEnd",
		"WrDispatchInt",
		"WrPreempted",
		"WrYieldExecution",
		"WrFastMutex",
		"WrGuardedMutex",
		"WrRundown",
		"WrAlertByThreadId",
		"WrDeferredPreempt"
	};

	return wr<RTL_NUMBER_OF(wrNames) ? wrNames[wr] : "?";
}

PCSTR GetThreadStateName(THREAD_STATE st)
{
	static LPCSTR stNames[] = 
	{
		"StateInitialized",
		"StateReady",
		"StateRunning",
		"StateStandby",
		"StateTerminated",
		"StateWait",
		"StateTransition"
	};

	return st<RTL_NUMBER_OF(stNames) ? stNames[st] : "?";
}

void ShowNTStatus(HWND hwnd, NTSTATUS status, PCWSTR caption)
{
	WCHAR buf[256], *sz = buf;

	sz += swprintf(sz, L"//%08x\r\n", status);
	
	FormatMessage(FORMAT_MESSAGE_IGNORE_INSERTS|FORMAT_MESSAGE_FROM_HMODULE,
		ZDll::_hmod_nt, status, 0, sz, RTL_NUMBER_OF(buf) - (DWORD)(sz - buf), 0);

	switch ((DWORD)status >> 30)
	{
	case 0:
		status = MB_OK;
		break;
	case 1:
		status = MB_OK|MB_ICONINFORMATION;
		break;
	case 2:
		status = MB_OK|MB_ICONWARNING;
		break;
	case 3:
		status = MB_OK|MB_ICONHAND;
		break;
	default:__assume(false);
	}

	MessageBox(hwnd, buf, caption, status);
}

void ShowText(PCWSTR caption, PCWSTR text)
{
	_ZGLOBALS* globals = ZGLOBALS::get();

	if (HWND hwnd = CreateWindowExW(0, WC_EDIT, caption, WS_OVERLAPPEDWINDOW|WS_VSCROLL|WS_HSCROLL|ES_MULTILINE|ES_WANTRETURN,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, globals->hwndMain, 0, 0, 0))
	{
		SendMessage(hwnd, WM_SETFONT, (WPARAM)globals->Font->getFont(), 0);

		SetWindowText(hwnd, text);
		ShowWindow(hwnd, SW_SHOW);
	}
}

typedef struct SYSTEM_PROCESS_ID_INFORMATION
{
	HANDLE ProcessId;
	UNICODE_STRING ImageName;
} *PSYSTEM_PROCESS_ID_INFORMATION;

NTSTATUS GetPathFromProcessID(HANDLE dwProcessId, _Out_ PWSTR* ppszPath)
{
	*ppszPath = 0;

	SYSTEM_PROCESS_ID_INFORMATION spii = { dwProcessId, { 0, 0x200 } };
	NTSTATUS status;

	ULONG rcb;
	do 
	{
		status = STATUS_NO_MEMORY;

		if (spii.ImageName.Buffer = new WCHAR[spii.ImageName.MaximumLength / sizeof(WCHAR)])
		{
			if (0 <= (status = NtQuerySystemInformation(SystemProcessIdInformation, &spii, sizeof(spii), &rcb)))
			{
				*ppszPath = spii.ImageName.Buffer;

				return STATUS_SUCCESS;
			}

			delete [] spii.ImageName.Buffer;
		}

	} while (status == STATUS_INFO_LENGTH_MISMATCH);

	return status;
}

void ShowImagePath(HWND hwnd, HANDLE UniqueProcess)
{
	WCHAR caption[32], *psz;
	if (NTSTATUS status = GetPathFromProcessID(UniqueProcess, &psz))
	{
		ShowNTStatus(hwnd, status, 0);
		return ;
	}
	swprintf(caption, L"%X Process", (DWORD)(ULONG_PTR)UniqueProcess);
	ShowText(caption, psz);
	delete [] psz;
}

void ShowCmdLineUI(HANDLE UniqueProcess, PUNICODE_STRING CmdLine)
{
	*(PWSTR)RtlOffsetToPointer(CmdLine->Buffer, CmdLine->Length) = 0;
	WCHAR caption[32];
	swprintf(caption, L"%X Process CmdLine", (DWORD)(ULONG_PTR)UniqueProcess);
	ShowText(caption, CmdLine->Buffer);
}

// since WINBLUE (8.1)
NTSTATUS ShowCmdLine8(HANDLE UniqueProcess)
{
	HANDLE hProcess;
	CLIENT_ID cid = { UniqueProcess };

	NTSTATUS status = NtOpenProcess(&hProcess, PROCESS_QUERY_LIMITED_INFORMATION, &zoa, &cid);

	if (0 <= status)
	{
		PVOID stack = alloca(sizeof(WCHAR));

		union {
			PVOID buf;
			PUNICODE_STRING CmdLine;
		};

		ULONG cb = 0, rcb = 512;

		do 
		{
			if (cb < rcb) cb = RtlPointerToOffset(buf = alloca(rcb - cb), stack);

			if (0 <= (status = NtQueryInformationProcess(hProcess, ProcessCommandLineInformation, buf, cb, &rcb)))
			{
				ShowCmdLineUI(UniqueProcess, CmdLine);
				break;
			}

		} while (status == STATUS_INFO_LENGTH_MISMATCH);

		NtClose(hProcess);
	}

	return status;
}

NTSTATUS ShowCmdLineOld(HANDLE UniqueProcess)
{
	HANDLE hProcess;
	CLIENT_ID cid = { UniqueProcess };
	NTSTATUS status = MyOpenProcess(&hProcess, PROCESS_QUERY_INFORMATION|PROCESS_VM_READ, &zoa, &cid);
	if (0 <= status)
	{
		PROCESS_BASIC_INFORMATION pbi;
		UNICODE_STRING CmdLine;
		union {
			_RTL_USER_PROCESS_PARAMETERS * ProcessParameters;
			PVOID buf;
			PWSTR psz;
		};

		if (
			0 <= (status = NtQueryInformationProcess(hProcess, ProcessBasicInformation, &pbi, sizeof(pbi), 0)) &&
			0 <= (status = ZwReadVirtualMemory(hProcess, &((_PEB*)pbi.PebBaseAddress)->ProcessParameters, &ProcessParameters, sizeof(ProcessParameters), 0)) &&
			0 <= (status = ZwReadVirtualMemory(hProcess, &ProcessParameters->CommandLine, &CmdLine, sizeof(CmdLine), 0)) &&
			0 <= (status = ZwReadVirtualMemory(hProcess, CmdLine.Buffer, buf = alloca(CmdLine.Length + sizeof(WCHAR)), CmdLine.Length, 0))
			)
		{
			CmdLine.Buffer = psz;
			ShowCmdLineUI(UniqueProcess, &CmdLine);
		}

		NtClose(hProcess);
	}

	return status;
}

NTSTATUS (*ShowCmdLine)(HANDLE UniqueProcess);

void InitShowCmdLine()
{
	ShowCmdLine = g_nt_ver.Version < _WIN32_WINNT_WINBLUE ? ShowCmdLineOld : ShowCmdLine8;
}

void ShowCmdLineEx(HANDLE UniqueProcess)
{
	NTSTATUS status = ShowCmdLine(UniqueProcess);
	if (0 > status)
	{
		WCHAR caption[32];
		swprintf(caption, L"%X Process CmdLine", (DWORD)(ULONG_PTR)UniqueProcess);
		ShowNTStatus(0, status, caption);
	}
}

void ShowTypeInfo()
{
	if (ULONG n = g_AOTI.count())
	{
		const OBJECT_TYPE_INFORMATION* pti = g_AOTI;

		enum { size = 0x10000 };

		if (PVOID buf = LocalAlloc(0, size))
		{
			ULONG cch = size / sizeof(WCHAR);
			PWSTR psz = (PWSTR)buf;

			int len = swprintf_s(psz, cch, 
				L" I  Access       GE        GR        GW        GA        O        mO         H       mH        PT       IA    Name\r\n"
				L"==================================================================================================================\r\n");

			if (0 < len)
			{
				psz += len, cch -= len;

				do 
				{
					if (0 >= (len = swprintf_s(psz, cch, 
						L"%02x %08x { %08x, %08x, %08x, %08x} %08x(%08x) %08x(%08x) %08x %08x %wZ\r\n", 
						pti->TypeIndex, 
						pti->ValidAccessMask,
						pti->GenericMapping.GenericExecute,
						pti->GenericMapping.GenericRead,
						pti->GenericMapping.GenericWrite,
						pti->GenericMapping.GenericAll,
						pti->TotalNumberOfObjects,
						pti->HighWaterNumberOfObjects,
						pti->TotalNumberOfHandles,
						pti->HighWaterNumberOfHandles,
						pti->PoolType,
						pti->InvalidAttributes,
						&pti->TypeName))) break;

				} while (psz += len, cch -= len, pti++, --n);

				if (!n)
				{
					_ZGLOBALS* globals = ZGLOBALS::get();

					if (HWND hwnd = CreateWindowExW(0, WC_EDIT, L"Types", WS_OVERLAPPEDWINDOW|WS_VSCROLL|ES_MULTILINE,
						CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, globals->hwndMain, 0, 0, 0))
					{
						SendMessage(hwnd, WM_SETFONT, (WPARAM)globals->Font->getFont(), 0);

						SetEditText(hwnd, buf), buf = 0;
						ShowWindow(hwnd, SW_SHOW);
					}
				}
			}

			LocalFree(buf);
		}
	}
}

// from WRK\WRK-v1.2\public\sdk\inc\ntpsapi.h  

//
// Define process debug flags
//
#define PROCESS_DEBUG_INHERIT 0x00000001

NTSTATUS IsDebugInherit(HANDLE hProcess, BOOLEAN& bInherit)
{
	ULONG DebugFlags;

	// hProcess must have PROCESS_QUERY_INFORMATION access
	NTSTATUS status = NtQueryInformationProcess(hProcess, ProcessDebugFlags, &DebugFlags, sizeof(DebugFlags), 0);

	bInherit = DebugFlags & PROCESS_DEBUG_INHERIT;

	return status;
}

NTSTATUS SetDebugInherit(HANDLE hProcess, BOOLEAN bInherit)
{
	ULONG DebugFlags;

	// hProcess must have PROCESS_SET_INFORMATION|PROCESS_QUERY_INFORMATION access

	NTSTATUS status = NtQueryInformationProcess(hProcess, ProcessDebugFlags, &DebugFlags, sizeof(DebugFlags), 0);

	if (0 <= status)
	{
		bInherit ? DebugFlags |= PROCESS_DEBUG_INHERIT : DebugFlags &= ~PROCESS_DEBUG_INHERIT;

		status = NtSetInformationProcess(hProcess, ProcessDebugFlags, &DebugFlags, sizeof(DebugFlags));
	}

	return status;
}

NTSTATUS XxDebugActiveProcess(HANDLE hProcess)
{
	DoIoControl(IOCTL_SetProtectedProcess);
	NTSTATUS status = NtDebugActiveProcess(hProcess, DbgUiGetThreadDebugObject());
	DoIoControl(IOCTL_DelProtectedProcess);
	SetDebugInherit(hProcess, TRUE);
	return status;
}

NTSTATUS DebugActiveProcess(HANDLE hProcess)
{
	HANDLE hDebug;
	if (0 > ZwQueryInformationProcess(hProcess, ProcessDebugObjectHandle, &hDebug, sizeof(HANDLE), 0)) 
		return XxDebugActiveProcess(hProcess);

	ZwSuspendProcess(hProcess);
	NtRemoveProcessDebug(hProcess, hDebug);
	NtClose(hDebug);
	NTSTATUS status = XxDebugActiveProcess(hProcess);
	ZwResumeProcess(hProcess);
	return status;
}

void OnDropdownComboThreads(HWND hwndCtl, HANDLE dwProcessId)
{
	ComboBox_ResetContent(hwndCtl);

	COMBOBOXINFO cbi = { sizeof cbi };

	if (!GetComboBoxInfo(hwndCtl, &cbi)) return ;

	HDC hdc = ::GetDC(cbi.hwndList);
	if (!hdc) return ;
	HGDIOBJ o = (HGDIOBJ)SendMessage(cbi.hwndList, WM_GETFONT, 0, 0);
	if (o) o = SelectObject(hdc, o); else o = (HGDIOBJ)-1;

	SIZE size;
	RECT rc;
	GetClientRect(cbi.hwndList, &rc);
	SCROLLINFO si = { sizeof si };
	si.fMask = SIF_ALL;
	si.nPage = rc.right - rc.left;

	NTSTATUS status;
	DWORD cb = 0x20000;

	do 
	{
		status = STATUS_INSUFFICIENT_RESOURCES;

		if (PVOID buf = new UCHAR[cb])
		{
			if (0 <= (status = ZwQuerySystemInformation(SystemExtendedProcessInformation, buf, cb, &cb)))
			{
				union {
					PVOID pv;
					PBYTE pb;
					PSYSTEM_PROCESS_INFORMATION pspi;
				};

				pv = buf;

				ULONG NextEntryOffset = 0;
				do 
				{
					pb += NextEntryOffset;

					if (pspi->UniqueProcessId == dwProcessId)
					{
						if (ULONG NumberOfThreads = pspi->NumberOfThreads)
						{
							PSYSTEM_EXTENDED_THREAD_INFORMATION TH = pspi->TH;
							do 
							{
								if (!TH->Win32StartAddress) TH->Win32StartAddress = TH->StartAddress;

								WCHAR sz[128];

								swprintf(sz, L" %4X %2u %p %p %S ", 
									(DWORD)(ULONG_PTR)TH->ClientId.UniqueThread, TH->Priority,
									TH->Win32StartAddress, TH->TebAddress, TH->ThreadState == StateWait ? 
									GetThreadWaitReason(TH->WaitReason) : GetThreadStateName(TH->ThreadState));

								GetTextExtentPoint32(hdc, sz, (int)wcslen(sz), &size);

								if (si.nMax < size.cx) si.nMax = size.cx;

								ComboBox_AddStringEx(hwndCtl, sz, TH->ClientId.UniqueThread);

							} while (TH++, --NumberOfThreads);
						}

						break;
					}

				} while (NextEntryOffset = pspi->NextEntryOffset);
			}
			delete [] buf;
		}

	} while (status == STATUS_INFO_LENGTH_MISMATCH);

	ReleaseDC(cbi.hwndList, hdc);

	SendMessage(cbi.hwndList, LB_SETHORIZONTALEXTENT, si.nMax, 0);
	si.nMax += 16;
	SetScrollInfo(cbi.hwndList, SB_HORZ, &si, TRUE);
}

int OnDropDownProcesses(HWND hwndCtl, PHANDLE phSysToken = 0)
{
	ComboBox_ResetContent(hwndCtl);

	COMBOBOXINFO cbi = { sizeof cbi };

	if (!GetComboBoxInfo(hwndCtl, &cbi)) return -1;

	int index = - 1;

	PUCHAR buf;

	if (0 <= GetProcessList(&buf))
	{
		union {
			SYSTEM_PROCESS_INFORMATION* sp;
			PBYTE pb;
		};

		if (phSysToken)
		{
			GetSystemToken(buf, phSysToken);
		}

		if (HDC hdc = GetDC(cbi.hwndList))
		{
			HGDIOBJ o = (HGDIOBJ)SendMessage(cbi.hwndList, WM_GETFONT, 0, 0);
			if (o) o = SelectObject(hdc, o); else o = (HGDIOBJ)-1;

			DWORD NextEntryDelta = 0, cb = 0, rcb;
			PVOID stack = alloca(guz);
			PWSTR sz = 0;

			SIZE size;
			RECT rc;
			GetClientRect(cbi.hwndList, &rc);
			SCROLLINFO si = { sizeof si };
			si.fMask = SIF_ALL;
			si.nPage = rc.right - rc.left;
			HANDLE id = (HANDLE)GetCurrentProcessId();


#ifdef _WIN64
			OBJECT_ATTRIBUTES oa = { sizeof oa };
			CLIENT_ID cid = { };
#endif

			pb = buf;
			do
			{
				pb += NextEntryDelta;

				if (sp->UniqueProcessId)
				{
#ifdef _WIN64
					cid.UniqueProcess = (HANDLE)(ULONG_PTR)sp->UniqueProcessId;
					HANDLE hProcess;
					PVOID wow;
					WCHAR c = L'?';

					if (0 <= MyOpenProcess(&hProcess, PROCESS_QUERY_LIMITED_INFORMATION, &oa, &cid))
					{		
						if (0 <= ZwQueryInformationProcess(hProcess, ProcessWow64Information, &wow, sizeof(wow), 0))
						{
							c = wow ? '*' : ' ';
						}

						NtClose(hProcess);
					}
#else
					WCHAR c = L' ';
#endif

					rcb = sp->ImageName.Length + 64;

					if (cb < rcb) cb = RtlPointerToOffset(sz = (PWSTR)alloca(rcb - cb), stack);

					swprintf_s(sz, cb / sizeof(WCHAR), L" %4X(%4X) %d %3d %c %wZ ", 
						(DWORD)(ULONG_PTR)sp->UniqueProcessId, (DWORD)(ULONG_PTR)sp->InheritedFromUniqueProcessId, 
						sp->SessionId, sp->NumberOfThreads, c, &sp->ImageName);

					GetTextExtentPoint32(hdc, sz, (int)wcslen(sz), &size);

					if (si.nMax < size.cx) si.nMax = size.cx;

					int i = ComboBox_AddString(hwndCtl, sz);

					if (id == sp->UniqueProcessId) index = i;

					ComboBox_SetItemData(hwndCtl, i, sp->UniqueProcessId);
				}

			} while(NextEntryDelta = sp->NextEntryOffset);

			if (o != (HGDIOBJ)-1) SelectObject(hdc, o);

			ReleaseDC(cbi.hwndList, hdc);

			SendMessage(cbi.hwndList, LB_SETHORIZONTALEXTENT, si.nMax, 0);
			si.nMax += 16;
			SetScrollInfo(cbi.hwndList, SB_HORZ, &si, TRUE);

		}
		delete [] buf;
	}

	return index;
}

void OnDropdownComboDrivers(HWND hwndDlg, HWND hwndCtl)
{
	ComboBox_ResetContent(hwndCtl);

	COMBOBOXINFO cbi = { sizeof cbi };

	if (!GetComboBoxInfo(hwndCtl, &cbi)) return ;

	NTSTATUS status;
	DWORD cb = 0, rcb = 0x10000;
	PRTL_PROCESS_MODULES sp = 0;
	PVOID stack = alloca(guz);

	do 
	{
		if (cb < rcb) cb = RtlPointerToOffset(sp = (PRTL_PROCESS_MODULES)alloca(rcb - cb), stack);

		if (0 <= (status = ZwQuerySystemInformation(SystemModuleInformation, sp, cb, &rcb)))
		{
			if (int Num = sp->NumberOfModules)
			{
				PRTL_PROCESS_MODULE_INFORMATION csmi = sp->Modules;

				int ksort = 3, i;
				do ; while(ksort && (IsDlgButtonChecked(hwndDlg, IDC_RADIO29 + --ksort) != BST_CHECKED));

				PRTL_PROCESS_MODULE_INFORMATION* arr = (PRTL_PROCESS_MODULE_INFORMATION*)alloca(Num * sizeof PVOID);

				i = Num - 1; 

				do arr[i] = csmi + i; while (i--);

				if (ksort < 2)
				{
					SORTMODE rf;
					rf.mode = ksort;
					qsort(arr, Num, sizeof PVOID, (int (__cdecl *)(const void *, const void *))KModCompare);
				}

				HDC hdc = GetDC(cbi.hwndList);
				if (!hdc) return ;
				HGDIOBJ o = (HGDIOBJ)SendMessage(cbi.hwndList, WM_GETFONT, 0, 0);
				if (o) o = SelectObject(hdc, o); else o = (HGDIOBJ)-1;

				SIZE size;
				RECT rc;
				GetClientRect(cbi.hwndList, &rc);
				SCROLLINFO si = { sizeof si };
				si.fMask = SIF_ALL;
				si.nPage = rc.right - rc.left;

				WCHAR sz[MAX_PATH + 32];

				i = 0;
				do
				{
					csmi = arr[i];

					swprintf(sz, L"[%p %p) %S",
						csmi->ImageBase, (PBYTE)csmi->ImageBase + csmi->ImageSize,
						csmi->FullPathName + csmi->OffsetToFileName);

					GetTextExtentPoint32(hdc, sz, (int)wcslen(sz), &size);
					if (si.nMax < size.cx) si.nMax = size.cx;
					ComboBox_AddStringEx(hwndCtl, sz, csmi->ImageBase);

				} while(++csmi, ++i < Num);

				if (o != (HGDIOBJ)-1)SelectObject(hdc, o);

				ReleaseDC(cbi.hwndList, hdc);

				SendMessage(hwndCtl, CB_SETHORIZONTALEXTENT, si.nMax, 0);

				si.nMax += 16;
				SetScrollInfo(cbi.hwndList, SB_HORZ, &si, TRUE);
			}
		}

	} while(status == STATUS_INFO_LENGTH_MISMATCH);
}

void OnDropdownComboDlls(HWND hwndDlg, HWND hwndCtl, HANDLE dwProcessId)
{
	ComboBox_ResetContent(hwndCtl);

	COMBOBOXINFO cbi = { sizeof cbi };

	if (!GetComboBoxInfo(hwndCtl, &cbi)) return ;

	PRTL_PROCESS_MODULES psmi;
	NTSTATUS status = QueryPM(dwProcessId, &psmi);
	if (0 <= status)
	{
		if (int Num = psmi->NumberOfModules)
		{
			PRTL_PROCESS_MODULE_INFORMATION csmi = psmi->Modules;

			int ksort = 3, i;
			do ; while(ksort && (IsDlgButtonChecked(hwndDlg, IDC_RADIO29 + --ksort) != BST_CHECKED));

			PRTL_PROCESS_MODULE_INFORMATION* arr = (PRTL_PROCESS_MODULE_INFORMATION*)alloca(Num * sizeof PVOID);

			i = Num - 1; 

			do arr[i] = csmi + i; while (i--);

			if (ksort < 2) 
			{
				SORTMODE rf;
				rf.mode = -ksort;

				qsort(arr, Num, sizeof(PVOID), (int (__cdecl *)(const void *, const void *))KModCompare);
			}

			HDC hdc = ::GetDC(cbi.hwndList);
			HGDIOBJ o = (HGDIOBJ)::SendMessage(cbi.hwndList, WM_GETFONT, 0, 0);
			if (o) o = SelectObject(hdc, o); else o = (HGDIOBJ)-1;

			SIZE size;
			RECT rc;
			GetClientRect(cbi.hwndList, &rc);
			SCROLLINFO si = { sizeof si };
			si.fMask = SIF_ALL;
			si.nPage = rc.right - rc.left;

			WCHAR sz[MAX_PATH + 32];

			i = 0;
			do
			{
				csmi = arr[i];

				swprintf(sz, L"[%p %p%c %c %c %S",
					csmi->ImageBase, (PBYTE)csmi->ImageBase + csmi->ImageSize,
					csmi->Flags & LDRP_IMAGE_NOT_AT_BASE ? L'[' : L')',
					csmi->LoadCount == MAXUSHORT ? L'@' : L'$',
					csmi->Flags & LDRP_STATIC_LINK ? L's' : L'd',
					csmi->FullPathName);

				GetTextExtentPoint32(hdc, sz, (int)wcslen(sz), &size);
				if (si.nMax < size.cx) si.nMax = size.cx;
				ComboBox_AddStringEx(hwndCtl, sz, csmi->ImageBase);

			} while(++i < Num);

			EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON27), TRUE);

			if (o != (HGDIOBJ)-1)SelectObject(hdc, o);

			ReleaseDC(cbi.hwndList, hdc);

			SendMessage(hwndCtl, CB_SETHORIZONTALEXTENT, si.nMax, 0);

			si.nMax += 16;
			SetScrollInfo(cbi.hwndList, SB_HORZ, &si, TRUE);
		}

		FreePM(psmi);
	}
	else
	{
		ShowNTStatus(hwndDlg, status, L"Query Modules Fail");
	}
}

void CopyContext(HANDLE dwThreadId)
{
	CLIENT_ID cid = { 0, dwThreadId };
	HANDLE hThread;

	WCHAR sz[1024], caption[64], *psz = sz;
	swprintf(caption, L"%X Thread Context", (DWORD)(ULONG_PTR)dwThreadId);
	NTSTATUS status;
	if (0 <= (status = MyOpenThread(&hThread, THREAD_GET_CONTEXT|THREAD_QUERY_INFORMATION, &zoa, &cid)))
	{
		union {
			CONTEXT cntx;
#ifdef _WIN64
			WOW64_CONTEXT x86_ctx;
#endif
		};
		cntx.ContextFlags = CONTEXT_DEBUG_REGISTERS|CONTEXT_INTEGER|CONTEXT_CONTROL|CONTEXT_SEGMENTS;

		if (0 <= (status = ZwGetContextThread(hThread, &cntx)))
		{
			psz += swprintf(sz,
#ifdef _WIN64
				L"RAX=%p RBX=%p RCX=%p RDX=%p\r\n"
				L"RSI=%p RDI=%p RBP=%p RSP=%p\r\n"
				L"R8 =%p R9 =%p R10=%p R11=%p\r\n"
				L"R12=%p R13=%p R14=%p R15=%p\r\n"
				L"Dr0=%p Dr1=%p Dr2=%p Dr3=%p\r\n"
				L"Dr7=%p RIP=%p FLG=%08X\r\n"
				L"Cs=%X Ss=%X Ds=%X Es=%X Fs=%X Gs=%X\r\n", 
				cntx.Rax, cntx.Rbx, cntx.Rcx, cntx.Rdx,
				cntx.Rsi, cntx.Rdi, cntx.Rbp, cntx.Rsp,
				cntx.R8,  cntx.R9,  cntx.R10, cntx.R11,
				cntx.R12, cntx.R13, cntx.R14, cntx.R15,
				cntx.Dr0, cntx.Dr1, cntx.Dr2, cntx.Dr3,
				cntx.Dr7, cntx.Rip, cntx.EFlags, 
				cntx.SegCs,cntx.SegSs, cntx.SegDs, cntx.SegEs, cntx.SegFs,cntx.SegGs
#else
				L"EAX=%08X EBX=%08X ECX=%08X EDX=%08X\r\n"
				L"ESI=%08X EDI=%08X EBP=%08X ESP=%08X\r\n"
				L"Dr0=%08X Dr1=%08X Dr2=%08X Dr3=%08X\r\n"
				L"Dr7=%08X EIP=%08X EFLAGS=%08X\r\n"
				L"Cs=%X Ss=%X Ds=%X Es=%X Fs=%X\r\n", 
				cntx.Eax, cntx.Ebx, cntx.Ecx, cntx.Edx,
				cntx.Esi, cntx.Edi, cntx.Ebp, cntx.Esp,
				cntx.Dr0, cntx.Dr1, cntx.Dr2, cntx.Dr3,
				cntx.Dr7, cntx.Eip, cntx.EFlags,
				cntx.SegCs,cntx.SegSs, cntx.SegDs, cntx.SegEs, cntx.SegFs
#endif
				);
		}
#ifdef _WIN64
		x86_ctx.ContextFlags = WOW64_CONTEXT_DEBUG_REGISTERS|WOW64_CONTEXT_FULL;

		ULONG cb;
		if (0 <= ZwQueryInformationThread(hThread, ThreadWow64Context, &x86_ctx, sizeof(x86_ctx), &cb))
		{
			swprintf(psz, L"\r\n----- wow -----\r\n"
				L"EAX=%08X EBX=%08X ECX=%08X EDX=%08X\r\n"
				L"ESI=%08X EDI=%08X EBP=%08X ESP=%08X\r\n"
				L"Dr0=%08X Dr1=%08X Dr2=%08X Dr3=%08X\r\n"
				L"Dr7=%08X EIP=%08X EFLAGS=%08X\r\n"
				L"Cs=%X Ss=%X Ds=%X Es=%X Fs=%X\r\n", 
				x86_ctx.Eax, x86_ctx.Ebx, x86_ctx.Ecx, x86_ctx.Edx,
				x86_ctx.Esi, x86_ctx.Edi, x86_ctx.Ebp, x86_ctx.Esp,
				cntx.Dr0, cntx.Dr1, cntx.Dr2, cntx.Dr3,
				cntx.Dr7, cntx.Rip, cntx.EFlags,
				x86_ctx.SegCs,x86_ctx.SegSs, x86_ctx.SegDs, x86_ctx.SegEs, x86_ctx.SegFs
				);
		}
#endif
		NtClose(hThread);
	}
	
	if (0 > status)
	{
		FormatMessage(FORMAT_MESSAGE_IGNORE_INSERTS|FORMAT_MESSAGE_FROM_HMODULE,
			ZDll::_hmod_nt, status, 0, sz + swprintf(sz, L"%x: ", status) , RTL_NUMBER_OF(sz) - 16, 0);
	}

	ShowText(caption, sz);
}

struct ENUM_WND_DATA 
{
	PWSTR buf;
	PWSTR end;
	BOOL bVisibleOnly;
};

PWSTR PrintHWND(PWSTR buf, HWND hwnd, BOOLEAN bPrintId);

BOOL CALLBACK EnumWindowsProc(HWND hwnd, ENUM_WND_DATA& ctx)
{
	PWSTR buf = ctx.buf;

	if (buf + 512 > ctx.end) return FALSE;
	
	WINDOWINFO wi;
	wi.cbSize = sizeof(wi);

	if (GetWindowInfo(hwnd, &wi) && (wi.dwStyle & WS_VISIBLE || !ctx.bVisibleOnly))
	{
		*buf++ = wi.dwStyle & WS_VISIBLE ? '@' : ' ', *buf++ = ' ';

		buf = PrintHWND(buf, hwnd, TRUE);

		ULONG len;

		*buf++ = '<';
		if (len = GetClassName(hwnd, buf, 128))
		{
			buf += len;
		}
		else
		{
			buf += swprintf(buf, L"err=%u", GetLastError());
		}
		*buf++ = '>';

		*buf++=':';

		*buf++ = '<';

		if (len = GetWindowText(hwnd, buf, 256))
		{
			buf += len;
		}
		*buf++ = '>';

		ctx.buf = buf + swprintf(buf, L" (%d,%d)-(%d,%d) %ux%u\r\n", 
			wi.rcWindow.left, wi.rcWindow.top, wi.rcWindow.right, wi.rcWindow.bottom, 
			wi.rcWindow.right - wi.rcWindow.left, wi.rcWindow.bottom - wi.rcWindow.top);
	}

	return TRUE;
}

ZDbgDoc* GetDocumentByPID(ULONG dwProcessId);

class CMainDlg : public ZDlg, ZIdle
{
	HANDLE m_dwThreadId, m_dwProcessId;
	BOOL _fLoadEnabled;

	~CMainDlg()
	{
		if (ZToolBar* tb = ZGLOBALS::getMainFrame())
		{
			tb->EnableCmd(IDB_BITMAP20, TRUE);
		}
	}

	void SuspendResumeThread(HWND hwndDlg, NTSTATUS (*pfn)(HANDLE hThread, PULONG SuspendCount), int d)
	{
		if (m_dwThreadId)
		{
			HANDLE hThread;
			CLIENT_ID cid = { 0, m_dwThreadId };
			if (0 <= ZwOpenThread(&hThread, THREAD_SUSPEND_RESUME, &zoa, &cid))
			{
				ULONG SuspendCount;
				if (0 <= pfn(hThread, &SuspendCount))
				{
					WCHAR sz[16];
					swprintf(sz, L"SusCnt %X", SuspendCount + d);
					SetAndAnimate(GetDlgItem(hwndDlg, IDC_STATIC2), sz);
				}
				NtClose(hThread);
			}
		}
	}

	static void SuspendResumeProcess(HANDLE dwProcessId, NTSTATUS (*pfn)(HANDLE hThread))
	{
		if (dwProcessId)
		{
			HANDLE hThread;
			CLIENT_ID cid = { dwProcessId };
			if (0 <= MyOpenProcess(&hThread, PROCESS_SUSPEND_RESUME, &zoa, &cid))
			{
				pfn(hThread);
				NtClose(hThread);
			}
		}
	}

	void OnInitDialog(HWND hwndDlg)
	{
		COMBOBOXINFO cbi = { sizeof (cbi) };

		int i = IDC_COMBO4;
		do 
		{
			HWND hwndCtl = GetDlgItem(hwndDlg, i);
			GetComboBoxInfo(hwndCtl, &cbi);
			ComboBox_SetMinVisible(hwndCtl, 12);
			LONG style = GetWindowLong(cbi.hwndList, GWL_STYLE);
			if (!(style & WS_HSCROLL)) SetWindowLong(cbi.hwndList, GWL_STYLE, style | WS_HSCROLL);
		} while(i-- != IDC_COMBO1);

		CheckDlgButton(hwndDlg, IDC_RADIO31, BST_CHECKED);

		HWND hwnd = GetDlgItem(hwndDlg, IDC_COMBO5);
		static const PCWSTR szNum[] = { L"3", L"2", L"1", L"0" };
		i = _countof(szNum);
		do 
		{
			ComboBox_AddString(hwnd, szNum[--i]);
		} while (i);
		ComboBox_SetCurSel(hwnd, 3);

		hwnd = GetDlgItem(hwndDlg, IDC_COMBO6);
		static const PCWSTR szState[] = { L"R", L"D", L"W", L"E" };
		i = _countof(szState);
		do 
		{
			ComboBox_AddString(hwnd, szState[--i]);
		} while (i);
		ComboBox_SetCurSel(hwnd, 2);

		hwnd = GetDlgItem(hwndDlg, IDC_COMBO7);
		static const PCWSTR szLen[] = { L"4", L"2", L"1" };
		i = _countof(szLen);
		do 
		{
			ComboBox_AddString(hwnd, szLen[--i]);
		} while (i);
		ComboBox_SetCurSel(hwnd, 0);
	}

	virtual void OnIdle()
	{
		LoadOrEnable(FALSE);
	}

	void LoadOrEnable(BOOL fLoad)
	{
		if (HWND hwnd = GetDlgItem(getHWND(), IDC_COMBO4))
		{
			int i = ComboBox_GetCurSel(hwnd);
			if (0 <= i)
			{
				INT_PTR Base = ComboBox_GetItemData(hwnd, i);
				if (Base < 0)
				{
					if (ZSDIFrameWnd* pFrame = ZGLOBALS::getMainFrame())
					{
						if (ZDocument* pDoc = pFrame->GetActiveDoc())
						{
							ZDbgDoc* pDbg;
							if (0 <= pDoc->QI(IID_PPV(pDbg)))
							{
								if (!pDbg->getDllByBaseNoRefNoParse((PVOID)Base))
								{
									if (fLoad)
									{
										fLoad = FALSE;
										WCHAR sz[300], *c = sz, *end = sz + RTL_NUMBER_OF(sz);
										if (ComboBox_GetText(hwnd, sz, RTL_NUMBER_OF(sz)))
										{
											int n = 2;
											do 
											{
											} while ((c = wtrnchr(end - c, c, ' ')) && --n);

											if (c)
											{
												DBGKM_LOAD_DLL LoadDll = { 0, (PVOID)Base, 0, 0, c };
												pDbg->Load(&LoadDll, FALSE);
											}
										}
									}
									else
									{
										fLoad = TRUE;
									}
								}
								pDbg->Release();
							}
						}
					}
				}
			}
		}

		if (_fLoadEnabled != fLoad)
		{
			EnableWindow(GetDlgItem(getHWND(), IDC_BUTTON29), fLoad);
			_fLoadEnabled = fLoad;
		}
	}

	void Attach(HWND hwndDlg)
	{
		if (m_dwProcessId)
		{
			NTSTATUS status = STATUS_UNSUCCESSFUL;

			if (ZDbgDoc* p = new ZDbgDoc(FALSE))
			{
				if (0 > (status = p->Attach((DWORD)(ULONG_PTR)m_dwProcessId)))
				{
					p->Rundown();
				}
				p->Release();
			}

			if (0 > status)
			{
				ShowNTStatus(hwndDlg, status, L"Atach to Process Fail");
			}
			else
			{
				DestroyWindow(hwndDlg);
			}
		}
	}

	void Debug(HWND hwndDlg)
	{
		CLIENT_ID cid = { m_dwProcessId };
		
		if (cid.UniqueProcess)
		{
			HANDLE hProcess;
			NTSTATUS status = MyOpenProcess(&hProcess, PROCESS_ALL_ACCESS_XP, &zoa, &cid);

			if (0 <= status)
			{
				status = DebugActiveProcess(hProcess);
				NtClose(hProcess);
			}

			if (0 > status)
			{
				ShowNTStatus(hwndDlg, status, L"Debug Process Fail");
			}
			else
			{
				DestroyWindow(hwndDlg);
			}
		}
	}

	virtual INT_PTR DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		NTSTATUS status;
		union {
			HANDLE hProcess;
			HANDLE hThread;
		};
		HANDLE hToken;
		union {
			CLIENT_ID cid;
			CONTEXT ctx;
		};
		PCWSTR caption;
		WCHAR sz[17], *c;

		switch (uMsg)
		{
		case WM_DESTROY:
			if (g_hDrv) Remove();
			break;

		case WM_INITDIALOG:
			OnInitDialog(hwndDlg);
			if (g_hDrv) Insert();
			break;

		case WM_COMMAND:
			switch (wParam)
			{
			case IDCANCEL:
				DestroyWindow(hwndDlg);
				break;

			case IDC_BUTTON28:
				if (m_dwProcessId)
				{
					ShowImagePath(hwndDlg, m_dwProcessId);
				}
				break;
			
			case MAKEWPARAM(IDC_EDIT7, EN_CHANGE):
				cid.UniqueThread = 0;
				if (GetDlgItemText(hwndDlg, IDC_EDIT7, sz, RTL_NUMBER_OF(sz)))
				{
					cid.UniqueThread = (HANDLE)(ULONG_PTR)wcstoul(sz, &c, 16);
					if (*c)
					{
						cid.UniqueThread = 0;
					}
				}
				_OnChangeThreads(hwndDlg, cid.UniqueThread);
				break;

			case MAKEWPARAM(IDC_EDIT8, EN_CHANGE):
				cid.UniqueProcess = 0;
				if (GetDlgItemText(hwndDlg, IDC_EDIT8, sz, RTL_NUMBER_OF(sz)))
				{
					cid.UniqueProcess = (HANDLE)(ULONG_PTR)wcstoul(sz, &c, 16);
					if (*c)
					{
						cid.UniqueProcess = 0;
					}
				}
				_OnChangeProcess(hwndDlg, cid.UniqueProcess);
				break;

			case IDC_BUTTON23:
				if (hwndDlg = CreateWindowExW(0, WC_TREEVIEW, L"Windows", WS_CAPTION|
					WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_THICKFRAME |
					TVS_LINESATROOT|TVS_HASLINES|TVS_HASBUTTONS|TVS_DISABLEDRAGDROP|
					TVS_TRACKSELECT|TVS_EDITLABELS, 
					CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, ZGLOBALS::getMainHWND(), 0, 0, 0))
				{
					SendMessage(hwndDlg, WM_SETFONT, (WPARAM)ZGLOBALS::getFont()->getFont(), 0);
					EnumAllWins(hwndDlg);
				}
				break;

			case IDC_BUTTON24:
				PrintWindows();
				break;

			case IDC_BUTTON32:
				if (PWSTR buf = new WCHAR[0x10000])
				{
					ENUM_WND_DATA ewd = { buf, buf + 0x10000, IsDlgButtonChecked(hwndDlg, IDC_CHECK1) };
					EnumWindows((WNDENUMPROC)EnumWindowsProc, (LPARAM)&ewd);
					ShowText(L"Windows", buf);
					delete [] buf;
				}
				break;

			case IDC_BUTTON25:
				if (ZHandlesDlg* p = new ZHandlesDlg)
				{
					p->Create((HINSTANCE)&__ImageBase, MAKEINTRESOURCE(ZHandlesDlg::IDD), ZGLOBALS::getMainHWND(), 0);
					p->Release();
				}
				break;

			case IDC_BUTTON26:
				ShowTypeInfo();
				break;

			case IDC_BUTTON29:
				LoadOrEnable(TRUE);
				break;

			case IDC_BUTTON2:
				Debug(hwndDlg);
				break;

			case IDC_BUTTON1:
				Attach(hwndDlg);
				break;

			case IDC_BUTTON18:
				if (m_dwThreadId)
				{
					//if (m_dwThreadId != (HANDLE)(ULONG_PTR)GetCurrentThreadId()) Impersonate();
					caption = L"Fail Open Thread";
					cid.UniqueProcess = 0;
					cid.UniqueThread = m_dwThreadId;
					if (0 <= (status = ZwOpenThread(&hThread, 
						THREAD_QUERY_LIMITED_INFORMATION, 
						&zoa, &cid)))
					{
						caption = L"Fail Open Thread Token";

						status = NtOpenThreadTokenEx(hThread, TOKEN_QUERY_SOURCE|TOKEN_QUERY|READ_CONTROL, FALSE, 0, &hToken);

						if (status == STATUS_ACCESS_DENIED)
						{
							if (0 <= ImpersonateSystemToken())
							{
								status = NtOpenThreadTokenEx(hThread, TOKEN_QUERY_SOURCE|TOKEN_QUERY|READ_CONTROL, FALSE, 0, &hToken);

								RtlRevertToSelf();
							}
						}

						NtClose(hThread);

						if (0 <= status)
						{
							ShowToken(hToken, (ULONG)(ULONG_PTR)cid.UniqueThread, L"Thread");
							//NtClose(hToken);
						}
					}

					if (0 > status)
					{
						ShowNTStatus(hwndDlg, status, caption);
					}
					//if (m_dwThreadId != (HANDLE)(ULONG_PTR)GetCurrentThreadId()) RevertToSelf();
				}
				break;

			case IDC_BUTTON9:
				if (m_dwProcessId)
				{
					caption = L"Fail Open Process";
					cid.UniqueProcess = m_dwProcessId;
					cid.UniqueThread = 0;
					
					if (0 <= (status = ZwOpenProcess(&hProcess, 
						PROCESS_QUERY_LIMITED_INFORMATION, 
						&zoa, &cid)))
					{
						caption = L"Fail Open Process Token";
						status = NtOpenProcessToken(hProcess, TOKEN_QUERY_SOURCE|TOKEN_QUERY|READ_CONTROL, &hToken);

						if (status == STATUS_ACCESS_DENIED)
						{
							if (0 <= ImpersonateSystemToken())
							{
								status = NtOpenProcessToken(hProcess, TOKEN_QUERY_SOURCE|TOKEN_QUERY|READ_CONTROL, &hToken);

								RtlRevertToSelf();
							}
						}
						NtClose(hProcess);

						if (0 <= status)
						{
							ShowToken(hToken, (ULONG)(ULONG_PTR)cid.UniqueProcess, L"Process");
							//NtClose(hToken);
						}
					}

					if (0 > status)
					{
						ShowNTStatus(hwndDlg, status, caption);
					}
				}
				break;

			case IDC_BUTTON20:
				if (m_dwThreadId)
				{
					cid.UniqueProcess = 0;
					cid.UniqueThread = m_dwThreadId;
					if (0 <= (status = MyOpenThread(&hThread, READ_CONTROL, &zoa, &cid)))
					{
						ShowObjectSecurity(hThread, (ULONG)(ULONG_PTR)cid.UniqueThread, L"Thread");
						//NtClose(hThread);
					}
					else
					{
						ShowNTStatus(hwndDlg, status, L"Fail Open Thread");
					}
				}
				break;

			case IDC_BUTTON10:
				if (m_dwProcessId)
				{
					cid.UniqueProcess = m_dwProcessId;
					cid.UniqueThread = 0;
					if (0 <= (status = MyOpenProcess(&hProcess, READ_CONTROL, &zoa, &cid)))
					{
						ShowObjectSecurity(hProcess, (ULONG)(ULONG_PTR)cid.UniqueProcess, L"Process");
						//NtClose(hProcess);
					}
					else
					{
						ShowNTStatus(hwndDlg, status, L"Fail Open Process");
					}
				}
				break;

			case IDC_BUTTON4:
				if (m_dwProcessId)
				{
					ZVmDialog::Create(m_dwProcessId);
				}
				break;

			case IDC_BUTTON5:
				SuspendResumeProcess(m_dwProcessId, ZwSuspendProcess);
				break;

			case IDC_BUTTON6:
				SuspendResumeProcess(m_dwProcessId, ZwResumeProcess);
				break;

			case IDC_BUTTON7:
				if (m_dwProcessId)
				{
					ShowProcessHandles((DWORD)(ULONG_PTR)m_dwProcessId);
				}
				break;

			case IDC_BUTTON3:
				if (m_dwProcessId)
				{
					ShowProcessMemory((DWORD)(ULONG_PTR)m_dwProcessId);
				}
				break;

			case IDC_BUTTON8:
				if (m_dwProcessId)
				{
					ShowCmdLineEx(m_dwProcessId);
				}
				break;

			case IDC_BUTTON27:
				if (cid.UniqueProcess = m_dwProcessId)
				{
					HWND hwndCB = GetDlgItem(hwndDlg, IDC_COMBO2);
					int i = ComboBox_GetCurSel(hwndCB);
					if (0 <= i)
					{
						if (HMODULE hmod = (HMODULE)ComboBox_GetItemData(hwndCB, i))
						{
							cid.UniqueThread = 0;
							status = RemoteUnload(hmod, &cid);

							if (0 > status)
							{
								ShowNTStatus(hwndDlg, status, L"RemoteUnload");
							}
							else
							{
								MessageBox(0, 0, L"Unload Ok", 0);
							}
						}
					}
				}
				break;

			case IDC_BUTTON12:
				if (m_dwProcessId)
				{
					CInjectDlg dlg(m_dwProcessId);
					dlg.DoModal((HINSTANCE)&__ImageBase, MAKEINTRESOURCEW(IDD_DIALOG22), hwndDlg, 0);
				}
				break;

			case IDC_BUTTON15:
				if (m_dwThreadId)
				{
					ULONG m[3], i = _countof(m), Dr7h = 0, Dr7l = 0, mh = 0xF0000, ml = 1;
					do 
					{
						int s = ComboBox_GetCurSel(GetDlgItem(hwndDlg, IDC_COMBO5 + --i));
						if (0 > s)
						{
							return 0;
						}
						m[i] = s;
					} while (i);

					if ((i = m[0]) > 3)
					{
						return 0;
					}

					if (!GetDlgItemTextW(hwndDlg, IDC_EDIT1, sz, _countof(sz)))
					{
						return 0;
					}

					ULONG_PTR DrN = (ULONG_PTR)_wcstoui64(sz, &c, 16);

					if (*c)
					{
						return 0;
					}

					switch (m[1])
					{
					case 0:// execute
						Dr7l = 0x1;
						goto __E;
					case 1:// write
						Dr7h = 0x10000;
						Dr7l = 0x1;
						break;
					case 2://disable
						Dr7h = 0x20000;
						break;
					case 3:// read
						Dr7h = 0x30000;
						Dr7l = 0x1;
						break;
					default:
						return 0;
					}

					switch (m[2])
					{
					case 0:// 1 byte
						break;
					case 1:// 2 byte
						Dr7h |= 0x40000;
						break;
					case 2://4 byte
						Dr7h |= 0xc0000;
						break;
					default:
						return 0;
					}

__E:
					RtlZeroMemory(&ctx, sizeof(ctx));
					ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;

					PCONTEXT pReg = 0;

					if (ZDbgDoc* pDoc = ZDbgDoc::find((ULONG)(ULONG_PTR)m_dwProcessId))
					{
						if (pDoc->IsCurrentThread((ULONG)(ULONG_PTR)m_dwThreadId))
						{
							pReg = pDoc->getContext();
							memcpy(&ctx, pReg, sizeof(CONTEXT));
						}
						pDoc->Release();
					}
					
					cid.UniqueProcess = 0;
					cid.UniqueThread = m_dwThreadId;

					if (0 <= (status = pReg ? STATUS_SUCCESS : 
						MyOpenThread(&hThread, THREAD_SET_CONTEXT|THREAD_GET_CONTEXT, &zoa, &cid)))
					{
						*(&ctx.Dr0 + i)= DrN;

						ctx.Dr7 &= ~(ULONG_PTR)((mh << 4*i)|(ml << 2*i));
						ctx.Dr7 |= (Dr7h << 4*i)|(Dr7l << 2*i);
						ctx.Dr6 = 0;

						if (pReg)
						{
							static_cast<ZRegView*>(pReg)->SetContext(&ctx);
						}
						else
						{
							status = MySetContextThread(hThread, &ctx);
							NtClose(hThread);
						}
					}

					if (0 > status)
					{
						ShowNTStatus(hwndDlg, status, L"Set DrX");
					}
				}
				break;

			case IDC_BUTTON11:
				if (m_dwProcessId)
				{
					if (MessageBoxW(hwndDlg, L"Are you sure ?", L"Terminate Process", MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2) == IDYES)
					{
						cid.UniqueProcess = m_dwProcessId;
						cid.UniqueThread = 0;
						if (0 <= (status = MyOpenProcess(&hProcess, PROCESS_TERMINATE , &zoa, &cid)))
						{
							status = ZwTerminateProcess(hProcess, 0);
							NtClose(hProcess);
						}
						if (0 > status)
						{
							ShowNTStatus(hwndDlg, status, L"Terminate Process");
						}
					}
				}
				break;

			case IDC_BUTTON22:
				if (m_dwThreadId)
				{
					if (MessageBoxW(hwndDlg, L"Are you sure ?", L"Terminate Thread", MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2) == IDYES)
					{
						cid.UniqueProcess = 0;
						cid.UniqueThread = m_dwThreadId;
						if (0 <= (status = MyOpenThread(&hThread, THREAD_TERMINATE, &zoa, &cid)))
						{
							status = ZwTerminateThread(hThread, 0);
							NtClose(hThread);
						}
						if (0 > status)
						{
							ShowNTStatus(hwndDlg, status, L"Terminate Thread");
						}
					}
				}
				break;

			case IDC_BUTTON17:
				SuspendResumeThread(hwndDlg, ZwResumeThread, -1);
				break;
			case IDC_BUTTON19:
				SuspendResumeThread(hwndDlg, ZwSuspendThread, +1);
				break;

			case IDC_BUTTON16:
				if (m_dwThreadId)
				{
					ShowThreadWindows((DWORD)(ULONG_PTR)m_dwThreadId);
				}
				break;
			case IDC_BUTTON14:
				if (m_dwThreadId)
				{
					cid.UniqueProcess = 0;
					cid.UniqueThread = m_dwThreadId;
					if (0 <= MyOpenThread(&hThread, THREAD_ALERT, &zoa, &cid))
					{
						ZwAlertThread(hThread);
						NtClose(hThread);
					}
				}
				break;
			case IDC_BUTTON21:
				if (m_dwThreadId)
				{
					cid.UniqueProcess = 0;
					cid.UniqueThread = m_dwThreadId;
					if (0 <= ZwOpenThread(&hThread, THREAD_GET_CONTEXT|THREAD_SET_CONTEXT|THREAD_SUSPEND_RESUME, &zoa, &cid))
					{
						if (0 <= ZwSuspendThread(hThread, 0))
						{
							ctx.ContextFlags = CONTEXT_CONTROL;
							if (0 <= ZwGetContextThread(hThread, &ctx))
							{
								ctx.EFlags |= TRACE_FLAG;
								MySetContextThread(hThread, &ctx);
							}
							ZwResumeThread(hThread, 0);
						}
						NtClose(hThread);
					}
				}
				break;
			case IDC_BUTTON13:
				CopyContext(m_dwThreadId);
				break;

			case IDC_BUTTON31:
				if (g_hDrv)
				{
					if (GetDlgItemText(hwndDlg, IDC_EDIT5, sz, RTL_NUMBER_OF(sz)))
					{
						ULONG_PTR id = wcstoul(sz, &c, 16);
						if (id && !*c)
						{
							DWORD ioctl = 0;
							if (IsDlgButtonChecked(hwndDlg, IDC_RADIO32) == BST_CHECKED)
							{
								ioctl = IOCTL_LookupThreadByThreadId;
							}
							else if (IsDlgButtonChecked(hwndDlg, IDC_RADIO33) == BST_CHECKED)
							{
								ioctl = IOCTL_LookupProcessByProcessId;
							}
							if (ioctl)
							{
								IO_STATUS_BLOCK iosb;
								if (0 <= ZwDeviceIoControlFile(g_hDrv, 0, 0, 0, &iosb, ioctl, &id, sizeof(id), 0, 0))
								{
									swprintf(sz, L"%p", iosb.Information);
									SetAndAnimate(GetDlgItem(hwndDlg, IDC_EDIT6), sz);
								}
							}
						}
					}
				}
				break;

			case MAKEWPARAM(IDC_COMBO2, CBN_DROPDOWN):
				OnDropdownComboDlls(hwndDlg, (HWND)lParam, m_dwProcessId);
				break;
			case MAKEWPARAM(IDC_COMBO4, CBN_DROPDOWN):
				OnDropdownComboDrivers(hwndDlg, (HWND)lParam);
				break;
			case MAKEWPARAM(IDC_COMBO1, CBN_DROPDOWN):
				OnDropdownComboThreads((HWND)lParam, m_dwProcessId);
				break;
			case MAKEWPARAM(IDC_COMBO1, CBN_CLOSEUP):
				OnCloseComboThreads(hwndDlg);
				break;
			case MAKEWPARAM(IDC_COMBO1, CBN_SELCHANGE):
				OnSelChangeComboThreads(hwndDlg, (HWND)lParam);
				break;
			case MAKEWPARAM(IDC_COMBO3, CBN_DROPDOWN):
				OnDropDownProcesses((HWND)lParam);
				break;
			case MAKEWPARAM(IDC_COMBO3, CBN_CLOSEUP):
				OnCloseComboProcess(hwndDlg);
				break;
			case MAKEWPARAM(IDC_COMBO3, CBN_SELCHANGE):
				OnSelChangeComboProcess(hwndDlg, (HWND)lParam);
				break;

			case MAKEWPARAM(IDC_EDIT1, EN_CHANGE):
				EnableDR_Value(hwndDlg, TRUE, (HWND)lParam);
				break;
			}
			break;
		}

		return ZDlg::DialogProc(hwndDlg, uMsg, wParam, lParam);
	}

	void OnSelChangeComboThreads(HWND hwndDlg, HWND hwndCtl)
	{
		int i = ComboBox_GetCurSel(hwndCtl);

		OnChangeThreads(hwndDlg, 0 > i ? 0 : (HANDLE)ComboBox_GetItemData(hwndCtl, i));
	}

	void OnCloseComboThreads(HWND hwndDlg)
	{
		if (0 > ComboBox_GetCurSel(GetDlgItem(hwndDlg, IDC_COMBO1))) OnChangeThreads(hwndDlg, 0);
	}

	void OnChangeThreads(HWND hwndDlg, HANDLE ThreadId)
	{
		WCHAR szid[16];
		swprintf(szid, L"%x", (ULONG)(ULONG_PTR)ThreadId);
		SetDlgItemText(hwndDlg, IDC_EDIT7, szid);
	}

	static void EnableDR_Value(HWND hwndDlg, BOOL bThreadId, HWND hwnd)
	{
		if (bThreadId)
		{
			WCHAR sz[17], *psz;
			ULONG64 v = 0;
			if (GetWindowTextW(hwnd, sz, _countof(sz)))
			{
				v = _wcstoui64(sz, &psz, 16);
				bThreadId = !*psz;
			}
			else
			{
				bThreadId = FALSE;
			}
		}

		EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON15), bThreadId);
	}

	void _OnChangeThreads(HWND hwndDlg, HANDLE ThreadId)
	{
		m_dwThreadId = ThreadId;

		BOOL bThreadId = ThreadId != 0;

		int i = IDC_BUTTON22;
		do EnableWindow(GetDlgItem(hwndDlg, i), bThreadId); while(i-- != IDC_BUTTON13);

		SetDlgItemText(hwndDlg, IDC_STATIC2, L"");

		HWND hwnd = GetDlgItem(hwndDlg, IDC_COMBO5);
		EnableWindow(hwnd, bThreadId);

		if (bThreadId && 0 > ComboBox_GetCurSel(hwnd))
		{
			bThreadId = FALSE;
		}

		EnableWindow(hwnd = GetDlgItem(hwndDlg, IDC_COMBO6), bThreadId);

		if (bThreadId && 0 > ComboBox_GetCurSel(hwnd))
		{
			bThreadId = FALSE;
		}

		EnableWindow(hwnd = GetDlgItem(hwndDlg, IDC_COMBO7), bThreadId);

		if (bThreadId && 0 > ComboBox_GetCurSel(hwnd))
		{
			bThreadId = FALSE;
		}

		EnableWindow(hwnd = GetDlgItem(hwndDlg, IDC_EDIT1), bThreadId);

		EnableDR_Value(hwndDlg, bThreadId, hwnd);
	}

	void OnChangeProcess(HWND hwndDlg, HANDLE UniqueProcessId)
	{
		if (m_dwProcessId != UniqueProcessId)
		{
			WCHAR szid[16];
			swprintf(szid, L"%x", (ULONG)(ULONG_PTR)UniqueProcessId);
			SetDlgItemText(hwndDlg, IDC_EDIT8, szid);
		}
	}

	void _OnChangeProcess(HWND hwndDlg, HANDLE UniqueProcessId)
	{
		if (m_dwProcessId != UniqueProcessId)
		{
			m_dwProcessId = UniqueProcessId;

			ComboBox_ResetContent(GetDlgItem(hwndDlg, IDC_COMBO1));
			ComboBox_ResetContent(GetDlgItem(hwndDlg, IDC_COMBO2));
			EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON27), FALSE);
			OnChangeThreads(hwndDlg, 0);
		}

		BOOL bEnable = UniqueProcessId != 0;

		EnableWindow(GetDlgItem(hwndDlg, IDC_COMBO1), bEnable);
		EnableWindow(GetDlgItem(hwndDlg, IDC_COMBO2), bEnable);
		int i = IDC_BUTTON12, j = UniqueProcessId ? IDC_BUTTON3 : IDC_BUTTON1;
		do EnableWindow(GetDlgItem(hwndDlg, i), bEnable); while(i-- != j);

		EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON28), bEnable);

		if (bEnable)
		{
			if (ZDbgDoc* pDbg = ZDbgDoc::find((DWORD)(ULONG_PTR)UniqueProcessId))
			{
				bEnable = FALSE;
				pDbg->Release();
			}

			EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON1), bEnable);
			EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON2), bEnable && (UniqueProcessId != (HANDLE)GetCurrentProcessId()));
		}
	}

	void OnCloseComboProcess(HWND hwndDlg)
	{
		if (0 > ComboBox_GetCurSel(GetDlgItem(hwndDlg, IDC_COMBO3))) OnChangeProcess(hwndDlg, 0);
	}

	void OnSelChangeComboProcess(HWND hwndDlg, HWND hwndCtl)
	{
		int i = ComboBox_GetCurSel(hwndCtl);

		OnChangeProcess(hwndDlg, 0 > i ? 0 : (HANDLE)ComboBox_GetItemData(hwndCtl, i));
	}

public:

	CMainDlg()
	{
		m_dwThreadId = 0, m_dwProcessId = 0, _fLoadEnabled = FALSE;
		if (ZToolBar* tb = ZGLOBALS::getMainFrame())
		{
			tb->EnableCmd(IDB_BITMAP20, FALSE);
		}
	}
};

HWND CMainDlg_Create(HWND hwndParent)
{
	if (CMainDlg* p = new CMainDlg)
	{
		hwndParent = p->Create((HINSTANCE)&__ImageBase, MAKEINTRESOURCE(IDD_DIALOG0), hwndParent, 0);
		p->Release();
		return hwndParent;
	}
	return 0;
}

_NT_END