#include "stdafx.h"

_NT_BEGIN

#include "zdlgs.h"
#include "eval64.h"
#include "resource.h"
#include "../inc/idcres.h"
#include "DbgDoc.h"
#include "undname.h"
#include "../inc/rtlframe.h"
#include "common.h"
#include "types.h"

#define FormatStatus(err, module, status) FormatMessage(\
	FORMAT_MESSAGE_IGNORE_INSERTS|FORMAT_MESSAGE_FROM_HMODULE,\
	GetModuleHandleW(L ## # module),status, 0, err, RTL_NUMBER_OF(err), 0)

#define FormatWin32Status(err, status) FormatStatus(err, kernel32.dll, status)
#define FormatNTStatus(err, status) FormatStatus(err, ntdll.dll, status)

//////////////////////////////////////////////////////////////////////////
// ZPDBPathDlg

NTSTATUS OpenDosFile(PHANDLE hFile, PCWSTR FilePath);
NTSTATUS OpenNtFile(PHANDLE hFile, PCWSTR FilePath);

HRESULT OnBrowse(_In_ HWND hwndDlg, 
				 _In_ UINT cFileTypes, 
				 _In_ const COMDLG_FILTERSPEC *rgFilterSpec, 
				 _Out_ PWSTR* ppszFilePath, 
				 _In_ UINT iFileType = 0,
				 _In_ const CLSID* pclsid = &__uuidof(FileOpenDialog),
				 _In_ PCWSTR pszDefaultExtension = 0)
{
	IFileDialog *pFileOpen;

	HRESULT hr = CoCreateInstance(*pclsid, NULL, CLSCTX_ALL, IID_PPV_ARGS(&pFileOpen));

	if (SUCCEEDED(hr))
	{
		pFileOpen->SetOptions(FOS_NOVALIDATE|FOS_NOTESTFILECREATE|
			FOS_NODEREFERENCELINKS|FOS_DONTADDTORECENT|FOS_FORCESHOWHIDDEN);

		if (pszDefaultExtension)
		{
			pFileOpen->SetDefaultExtension(pszDefaultExtension);
		}

		if (0 <= (hr = pFileOpen->SetFileTypes(cFileTypes, rgFilterSpec)) && 
			0 <= (hr = pFileOpen->SetFileTypeIndex(1 + iFileType)) && 
			0 <= (hr = pFileOpen->Show(hwndDlg)))
		{
			IShellItem *pItem;
			hr = pFileOpen->GetResult(&pItem);

			if (SUCCEEDED(hr))
			{
				hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, ppszFilePath);
				pItem->Release();
			}
		}
		pFileOpen->Release();
	}

	return hr;
}

void OnBrowse(HWND hwndDlg, 
			  UINT nIDDlgItem, 
			  UINT cFileTypes, 
			  const COMDLG_FILTERSPEC *rgFilterSpec, 
			  _In_ UINT iFileType = 0,
			  _In_ const CLSID* pclsid = &__uuidof(FileOpenDialog),
			  _In_ PCWSTR pszDefaultExtension = 0)
{
	PWSTR pszFilePath;
	HRESULT hr = OnBrowse(hwndDlg, cFileTypes, rgFilterSpec, &pszFilePath, iFileType, pclsid, pszDefaultExtension);

	if (SUCCEEDED(hr))
	{
		SetDlgItemTextW(hwndDlg, nIDDlgItem, pszFilePath);
		CoTaskMemFree(pszFilePath);
	}
}

BOOL ZPDBPathDlg::OnOk(HWND hwndDlg)
{
	NTSTATUS status = -1;
	HWND hwnd;
	if (int len = GetWindowTextLength(hwnd = GetDlgItem(hwndDlg, IDC_EDIT1)))
	{
		PWSTR sz = (PWSTR)alloca((len + 1) << 1), name;
		GetWindowText(hwnd, sz, len + 1);

		UNICODE_STRING ObjectName;

		if (RtlDosPathNameToNtPathName_U(sz, &ObjectName, &name, 0))
		{
			if (!name || _wcsicmp(name, _PdbFileName))
			{
				MessageBox(hwndDlg, name, L"file name mismatch !", MB_ICONWARNING);
			}
			else
			{
				OBJECT_ATTRIBUTES oa = { sizeof(oa), 0, &ObjectName };
				IO_STATUS_BLOCK iosb;
				status = NtOpenFile(_hFile, FILE_GENERIC_READ, &oa, &iosb, FILE_SHARE_VALID_FLAGS, FILE_SYNCHRONOUS_IO_NONALERT);
				if (0 > status)
				{
					ShowNTStatus(hwndDlg, status, sz);
				}
			}
			RtlFreeUnicodeString(&ObjectName);
		}
	}

	return 0 <= status;
}

#ifdef _WIN64
#define CD_MAGIC 0x8e3420ad9691DAE6
#else
#define CD_MAGIC 0x9691DAE6
#endif

INT_PTR ZPDBPathDlg::DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_COMMAND:
		switch (wParam)
		{
		case IDOK:
			if (OnOk(hwndDlg))
			{
				EndDialog(hwndDlg, 0);
			}
			break;
		case IDCANCEL:
			EndDialog(hwndDlg, -1);
			break;
		}
		break;

	case WM_DESTROY:
		if (_getPdbDlg)
		{
			COPYDATASTRUCT cds = { CD_MAGIC };
			SendMessageW(_getPdbDlg, WM_COPYDATA, 0, (LPARAM)&cds);
			SetWindowLongPtrW(_getPdbDlg, GWLP_HWNDPARENT, 0);
		}
		break;

	case WM_INITDIALOG:
		PGUID Signature = _Signature;
		PWSTR sz = (PWSTR)alloca((wcslen(_NtSymbolPath)+2*wcslen(_PdbFileName)+44)*sizeof(WCHAR));

		ULONG len = swprintf(sz, L"%s%c%s*%08X%04X%04X%02X%02X%02X%02X%02X%02X%02X%02X%x", 
			_NtSymbolPath, 0, _PdbFileName,
			Signature->Data1, Signature->Data2, Signature->Data3,
			Signature->Data4[0], Signature->Data4[1], Signature->Data4[2], Signature->Data4[3],
			Signature->Data4[4], Signature->Data4[5], Signature->Data4[6], Signature->Data4[7],
			_Age);

		if (_getPdbDlg = FindWindowW(L"#32770", L"Download PDB file"))
		{
			COPYDATASTRUCT cds = { 
				CD_MAGIC, (len + 1)*sizeof(WCHAR), const_cast<PWSTR>(sz) 
			};
			if (SendMessageW(_getPdbDlg, WM_COPYDATA, (WPARAM)hwndDlg, (LPARAM)&cds) == CD_MAGIC)
			{
				SetWindowLongPtrW(_getPdbDlg, GWLP_HWNDPARENT, (LONG_PTR)hwndDlg);
			}
		}

		SetDlgItemText(hwndDlg, IDC_EDIT2, sz + wcslen(sz) + 1);

		swprintf(sz, L"%s\\%s\\%08X%04X%04X%02X%02X%02X%02X%02X%02X%02X%02X%x\\%s", 
			_NtSymbolPath, _PdbFileName,
			Signature->Data1, Signature->Data2, Signature->Data3,
			Signature->Data4[0], Signature->Data4[1], Signature->Data4[2], Signature->Data4[3],
			Signature->Data4[4], Signature->Data4[5], Signature->Data4[6], Signature->Data4[7],
			_Age, _PdbFileName);

		SetDlgItemText(hwndDlg, IDC_EDIT1, sz);
		break;
	}

	return ZDlg::DialogProc(hwndDlg, uMsg, wParam, lParam);
}
//////////////////////////////////////////////////////////////////////////
// ZSrcPathDlg

BOOL ZSrcPathDlg::OnOk(HWND hwndDlg)
{
	NTSTATUS status = -1;
	HWND hwnd;
	if (int len = GetWindowTextLength(hwnd = GetDlgItem(hwndDlg, IDC_EDIT1)))
	{
		PWSTR sz = (PWSTR)alloca((len + 1) << 1), name;
		GetWindowText(hwnd, sz, len + 1);

		UNICODE_STRING ObjectName;

		if (RtlDosPathNameToNtPathName_U(sz, &ObjectName, &name, 0))
		{
			if (!name || _wcsicmp(name, _filename))
			{
				MessageBox(hwndDlg, name, L"file name mismatch !", MB_ICONWARNING);
			}
			else
			{
				OBJECT_ATTRIBUTES oa = { sizeof(oa), 0, &ObjectName };
				FILE_BASIC_INFORMATION fbi;
				status = ZwQueryAttributesFile(&oa, &fbi);
				if (0 > status)
				{
					ShowNTStatus(hwndDlg, status, sz);
				}
				else
				{
					len = (int)wcslen(sz);
					if (_pq->path = new WCHAR[len + 1])
					{
						wcscpy(_pq->path, sz);
					}
				}
			}
			RtlFreeUnicodeString(&ObjectName);
		}
	}

	return 0 <= status;
}

INT_PTR ZSrcPathDlg::DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_COMMAND:
		switch (wParam)
		{
		case IDOK:
			if (_pq->bNotLoad || OnOk(hwndDlg))
			{
				EndDialog(hwndDlg, 0);
			}
			break;
		case IDCANCEL:
			EndDialog(hwndDlg, -1);
			break;
		case MAKEWPARAM(IDC_CHECK1, BN_CLICKED):
			_pq->bNotLoad = IsDlgButtonChecked(hwndDlg, IDC_CHECK1) == BST_CHECKED;
			EnableWindow(GetDlgItem(hwndDlg, IDC_EDIT1), !_pq->bNotLoad);
			break;
		}
		break;

	case WM_INITDIALOG:
		_pq = (ZZ*)lParam;
		SetDlgItemText(hwndDlg, IDC_STATIC1, _pq->FileName);
		SetWindowText(hwndDlg, _pq->DllName);
		{
			PCWSTR fn = wcsrchr(_pq->FileName, '\\');
			_filename = fn ? fn + 1 : _pq->FileName;
		}
		break;
	}

	return ZDlg::DialogProc(hwndDlg, uMsg, wParam, lParam);
}
//////////////////////////////////////////////////////////////////////////
// CSymbolsDlg
BOOL CSymbolsDlg::OnOK(HWND hwnd)
{
	if (int len = GetWindowTextLength(hwnd))
	{
		PWSTR sz = (PWSTR)alloca((len + 5) << 1);
		memcpy(sz, L"\\??\\", 4*sizeof(WCHAR));
		GetWindowText(hwnd, sz + 4, len + 1);

		if (GLOBALS_EX* globals = static_cast<GLOBALS_EX*>(ZGLOBALS::get()))
		{
			if (_wcsicmp(globals->getPath(), sz + 4))
			{
				HANDLE hFile;
				IO_STATUS_BLOCK iosb;
				UNICODE_STRING ObjectName;
				OBJECT_ATTRIBUTES oa = { sizeof(oa), 0, &ObjectName };
				RtlInitUnicodeString(&ObjectName, sz);
				NTSTATUS status = ZwCreateFile(&hFile, SYNCHRONIZE, &oa, &iosb, 0, 0, FILE_SHARE_VALID_FLAGS, FILE_OPEN_IF, FILE_DIRECTORY_FILE, 0, 0);
				if (0 <= status)
				{
					NtClose(hFile);
					globals->SetPath(sz + 4);
					return TRUE;
				}
				WCHAR err[256];
				FormatNTStatus(err, status);
				MessageBox(0, err, L"Invalid Path", MB_ICONWARNING);
			}
			else
			{
				return TRUE;
			}
		}
	}
	return FALSE;
}

INT_PTR CSymbolsDlg::DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	ZToolBar* tb;

	switch (uMsg)
	{
	case WM_SIZE:
		Resize(wParam, lParam);
		break;

	case WM_INITDIALOG:
		CreateLayout(hwndDlg);
		if (GLOBALS_EX* globals = static_cast<GLOBALS_EX*>(ZGLOBALS::get()))
		{
			SetDlgItemText(hwndDlg, IDC_EDIT1, globals->getPath());
		}
		if (tb = ZGLOBALS::getMainFrame())
		{
			tb->EnableCmd(ID_PATH, FALSE);
		}
		break;

	case WM_NCDESTROY:
		if (tb = ZGLOBALS::getMainFrame())
		{
			tb->EnableCmd(ID_PATH, TRUE);
		}
		break;
	case WM_NCHITTEST:
		{
			INT_PTR i = DefWindowProc(hwndDlg, uMsg, wParam, lParam);
			switch(i)
			{
			case HTBOTTOM:
			case HTBOTTOMLEFT:
			case HTBOTTOMRIGHT:
			case HTTOP:
			case HTTOPLEFT:
			case HTTOPRIGHT:
				i = HTBORDER;
			}
			SetWindowLongPtrW(hwndDlg, DWLP_MSGRESULT, i);
			return TRUE;
		}

	case WM_COMMAND:
		switch (wParam)
		{
		case IDOK:
			if (!OnOK(GetDlgItem(hwndDlg, IDC_EDIT1)))
			{
				break;
			}
		case IDCANCEL:
			DestroyWindow(hwndDlg);
			break;
		}
		break;

	case WM_GETMINMAXINFO:
		((PMINMAXINFO)lParam)->ptMinTrackSize.x = 256;
		return TRUE;
	}

	return ZDlg::DialogProc(hwndDlg, uMsg, wParam, lParam);
}

//////////////////////////////////////////////////////////////////////////
//
void ShowToken(HANDLE hToken, ULONG id, PCWSTR caption);

void CTokenDlg::OnOk(HWND hwndDlg)
{
	WCHAR sz_pid[16], sz_hToken[16], *c;
	if (!GetDlgItemText(hwndDlg, IDC_EDIT1, sz_pid, RTL_NUMBER_OF(sz_pid)) ||
		!GetDlgItemText(hwndDlg, IDC_EDIT2, sz_hToken, RTL_NUMBER_OF(sz_hToken))) return;

	CLIENT_ID cid = {  };
	
	cid.UniqueProcess = (HANDLE)(ULONG_PTR)wcstoul(sz_pid, &c, 16);

	if (!cid.UniqueProcess || *c)
	{
		SetFocus(GetDlgItem(hwndDlg, IDC_EDIT1));
		return ;
	}

	HANDLE hToken = (HANDLE)(ULONG_PTR)wcstoul(sz_hToken, &c, 16);

	if (!hToken || *c)
	{
		SetFocus(GetDlgItem(hwndDlg, IDC_EDIT2));
		return ;
	}

	HANDLE hProcess;
	
	NTSTATUS status = MyOpenProcess(&hProcess, PROCESS_DUP_HANDLE, &zoa, &cid);
	if (0 <= status)
	{
		status = ZwDuplicateObject(hProcess, hToken, NtCurrentProcess(), &hToken, TOKEN_QUERY|TOKEN_QUERY_SOURCE|READ_CONTROL, 0, 0);

		NtClose(hProcess);

		if (0 <= status)
		{
			TOKEN_SOURCE ts;
			ULONG cb;
			status = NtQueryInformationToken(hToken, TokenSource, &ts, sizeof(ts), &cb);
			if (0 > status)
			{
				NtClose(hToken);
			}
			else
			{
				ShowToken(hToken, 0, L"Token");
				//NtClose(hToken);
			}
		}
	}

	if (status)
	{
		ShowNTStatus(hwndDlg, status, L"Duplicate Token");
	}
}

