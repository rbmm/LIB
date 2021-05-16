#include "stdafx.h"
#include <Winhttp.h>
_NT_BEGIN
//#define _PRINT_CPP_NAMES_

#include "../winhttp/whttp.h"

class WLog
{
	PVOID _BaseAddress;
	ULONG _RegionSize, _Ptr;

public:
	PWSTR _buf()
	{
		return (PWSTR)_BaseAddress + _Ptr;
	}

	ULONG _cch()
	{
		return _RegionSize - _Ptr;
	}

	void operator >> (HWND hwnd)
	{
		PVOID pv = (PVOID)SendMessage(hwnd, EM_GETHANDLE, 0, 0);
		PVOID BaseAddress = LocalReAlloc(_BaseAddress, _cch()*sizeof(WCHAR), 0);
		if (!BaseAddress)
		{
			BaseAddress = _BaseAddress;
		}
		_BaseAddress = 0;
		SendMessage(hwnd, EM_SETHANDLE, (WPARAM)BaseAddress, 0);
		if (pv)
		{
			LocalFree(pv);
		}
	}

	ULONG Init(SIZE_T RegionSize)
	{
		if (_BaseAddress = LocalAlloc(0, RegionSize))
		{
			_RegionSize = (ULONG)RegionSize / sizeof(WCHAR), _Ptr = 0;
			return NOERROR;
		}
		return GetLastError();
	}

	~WLog()
	{
		if (_BaseAddress)
		{
			LocalFree(_BaseAddress);
		}
	}

	WLog(WLog&&) = delete;
	WLog(WLog&) = delete;
	WLog(): _BaseAddress(0) {  }

	operator PCWSTR()
	{
		return (PCWSTR)_BaseAddress;
	}

	WLog& operator ()(PSTR buf, ULONG cb)
	{
		_Ptr += MultiByteToWideChar(CP_UTF8, 0, buf, cb, _buf(), _cch());

		return *this;
	}

	WLog& operator+=(ULONG len)
	{
		_Ptr += len;
		return *this;
	}

	WLog& operator ()(PCWSTR format, ...)
	{
		va_list args;
		va_start(args, format);

		int len = _vsnwprintf_s(_buf(), _cch(), _TRUNCATE, format, args);

		if (0 < len)
		{
			_Ptr += len;
		}

		va_end(args);

		return *this;
	}

	WLog& operator[](NTSTATUS dwError)
	{
		static HMODULE ghnt;
		if (!ghnt && !(ghnt = GetModuleHandle(L"ntdll"))) return *this;
		LPCVOID lpSource = ghnt;
		ULONG dwFlags = FORMAT_MESSAGE_FROM_HMODULE|FORMAT_MESSAGE_IGNORE_INSERTS;

		if (dwFlags = FormatMessageW(dwFlags, lpSource, dwError, 0, _buf(), _cch(), 0))
		{
			_Ptr += dwFlags;
		}
		return *this;
	}
};

class CMyEndp : public CWinhttpEndpoint
{
	HWND _hwnd;
	WLog log;
	ULONG _time = GetTickCount();

	virtual BOOL OnConnect(HINTERNET hRequest)
	{
		log(L"\r\n%S<%p>\r\n", __FUNCTION__, this);

		ULONG dwBufferLength = log._cch() * sizeof(WCHAR), dwIndex = 0;

		if (WinHttpQueryHeaders(
			hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF, 0, log._buf(), &dwBufferLength, &dwIndex))
		{
			log += dwBufferLength / sizeof(WCHAR);
		}

		return TRUE;
	}

	virtual void OnRead(PVOID pv, ULONG dwNumberOfBytesRead)
	{
		log((PSTR)pv, dwNumberOfBytesRead);
	}

	virtual void OnDisconnect()
	{
		log(L"\r\n%S<%p>\r\n", __FUNCTION__, this);
	}

	virtual ~CMyEndp()
	{
		log(L"\r\n%S<%p>(%u)\r\n", __FUNCTION__, this, GetTickCount() - _time);

		log >> _hwnd;
	}

	virtual void LogHttpError(DWORD_PTR dwResult, ULONG dwError )
	{
		log(L"\r\n-------\r\nLogHttpError(%p, %u)\r\n-------\r\n", dwResult, dwError);
	}

