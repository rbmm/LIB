#include "stdafx.h"

_NT_BEGIN
//#define _PRINT_CPP_NAMES_
#include "../inc/initterm.h"
#include "../asio/ssl.h"
#include <mstcpip.h>

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
		PVOID BaseAddress = LocalReAlloc(_BaseAddress, _Ptr*sizeof(WCHAR), 0);
		if (!BaseAddress)
		{
			BaseAddress = _BaseAddress;
		}
		_BaseAddress = 0;
		if (_Ptr == _RegionSize)
		{
			((PWSTR)BaseAddress)[_Ptr - 1] = 0;
		}
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

	WLog& operator[](HRESULT dwError)
	{
		LPCVOID lpSource = 0;
		ULONG dwFlags = FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS;

		if (dwError & FACILITY_NT_BIT)
		{
			dwError &= ~FACILITY_NT_BIT;

			static HMODULE ghnt;
			if (!ghnt && !(ghnt = GetModuleHandle(L"ntdll"))) return *this;
			lpSource = ghnt;
			dwFlags = FORMAT_MESSAGE_FROM_HMODULE|FORMAT_MESSAGE_IGNORE_INSERTS;
		}

		operator ()(dwError < 0 || lpSource ? L"// 0x%x\r\n" : L"// %u\r\n", dwError);

		if (dwFlags = FormatMessageW(dwFlags, lpSource, dwError, 0, _buf(), _cch(), 0))
		{
			_Ptr += dwFlags;
		}

		return *this;
	}
};

class CSite : public CSSLEndpointEx
{
	HWND _hwnd;
	PCSTR _host, _path;
	WLog _log;
	ULONG _time = GetTickCount();

	virtual void OnIp(ULONG ip)
	{
		WCHAR szip[16];
		RtlIpv4AddressToStringW((in_addr *)&ip, szip);
		_log(L"\r\n%s<%p>(%s -> %s) \r\n", __FUNCTIONW__, this, m_pszTargetName, szip);
		if (ip)
		{
			SetTimeout(8000);
			Connect(ip, _byteswap_ushort(m_pCred ? 443 : 80));
		}
	}

	virtual void OnServerConnect(ULONG status)
	{
		_log(L"\r\n%s<%p>(%x)\r\n", __FUNCTIONW__, this, status);
		_log[status];
	}

	virtual void OnServerDisconnect()
	{
		StopTimeout();
		_log(L"\r\n%s<%p>\r\n", __FUNCTIONW__, this);
	}

	virtual SECURITY_STATUS OnEndHandshake()
	{
		_log(L"\r\n%s<%p>\r\n", __FUNCTIONW__, this);

		USHORT port;
		if (!GetPort(&port))
		{
			_log(L"\r\nMy Port = %u\r\n", _byteswap_ushort(port));
		}

		STATIC_ASTRING(get, 
			"GET /%s HTTP/1.1\r\n"
			"Host: %s\r\n"
			"Connection: Close\r\n"
			"\r\n");
		
		SECURITY_STATUS s = SEC_E_INTERNAL_ERROR;

		int len = _scprintf(get, _path, _host);

		if (0 < len)
		{
			PSTR buf;
			if (CDataPacket* packet = AllocPacket(len+1, buf))
			{
				if (0 < (len = sprintf_s(buf, len+1, get, _path, _host)))
				{
					_log(buf, len);
					s = SendUserData(len, packet);
				}

				packet->Release();
			}
		}

		return s;
	}