INT_PTR CTokenDlg::DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_COMMAND:
		switch (wParam)
		{
		case IDOK:
			OnOk(hwndDlg);
			break;
		case IDCANCEL:
			DestroyWindow(hwndDlg);
			break;
		}
		break;
	}
	return ZDlg::DialogProc(hwndDlg, uMsg, wParam, lParam);
}


//////////////////////////////////////////////////////////////////////////
// CCalcDlg

void CCalcDlg::OnChange(HWND hwnd, HWND hwndRes)
{
	int len = ::GetWindowTextLengthA(hwnd);

	if (!len++) 
	{
		SetWindowText(hwndRes, L"");
		return;
	}

	PSTR buf = (PSTR)alloca(len);
	if (len - 1 != GetWindowTextA(hwnd, buf, len)) return;

	CEvalutor64 eval(0, 0, 0, 0);

	INT_PTR res;

	WCHAR wz[64];

	if (eval.Evalute(buf, res))
	{
		swprintf(wz, 
#ifdef _WIN64
			L"%I64X(%I64d, %I64u)"
#else
			L"%X(%d, %u)"
#endif
			, 
			res, res, res);
		SetWindowText(hwndRes, wz);
		return ;
	}

	SetWindowText(hwndRes, L"error expression");
}

INT_PTR CCalcDlg::DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_INITDIALOG:
		CreateLayout(hwndDlg);
		break;

	case WM_CLOSE:
		DestroyWindow(hwndDlg);
		return 0;

	case WM_SIZE:
		Resize(wParam, lParam);
		break;

	case WM_NCHITTEST:
		{
			INT_PTR i = DefWindowProc(hwndDlg, uMsg, wParam, lParam);
			switch(i)
			{
			case HTBOTTOM:
			case HTBOTTOMLEFT:
			case HTBOTTOMRIGHT:
			case HTTOP:
			case HTTOPLEFT:
			case HTTOPRIGHT:
				i = HTBORDER;
			}
			SetWindowLongPtrW(hwndDlg, DWLP_MSGRESULT, i);
			return TRUE;
		}

	case WM_COMMAND:
		switch (wParam)
		{
		case MAKEWPARAM(IDC_EDIT1, EN_CHANGE):
			OnChange((HWND)lParam, GetDlgItem(hwndDlg, IDC_EDIT2));
			break;
		}
		break;

	case WM_GETMINMAXINFO:
		((PMINMAXINFO)lParam)->ptMinTrackSize.x = 128;
		return TRUE;
	}

	return ZDlg::DialogProc(hwndDlg, uMsg, wParam, lParam);
}

//////////////////////////////////////////////////////////////////////////
// CFileInUseDlg
NTSTATUS GetProcessList(PUCHAR* pbbuf);

NTSTATUS PrintProcessesUsingFile(PFILE_PROCESS_IDS_USING_FILE_INFORMATION ppiufi, PWSTR* pbuf)
{
	PBYTE buf;
	NTSTATUS status = GetProcessList(&buf);

	if (0 <= status)
	{
		union {
			PBYTE pb;
			PSYSTEM_PROCESS_INFORMATION pspi;
		};

		ULONG NextEntryOffset, len = 0, NumberOfProcessIdsInList;
		ULONG_PTR *ProcessIdList, UniqueProcessId;

		NumberOfProcessIdsInList = ppiufi->NumberOfProcessIdsInList;
		ProcessIdList = ppiufi->ProcessIdList;

		do 
		{
			UniqueProcessId = *ProcessIdList++;

			pb = buf, NextEntryOffset = 0;

			len += 19;

			do 
			{
				pb += NextEntryOffset;

				if (UniqueProcessId == (ULONG_PTR)pspi->UniqueProcessId)
				{
					len += pspi->ImageName.Length;
					break;
				}

			} while (NextEntryOffset = pspi->NextEntryOffset);

		} while (--NumberOfProcessIdsInList);

		if (PWSTR psz = new WCHAR[len+1])
		{
			*pbuf = psz;

			NumberOfProcessIdsInList = ppiufi->NumberOfProcessIdsInList;
			ProcessIdList = ppiufi->ProcessIdList;

			do 
			{
				UniqueProcessId = *ProcessIdList++;

				pb = buf, NextEntryOffset = 1;
				goto __0;

				do 
				{
					pb += NextEntryOffset;
__0:
					if (UniqueProcessId == (ULONG_PTR)pspi->UniqueProcessId)
					{
						psz += swprintf(psz, L"%p %wZ\r\n", pspi->UniqueProcessId, &pspi->ImageName);
						break;
					}

				} while (NextEntryOffset = pspi->NextEntryOffset);

				if (!NextEntryOffset)
				{
					psz += swprintf(psz, L"%p\r\n", pspi->UniqueProcessId);
				}

			} while (--NumberOfProcessIdsInList);
		}
		else
		{
			status = STATUS_INSUFFICIENT_RESOURCES;
		}

		delete [] buf;
	}

	return status;
}

NTSTATUS PrintProcessesUsingFile(HANDLE hFile, PWSTR* pbuf)
{
	NTSTATUS status;
	IO_STATUS_BLOCK iosb;

	ULONG cb = 0, rcb = FIELD_OFFSET(FILE_PROCESS_IDS_USING_FILE_INFORMATION, ProcessIdList[64]);

	union {
		PVOID buf;
		PFILE_PROCESS_IDS_USING_FILE_INFORMATION ppiufi;
	};

	PVOID stack = alloca(guz);

	do 
	{
		if (cb < rcb)
		{
			cb = RtlPointerToOffset(buf = alloca(rcb - cb), stack);
		}

		if (0 <= (status = NtQueryInformationFile(hFile, &iosb, ppiufi, cb, FileProcessIdsUsingFileInformation)))
		{
			if (ppiufi->NumberOfProcessIdsInList)
			{
				return PrintProcessesUsingFile(ppiufi, pbuf);
			}

			return STATUS_NOT_FOUND;
		}

		rcb = (ULONG)iosb.Information;

	} while (status == STATUS_INFO_LENGTH_MISMATCH);

	return status;
}

NTSTATUS PrintProcessesUsingFile(POBJECT_ATTRIBUTES poa, PWSTR* pbuf)
{
	IO_STATUS_BLOCK iosb;
	HANDLE hFile;
	NTSTATUS status;
	if (0 <= (status = NtOpenFile(&hFile, FILE_READ_ATTRIBUTES, poa, &iosb, FILE_SHARE_VALID_FLAGS, 0)))
	{
		status = PrintProcessesUsingFile(hFile, pbuf);
		NtClose(hFile);
	}

	return status;
}

NTSTATUS PrintProcessesUsingFile(PCWSTR FileName, PWSTR* pbuf)
{
	UNICODE_STRING ObjectName;
	NTSTATUS status = STATUS_OBJECT_PATH_INVALID;
	if (RtlDosPathNameToNtPathName_U(FileName, &ObjectName, 0, 0))
	{
		OBJECT_ATTRIBUTES oa = { sizeof(oa), 0, &ObjectName };
		status = PrintProcessesUsingFile(&oa, pbuf);
		RtlFreeUnicodeString(&ObjectName);
	}

	return status;
}

INT_PTR CFileInUseDlg::DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_INITDIALOG:
		CreateLayout(hwndDlg);
		break;

	case WM_CLOSE:
		DestroyWindow(hwndDlg);
		return 0;

	case WM_SIZE:
		Resize(wParam, lParam);
		break;

	case WM_NCHITTEST:
		{
			INT_PTR i = DefWindowProc(hwndDlg, uMsg, wParam, lParam);
			switch(i)
			{
			case HTBOTTOM:
			case HTBOTTOMLEFT:
			case HTBOTTOMRIGHT:
			case HTTOP:
			case HTTOPLEFT:
			case HTTOPRIGHT:
				i = HTBORDER;
			}
			SetWindowLongPtrW(hwndDlg, DWLP_MSGRESULT, i);
			return TRUE;
		}

	case WM_COMMAND:
		switch (wParam)
		{
		case MAKEWPARAM(IDOK, BN_CLICKED):
			if (ULONG len = GetWindowTextLengthW(GetDlgItem(hwndDlg, IDC_EDIT1)))
			{
				if (len++ < MAXUSHORT)
				{
					PWSTR FileName = (PWSTR)alloca(len*sizeof(WCHAR));
					if (GetDlgItemTextW(hwndDlg, IDC_EDIT1, FileName, len))
					{
						PWSTR buf;
						NTSTATUS status = PrintProcessesUsingFile(FileName, &buf);

						if (0 > status)
						{
							if (FormatMessageW(
								FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_HMODULE|FORMAT_MESSAGE_IGNORE_INSERTS, 
								GetModuleHandle(L"ntdll"), status, 0, (PWSTR)&buf, 0, 0))
							{
								ShowText(FileName, buf);
								LocalFree(buf);
							}
						}
						else
						{
							ShowText(FileName, buf);
							delete [] buf;
						}
					}
				}
			}
			break;
		case MAKEWPARAM(IDC_BUTTON1, BN_CLICKED):
			{
				static const COMDLG_FILTERSPEC rgSpec[] =
				{ 
					{ L"All files", L"*" },
				};
				OnBrowse(hwndDlg, IDC_EDIT1, _countof(rgSpec), rgSpec);
			}
			break;
		}
		break;

	case WM_GETMINMAXINFO:
		((PMINMAXINFO)lParam)->ptMinTrackSize.x = 256;
		return TRUE;
	}

	return ZDlg::DialogProc(hwndDlg, uMsg, wParam, lParam);
}

//////////////////////////////////////////////////////////////////////////
// CRvaToOfs

NTSTATUS RvaToOfs(PCWSTR FileName, ULONG Rva, PULONG pOfs, BOOLEAN bRvaToOfs, DWORD& Characteristics, PSTR Name)
{
	UNICODE_STRING ObjectName;
	NTSTATUS status = STATUS_OBJECT_PATH_INVALID;
	if (RtlDosPathNameToNtPathName_U(FileName, &ObjectName, 0, 0))
	{
		OBJECT_ATTRIBUTES oa = { sizeof(oa), 0, &ObjectName };
		IO_STATUS_BLOCK iosb;
		HANDLE hFile;
		status = NtOpenFile(&hFile, FILE_READ_DATA|SYNCHRONIZE, &oa, &iosb, FILE_SHARE_VALID_FLAGS, FILE_SYNCHRONOUS_IO_NONALERT);
		RtlFreeUnicodeString(&ObjectName);
		if (0 <= status)
		{
			union {
				IMAGE_DOS_HEADER idh;
				IMAGE_NT_HEADERS32 inth32;
				IMAGE_NT_HEADERS64 inth64;
			};

			if (0 <= (status = NtReadFile(hFile, 0, 0, 0, &iosb, &idh, sizeof(idh), 0, 0)))
			{
				status = STATUS_INVALID_IMAGE_FORMAT;

				if (iosb.Information == sizeof(idh) && idh.e_magic == IMAGE_DOS_SIGNATURE)
				{
					LARGE_INTEGER ByteOffset = { idh.e_lfanew };

					if (0 <= NtReadFile(hFile, 0, 0, 0, &iosb, &inth64, sizeof(inth64), &ByteOffset, 0))
					{
						if (iosb.Information == sizeof(inth64) && inth64.Signature == IMAGE_NT_SIGNATURE)
						{
							if (inth64.FileHeader.NumberOfSections)
							{
								ByteOffset.QuadPart += FIELD_OFFSET( IMAGE_NT_HEADERS, OptionalHeader ) + inth64.FileHeader.SizeOfOptionalHeader;

								ULONG size = inth64.FileHeader.NumberOfSections * sizeof(IMAGE_SECTION_HEADER);

								PVOID buf = 0;

								if (size < 0x10000)
								{
									buf = alloca(size);
								}
								else
								{
									buf = new UCHAR[size];
								}

								if (buf)
								{
									if (0 <= NtReadFile(hFile, 0, 0, 0, &iosb, buf, size, &ByteOffset, 0))
									{
										status = STATUS_NOT_FOUND;

										PIMAGE_SECTION_HEADER pish = (PIMAGE_SECTION_HEADER)buf;

										ULONG d = 0;
										do 
										{
											if (bRvaToOfs)
											{
												d = Rva - pish->VirtualAddress;
												
												if (d < pish->Misc.VirtualSize)
												{
													d += pish->PointerToRawData;
													break;
												}
											}
											else
											{

												d = Rva - pish->PointerToRawData;

												if (d < pish->SizeOfRawData)
												{
													d += pish->VirtualAddress;
													break;
												}
											}
										} while (pish++, --inth64.FileHeader.NumberOfSections);

										if (inth64.FileHeader.NumberOfSections)
										{
											status = STATUS_SUCCESS;
											*pOfs = d;
											Characteristics = pish->Characteristics;
											memcpy(Name, pish->Name, IMAGE_SIZEOF_SHORT_NAME);
											Name[IMAGE_SIZEOF_SHORT_NAME] = 0;
										}
									}

									if (size >= 0x10000)
									{
										delete [] buf;
									}
								}
							}
							else
							{
								status = STATUS_SUCCESS;
								*pOfs = Rva;
							}
						}
					}
				}
			}
			NtClose(hFile);
		}
	}

	return status;

}

ULONG GetCount(PWSTR psz, PWSTR end)
{
	ULONG ofsCount = 0;
	PCWSTR lastName = 0;

	for (;;)
	{
		ULONG o = wcstoul(psz, &psz, 16);
		if (!o || *psz++ != ' ')
		{
			break;
		}

		PWSTR pb = wtrnchr(RtlPointerToOffset(psz, end), psz, '\r');

		if (!pb)
		{
			break;
		}

		if (!lastName || wcscmp(lastName, psz))
		{
			lastName = psz;
		}

		ofsCount++;
		psz = pb;
	}

	return ofsCount;
}

void AddCB(HWND hwndCB, PCWSTR lastName, ULONG n, ULONG m)
{
	if (lastName)
	{
		int index = ComboBox_AddString(hwndCB, lastName);
		if (0 <= index)
		{
			ComboBox_SetItemData(hwndCB, index, MAKELPARAM(n, m));
		}
	}
}

void CRvaToOfs::OnPaste(HWND hwndCB, HWND hwndOfs, PWSTR psz, PWSTR end)
{
	ComboBox_ResetContent(hwndCB);
	delete [] _pofs, _pofs = 0;

	if (psz == end) return ;

	end[-1] = 0;

	if (ULONG ofsCount = GetCount(psz, end))
	{
		if (PULONG pu = new ULONG[ofsCount])
		{
			_pofs = pu;

			ULONG i = 0, m = 0, n = 0;
			PWSTR lastName = 0, pb;

			for (;;)
			{
				ULONG o = wcstoul(psz, &psz, 16);

				if (!o || *psz++ != ' ' || (!(pb = wtrnchr(RtlPointerToOffset(psz, end), psz, '\r'))))
				{
					AddCB(hwndCB, lastName, n, m);
					ComboBox_SetCurSel(hwndCB, 0);
					ShowOfsForModule(hwndCB, hwndOfs);
					return ;
				}

				pb[-1] = 0;

				if (!lastName || wcscmp(lastName, psz))
				{
					AddCB(hwndCB, lastName, n, m);
					lastName = psz, m = 0, n = i;
				}
				m++;

				*pu++ = o;

				psz = pb, i++;
			}
		}
	}
}