	virtual void OnSecureFailure(ULONG flags/* WINHTTP_CALLBACK_STATUS_FLAG_ */)
	{
		//PCERT_CONTEXT pCertContext;
		//ULONG cb = sizeof(pCertContext);
		//if (GetOption(WINHTTP_OPTION_SERVER_CERT_CONTEXT, &pCertContext, &cb))
		//{
		//	CertFreeCertificateContext(pCertContext);
		//}
		log(L"\r\n-------\r\nOnSecureFailure(%x)\r\n-------\r\n", flags);//WINHTTP_CALLBACK_STATUS_FLAG_INVALID_CA
	}

public:
	CMyEndp(HWND hwnd, CHttpConnection* pTarget) : CWinhttpEndpoint(pTarget), _hwnd(hwnd)
	{
	}

	ULONG InitLog(SIZE_T RegionSize)
	{
		return log.Init(RegionSize);
	}
};

enum : ULONG {
	CONTROL_WPARAM_START = 0xB426E606,
	CONTROL_LPARAM_START = 0x4AC27B7B,
};

LRESULT CALLBACK CallWndProc(int nCode,
							 WPARAM wParam,
							 LPARAM lParam)
{
	if (nCode == HC_ACTION)
	{
		PCWPSTRUCT pcs = (PCWPSTRUCT)lParam;

		if (pcs->message == WM_NULL && pcs->wParam == CONTROL_WPARAM_START && pcs->lParam == CONTROL_LPARAM_START)
		{
			//LoadLibraryW(L"o:\\vc\\release\\mojo.dll");

			if (CHttpConnection* pCon = new CHttpConnection)
			{
				ULONG sp = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;

				if (!pCon->Open() &&
					pCon->SetOption(WINHTTP_OPTION_SECURE_PROTOCOLS, &sp, sizeof(sp)) &&
					!pCon->Connect(L"www.facebook.com", INTERNET_DEFAULT_HTTPS_PORT))
				{
					if (CMyEndp* pEndp = new CMyEndp(pcs->hwnd, pCon))
					{
						if (!pEndp->InitReadBuffer(0x10000) && !pEndp->InitLog(0x100000))
						{
							if (HINTERNET hRequest = pEndp->OpenRequest(WINHTTP_FLAG_SECURE, L"GET", L"/unsupportedbrowser"))
							{
								pEndp->SendRequest(hRequest);
							}
						}
						pEndp->Release();
					}
				}
				pCon->Release();
			}
		}
	}

	return CallNextHookEx(0, nCode, wParam, lParam);
}

BOOL CALLBACK EnumThreadWndProc( HWND hwnd, LPARAM lParam)
{
	if (hwnd = FindWindowExW(hwnd, 0, WC_EDITW, 0))
	{
		if (HHOOK hhk = SetWindowsHookExW(WH_CALLWNDPROC, CallWndProc, (HMODULE)&__ImageBase, (ULONG)lParam))
		{
			SendMessageW(hwnd, WM_NULL, CONTROL_WPARAM_START, CONTROL_LPARAM_START);

			UnhookWindowsHookEx(hhk);
		}

		return FALSE;
	}

	return TRUE;
}

STDAPI DllRegisterServer()
{
	WCHAR appname[MAX_PATH];
	if (SearchPathW(0, L"notepad.exe", 0, _countof(appname), appname, 0))
	{
		STARTUPINFO si = { sizeof(si) };
		PROCESS_INFORMATION pi;
		if (CreateProcessW(appname, 0, 0, 0, 0, 0, 0, 0, &si, &pi))
		{
			CloseHandle(pi.hThread);
			if (WaitForInputIdle(pi.hProcess, INFINITE) == WAIT_OBJECT_0)
			{
				EnumThreadWindows(pi.dwThreadId, EnumThreadWndProc, pi.dwThreadId);
			}
			CloseHandle(pi.hProcess);
		}
	}

	return S_OK;
}

#include "../inc/initterm.h"

BOOLEAN APIENTRY DllMain(HMODULE hModule,
						 DWORD  ul_reason_for_call,
						 LPVOID /*lpReserved*/)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(hModule);
		initterm();
		return TRUE;

	case DLL_PROCESS_DETACH:
		destroyterm();
		break;
	}
	return FALSE;
}

_NT_END