	virtual BOOL OnUserData(PSTR buf, ULONG cb)
	{
		_log(buf, cb);

		//DbgPrint("\r\n\r\nOnUserData(%x, %p)\r\n\r\n", cb, buf);
		//ULONG cch;
		//do 
		//{
		//	cch = min(0x100, cb);
		//	DbgPrint("%.*s\r\n", cch, buf);
		//} while (buf += cch, cb -= cch);

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

	virtual void OnShutdown()
	{
		_log(L"\r\n%s<%p>\r\n", __FUNCTIONW__, this);
	}

	virtual void LogError(DWORD opCode, DWORD dwError)
	{
		DbgPrint("\r\n\r\nSocket Error(%.4s %x)\r\n\r\n", &opCode, dwError);
		_log(L"\r\n%s<%p>(%.4S %x)\r\n", __FUNCTIONW__, this, &opCode, dwError);
		_log[dwError];
	}

	virtual void OnEncryptDecryptError(SECURITY_STATUS ss)
	{
		DbgPrint("\r\n\r\nOnEncryptDecryptError(%x)\r\n\r\n", ss);
		_log(L"\r\n%s<%p>(%x)\r\n", __FUNCTIONW__, this, ss);
		_log[ss];
	}

	virtual ~CSite()
	{
		_log(L"\r\n%s<%p>(%u)\r\n", __FUNCTIONW__, this, GetTickCount() - _time);
		_log >> _hwnd;
		SendMessageW(_hwnd, EM_SCROLL, SB_BOTTOM, 0);
	}

public:
	CSite(HWND hwnd, SharedCred* pCred, PCSTR host, PCSTR path, PWSTR pszTargetName) : 
	  CSSLEndpointEx(pCred), _hwnd(hwnd), _host(host), _path(path ? path : "")
	{
		m_pszTargetName = pszTargetName;
		_log.Init(0x100000);// up to 1Mb
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
			PCSTR host = "www.myip.com";
			PCSTR path = "";
			PCWSTR pszTargetName = L"www.myip.com";
			BOOL bSSl = TRUE;

			PSTR cmd = GetCommandLineA(), sz, psz;

			if (sz = strstr(cmd, "https://"))
			{
				if (psz = strchr(sz += _countof("https://") - 1, '/'))
				{
					*psz++ = 0, host = sz, path = psz;
					if (PWSTR pwz = wcsstr(GetCommandLineW(), L"https://"))
					{
						if (PWSTR wz = wcschr(pwz += _countof("https://") - 1, '/'))
						{
							*wz = 0, pszTargetName = pwz;
						}
					}
				}
			}
			else if (sz = strstr(cmd, "http://"))
			{
				if (psz = strchr(sz += _countof("http://") - 1, '/'))
				{
					*psz++ = 0, host = sz, path = psz, bSSl = FALSE;
				}
			}

			if (bSSl)
			{
				if (SharedCred* pCred = new SharedCred)
				{
					if (0 <= pCred->Acquire(SECPKG_CRED_OUTBOUND, 0, 
						SCH_CRED_MANUAL_CRED_VALIDATION|SCH_CRED_NO_DEFAULT_CREDS, 
						SP_PROT_TLS1_1PLUS_CLIENT))
					{
						if (CSite* p = new CSite(pcs->hwnd, pCred, host, path, const_cast<PWSTR>(pszTargetName)))
						{
							if (!p->Create(0x8000))
							{
								p->DnsToIp(host);
							}
							p->Release();
						}
					}
					pCred->Release();
				}
			}
			else
			{
				if (CSite* p = new CSite(pcs->hwnd, 0, host, path, 0))
				{
					if (!p->Create(0x8000))
					{
						p->DnsToIp(host);
					}
					p->Release();
				}
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

STDAPI DllInstall(BOOL bInstall, PWSTR pszCmdLine)
{
	if (bInstall)
	{
		WCHAR appname[MAX_PATH];
		if (SearchPathW(0, L"notepad.exe", 0, _countof(appname), appname, 0))
		{
			STARTUPINFO si = { sizeof(si) };
			PROCESS_INFORMATION pi;
			if (CreateProcessW(appname, pszCmdLine, 0, 0, 0, 0, 0, 0, &si, &pi))
			{
				CloseHandle(pi.hThread);
				if (WaitForInputIdle(pi.hProcess, INFINITE) == WAIT_OBJECT_0)
				{
					EnumThreadWindows(pi.dwThreadId, EnumThreadWndProc, pi.dwThreadId);
				}
				CloseHandle(pi.hProcess);
			}
		}
	}

	return S_OK;
}

STDAPI DllRegisterServer()
{
	return DllInstall(TRUE, 0);
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