void CRvaToOfs::ShowOfsForModule(HWND hwndCbModules, HWND hwndOfs)
{
	ComboBox_ResetContent(hwndOfs);
	int i = ComboBox_GetCurSel(hwndCbModules);
	if (0 <= i)
	{
		ULONG u = (ULONG)ComboBox_GetItemData(hwndCbModules, i);

		if (ULONG n = u >> 16)
		{
			PULONG pu = _pofs + (u & 0xFFFF);
			do 
			{
				WCHAR sz[9];
				swprintf(sz, L"%08x", *pu++);
				ComboBox_AddString(hwndOfs, sz);
			} while (--n);
			ComboBox_SetCurSel(hwndOfs, 0);
		}
	}
}

void CRvaToOfs::Convert(HWND hwndDlg)
{
	ULONG id = _bRvaToOfs ? IDC_EDIT2 : IDC_COMBO2;

	ULONG len = GetWindowTextLengthW(GetDlgItem(hwndDlg, id));

	if (len - 1 > 7)
	{
__error:
		SetFocus(GetDlgItem(hwndDlg, id));
		MessageBeep(0);
		return ;
	}

	WCHAR sz[9], *c;

	GetWindowTextW(GetDlgItem(hwndDlg, id), sz, RTL_NUMBER_OF(sz));

	ULONG Rva = wcstoul(sz, &c, 16), Ofs, Characteristics;

	if (!Rva || *c)
	{
		goto __error;
	}

	len = GetWindowTextLengthW(GetDlgItem(hwndDlg, IDC_COMBO1));

	if (len - 1 > MAXUSHORT - 1)
	{
		id = IDC_EDIT1;
		goto __error;
	}

	PWSTR FileName = (PWSTR)alloca(++len*sizeof(WCHAR));

	if (!GetDlgItemTextW(hwndDlg, IDC_COMBO1, FileName, len))
	{
		id = IDC_EDIT1;
		goto __error;
	}

	char Name[IMAGE_SIZEOF_SHORT_NAME+1];
	
	NTSTATUS status = RvaToOfs(FileName, Rva, &Ofs, _bRvaToOfs, Characteristics, Name);

	PWSTR buf;
	if (0 > status)
	{
		if (FormatMessageW(
			FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_HMODULE|FORMAT_MESSAGE_IGNORE_INSERTS, 
			GetModuleHandle(L"ntdll"), status, 0, (PWSTR)&buf, 0, 0))
		{
			MessageBox(hwndDlg, buf, FileName, 0);
			LocalFree(buf);
		}
	}
	else
	{
		swprintf(sz, L"%08x", Ofs);
		SetDlgItemTextW(hwndDlg, _bRvaToOfs ? IDC_COMBO2 : IDC_EDIT2, sz);
		SetDlgItemTextA(hwndDlg, IDC_EDIT4, Name);
		CheckDlgButton(hwndDlg, IDC_CHECK1, Characteristics & IMAGE_SCN_MEM_WRITE ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hwndDlg, IDC_CHECK2, Characteristics & IMAGE_SCN_MEM_EXECUTE ? BST_CHECKED : BST_UNCHECKED);
	}
}

void CRvaToOfs::OnJmp(HWND hwndDlg)
{
	WCHAR sz[9], *pc;
	if (!GetDlgItemTextW(hwndDlg, IDC_EDIT2, sz, _countof(sz))) return;
	ULONG rva = wcstoul(sz, &pc, 16);

	if (!rva || *pc)
	{
		return ;
	}
	
	ULONG len = GetWindowTextLengthW(GetDlgItem(hwndDlg, IDC_COMBO1));

	if (len - 1 > MAXUSHORT - 1)
	{
		return;
	}

	PWSTR FileName = (PWSTR)alloca(++len*sizeof(WCHAR));

	if (!GetDlgItemTextW(hwndDlg, IDC_COMBO1, FileName, len))
	{
		return;
	}

	if (ZSDIFrameWnd* pFrame = ZGLOBALS::getMainFrame())
	{
		if (ZDocument* pDoc = pFrame->GetActiveDoc())
		{
			ZDbgDoc* pDbg;
			if (0 <= pDoc->QI(IID_PPV(pDbg)))
			{
				if (ZDll* pDll = pDbg->getDllByPathNoRef(FileName))
				{
					if (rva < pDll->getSize())
					{
						pDbg->GoTo(RtlOffsetToPointer(pDll->getBase(), rva), false);
					}
				}
				pDbg->Release();
			}
		}
	}
}

INT_PTR CRvaToOfs::DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static const COMDLG_FILTERSPEC rgSpec[] =
	{ 
		{ L"Exe files", L"*.exe" },
		{ L"Dll files", L"*.dll" },
		{ L"Sys files", L"*.sys" },
		{ L"All files", L"*" },
	};

	switch (uMsg)
	{
	case WM_INITDIALOG:
		{
			RECT rc;
			GetWindowRect(hwndDlg, &rc);
			_cxMin = rc.right - rc.left;
			GetWindowRect(GetDlgItem(hwndDlg, IDC_BUTTON1), &rc);
			ScreenToClient(hwndDlg, (POINT*)&rc);
			CreateLayout(hwndDlg, rc.left-1);
		}
		SendMessage(GetDlgItem(hwndDlg, IDC_EDIT2), EM_SETREADONLY, TRUE, 0);
		CheckDlgButton(hwndDlg, IDC_RADIO2, BST_CHECKED);
		_bRvaToOfs = FALSE;
		break;

	case WM_DESTROY:
		delete [] _pofs;
		break;

	case WM_CLOSE:
		DestroyWindow(hwndDlg);
		return 0;

	case WM_SIZE:
		Resize(wParam, lParam);
		break;

	case WM_NCHITTEST:
		{
			INT_PTR i = DefWindowProc(hwndDlg, uMsg, wParam, lParam);
			switch(i)
			{
			case HTBOTTOM:
			case HTBOTTOMLEFT:
			case HTBOTTOMRIGHT:
			case HTTOP:
			case HTTOPLEFT:
			case HTTOPRIGHT:
				i = HTBORDER;
			}
			SetWindowLongPtrW(hwndDlg, DWLP_MSGRESULT, i);
			return TRUE;
		}

	case WM_COMMAND:
		switch (wParam)
		{
		case MAKEWPARAM(IDC_RADIO1, BN_CLICKED):
			SendMessage(GetDlgItem(hwndDlg, IDC_EDIT3), EM_SETREADONLY, TRUE, 0);
			SendMessage(GetDlgItem(hwndDlg, IDC_EDIT2), EM_SETREADONLY, FALSE, 0);
			_bRvaToOfs = TRUE;
			break;

		case MAKEWPARAM(IDC_RADIO2, BN_CLICKED):
			SendMessage(GetDlgItem(hwndDlg, IDC_EDIT3), EM_SETREADONLY, FALSE, 0);
			SendMessage(GetDlgItem(hwndDlg, IDC_EDIT2), EM_SETREADONLY, TRUE, 0);
			_bRvaToOfs = FALSE;
			break;

		case MAKEWPARAM(IDOK, BN_CLICKED):
			Convert(hwndDlg);
			break;

		case MAKEWPARAM(IDC_COMBO1, CBN_SELCHANGE):
			ShowOfsForModule((HWND)lParam, GetDlgItem(hwndDlg, IDC_COMBO2));
			break;

		case MAKEWPARAM(IDC_BUTTON2, BN_CLICKED):
			if (OpenClipboard(hwndDlg))
			{
				if (HANDLE h = GetClipboardData(CF_UNICODETEXT))
				{
					if (PVOID pv = GlobalLock(h))
					{
						OnPaste(GetDlgItem(hwndDlg, IDC_COMBO1), GetDlgItem(hwndDlg, IDC_COMBO2), 
							(PWSTR)pv, (PWSTR)RtlOffsetToPointer(pv, GlobalSize(h)));

						GlobalUnlock(h);
					}
				}
				GetLastError();

				CloseClipboard();
			}
			break;

		case MAKEWPARAM(IDC_BUTTON3, BN_CLICKED):
			OnJmp(hwndDlg);
			break;

		case MAKEWPARAM(IDC_BUTTON1, BN_CLICKED):
			OnBrowse(hwndDlg, IDC_COMBO1, _countof(rgSpec), rgSpec);
			break;
		}
		break;

	case WM_GETMINMAXINFO:
		((PMINMAXINFO)lParam)->ptMinTrackSize.x = _cxMin;
		return TRUE;
	}

	return ZDlg::DialogProc(hwndDlg, uMsg, wParam, lParam);
}

//////////////////////////////////////////////////////////////////////////
// ZBpExp

void ZBpExp::OnDetach()
{
	if (HWND hwnd = getHWND())
	{
		DestroyWindow(hwnd);
	}
}

INT_PTR ZBpExp::DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CLOSE:
		DestroyWindow(hwndDlg);
		break;
	case WM_INITDIALOG:
		_pDoc->AddNotify(this);
		if (lParam) SetDlgItemText(hwndDlg, IDC_EDIT1, (PCWSTR)lParam);
		break;
	case WM_DESTROY:
		_pDoc->RemoveNotify(this);
		break;
	case WM_COMMAND:
		switch (wParam)
		{
		case IDOK:
			if (HWND hwnd = GetDlgItem(hwndDlg, IDC_EDIT1))
			{
				if (int len = GetWindowTextLength(hwnd))
				{
					if (len > PAGE_SIZE)
					{
						MessageBox(hwndDlg, 0, L"Script Too Long (> 4096 symbols)", MB_ICONWARNING);
					}
					else
					{
						PWSTR sz = (PWSTR)alloca((++len) << 1);
						GetWindowText(hwnd, sz, len);
						BOOL b;
						CONTEXT ctx;

						if (JsScript::_RunScript(sz, &b, &ctx, 0, 0, 0, (void**)&ctx))
						{
							MessageBox(hwndDlg, 0, L"Script Error", MB_ICONHAND);
						}
						else
						{
							if (_pDoc->SetBpCondition(_Va, sz, len))
							{
								DestroyWindow(hwndDlg);
							}
						}
					}
				}
				else
				{
					if (_pDoc->SetBpCondition(_Va, 0, 0))
					{
						DestroyWindow(hwndDlg);
					}
				}

			}
			break;
		}
	}

	return ZDlg::DialogProc(hwndDlg, uMsg, wParam, lParam);
}

ZBpExp::~ZBpExp()
{
	_pDoc->Release();
}

HWND ZBpExp::Create(PCWSTR exp)
{
	return ZDlg::Create((HINSTANCE)&__ImageBase, MAKEINTRESOURCE(IDD_DIALOG7), ZGLOBALS::getMainHWND(), (LPARAM)exp);
}

ZBpExp::ZBpExp(ZDbgDoc* pDoc, PVOID Va)
{
	_Va = Va;
	_pDoc = pDoc;
	pDoc->AddRef();
}

//////////////////////////////////////////////////////////////////////////
// ZBPDlg

extern HIMAGELIST g_himl16;

void ZBPDlg::OnInitDialog(HWND hwndDlg)
{
	HWND hwndLV = GetDlgItem(hwndDlg, IDC_LIST1);

	ListView_SetExtendedListViewStyle(hwndLV, LVS_EX_CHECKBOXES|LVS_EX_BORDERSELECT|LVS_EX_INFOTIP|LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);

	SIZE size = { 8, 16 };
	if (HDC hdc = GetDC(hwndLV))
	{
		HGDIOBJ o = SelectObject(hdc, (HGDIOBJ)SendMessage(hwndLV, WM_GETFONT, 0, 0));
		GetTextExtentPoint32(hdc, L"W", 1, &size);
		SelectObject(hdc, o);
		ReleaseDC(hwndLV, hdc);
	}

	LVCOLUMN lvc = { LVCF_FMT|LVCF_WIDTH|LVCF_TEXT|LVCF_SUBITEM, LVCFMT_LEFT };

	static PCWSTR headers[] = { L" Va ", L" # ", L" Dll " };
	DWORD lens[] = { 6+sizeof(PVOID)*2, 5, 30 };

	do 
	{
		lvc.pszText = (PWSTR)headers[lvc.iSubItem], lvc.cx = lens[lvc.iSubItem] * size.cx;
		ListView_InsertColumn(hwndLV, lvc.iSubItem, &lvc);
	} while (++lvc.iSubItem < RTL_NUMBER_OF(headers));

	SendMessage(ListView_GetToolTips(hwndLV), TTM_SETDELAYTIME, TTDT_AUTOPOP, MAKELONG(MAXSHORT, 0));

	if (HIMAGELIST himl = ListView_GetImageList(hwndLV, LVSIL_STATE))
	{
		int cx, cy;
		IImageList2* pi;
		if (!HIMAGELIST_QueryInterface(himl, IID_PPV(pi)))
		{
			if (!pi->GetIconSize(&cx, &cy) && 16 < cy && !pi->Resize(16, 16))
			{
				ListView_SetImageList(hwndLV, himl, LVSIL_STATE);//himl = 
			}
			pi->Release();
		}
	}

	ListView_SetImageList(hwndLV, g_himl16, LVSIL_SMALL);
	CreateBPV(hwndLV);
}

void ZBPDlg::EnableAllBps(HWND hwndDlg, BOOL bEnable)
{
	_pDoc->EnableAllBps(bEnable);
	InvalidateRect(GetDlgItem(hwndDlg, IDC_LIST1), 0, TRUE);
}

