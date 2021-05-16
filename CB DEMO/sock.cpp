#include "stdafx.h"

_NT_BEGIN
//#define _PRINT_CPP_NAMES_
#include "../inc/initterm.h"
#include "../asio/ssl.h"

enum : ULONG {
	CONTROL_WPARAM_START = 0xB426E606,
	CONTROL_LPARAM_START = 0x4AC27B7B,
};

class WLog
{
	PVOID _BaseAddress;
	ULONG _RegionSize, _Ptr;

	PWSTR _buf()
	{
		return (PWSTR)_BaseAddress + _Ptr;
	}

	ULONG _cch()
	{
		return _RegionSize - _Ptr;
	}

public:
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

class CSite : public CSSLEndpoint
{
	HWND _hwnd;
	WLog _log;
	ULONG _time = GetTickCount();

	virtual SECURITY_STATUS OnEndHandshake()
	{
		_log(L"\r\n%s<%p>\r\n", __FUNCTIONW__, this);

		STATIC_ASTRING(get, 
			"GET /unsupportedbrowser HTTP/1.1\r\n"
			"Host: www.facebook.com\r\n"
			"Connection: Close\r\n"
			"\r\n");

		return SendUserData(get, sizeof(get));
	}

	virtual BOOL OnUserData(PSTR buf, ULONG cb)
	{
		_log(buf, cb);

		return TRUE;
	}

	virtual BOOL IsServer(PBOOLEAN pbMutualAuth = 0)
	{
		if (pbMutualAuth)
		{
			*pbMutualAuth = FALSE;
		}
		return FALSE;
	}

	virtual void OnIp(ULONG ip)
	{
		_log(L"\r\n%s<%p>(%08x)\r\n", __FUNCTIONW__, this, ip);
		if (ip)
		{
			SetTimeout(15000);
			Connect(ip, _byteswap_ushort(443));
		}
	}

	virtual void OnShutdown()
	{
		_log(L"\r\n%s<%p>\r\n", __FUNCTIONW__, this);
	}

	virtual void OnDisconnect()
	{
		StopTimeout();
		_log(L"\r\n%s<%p>\r\n", __FUNCTIONW__, this);
		__super::OnDisconnect();
	}

	virtual void OnEncryptDecryptError(SECURITY_STATUS ss)
	{
		_log(L"\r\n%s<%p>\r\n", __FUNCTIONW__, this, ss);
	}

	virtual ~CSite()
	{
		_log(L"\r\n%s<%p>(%u)\r\n", __FUNCTIONW__, this, GetTickCount() - _time);

		_log >> _hwnd;
	}

public:
	CSite(HWND hwnd, SharedCred* pCred) : CSSLEndpoint(pCred), _hwnd(hwnd)
	{
		_log.Init(0x100000);
	}
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
			if (SharedCred* pCred = new SharedCred)
			{
				if (0 <= pCred->Acquire(SECPKG_CRED_OUTBOUND, 0, 
					SCH_CRED_MANUAL_CRED_VALIDATION|SCH_CRED_NO_DEFAULT_CREDS, 
					SP_PROT_TLS1_1PLUS_CLIENT))
				{
					if (CSite* p = new CSite(pcs->hwnd, pCred))
					{
						if (!p->Create(0x8000))
						{
							p->DnsToIp("www.facebook.com");
						}
						p->Release();
					}
				}
				pCred->Release();
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

BOOLEAN APIENTRY DllMain(HMODULE hModule,
						 DWORD  ul_reason_for_call,
						 LPVOID /*lpReserved*/)
{

	static bool _bWsaCleanup;	

	WSADATA wd;
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(hModule);
		if (WSAStartup(WINSOCK_VERSION, &wd) != NOERROR)
		{
			return FALSE;
		}
		_bWsaCleanup = true;
		initterm();
		return TRUE;

	case DLL_PROCESS_DETACH:
		destroyterm();
		if (_bWsaCleanup)
		{
			WSACleanup();
		}
		break;
	}
	return FALSE;
}

_NT_END