INT_PTR ZBPDlg::DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	DWORD iItem;
	int cchTextMax;
	PWSTR pszText;
	ZBreakPoint* pbp;
	WCHAR c;
	PCWSTR exp;

	switch (uMsg)
	{
	case WM_BPADDDEL:
		CreateBPV(GetDlgItem(hwndDlg, IDC_LIST1));
		return 0;

	case WM_BPENBDIS:
		InvalidateRect(GetDlgItem(hwndDlg, IDC_LIST1), 0, TRUE);
		break;

	case WM_INITDIALOG:
		_pDoc->SetBPDlg(hwndDlg);
		OnInitDialog(hwndDlg);
		break;

	case WM_DESTROY:
		_pDoc->SetBPDlg(0);
		break;

	case WM_CLOSE:
		DestroyWindow(hwndDlg);
		return 0;

	case WM_COMMAND:
		switch (wParam)
		{
		case IDC_BUTTON1:
			if ((iItem = ListView_GetSelectionMark(hwndDlg = GetDlgItem(hwndDlg, IDC_LIST1))) < _n)
			{
				pbp = _pV[iItem];
				if (pbp->_HitCount)
				{
					pbp->_HitCount = 0;
					ListView_RedrawItems(hwndDlg, iItem, iItem);
				}
			}
			break;

		case IDC_BUTTON2:
			if ((iItem = ListView_GetSelectionMark(hwndDlg = GetDlgItem(hwndDlg, IDC_LIST1))) < _n)
			{
				_pDoc->DelBp(_pV[iItem]);
			}
			break;

		case IDC_BUTTON5:
			if ((iItem = ListView_GetSelectionMark(GetDlgItem(hwndDlg, IDC_LIST1))) < _n)
			{
				pbp = _pV[iItem];
				if (ZBpExp* p = new ZBpExp(_pDoc, pbp->_Va))
				{
					p->Create(pbp->_expression);
					p->Release();
				}
			}
			break;
		case IDC_BUTTON3:
			EnableAllBps(hwndDlg, TRUE);
			break;
		case IDC_BUTTON4:
			EnableAllBps(hwndDlg, FALSE);
			break;
		case IDC_BUTTON6:
			_pDoc->DeleteAllBps();
			CreateBPV(GetDlgItem(hwndDlg, IDC_LIST1));
			break;
		}
		break;

	case WM_NOTIFY:

		if (((LPNMHDR)lParam)->idFrom == IDC_LIST1)
		{
			switch (((LPNMHDR)lParam)->code)
			{
			case NM_SETFOCUS:
				SendMessage(((LPNMHDR)lParam)->hwndFrom, WM_KILLFOCUS, 0, 0);
				break;

			case NM_CLICK:
				if ((iItem = ((LPNMITEMACTIVATE)lParam)->iItem) < _n)
				{
					LVHITTESTINFO lvti;
					lvti.pt = ((LPNMITEMACTIVATE)lParam)->ptAction;
					ListView_SubItemHitTest(hwndDlg = ((LPNMITEMACTIVATE)lParam)->hdr.hwndFrom, &lvti);

					if (lvti.flags == LVHT_ONITEMSTATEICON)
					{
						pbp = _pV[iItem];
						if (_pDoc->EnableBp(pbp, !pbp->_isActive, FALSE))
						{
							ListView_RedrawItems(hwndDlg, iItem, iItem);
						}
					}
				}
				break;

			case NM_DBLCLK:
				if ((iItem = ((LPNMITEMACTIVATE)lParam)->iItem) < _n)
				{
					_pDoc->GoTo(_pV[iItem]->_Va);
				}
				break;

			case LVN_GETINFOTIP:
				if (
					(iItem = ((LPNMLVGETINFOTIP)lParam)->iItem) < _n
					&&
					(cchTextMax = ((LPNMLVGETINFOTIP)lParam)->cchTextMax)
					&&
					(pszText = ((LPNMLVGETINFOTIP)lParam)->pszText)
					)
				{
					pszText[0] = 0;

					pbp = _pV[iItem];

					if (exp = pbp->_expression)
					{
						do 
						{
							*pszText++ = c = *exp++;
						} while (c && --cchTextMax);

						if (c)
						{
							pszText[-1] = 0;
						}
					}
				}
				break;

			case LVN_GETDISPINFO:

				if ((iItem = ((NMLVDISPINFO*)lParam)->item.iItem) < _n)
				{
					pbp = _pV[iItem];

					UINT mask = ((NMLVDISPINFO*)lParam)->item.mask;

					((NMLVDISPINFO*)lParam)->item.mask |= LVIF_STATE;
					((NMLVDISPINFO*)lParam)->item.state = INDEXTOSTATEIMAGEMASK(pbp->_isActive ? 2 : 1);
					((NMLVDISPINFO*)lParam)->item.stateMask = LVIS_STATEIMAGEMASK;

					if (mask & LVIF_IMAGE)
					{
						((NMLVDISPINFO*)lParam)->item.iImage = pbp->_isActive ? (pbp->_expression ? 4 : 2) : (pbp->_expression ? 5 : 3);
					}

					if (
						(mask & LVIF_TEXT) 
						&&
						(cchTextMax = ((NMLVDISPINFO*)lParam)->item.cchTextMax)
						)
					{			
						pszText = ((NMLVDISPINFO*)lParam)->item.pszText;

						switch (((NMLVDISPINFO*)lParam)->item.iSubItem)
						{
						case 0:
							_snwprintf(pszText, cchTextMax, L"%p", pbp->_Va);
							break;
						case 1:
							_snwprintf(pszText, cchTextMax, L"%u", pbp->_HitCount);
							break;
						case 2:
							if (PCWSTR sz = (iItem = pbp->_dllId) ? _pDoc->getNameByID(iItem) : 0)
							{
								do 
								{
									*pszText++ = c = *sz++;
								} while (c && --cchTextMax);

								if (c)
								{
									pszText[-1] = 0;
								}
							}
							break;
						default: pszText[0] = 0;
						}
					}
				}
				break;
			}
		}
		break;
	}

	return ZDlg::DialogProc(hwndDlg, uMsg, wParam, lParam);
}

int __cdecl sortBPs(ZBreakPoint*& p, ZBreakPoint*& q)
{
	if (p->_Va < q->_Va) return -1;
	if (p->_Va > q->_Va) return +1;
	return 0;
}

void ZBPDlg::CreateBPV(HWND hwndLV)
{
	PLIST_ENTRY head = &_pDoc->_bpListHead, entry = head;
	DWORD n = 0;
	ZBreakPoint* pbp, **pV = _pV;

	while ((entry = entry->Flink) != head)
	{
		pbp = static_cast<ZBreakPoint*>(entry);
		if (pbp->_Va)
		{
			n++;
		}
	}

	if (((n + 15) & ~15) != ((_n + 15) & ~15))
	{
		if (_pV)
		{
			delete _pV;
		}

		if (!(pV = new ZBreakPoint*[(n + 15) & ~15]))
		{
			n = 0;
		}

		_pV = pV;
	}

	_n = n;

	if (n)
	{
		while ((entry = entry->Flink) != head)
		{
			pbp = static_cast<ZBreakPoint*>(entry);

			if (pbp->_Va)
			{
				*pV++ = pbp;
			}
		}

		qsort(_pV, n, sizeof(PVOID), QSORTFN(sortBPs));
	}

	ListView_SetItemCountEx(hwndLV, n, 0);
	InvalidateRect(hwndLV, 0, TRUE);
}

ZBPDlg::~ZBPDlg()
{
	if (_pV)
	{
		delete _pV;
	}

	_pDoc->Release();
}

HWND ZBPDlg::Create()
{
	return ZDlg::Create((HINSTANCE)&__ImageBase, MAKEINTRESOURCE(IDD_DIALOG6), ZGLOBALS::getMainHWND(), 0);
}

ZBPDlg::ZBPDlg(ZDbgDoc* pDoc)
{
	_pDoc = pDoc;
	pDoc->AddRef();
	_pV = 0;
	_n = 0;
}

//////////////////////////////////////////////////////////////////////////
// ZModulesDlg

ZModulesDlg::~ZModulesDlg()
{
	if (_ppDll)
	{
		DWORD n = _nDllCount;
		ZDll** ppDll = _ppDll;
		do 
		{
			(*ppDll++)->Release();
		} while (--n);

		delete _ppDll;
	}
	_pDoc->Release();
}

ZModulesDlg::ZModulesDlg(ZDbgDoc* pDoc)
{
	_pDoc = pDoc;
	pDoc->AddRef();
	_SortOrder = 1;
	_ppDll = 0;
}

HWND ZModulesDlg::Create(DWORD nDllCount, PLIST_ENTRY head)
{
	if (nDllCount)
	{
		if (ZDll** ppDll = new ZDll*[nDllCount])
		{
			_nDllCount = nDllCount;
			_ppDll = ppDll;

			PLIST_ENTRY entry = head;

			while ((entry = entry->Flink) != head)
			{
				ZDll* pDll = static_cast<ZDll*>(entry);

				*ppDll++ = pDll;
				pDll->AddRef();
			}

			return ZDlg::Create((HINSTANCE)&__ImageBase, MAKEINTRESOURCE(IDD_DIALOG3), ZGLOBALS::getMainHWND(), (LPARAM)head);
		}
	}

	return 0;
}

void ZModulesDlg::OnInitDialog(HWND hwnd)
{
	_pDoc->SetModDlg(hwnd);
	
	_pDoc->SetBOLText(hwnd, IDC_EDIT1);

	HWND hwndLV = GetDlgItem(hwnd, IDC_LIST1);

	RECT rc;
	GetWindowRect(hwndLV, &rc);
	ScreenToClient(hwnd, 1 + (POINT*)&rc);
	CreateLayout(hwnd, rc.right - 1, rc.bottom - 1);

	ListView_SetExtendedListViewStyle(hwndLV, LVS_EX_BORDERSELECT|LVS_EX_INFOTIP|LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);

	SIZE size = { 8, 16 };
	if (HDC hdc = GetDC(hwndLV))
	{
		HGDIOBJ o = SelectObject(hdc, (HGDIOBJ)SendMessage(hwndLV, WM_GETFONT, 0, 0));
		GetTextExtentPoint32(hdc, L"W", 1, &size);
		SelectObject(hdc, o);
		ReleaseDC(hwndLV, hdc);
	}

	LVCOLUMN lvc = { LVCF_FMT|LVCF_WIDTH|LVCF_TEXT|LVCF_SUBITEM, LVCFMT_LEFT };

	static PCWSTR headers[] = { L" # ", L" Base ", L" Size ", L" Name "};
	DWORD lens[] = { 5, 2+sizeof(PVOID)*2, 10, 0};

	DWORD n = _nDllCount;
	ZDll** ppDll = _ppDll;
	do 
	{
		if (PWSTR ImageName = (*ppDll++)->_ImageName)
		{
			DWORD len = 2 + (DWORD)wcslen(ImageName);

			if (lens[3] < len)
			{
				lens[3] = len;
			}
		}
	} while (--n);

	do 
	{
		lvc.pszText = (PWSTR)headers[lvc.iSubItem], lvc.cx = lens[lvc.iSubItem] * size.cx;
		ListView_InsertColumn(hwndLV, lvc.iSubItem, &lvc);
	} while (++lvc.iSubItem < RTL_NUMBER_OF(headers));

	SendMessage(ListView_GetToolTips(hwndLV), TTM_SETDELAYTIME, TTDT_AUTOPOP, MAKELONG(MAXSHORT, 0));

	ListView_SetItemCountEx(hwndLV, _nDllCount, 0);
}

struct _SORT_DLLS
{
	int iSubItem;
	int s;
};

typedef RTL_FRAME<_SORT_DLLS> SORT_DLLS;

int __cdecl compareDLL(ZDll** p, ZDll** q)
{
	_SORT_DLLS* aux = SORT_DLLS::get();
	return ZDll::Compare(*p, *q, aux->iSubItem, aux->s);
}

int ZDll::Compare(ZDll* p, ZDll* q, int iSubItem, int s)
{
	ULONG_PTR a, b;
	
	switch (iSubItem)
	{
	default: __assume(false);
	case 0:
		a = p->_index, b = q->_index;
		break;
	
	case 1:
		a = (ULONG_PTR)p->_BaseOfDll, b = (ULONG_PTR)q->_BaseOfDll;
		break;

	case 2:
		a = p->_SizeOfImage, b = q->_SizeOfImage;
		break;

	case 3:
		PCWSTR pa = p->_ImageName, pb = q->_ImageName;
		if (!pa) return -s;
		if (!pb) return +s;
		return _wcsicmp(pa, pb)*s;
	}

	if (a < b) return -s;
	if (a > b) return +s;
	return 0;
}

INT_PTR ZModulesDlg::DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	DWORD iItem, cchTextMax;
	PWSTR ImagePath, pszText, sz;
	ZDll* pDll;
	WCHAR c;
	int len;

	switch (uMsg)
	{
	case WM_SIZE:
		Resize(wParam, lParam);
		break;

	case WM_GETMINMAXINFO:
		((PMINMAXINFO)lParam)->ptMinTrackSize.x = 500;
		((PMINMAXINFO)lParam)->ptMinTrackSize.y = 300;
		return TRUE;

	case WM_DESTROY:
		_pDoc->SetModDlg(0);
		break;

	case WM_INITDIALOG:
		OnInitDialog(hwndDlg);
		break;

	case WM_CLOSE:
		DestroyWindow(hwndDlg);
		return 0;

	case WM_COMMAND:
		switch (wParam)
		{
		case IDC_BUTTON6:
			if ((iItem = ListView_GetSelectionMark(GetDlgItem(hwndDlg, IDC_LIST1))) < _nDllCount)
			{
				pDll = _ppDll[iItem];

				if (pDll->_EntryPoint)
				{
					_pDoc->GoTo(pDll->_EntryPoint);
				}
			}
			break;

		case IDOK:
			if (len = GetWindowTextLength(GetDlgItem(hwndDlg, IDC_EDIT1)))
			{
				sz = pszText = (PWSTR)alloca((len+1) << 1);
				GetDlgItemText(hwndDlg, IDC_EDIT1, pszText, len + 1);

				_pDoc->CreateBOL(pszText);
			}
			else
			{
				_pDoc->deleteBOL();
			}
			DestroyWindow(hwndDlg);
			break;

		case IDC_BUTTON3:
			if ((iItem = ListView_GetSelectionMark(GetDlgItem(hwndDlg, IDC_LIST1))) < _nDllCount)
			{
				if (PCWSTR ImageName = _ppDll[iItem]->_ImageName)
				{
					len = GetWindowTextLength(hwndDlg = GetDlgItem(hwndDlg, IDC_EDIT1));
					DWORD ex = 2 + (DWORD)wcslen(ImageName);
					sz = pszText = (PWSTR)alloca((len+ex) << 1);
					if (len)
					{
						GetWindowText(hwndDlg, pszText, len + 1);
						do 
						{
							*sz = towlower(c = *sz);
						} while (sz++, c);
						sz--;
					}
					*sz++ = '\\';
					do 
					{
						*sz++ = towlower(c = *ImageName++);
					} while (c);

					if (len)
					{
						if (wtrnstr(len, pszText, ex - 1, pszText + len)) break;
					}
					SetWindowText(hwndDlg, pszText);
				}
			}
			break;

		case IDC_BUTTON5:
			if ((iItem = ListView_GetSelectionMark(GetDlgItem(hwndDlg, IDC_LIST1))) < _nDllCount)
			{
				pDll = _ppDll[iItem];

				pDll->Parse(_pDoc);

				if (pDll->_nForwards)
				{
					if (ZForwardDlg* p = new ZForwardDlg(_pDoc, pDll))
					{
						p->Create();
						p->Release();
					}
				}
			}
			break;
		case IDC_BUTTON2:
			iItem = ListView_GetSelectionMark(GetDlgItem(hwndDlg, IDC_LIST1));
__showexp:
			if (iItem < _nDllCount)
			{
				pDll = _ppDll[iItem];

				pDll->Parse(_pDoc);

				if (pDll->_nSymbols)
				{
					if (ZSymbolsDlg* p = new ZSymbolsDlg(_pDoc, pDll))
					{
						p->Create();
						p->Release();
					}
				}
			}
			break;
		}
		break;

	case WM_NOTIFY:

		if (((LPNMHDR)lParam)->idFrom == IDC_LIST1)
		{
			switch (((LPNMHDR)lParam)->code)
			{
			case NM_SETFOCUS:
				SendMessage(((LPNMHDR)lParam)->hwndFrom, WM_KILLFOCUS, 0, 0);
				break;

			case NM_DBLCLK:
				iItem = ((LPNMITEMACTIVATE)lParam)->iItem;
				goto __showexp;

			case LVN_COLUMNCLICK:
				if ((iItem = ((LPNMLISTVIEW)lParam)->iSubItem) < 4)
				{
					SORT_DLLS ss;
					ss.s = _bittestandcomplement(&_SortOrder, iItem) ? -1 : +1;
					ss.iSubItem = iItem;
					qsort(_ppDll, _nDllCount, sizeof(PVOID), QSORTFN(compareDLL));
					InvalidateRect(((LPNMHDR)lParam)->hwndFrom, 0, 0);
				}
				break;

			case LVN_GETINFOTIP:
				if (
					(iItem = ((LPNMLVGETINFOTIP)lParam)->iItem) < _nDllCount
					&&
					(cchTextMax = ((LPNMLVGETINFOTIP)lParam)->cchTextMax)
					&&
					(pszText = ((LPNMLVGETINFOTIP)lParam)->pszText)
					&&
					(ImagePath = _ppDll[iItem]->_ImagePath)
					)
				{
					goto __copy;
				}
				break;

			case LVN_GETDISPINFO:

				if (
					(((NMLVDISPINFO*)lParam)->item.mask & LVIF_TEXT) 
					&&
					(iItem = ((NMLVDISPINFO*)lParam)->item.iItem) < _nDllCount
					&&
					(cchTextMax = ((NMLVDISPINFO*)lParam)->item.cchTextMax)
					)
				{			
					pszText = ((NMLVDISPINFO*)lParam)->item.pszText;

					pDll = _ppDll[iItem];

					switch (((NMLVDISPINFO*)lParam)->item.iSubItem)
					{
					case 0:
						_snwprintf(pszText, cchTextMax, L"%03u", pDll->_index);
						break;
					case 1:
						_snwprintf(pszText, cchTextMax, L"%p", pDll->_BaseOfDll);
						break;
					case 2:
						_snwprintf(pszText, cchTextMax, L"%08x", pDll->_SizeOfImage);
						break;
					case 3:
						if (ImagePath = pDll->_ImageName)
						{
__copy:
							do 
							{
								*pszText++ = c = *ImagePath++;
							} while (c && --cchTextMax);

							if (c)
							{
								pszText[-1] = 0;
							}
						}
						break;
					default: pszText[0] = 0;
					}
				}
				break;
			}
		}
		break;
	}

	return ZDlg::DialogProc(hwndDlg, uMsg, wParam, lParam);
}

//////////////////////////////////////////////////////////////////////////
//

ZHandlesDlg::~ZHandlesDlg()
{
	if (_ItoI)
	{
		delete [] _ItoI;
	}

	if (_pshti)
	{
		delete [] _pshti;
	}
}

ZHandlesDlg::ZHandlesDlg(): _pshti(0), _ItoI(0), _bTypeCanceled(false), _bDropDown(false)
{
}

void SetHandlesCount(HWND hwndDlg, ULONG NumberOfHandles)
{
	WCHAR sz[128];
	swprintf_s(sz, _countof(sz), L"%x(%u) Handles", (ULONG)NumberOfHandles, (ULONG)NumberOfHandles);
	SetWindowText(hwndDlg, sz);
}

NTSTATUS ZHandlesDlg::BuildList(HWND hwndDlg)
{
	DWORD cb = 0x300000;
	NTSTATUS status;
	union {
		PVOID buf;
		PSYSTEM_HANDLE_INFORMATION_EX pshti;
	};

	do 
	{
		status = STATUS_INSUFFICIENT_RESOURCES;

		if (buf = new UCHAR[cb += PAGE_SIZE])
		{
			if (0 <= (status = NtQuerySystemInformation(SystemExtendedHandleInformation, buf, cb, &cb)))
			{
				if (ULONG_PTR NumberOfHandles = pshti->NumberOfHandles)
				{
					_pshti = pshti;
					_nItems = (ULONG)NumberOfHandles;
					SetHandlesCount( hwndDlg, (ULONG)NumberOfHandles);
					break;
				}
			}
			delete [] buf;
		}

	} while (STATUS_INFO_LENGTH_MISMATCH == status);

	return status;
}

BOOL ZHandlesDlg::OnInitDialog(HWND hwndDlg)
{
	if (0 > BuildList(hwndDlg))
	{
		return FALSE;
	}

	HWND hwnd = GetDlgItem(hwndDlg, IDC_LIST1);

	ListView_SetExtendedListViewStyle(hwnd, LVS_EX_INFOTIP|LVS_EX_BORDERSELECT|LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);

	SIZE size = { 8, 16 };
	if (HDC hdc = GetDC(hwnd))
	{
		HGDIOBJ o = SelectObject(hdc, (HGDIOBJ)SendMessage(hwnd, WM_GETFONT, 0, 0));
		GetTextExtentPoint32(hdc, L"W", 1, &size);
		SelectObject(hdc, o);
		ReleaseDC(hwnd, hdc);
	}

	LVCOLUMN lvc = { LVCF_FMT|LVCF_WIDTH|LVCF_TEXT|LVCF_SUBITEM, LVCFMT_LEFT };

	static PCWSTR headers[] = { L" PID ", L" HANDLE ", L" Object ", L" Access ", L" A ", L" TypeName " };

	DWORD lens[] = { 2+8, 2+8, 2+sizeof(PVOID)*2, 10, 6, 32 };

	do 
	{
		lvc.pszText = (PWSTR)headers[lvc.iSubItem], lvc.cx = lens[lvc.iSubItem] * size.cx;
		ListView_InsertColumn(hwnd, lvc.iSubItem, &lvc);
	} while (++lvc.iSubItem < RTL_NUMBER_OF(headers));

	SendMessage(ListView_GetToolTips(hwnd), TTM_SETDELAYTIME, TTDT_AUTOPOP, MAKELONG(MAXSHORT, 0));
	ListView_SetItemCountEx(hwnd, _nItems, 0);

	hwnd = GetDlgItem(hwndDlg, IDC_COMBO1);

	if (ULONG n = g_AOTI.count())
	{
		WCHAR tn[256];
		const OBJECT_TYPE_INFORMATION* poti = g_AOTI;
		do 
		{
			swprintf_s(tn, RTL_NUMBER_OF(tn), L"%wZ", &poti++->TypeName);
			ComboBox_AddString(hwnd, tn);
		} while (--n);
	}
	
	return TRUE;
}

void ZHandlesDlg::Apply(HWND hwndDlg)
{
	WCHAR sz[32], *c;
	ULONG_PTR pid = 0;
	PVOID obj = 0;

	if (GetDlgItemText(hwndDlg, IDC_EDIT1, sz, RTL_NUMBER_OF(sz)))
	{
		pid = uptoul(sz, &c, 16);

		if (!pid || *c)
		{
			SetFocus(GetDlgItem(hwndDlg, IDC_EDIT1));
			return;
		}
	}

	if (GetDlgItemText(hwndDlg, IDC_EDIT2, sz, RTL_NUMBER_OF(sz)))
	{
		obj = (PVOID)uptoul(sz, &c, 16);

		if (!obj || *c)
		{
			SetFocus(GetDlgItem(hwndDlg, IDC_EDIT2));
			return;
		}
	}

	HWND hwnd = GetDlgItem(hwndDlg, IDC_COMBO1);

	int i = ComboBox_GetCurSel(hwnd);

	if (_ItoI)
	{
		delete [] _ItoI;
		_ItoI = 0;
		_nItems = (ULONG)_pshti->NumberOfHandles;
	}

	if (pid || obj || i != CB_ERR)
	{
		ULONG n = 0;

		ULONG_PTR NumberOfHandles = _pshti->NumberOfHandles;

		SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX* Handles = _pshti->Handles;
		do 
		{
			if (pid && Handles->UniqueProcessId != pid)
			{
				continue;
			}

			if (obj && Handles->Object != obj)
			{
				continue;
			}

			if (i != CB_ERR && i != (int)g_AOTI.TypeIndexToIndex(Handles->ObjectTypeIndex))
			{
				continue;
			}

			n++;

		} while (Handles++, --NumberOfHandles);

		if (n)
		{
			if (PULONG pu = new ULONG [n])
			{
				_nItems = n;
				_ItoI = pu;

				NumberOfHandles = _pshti->NumberOfHandles;

				Handles = _pshti->Handles;
				int j = 0;
				do 
				{
					if (pid && Handles->UniqueProcessId != pid)
					{
						continue;
					}

					if (obj && Handles->Object != obj)
					{
						continue;
					}

					if (i != CB_ERR && i != (int)g_AOTI.TypeIndexToIndex(Handles->ObjectTypeIndex))
					{
						continue;
					}

					*pu++ = j;

				} while (Handles++, j++, --NumberOfHandles);
			}
		}
	}

	SetHandlesCount( hwndDlg, _nItems);
	ListView_SetItemCountEx(GetDlgItem(hwndDlg, IDC_LIST1), _nItems, 0);
}

INT_PTR ZHandlesDlg::DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_SIZE:
		Resize(wParam, lParam);
		break;

	case WM_INITDIALOG:
		CreateLayout(hwndDlg);
		if (!OnInitDialog(hwndDlg))
		{
			DestroyWindow(hwndDlg);
		}
		else
		{
			ShowWindow(hwndDlg, SW_SHOW);
		}
		break;

	case WM_GETMINMAXINFO:
		((PMINMAXINFO)lParam)->ptMinTrackSize.x = 400;
		((PMINMAXINFO)lParam)->ptMinTrackSize.y = 300;
		return TRUE;

	case WM_COMMAND:

		switch (wParam)
		{
		case IDCANCEL:
			DestroyWindow(hwndDlg);
			break;
		
		case IDC_BUTTON1:
			if (0 <= BuildList(hwndDlg))
			{
				if (_ItoI)
				{
					delete [] _ItoI;
					_ItoI = 0;
				}
				ListView_SetItemCountEx(GetDlgItem(hwndDlg, IDC_LIST1), _nItems, 0);
			}
			break;

		case IDOK:
			Apply(hwndDlg);
			break;

		case MAKEWPARAM(IDC_COMBO1, CBN_CLOSEUP ):
			_bDropDown = false;
			if (_bTypeCanceled)
			{
				_bTypeCanceled = false;
				ComboBox_SetCurSel((HWND)lParam, -1);
			}
			break;
		case MAKEWPARAM(IDC_COMBO1, CBN_SELENDCANCEL ):
			if (_bDropDown)
			{
				_bTypeCanceled = true;
			}
			break;
		case MAKEWPARAM(IDC_COMBO1, CBN_SELENDOK ):
			_bTypeCanceled = false;
			break;
		case MAKEWPARAM(IDC_COMBO1, CBN_DROPDOWN ):
			_bDropDown = true;
			break;
		}
		break;

	case WM_NOTIFY:
		if (wParam == IDC_LIST1)
		{
			int cchTextMax;
			PWSTR pszText;
			SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX* phi;

			switch (((LPNMHDR)lParam)->code)
			{
			case NM_SETFOCUS:
				SendMessage(((LPNMHDR)lParam)->hwndFrom, WM_KILLFOCUS, 0, 0);
				break;

			case LVN_BEGINLABELEDIT:
				return FALSE;

			case NM_DBLCLK:
				if (((LPNMITEMACTIVATE)lParam)->iSubItem == Obj)
				{
					if (phi = Item(((LPNMITEMACTIVATE)lParam)->iItem))
					{
						WCHAR sz[64];
						swprintf(sz, L"%p", phi->Object);
						SetDlgItemTextW(hwndDlg, IDC_EDIT2, sz);
					}
				}
				return 0;

			case LVN_GETINFOTIP:
				if (phi = Item(((LPNMLVGETINFOTIP)lParam)->iItem))
				{
					struct SYSTEM_PROCESS_ID_INFORMATION
					{
						HANDLE ProcessId;
						UNICODE_STRING ImageName;
					} spii = { (HANDLE)phi->UniqueProcessId };

					PVOID stack = alloca(guz);
					ULONG cb = 0, rcb = 0x40;
					NTSTATUS status;
					do 
					{
						if (cb < rcb)
						{
							cb = RtlPointerToOffset(spii.ImageName.Buffer = (PWSTR)alloca(rcb - cb), stack);
							spii.ImageName.MaximumLength = (USHORT)cb;
						}

						if (0 <= (status = NtQuerySystemInformation(SystemProcessIdInformation, &spii, sizeof(spii), 0)))
						{
							swprintf_s(((LPNMLVGETINFOTIP)lParam)->pszText, 
								((LPNMLVGETINFOTIP)lParam)->cchTextMax, 
								L"%wZ", &spii.ImageName);
							break;
						}
						rcb = spii.ImageName.MaximumLength;

					} while (status == STATUS_INFO_LENGTH_MISMATCH);
				}
				break;
			case LVN_GETDISPINFO:

				if (
					(((NMLVDISPINFO*)lParam)->item.mask & LVIF_TEXT) 
					&&
					(phi = Item(((NMLVDISPINFO*)lParam)->item.iItem))
					&&
					1 < (cchTextMax = ((NMLVDISPINFO*)lParam)->item.cchTextMax)
					)
				{			
					pszText = ((NMLVDISPINFO*)lParam)->item.pszText;

					switch (((NMLVDISPINFO*)lParam)->item.iSubItem)
					{
					case Pid:
						swprintf_s(pszText, cchTextMax, L"%I64x", (ULONG64)phi->UniqueProcessId);
						break;
					case Han:
						swprintf_s(pszText, cchTextMax, L"%I64x", (ULONG64)phi->HandleValue);
						break;
					case Obj:
						swprintf_s(pszText, cchTextMax, L"%p", phi->Object);
						break;
					case Access:
						swprintf_s(pszText, cchTextMax, L"%08x", phi->GrantedAccess);
						break;
					case Attr:
						swprintf_s(pszText, cchTextMax, L"%02x", phi->HandleAttributes);
						break;
					case TN:
						if (const OBJECT_TYPE_INFORMATION* poti = g_AOTI[(ULONG)g_AOTI.TypeIndexToIndex(phi->ObjectTypeIndex)])
						{
							swprintf_s(pszText, cchTextMax, L"%wZ", &poti->TypeName);
							break;
						}
					default: pszText[0] = 0;
					}
				}
				break;
			}
		}
		break;
	}

	return ZDlg::DialogProc(hwndDlg, uMsg, wParam, lParam);
}
//////////////////////////////////////////////////////////////////////////
// ZSymbolsDlg

ZSymbolsDlg::~ZSymbolsDlg()
{
	if (_pIndexes)
	{
		delete _pIndexes;
	}
	_pDll->Release();
	_pDoc->Release();
}

ZSymbolsDlg::ZSymbolsDlg(ZDbgDoc* pDoc, ZDll* pDll)
{
	_pDoc = pDoc;
	pDoc->AddRef();
	_pDll = pDll;
	pDll->AddRef();
	_SortOrder = 1;
	_pIndexes = 0;
	if (DWORD n = pDll->_nSymbols)
	{
		if (PDWORD pd = new DWORD[n])
		{
			_nItems = n;
			_pIndexes = pd;
			do 
			{
				--n;
				pd[n] = n;
			} while (n);
		}
	}
}

HWND ZSymbolsDlg::Create()
{
	return _pIndexes ? ZDlg::Create((HINSTANCE)&__ImageBase, MAKEINTRESOURCE(IDD_DIALOG4), ZGLOBALS::getMainHWND(), 0) : 0;
}

void ZSymbolsDlg::OnDetach()
{
	DestroyWindow(getHWND());
}

void ZSymbolsDlg::OnInitDialog(HWND hwnd)
{
	if (PCWSTR ImageName = _pDll->_ImageName)
	{
		PWSTR buf = (PWSTR)alloca((wcslen(ImageName) + 32)<< 1);
		swprintf(buf, L"[%x] %s (%x) symbols", (DWORD)_pDoc->getId(), ImageName, _nItems);
		SetWindowText(hwnd, buf);
	}

	hwnd = GetDlgItem(hwnd, IDC_LIST1);

	ListView_SetExtendedListViewStyle(hwnd, LVS_EX_BORDERSELECT|LVS_EX_INFOTIP|LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);

	SIZE size = { 8, 16 };
	if (HDC hdc = GetDC(hwnd))
	{
		HGDIOBJ o = SelectObject(hdc, (HGDIOBJ)SendMessage(hwnd, WM_GETFONT, 0, 0));
		GetTextExtentPoint32(hdc, L"W", 1, &size);
		SelectObject(hdc, o);
		ReleaseDC(hwnd, hdc);
	}

	LVCOLUMN lvc = { LVCF_FMT|LVCF_WIDTH|LVCF_TEXT|LVCF_SUBITEM, LVCFMT_LEFT };

	static PCWSTR headers[] = { L" Address ", L"E", L" Name " };
	
	DWORD lens[] = { 2+sizeof(PVOID)*2, 3, 64 };

	do 
	{
		lvc.pszText = (PWSTR)headers[lvc.iSubItem], lvc.cx = lens[lvc.iSubItem] * size.cx;
		ListView_InsertColumn(hwnd, lvc.iSubItem, &lvc);
	} while (++lvc.iSubItem < RTL_NUMBER_OF(headers));

	SendMessage(ListView_GetToolTips(hwnd), TTM_SETDELAYTIME, TTDT_AUTOPOP, MAKELONG(MAXSHORT, 0));
	
	ListView_SetItemCountEx(hwnd, _nItems, 0);
}

struct _SORT_SYMBOLS
{
	union{
		RVAOFS* pRO;
		PCSTR Names;
	};
	PSTR undNames, buf;
	PDWORD ofs;
	DWORD cb;
	int iSubItem;
	int s;

	_SORT_SYMBOLS()
	{
		ofs = 0;
	}
	
	~_SORT_SYMBOLS()
	{
		if (ofs)
		{
			delete ofs;
		}
	}

	PCSTR getName(DWORD i, DWORD o, PSTR sz)
	{
		DWORD l = NANE_OFS(o), len;

		if (o & FLAG_ORDINAL)
		{
			sprintf(sz, "#%u", l);
			return sz;
		}
		
		PSTR name = (PSTR)Names + l;

		if (*name == '?')
		{
			if (o = ofs[i])
			{
				name = RtlOffsetToPointer(ofs, o);
			}
			else
			{
				if (!cb || !(name = _unDName(buf, name, cb, UNDNAME_NAME_ONLY)))
				{
					return Names + l;
				}
				if ((len = 1 + (ULONG)strlen(name)) > 128)
				{
					name[127] = 0;
					len = 128;
				}
				name = strcpy(buf, name);
				ofs[i] = RtlPointerToOffset(ofs, name);
				o = len;
				buf += o, cb -= o;
			}
		}

		return name;
	}
};

typedef RTL_FRAME<_SORT_SYMBOLS> SORT_SYMBOLS;

int __cdecl compareSymbols(int& i, int& j)
{
	_SORT_SYMBOLS* p = SORT_SYMBOLS::get();
	RVAOFS* a = &p->pRO[i], *b = &p->pRO[j];
	int s = p->s;
	ULONG af, bf;

	switch (p->iSubItem)
	{
	default:__assume(false);
	case 0:
		if (a->rva < b->rva) return -s;
		if (a->rva > b->rva) return +s;
		return 0;
	case 1:
		af = NAME_FLAGS(a->ofs), bf = NAME_FLAGS(b->ofs);
		if (af < bf) return -s;
		if (af > bf) return +s;
	case 2:
		CHAR sza[16], szb[16];
		return strcmp(p->getName(i, a->ofs, sza), p->getName(j, b->ofs, szb))*s;
	}
}

INT_PTR ZSymbolsDlg::DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	DWORD iItem;
	CHAR undName[512];
	PWSTR pszText;
	int cchTextMax;

	switch (uMsg)
	{
	case WM_DESTROY:
		_pDoc->RemoveNotify(this);
		break;

	case WM_INITDIALOG:
		_pDoc->AddNotify(this);
		CreateLayout(hwndDlg);
		OnInitDialog(hwndDlg);
		break;

	case WM_CLOSE:
		DestroyWindow(hwndDlg);
		return 0;

	case WM_SIZE:
		Resize(wParam, lParam);
		break;

	case WM_GETMINMAXINFO:
		((PMINMAXINFO)lParam)->ptMinTrackSize.x = 400;
		((PMINMAXINFO)lParam)->ptMinTrackSize.y = 300;
		return TRUE;

	case WM_NOTIFY:

		if (wParam == IDC_LIST1)
		{
			switch (((LPNMHDR)lParam)->code)
			{
			case NM_SETFOCUS:
				SendMessage(((LPNMHDR)lParam)->hwndFrom, WM_KILLFOCUS, 0, 0);
				break;

			case NM_DBLCLK:
				if ((iItem = ((LPNMITEMACTIVATE)lParam)->iItem) < _nItems)
				{
					_pDoc->GoTo(RtlOffsetToPointer(_pDll->_BaseOfDll, (LONG)_pDll->_pSymbols[_pIndexes[iItem]].rva));
				}
				break;

			case LVN_COLUMNCLICK:
				if ((iItem = ((LPNMLISTVIEW)lParam)->iSubItem) < 3)
				{
					SORT_SYMBOLS ss;
					ss.iSubItem = iItem;
					ss.s = _bittestandcomplement(&_SortOrder, iItem) ? -1 : +1;
					ss.pRO = _pDll->_pSymbols;

					switch (iItem)
					{
					case 1:
					case 2:
						if (!(ss.ofs = new ULONG[_nItems << 6]))
						{
							return 0;
						}
						RtlFillMemoryUlong(ss.ofs, _nItems << 6, 0);
						ss.cb = (_nItems << 8) - (_nItems << 2);
						ss.buf = ss.undNames = (PSTR)(ss.ofs + _nItems);
						break;
					}
					qsort(_pIndexes, _nItems, sizeof(DWORD), QSORTFN(compareSymbols));
					InvalidateRect(((LPNMHDR)lParam)->hwndFrom, 0, 0);
				}
				break;

			case LVN_GETINFOTIP:
				if ((iItem = ((LPNMLVGETINFOTIP)lParam)->iItem) < _nItems)
				{
					pszText = ((LPNMLVGETINFOTIP)lParam)->pszText;
					cchTextMax = ((LPNMLVGETINFOTIP)lParam)->cchTextMax;

					if ((iItem = _pDll->_pSymbols[_pIndexes[iItem]].ofs) & FLAG_ORDINAL)
					{
						_snwprintf(pszText, cchTextMax, L"#%u", NANE_OFS(iItem));
					}
					else
					{
						_snwprintf(pszText, cchTextMax, L"%S",
							unDNameEx(undName, _pDll->_szSymbols + NANE_OFS(iItem), sizeof(undName), UNDNAME_DEFAULT));
					}
				}
				break;

			case LVN_GETDISPINFO:

				if (
					(((NMLVDISPINFO*)lParam)->item.mask & LVIF_TEXT) 
					&&
					(iItem = ((NMLVDISPINFO*)lParam)->item.iItem) < _nItems
					&&
					1 < (cchTextMax = ((NMLVDISPINFO*)lParam)->item.cchTextMax)
					)
				{			
					pszText = ((NMLVDISPINFO*)lParam)->item.pszText;

					RVAOFS* pRO = &_pDll->_pSymbols[_pIndexes[iItem]];

					switch (((NMLVDISPINFO*)lParam)->item.iSubItem)
					{
					case 0:
						_snwprintf(pszText, cchTextMax, L"%p", RtlOffsetToPointer(_pDll->_BaseOfDll, (LONG)pRO->rva));
						break;
					case 1:
						switch (NAME_FLAGS(pRO->ofs))
						{
						case FLAG_NOEXPORT:
							pszText[0] = 0;
							break;
						case FLAG_RVAEXPORT:
							pszText[0] = L'·';
							break;
						default:pszText[0] = '*';
						}
						pszText[1] = 0;
						break;
					case 2:
						if ((iItem = pRO->ofs) & FLAG_ORDINAL)
						{
							_snwprintf(pszText, cchTextMax, L"#%u", NANE_OFS(iItem));
						}
						else
						{
							_snwprintf(pszText, cchTextMax, L"%S",
								unDNameEx(undName, _pDll->_szSymbols + NANE_OFS(iItem), sizeof(undName), UNDNAME_NAME_ONLY));
						}
						break;
					default: pszText[0] = 0;
					}
				}
				break;
			}
		}
		break;
	}

	return ZDlg::DialogProc(hwndDlg, uMsg, wParam, lParam);
}

//////////////////////////////////////////////////////////////////////////
// ZForwardDlg
struct _SORT_FORWARDS
{
	PCSTR Names;
	int iSubItem;
	int s;
};

typedef RTL_FRAME<_SORT_FORWARDS> SORT_FORWARDS;

int __cdecl compareForwards(RVAOFS* p, RVAOFS* q)
{
	_SORT_FORWARDS* f = SORT_FORWARDS::get();
	PCSTR Names = f->Names, pa, pb;
	char a[20], b[20];
	ULONG ofs;
	switch (f->iSubItem)
	{
	case 0:
		if ((ofs = p->ofs) & FLAG_ORDINAL)
		{
			sprintf(a, "#%u", NANE_OFS(ofs)), pa = a;
		}
		else
		{
			pa = Names + NANE_OFS(ofs);
		}
		if ((ofs = q->ofs) & FLAG_ORDINAL)
		{
			sprintf(b, "#%u", NANE_OFS(ofs)), pb = b;
		}
		else
		{
			pb = Names + NANE_OFS(ofs);
		}
		break;
	default:
		pa = Names + p->rva, pb = Names + q->rva;
	}
	return strcmp(pa, pb) * f->s;
}

ZForwardDlg::ZForwardDlg(ZDbgDoc* pDoc, ZDll* pDll)
{
	_pDoc = pDoc;
	pDoc->AddRef();
	_pDll = pDll;
	pDll->AddRef();
	_SortOrder = 1;
	_nItems = pDll->_nForwards;
}

ZForwardDlg::~ZForwardDlg()
{
	_pDll->Release();
	_pDoc->Release();
}

HWND ZForwardDlg::Create()
{
	return ZDlg::Create((HINSTANCE)&__ImageBase, MAKEINTRESOURCE(IDD_DIALOG5), ZGLOBALS::getMainHWND(), 0);
}

void ZForwardDlg::OnDetach()
{
	DestroyWindow(getHWND());
}

void ZForwardDlg::OnInitDialog(HWND hwnd)
{
	if (PCWSTR ImageName = _pDll->_ImageName)
	{
		PWSTR buf = (PWSTR)alloca((wcslen(ImageName) + 32)<< 1);
		swprintf(buf, L"[%x] %s forwards", (DWORD)_pDoc->getId(), ImageName);
		SetWindowText(hwnd, buf);
	}

	hwnd = GetDlgItem(hwnd, IDC_LIST1);

	ListView_SetExtendedListViewStyle(hwnd, LVS_EX_BORDERSELECT|LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);

	SIZE size = { 8, 16 };
	if (HDC hdc = GetDC(hwnd))
	{
		HGDIOBJ o = SelectObject(hdc, (HGDIOBJ)SendMessage(hwnd, WM_GETFONT, 0, 0));
		GetTextExtentPoint32(hdc, L"W", 1, &size);
		SelectObject(hdc, o);
		ReleaseDC(hwnd, hdc);
	}

	LVCOLUMN lvc = { LVCF_FMT|LVCF_WIDTH|LVCF_TEXT|LVCF_SUBITEM, LVCFMT_LEFT };

	static PCWSTR headers[] = { L" Name ", L" Forward " };

	DWORD lens[] = { 32, 48 };

	do 
	{
		lvc.pszText = (PWSTR)headers[lvc.iSubItem], lvc.cx = lens[lvc.iSubItem] * size.cx;
		ListView_InsertColumn(hwnd, lvc.iSubItem, &lvc);
	} while (++lvc.iSubItem < RTL_NUMBER_OF(headers));

	ListView_SetItemCountEx(hwnd, _nItems, 0);
}

INT_PTR ZForwardDlg::DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	DWORD iItem;
	PWSTR pszText;
	int cchTextMax;

	switch (uMsg)
	{
	case WM_DESTROY:
		_pDoc->RemoveNotify(this);
		break;

	case WM_INITDIALOG:
		_pDoc->AddNotify(this);
		CreateLayout(hwndDlg);
		OnInitDialog(hwndDlg);
		break;

	case WM_CLOSE:
		DestroyWindow(hwndDlg);
		return 0;

	case WM_SIZE:
		Resize(wParam, lParam);
		break;

	case WM_GETMINMAXINFO:
		((PMINMAXINFO)lParam)->ptMinTrackSize.x = 400;
		((PMINMAXINFO)lParam)->ptMinTrackSize.y = 300;
		return TRUE;

	case WM_NOTIFY:

		if (((LPNMHDR)lParam)->idFrom == IDC_LIST1)
		{
			switch (((LPNMHDR)lParam)->code)
			{
			case NM_SETFOCUS:
				SendMessage(((LPNMHDR)lParam)->hwndFrom, WM_KILLFOCUS, 0, 0);
				break;

			case NM_DBLCLK:
				if ((iItem = ((LPNMITEMACTIVATE)lParam)->iItem) < _nItems)
				{
					if (PVOID Va = _pDoc->getVaByName(_pDll->_szForwards + _pDll->_pForwards[iItem].rva))
					{
						_pDoc->GoTo(Va);
					}
				}
				break;

			case LVN_COLUMNCLICK:
				if ((iItem = ((LPNMLISTVIEW)lParam)->iSubItem) < 2)
				{
					SORT_FORWARDS ss;
					ss.iSubItem = iItem;
					ss.s = _bittestandcomplement(&_SortOrder, iItem) ? -1 : +1;
					ss.Names = _pDll->_szForwards;
					qsort(_pDll->_pForwards, _nItems, sizeof(RVAOFS), QSORTFN(compareForwards));
					InvalidateRect(((LPNMHDR)lParam)->hwndFrom, 0, 0);
				}
				break;

			case LVN_GETDISPINFO:

				if (
					(((NMLVDISPINFO*)lParam)->item.mask & LVIF_TEXT) 
					&&
					(iItem = ((NMLVDISPINFO*)lParam)->item.iItem) < _nItems
					&&
					1 < (cchTextMax = ((NMLVDISPINFO*)lParam)->item.cchTextMax)
					)
				{			

					pszText = ((NMLVDISPINFO*)lParam)->item.pszText;
					RVAOFS* pRO = &_pDll->_pForwards[iItem];

					switch (((NMLVDISPINFO*)lParam)->item.iSubItem)
					{
					case 0:
						if ((iItem = pRO->ofs) & FLAG_ORDINAL)
						{
							_snwprintf(pszText, cchTextMax, L"#%u", NANE_OFS(iItem));
						}
						else
						{
							_snwprintf(pszText, cchTextMax, L"%S", _pDll->_szForwards + NANE_OFS(iItem));

						}
						break;
					case 1:
						_snwprintf(pszText, cchTextMax, L"%S", _pDll->_szForwards + pRO->rva);
						break;
					}
				}
				break;
			}
		}
		break;
	}

	return ZDlg::DialogProc(hwndDlg, uMsg, wParam, lParam);
}

//////////////////////////////////////////////////////////////////////////
// ZExecDlg

int OnDropDownProcesses(HWND hwndCtl, PHANDLE phSysToken = 0);

void ZExecDlg::OnInitDialog(HWND hwndDlg)
{
	COMBOBOXINFO cbi = { sizeof cbi };

	if (HWND hwndCtl = GetDlgItem(hwndDlg, IDC_COMBO1))
	{
		GetComboBoxInfo(hwndCtl, &cbi);
		ComboBox_SetMinVisible(hwndCtl, 12);
		LONG style = GetWindowLong(cbi.hwndList, GWL_STYLE);
		if (!(style & WS_HSCROLL)) SetWindowLong(cbi.hwndList, GWL_STYLE, style | WS_HSCROLL);
		
		if (0 <= (style = OnDropDownProcesses(hwndCtl, &_hSysToken)))
		{
			ComboBox_SetCurSel(hwndCtl, style);
		}
	}
	
	SetFocus(GetDlgItem(hwndDlg, IDC_EDIT1));
}

NTSTATUS CreateProcessExLow(HANDLE hProcess, 
							PCWSTR lpApplicationName, 
							PWSTR lpCommandLine, 
							DWORD dwCreationFlags, 
							PCWSTR lpCurrentDirectory,
							STARTUPINFOW* si, 
							PROCESS_INFORMATION* pi);

NTSTATUS CreateProcessEx(PCLIENT_ID cid, 
						 BOOLEAN bLow,
						 PCWSTR lpApplicationName, 
						 PWSTR lpCommandLine, 
						 DWORD dwCreationFlags, 
						 PCWSTR lpCurrentDirectory,
						 STARTUPINFOEXW* si, 
						 PROCESS_INFORMATION* pi);

struct AutoImpesonate
{
	BOOLEAN _bRevertToSelf;

	~AutoImpesonate()
	{
		if (_bRevertToSelf)
		{
			HANDLE hToken = 0;
			NtSetInformationThread(NtCurrentThread(), ThreadImpersonationToken, &hToken, sizeof(hToken));
		}
	}

	AutoImpesonate(HANDLE hToken)
	{
		_bRevertToSelf = hToken ? 
			0 <= NtSetInformationThread(NtCurrentThread(), ThreadImpersonationToken, &hToken, sizeof(hToken)) : FALSE;
	}
};

NTSTATUS GetLastNtStatus(BOOL fOk);

BOOL ZExecDlg::OnOk(HWND hwndDlg)
{
	HWND hwnd = GetDlgItem(hwndDlg, IDC_EDIT1);
	
	int len = GetWindowTextLength(hwnd);

	if (!len) 
	{
		SetFocus(hwnd);
		return FALSE;
	}

	PWSTR AppName = (PWSTR)alloca((len + 1) << 1);

	GetWindowText(hwnd, AppName, len + 1);

	PWSTR workDir = 0;

	if (len = GetWindowTextLength(hwnd = GetDlgItem(hwndDlg, IDC_EDIT3)))
	{
		workDir = (PWSTR)alloca((len + 1) << 1);

		GetWindowText(hwnd, workDir, len + 1);
	}

	if (_bDump)
	{
		NTSTATUS status = STATUS_UNSUCCESSFUL;
		if (workDir)
		{
			UNICODE_STRING ObjectName;
			if (RtlDosPathNameToNtPathName_U(workDir, &ObjectName, 0, 0))
			{
				OBJECT_ATTRIBUTES oa = { sizeof(oa), 0, &ObjectName, OBJ_CASE_INSENSITIVE };
				HANDLE hFile;
				IO_STATUS_BLOCK iosb;

				status = ZwOpenFile(&hFile, FILE_GENERIC_READ, &oa, &iosb, FILE_SHARE_VALID_FLAGS, FILE_DIRECTORY_FILE);

				RtlFreeUnicodeString(&ObjectName);

				if (0 > status)
				{
					ShowNTStatus(hwndDlg, status, L"NT_SYMBOL_PATH invalid !");
					workDir = 0;
				}
				else
				{
					NtClose(hFile);
				}
			}
		}
		
		if (!workDir) 
		{
			SetFocus(hwnd);
			return FALSE;
		}

		if (ZDbgDoc* p = new ZDbgDoc(FALSE))
		{
			if (0 > (status = p->OpenDump(AppName, workDir)))
			{
				p->Rundown();
			}
			p->Release();
		}

		if (0 > status)
		{
			ShowNTStatus(hwndDlg, status, L"Atach to Process Fail");
			return FALSE;
		}

		return TRUE;
	}

	hwnd = GetDlgItem(hwndDlg, IDC_COMBO1);

	int i = ComboBox_GetCurSel(hwnd);

	if (0 > i) 
	{
		SetFocus(hwnd);
		return FALSE;
	}

	CLIENT_ID cid = { (HANDLE)ComboBox_GetItemData(hwnd, i) };

	BOOLEAN bLow = IsDlgButtonChecked(hwndDlg, IDC_CHECK1) == BST_CHECKED;

	PWSTR cmdline = 0;

	if (len = GetWindowTextLength(hwnd = GetDlgItem(hwndDlg, IDC_EDIT2)))
	{
		cmdline = (PWSTR)alloca((len + 1) << 1);

		GetWindowText(hwnd, cmdline, len + 1);
	}

	PROCESS_INFORMATION pi;
	STARTUPINFOEXW si = { {sizeof(si) } };

	NTSTATUS status;

	if ((ULONG_PTR)cid.UniqueProcess == (ULONG_PTR)GetCurrentProcessId())
	{
		if (bLow)
		{
			status = CreateProcessExLow(NtCurrentProcess(), 
				AppName, cmdline, CREATE_PRESERVE_CODE_AUTHZ_LEVEL|DEBUG_PROCESS, 
				workDir, &si.StartupInfo, &pi);
		}
		else
		{
			status = GetLastNtStatus(CreateProcessW(AppName, cmdline, 0, 0, 0, CREATE_PRESERVE_CODE_AUTHZ_LEVEL|DEBUG_PROCESS, 
				0, workDir, &si.StartupInfo, &pi));
		}
	}
	else
	{
		AutoImpesonate ai(_hSysToken);

		status = CreateProcessEx(&cid, bLow, AppName, cmdline, 
			CREATE_PRESERVE_CODE_AUTHZ_LEVEL|DEBUG_PROCESS|EXTENDED_STARTUPINFO_PRESENT, 
			workDir, &si, &pi);
	}
	
	if (0 > status)
	{
		ShowNTStatus(hwndDlg, status, L"Fail create Process");
		return FALSE;
	}
	else
	{
		NtClose(pi.hThread);		
		NtClose(pi.hProcess);
	}

	return TRUE;
}

INT_PTR ZExecDlg::DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_INITDIALOG:
		_bDump = FALSE;
		OnInitDialog(hwndDlg);
		break;

	case WM_NCDESTROY:
		if (_hSysToken)
		{
			NtClose(_hSysToken);
		}
		break;

	case WM_COMMAND:
		switch (wParam)
		{
		case IDOK:
			if (!OnOk(hwndDlg))
			{
				break;
			}
		case IDCANCEL:
			DestroyWindow(hwndDlg);
			break;
		case MAKEWPARAM(IDC_COMBO1, CBN_DROPDOWN):
			OnDropDownProcesses((HWND)lParam);
			break;
		case MAKEWPARAM(IDC_CHECK2, BN_CLICKED):
			EnableWindow(GetDlgItem(hwndDlg, IDC_EDIT2), _bDump);
			EnableWindow(GetDlgItem(hwndDlg, IDC_COMBO1), _bDump);
			EnableWindow(GetDlgItem(hwndDlg, IDC_CHECK1), _bDump);
			_bDump = !_bDump;
			SetDlgItemText(hwndDlg, IDC_EDIT3, _bDump ? static_cast<GLOBALS_EX*>(ZGLOBALS::get())->getPath() : L"");
			SetDlgItemText(hwndDlg, IDC_STATIC1, _bDump ? L"CrashDmp" : L"App Name");
			SetDlgItemText(hwndDlg, IDC_STATIC3, _bDump ? L"Sym Path" : L"Work Dir");
			SetDlgItemText(hwndDlg, IDOK, _bDump ? L"Open" : L"Run");
			break;
		case MAKEWPARAM(IDC_BUTTON1, BN_CLICKED):
			{
				static const COMDLG_FILTERSPEC rgSpec[] =
				{ 
					{ L"Executable", L"*.exe" },
					{ L"Crash Dumps", L"*.dmp" },
					{ L"All files", L"*" },
				};
				OnBrowse(hwndDlg, IDC_EDIT1, _countof(rgSpec), rgSpec, _bDump ? 1 : 0);
			}
			break;
		}
		break;
	}
	return ZDlg::DialogProc(hwndDlg, uMsg, wParam, lParam);
}

//////////////////////////////////////////////////////////////////////////
// ZExceptionDlg

void ZExceptionDlg::OnInitDialog(HWND hwnd)
{
	_wait_1 = FALSE, _bCanDel = FALSE;
	ListView_SetExtendedListViewStyle(hwnd, LVS_EX_CHECKBOXES|LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);

	RECT rc;
	GetClientRect(hwnd, &rc);
	LVCOLUMN lvc = { LVCF_FMT|LVCF_WIDTH, LVCFMT_LEFT, rc.right - GetSystemMetrics(SM_CXVSCROLL)};
	ListView_InsertColumn(hwnd, 0, &lvc);
	ListView_SetItemCountEx(hwnd, _N + 1, 0);
	ListView_SetCheckState(hwnd, 0, TRUE);
	ListView_SetCheckState(hwnd, 1, FALSE);
}

INT_PTR ZExceptionDlg::DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	DWORD iItem;
	WCHAR sz[9], *c;

	switch (uMsg)
	{
	case WM_INITDIALOG:
		OnInitDialog(GetDlgItem(hwndDlg, IDC_LIST1));
		break;
	case WM_COMMAND:
		switch (wParam)
		{
		case IDOK:
			static_cast<ZExceptionFC*>(static_cast<ZMyApp*>(ZGLOBALS::getApp()))->Save(_bits, _N - 10, _status + 10);
		case IDCANCEL:
			DestroyWindow(hwndDlg);
			break;
		case MAKEWPARAM(IDC_EDIT1, EN_UPDATE):
			EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON1), 
				_N < 31 && GetWindowText((HWND)lParam, sz, RTL_NUMBER_OF(sz)) &&
				(iItem = wcstoul(sz, &c, 16)) && !*c && !findDWORD(_N, _status, iItem));
			break;
		case MAKEWPARAM(IDC_BUTTON1, BN_CLICKED):
			if (_N < 31 && GetWindowText(GetDlgItem(hwndDlg, IDC_EDIT1), sz, RTL_NUMBER_OF(sz)) &&
				(iItem = wcstoul(sz, &c, 16)) && !*c && !findDWORD(_N, _status, iItem))
			{
				EnableWindow((HWND)lParam, FALSE);
				_status[_N] = iItem;
				_bittestandset(&_bits, 1 + _N);
				ListView_SetItemCountEx(GetDlgItem(hwndDlg, IDC_LIST1), ++_N + 1, 0);
			}
			break;
		case MAKEWPARAM(IDC_BUTTON2, BN_CLICKED):
			if (_bCanDel)
			{
				if (iItem = _N - _iSel)
				{
					__movsd(_status + _iSel - 1, _status + _iSel, iItem);
					DWORD mask = (1 << _iSel) - 1;
					LONG b = _bits & ~mask;
					_bits &= mask;
					_bits |= (b >> 1);
				}
				else
				{
					_bCanDel = FALSE;
					EnableWindow((HWND)lParam, FALSE);
				}
				ListView_SetItemCountEx(GetDlgItem(hwndDlg, IDC_LIST1), --_N + 1, 0);
			}
			break;
		}
		break;

	case WM_NOTIFY:
		if (((LPNMHDR)lParam)->idFrom == IDC_LIST1)
		{
			switch (((LPNMHDR)lParam)->code)
			{
			case NM_SETFOCUS:
				SendMessage(((LPNMHDR)lParam)->hwndFrom, WM_KILLFOCUS, 0, 0);
				break;
			case LVN_ITEMCHANGED:
				if ((iItem = ((LPNMLISTVIEW)lParam)->iItem) == MAXDWORD)
				{
					if (!_wait_1 && _bCanDel)
					{
						_bCanDel = FALSE;
						EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON2), FALSE);
					}
				}
				else if (((LPNMLISTVIEW)lParam)->uChanged & LVIF_STATE)
				{
					if (((LPNMLISTVIEW)lParam)->uNewState & LVIS_SELECTED)
					{
						_iSel = iItem;
						_wait_1 = FALSE;
						if (iItem > 10)
						{
							if (!_bCanDel)
							{
								_bCanDel = TRUE;
								EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON2), TRUE);
							}
						}
						else
						{
							if (_bCanDel)
							{
								_bCanDel = FALSE;
								EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON2), FALSE);
							}
						}
					}
					else
					{
						_wait_1 = TRUE;
					}
				}
				break;
			case NM_CLICK:
				if ((iItem = ((LPNMITEMACTIVATE)lParam)->iItem) < _N + 1)
				{
					LVHITTESTINFO lvti;
					lvti.pt = ((LPNMITEMACTIVATE)lParam)->ptAction;
					ListView_SubItemHitTest(hwndDlg = ((LPNMITEMACTIVATE)lParam)->hdr.hwndFrom, &lvti);

					if (lvti.flags == LVHT_ONITEMSTATEICON)
					{
						_bittestandcomplement(&_bits, iItem);//LVIS_SELECTED 

						ListView_RedrawItems(hwndDlg, iItem, iItem);
					}
				}
				break;

			case LVN_GETDISPINFO:

				if ((iItem = ((NMLVDISPINFO*)lParam)->item.iItem) < _N + 1)
				{			
					((NMLVDISPINFO*)lParam)->item.mask |= LVIF_STATE;
					((NMLVDISPINFO*)lParam)->item.state = INDEXTOSTATEIMAGEMASK(1 + _bittest(&_bits, iItem));
					((NMLVDISPINFO*)lParam)->item.stateMask = LVIS_STATEIMAGEMASK;

					if (((NMLVDISPINFO*)lParam)->item.mask & LVIF_TEXT)
					{
						static PCSTR msg[] = {
							"SINGLE_STEP",
							"BREAKPOINT",
							"DATATYPE_MISALIGNMENT",
							"GUARD_PAGE_VIOLATION",
							"ACCESS_VIOLATION",
							"INTEGER_DIVIDE_BY_ZERO",
							"INTEGER_OVERFLOW",
							"ILLEGAL_INSTRUCTION",
							"PRIVILEGED_INSTRUCTION",
							"STACK_OVERFLOW",
						};

						PWSTR pszText = ((NMLVDISPINFO*)lParam)->item.pszText;
						int cchTextMax = ((NMLVDISPINFO*)lParam)->item.cchTextMax;

						if (iItem--)
						{
							_snwprintf(pszText, cchTextMax, L"%08X %S", _status[iItem], iItem < RTL_NUMBER_OF(msg) ? msg[iItem] : "");
						}
						else
						{
							_snwprintf(pszText, cchTextMax, L"Default");
						}
					}
				}
				break;
			}
		}
		break;
	}
	return ZDlg::DialogProc(hwndDlg, uMsg, wParam, lParam);
}

ZExceptionDlg::ZExceptionDlg()
{
	_ZGLOBALS* globals = ZGLOBALS::get();

	ZExceptionFC* p = static_cast<ZMyApp*>(globals->App);

	static NTSTATUS st[] = {
		STATUS_SINGLE_STEP,
		STATUS_BREAKPOINT,
		STATUS_DATATYPE_MISALIGNMENT,
		STATUS_GUARD_PAGE_VIOLATION,
		STATUS_ACCESS_VIOLATION,
		STATUS_INTEGER_DIVIDE_BY_ZERO,
		STATUS_INTEGER_OVERFLOW,
		STATUS_ILLEGAL_INSTRUCTION,
		STATUS_PRIVILEGED_INSTRUCTION,
		STATUS_STACK_OVERFLOW,
	};

	__movsd(_status, (PULONG)st, RTL_NUMBER_OF(st));

	DWORD n;
	_bits = p->get(&n, _status + RTL_NUMBER_OF(st));
	_N = n + RTL_NUMBER_OF(st);

	if (ZToolBar* tb = globals->MainFrame)
	{
		tb->EnableCmd(IDB_BITMAP1, FALSE);
	}
}

ZExceptionDlg::~ZExceptionDlg()
{
	if (ZToolBar* tb = ZGLOBALS::getMainFrame())
	{
		tb->EnableCmd(IDB_BITMAP1, TRUE);
	}
}

//////////////////////////////////////////////////////////////////////////
// ZVmDialog

void zasa(HWND hwndDlg, BOOL f1, BOOL f2, BOOL f3)
{
	static UINT s1[] = {IDC_CHECK6,IDC_CHECK7,IDC_CHECK8,IDC_CHECK9};
	static UINT s2[] = {IDC_RADIO4,IDC_RADIO5};
	static UINT s3[] = {IDC_RADIO8,IDC_RADIO7};

	int uMsg = RTL_NUMBER_OF(s1);
	do 
	{
		EnableWindow(GetDlgItem(hwndDlg, s1[--uMsg]), f1);
	} while (uMsg);
	uMsg = RTL_NUMBER_OF(s2);
	do 
	{
		EnableWindow(GetDlgItem(hwndDlg, s2[--uMsg]), f2);
	} while (uMsg);
	uMsg = RTL_NUMBER_OF(s3);
	do 
	{
		EnableWindow(GetDlgItem(hwndDlg, s3[--uMsg]), f3);
	} while (uMsg);
}

void SetAndAnimate(HWND hwnd, PCWSTR sz)
{
	SetWindowText(hwnd, sz);
	AnimateWindow(hwnd, 400, AW_HIDE|AW_HOR_POSITIVE|AW_SLIDE);
	AnimateWindow(hwnd, 400, AW_ACTIVATE|AW_HOR_NEGATIVE|AW_SLIDE);
}

INT_PTR ZVmDialog::DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	WCHAR sz[17], *c;
	NTSTATUS status;

	switch (uMsg)
	{
	case WM_INITDIALOG:
		CheckDlgButton(hwndDlg, IDC_RADIO1, BST_CHECKED);
		CheckDlgButton(hwndDlg, IDC_RADIO4, BST_CHECKED);
		CheckDlgButton(hwndDlg, IDC_RADIO7, BST_CHECKED);
		break;
	case WM_COMMAND:
		switch (wParam)
		{
		case IDC_RADIO1:
			_f = 0;
			zasa(hwndDlg, TRUE, TRUE, FALSE);
			break;
		case IDC_RADIO2:
			_f = 1;
			zasa(hwndDlg, FALSE, FALSE, TRUE);
			break;
		case IDC_RADIO3:
			_f = 2;
			zasa(hwndDlg, TRUE, FALSE, FALSE);
			break;
		case IDCANCEL:
			DestroyWindow(hwndDlg);
			return 0;
		case IDOK:
			status = IDC_EDIT1;
			if (GetDlgItemText(hwndDlg, IDC_EDIT1, sz, RTL_NUMBER_OF(sz)))
			{
				PVOID Address = (PVOID)uptoul(sz, &c, 16);
				
				if (*c)
				{
					goto __err;
				}
				
				status = IDC_EDIT2;

				if (!GetDlgItemText(hwndDlg, IDC_EDIT2, sz, RTL_NUMBER_OF(sz)))
				{
					goto __err;
				}

				SIZE_T RegionSize = wcstoul(sz, &c, 16);
				
				if (*c)
				{
					goto __err;
				}

				ULONG Type = 0, Protect = 0;
				if (IsDlgButtonChecked(hwndDlg, IDC_CHECK7) == BST_CHECKED)
				{
					Protect |= 1;
				}
				if (IsDlgButtonChecked(hwndDlg, IDC_CHECK8) == BST_CHECKED)
				{
					Protect |= 2;
				}
				if (IsDlgButtonChecked(hwndDlg, IDC_CHECK9) == BST_CHECKED)
				{
					Protect |= 4;
				}
				static ULONG zz[] = { 
					PAGE_NOACCESS,
					PAGE_READONLY,
					PAGE_READWRITE,
					PAGE_READWRITE,
					PAGE_EXECUTE,
					PAGE_EXECUTE_READ,
					PAGE_EXECUTE_READWRITE,
					PAGE_EXECUTE_READWRITE
				};
				
				Protect = zz[Protect];

				if (IsDlgButtonChecked(hwndDlg, IDC_CHECK6) == BST_CHECKED)
				{
					Protect |= PAGE_GUARD;
				}

				switch (_f)
				{
				case 0://alloc
					if (IsDlgButtonChecked(hwndDlg, IDC_RADIO4) == BST_CHECKED)
					{
						Type |= MEM_COMMIT;
					}
					if (IsDlgButtonChecked(hwndDlg, IDC_RADIO5) == BST_CHECKED)
					{
						Type |= MEM_RESERVE;
					}
					status = ZwAllocateVirtualMemory(_hProcess, &Address, 0, &RegionSize, Type, Protect);
					break;
				case 1://free
					if (IsDlgButtonChecked(hwndDlg, IDC_RADIO7) == BST_CHECKED)
					{
						Type = MEM_DECOMMIT;
					}
					if (IsDlgButtonChecked(hwndDlg, IDC_RADIO8) == BST_CHECKED)
					{
						Type = MEM_RELEASE;
					}
					status = ZwFreeVirtualMemory(_hProcess, &Address, &RegionSize, Type);
					break;
				case 2://protect
					status = ZwProtectVirtualMemory(_hProcess, &Address, &RegionSize, Protect, &Protect);
					break;
				default:__assume(false);
				}

				if (0 <= status)
				{
					swprintf(sz, L"%X", (ULONG)RegionSize);
					SetDlgItemText(hwndDlg, IDC_EDIT2, sz);
					swprintf(sz, L"%p", Address);
					SetDlgItemText(hwndDlg, IDC_EDIT1, sz);
				}
			}
			else
			{
__err:
				SetFocus(GetDlgItem(hwndDlg, status));
				status = STATUS_INVALID_PARAMETER;
			}
			swprintf(sz, L"%X", status);
			SetAndAnimate(GetDlgItem(hwndDlg, IDC_STATIC1), sz);
			break;
		}
		break;
	}
	return ZDlg::DialogProc(hwndDlg, uMsg, wParam, lParam);
}

ZVmDialog::ZVmDialog()
{
	_hProcess = 0;
	_f = 0;
}

ZVmDialog::~ZVmDialog()
{
	if (_hProcess)
	{
		NtClose(_hProcess);
	}
}

void ZVmDialog::Create(HANDLE UniqueProcess)
{
	if (ZVmDialog* p = new ZVmDialog)
	{
		CLIENT_ID cid = { UniqueProcess };
		if (0 <= MyOpenProcess(&p->_hProcess, PROCESS_VM_OPERATION, &zoa, &cid))
		{
			p->ZDlg::Create((HINSTANCE)&__ImageBase, MAKEINTRESOURCE(IDD_DIALOG10), ZGLOBALS::getMainHWND(), 0);
		}
		p->Release();
	}
}

//////////////////////////////////////////////////////////////////////////
// ZPrintFilter

STATIC_UNICODE_STRING_(PrMask);

void InitPrMask()
{
	KEY_VALUE_PARTIAL_INFORMATION kvpi;
	if (
		0 <= ZGLOBALS::getRegistry()->GetValue(&PrMask, KeyValuePartialInformation, &kvpi, sizeof(kvpi), &kvpi.TitleIndex) &&
		kvpi.Type == REG_DWORD && kvpi.DataLength == sizeof(DWORD) && _bittest(&g_printMask, prGen)
		)
	{
		g_printMask = *(ULONG*)kvpi.Data;
	}
}

INT_PTR ZPrintFilter::DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	int i;

	switch (uMsg)
	{
	case WM_INITDIALOG:
		i = prDbgPrint;
		do 
		{
			if (_bittest(&g_printMask, i))
			{
				CheckDlgButton(hwndDlg, IDC_CHECK1 + i, BST_CHECKED);
			}
		} while (i--);
		break;
	case WM_COMMAND:
		switch (wParam)
		{
		case IDOK:
			i = prDbgPrint;
			do 
			{
				if (IsDlgButtonChecked(hwndDlg, IDC_CHECK1 + i) == BST_CHECKED)
				{
					_bittestandset(&g_printMask, i);
				}
				else
				{
					_bittestandreset(&g_printMask, i);
				}
			} while (i--);
			ZGLOBALS::getRegistry()->SetValue(&PrMask, REG_DWORD, &g_printMask, sizeof(g_printMask));
		case IDCANCEL:
			DestroyWindow(hwndDlg);
			break;
		}
		break;
	}
	return ZDlg::DialogProc(hwndDlg, uMsg, wParam, lParam);
}

ZPrintFilter::ZPrintFilter()
{
	if (ZToolBar* tb = ZGLOBALS::getMainFrame())
	{
		tb->EnableCmd(ID_PRFLT, FALSE);
	}
}

ZPrintFilter::~ZPrintFilter()
{
	if (ZToolBar* tb = ZGLOBALS::getMainFrame())
	{
		tb->EnableCmd(ID_PRFLT, TRUE);
	}
}

//////////////////////////////////////////////////////////////////////////
// ZBreakDll

void ZBreakDll::OnDetach()
{
	DestroyWindow(getHWND());
}

INT_PTR ZBreakDll::DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	PCWSTR name;
	switch (uMsg)
	{
	case WM_DESTROY:
		_pDoc->RemoveNotify(this);
		break;

	case WM_INITDIALOG:
		_pDoc->AddNotify(this);
		if (!_ep)
		{
			EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON1), FALSE);
		}
		if (name = wcsrchr((PCWSTR)lParam, '\\'))
		{
			name++;
		}
		else
		{
			name = (PCWSTR)lParam;
		}
		SetWindowText(hwndDlg, name);
		SetDlgItemText(hwndDlg, IDC_STATIC1, (PCWSTR)lParam);
		break;
	case WM_COMMAND:
		switch (wParam)
		{
		case IDOK:
			if (_ep)
			{
				_pDoc->GoTo(_ep);
			}
		case IDCANCEL:
			DestroyWindow(hwndDlg);
			break;
		}
		break;
	}

	return ZDlg::DialogProc(hwndDlg, uMsg, wParam, lParam);
}

void ZBreakDll::create(PVOID ep, ZDbgDoc* pDoc, PCWSTR name)
{
	if (ZBreakDll* p = new ZBreakDll(ep, pDoc))
	{
		p->Create((HINSTANCE)&__ImageBase, MAKEINTRESOURCE(IDD_DIALOG12), ZGLOBALS::getMainHWND(), (LPARAM)name);
		p->Release();
	}
}

ZBreakDll::ZBreakDll(PVOID ep, ZDbgDoc* pDoc) : _ep(ep), _pDoc(pDoc)
{ 
	pDoc->AddRef(); 
}

ZBreakDll::~ZBreakDll() 
{ 
	_pDoc->Release(); 
}

//////////////////////////////////////////////////////////////////////////
// ZXrefs

void ZXrefs::OnDetach()
{
	DestroyWindow(getHWND());
}

INT_PTR ZXrefs::DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_DESTROY:
		_pDoc->RemoveNotify(this);
		break;

	case WM_INITDIALOG:
		_pDoc->AddNotify(this);
		break;

	case WM_COMMAND:
		switch(wParam)
		{
		case IDCANCEL:
			DestroyWindow(hwndDlg);
			break;
		case MAKEWPARAM(IDC_LIST1,LBN_DBLCLK):
			int i = ListBox_GetCurSel((HWND)lParam);
			if (0 <= i)
			{
				_pDoc->GoTo((PVOID)ListBox_GetItemData((HWND)lParam, i));
				_pDoc->ScrollAsmUp();
			}

			break;
		}
		break;
	}
	return ZDlg::DialogProc(hwndDlg, uMsg, wParam, lParam);
}

ZXrefs::ZXrefs(ZDbgDoc* pDoc) : _pDoc(pDoc)
{
	pDoc->AddRef(); 
}

ZXrefs::~ZXrefs()
{
	_pDoc->Release(); 
}

void ZXrefs::create(ZDbgDoc* pDoc, DWORD N, PLONG_PTR xrefs, LONG_PTR Va)
{
	if (ZXrefs* p = new ZXrefs(pDoc))
	{
		if (HWND hwnd = p->Create((HINSTANCE)&__ImageBase, MAKEINTRESOURCE(IDD_DIALOG13), ZGLOBALS::getMainHWND(), 0))
		{
			WCHAR sz[64];
			swprintf(sz, L"%u xrefs for %p found", N, (void*)Va);
			SetWindowText(hwnd, sz);
			if (HWND hwndLB = GetDlgItem(hwnd, IDC_LIST1))
			{
				do 
				{
					LONG_PTR lp = *xrefs++;
					swprintf(sz, L"%p", (void*)lp);
					ListBox_SetItemData(hwndLB, ListBox_AddString(hwndLB, sz), lp);
				} while (--N);
			}
			ShowWindow(hwnd, SW_SHOW);
		}
		p->Release();
	}
}

_NT_END