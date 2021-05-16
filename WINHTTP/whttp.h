#pragma once

#include "..\inc\asmfunc.h"

void ReferenceDll()ASM_FUNCTION;
void DereferenceDll()ASM_FUNCTION;

ULONG WINAPI GetWinHttpErrorString(DWORD Err, PWSTR& MsgBuffer);

VOID WINAPI LogWinHttpError(__in PCSTR prefix, __in DWORD Err, __in PCSTR Str);

class __declspec(novtable) InternetHandle
{
	HINTERNET _hInternet;
	LONG _dwRef;

protected:

	virtual ~InternetHandle()
	{
		if (_hInternet)
		{
			WinHttpCloseHandle(_hInternet);
		}
	}

	InternetHandle()
	{
		_dwRef = 1;
		_hInternet = 0;
	}

public:

	void AddRef()
	{
		InterlockedIncrementNoFence(&_dwRef);
	}

	void Release()
	{
		if (!InterlockedDecrement(&_dwRef)) delete this;
	}

	HINTERNET get_handle() { return _hInternet; }

	void set_handle(HINTERNET hInternet) 
	{ 
		if (hInternet = InterlockedExchangePointer(&_hInternet, hInternet))
		{
			WinHttpCloseHandle(hInternet);
		}
	}

	BOOL SetOption(_In_ DWORD dwOption, _In_ PVOID lpBuffer, _In_ DWORD dwBufferLength)
	{
		return WinHttpSetOption(_hInternet, dwOption, lpBuffer, dwBufferLength);
	}

	BOOL GetOption(_In_ DWORD dwOption, _Out_ PVOID lpBuffer, _Inout_ PDWORD lpdwBufferLength)
	{
		return WinHttpQueryOption(_hInternet, dwOption, lpBuffer, lpdwBufferLength);
	}
};

class CHttpConnection : public InternetHandle
{
	HINTERNET _hSession;

	virtual ~CHttpConnection()
	{
		if (_hSession) WinHttpCloseHandle(_hSession);
	}
public:

	ULONG Open(PCWSTR pszAgentW = 0);

	ULONG Connect(PCWSTR pswzServerName, INTERNET_PORT nServerPort);

	CHttpConnection() : _hSession(0)
	{
	}

	BOOL SetOption(_In_ DWORD dwOption, _In_ PVOID lpBuffer, _In_ DWORD dwBufferLength)
	{
		return WinHttpSetOption(_hSession, dwOption, lpBuffer, dwBufferLength);
	}

	BOOL GetOption(_In_ DWORD dwOption, _Out_ PVOID lpBuffer, _Inout_ PDWORD lpdwBufferLength)
	{
		return WinHttpQueryOption(_hSession, dwOption, lpBuffer, lpdwBufferLength);
	}
};

class __declspec(novtable) CWinhttpEndpoint : public InternetHandle
{
	CHttpConnection* _pTarget;
protected:
	PBYTE _lpBuffer = 0;
	ULONG _cbData = 0;
	ULONG _cbBuffer = 0;
private:

	static void WINAPI _StatusCallback(
		__in  HINTERNET hRequest,
		__in  DWORD_PTR dwContext,
		__in  DWORD dwInternetStatus,
		__in  LPVOID lpvStatusInformation,
		__in  DWORD dwStatusInformationLength
		)ASM_FUNCTION;

	BOOL StatusCallback(
		__in  HINTERNET hRequest,
		__in  DWORD dwInternetStatus,
		__in  LPVOID lpvStatusInformation,
		__in  DWORD dwStatusInformationLength
		);

	void Query(HINTERNET hRequest);

protected:

	virtual ~CWinhttpEndpoint()
	{
		if (_lpBuffer)
		{
			delete [] _lpBuffer;
		}
		_pTarget->Release();
		DereferenceDll();
	}

	CWinhttpEndpoint(CHttpConnection* pTarget) : _pTarget(pTarget)
	{
		ReferenceDll();
		pTarget->AddRef();
	}

	void Close()
	{
		set_handle(0);
	}

	virtual BOOL OnConnect(HINTERNET hRequest) = 0;
	virtual void OnRead(PVOID Buffer, ULONG dwNumberOfBytesRead) = 0;
	virtual void OnDisconnect() = 0;

	virtual void LogHttpError(DWORD_PTR dwResult, ULONG dwError );

	void OnHttpError(DWORD_PTR dwResult, ULONG dwError );

	virtual void OnSecureFailure(ULONG flags/* WINHTTP_CALLBACK_STATUS_FLAG_ */);

public:
	virtual ULONG InitReadBuffer(ULONG cbBuffer);

	HINTERNET OpenRequest( 
		__in DWORD dwFlags,
		__in LPCWSTR pwszVerb,
		__in LPCWSTR pwszObjectName,
		__in LPCWSTR pwszVersion = NULL,
		__in LPCWSTR pwszReferrer = WINHTTP_NO_REFERER,
		__in LPCWSTR * ppwszAcceptTypes = WINHTTP_DEFAULT_ACCEPT_TYPES
		);

	void SendRequest(
		__in      HINTERNET hRequest,
		__in_opt  LPVOID lpOptional = 0,
		__in      DWORD dwOptionalLength = 0,
		__in      DWORD dwTotalLength = 0,
		__in_opt  LPCWSTR pwszHeaders = WINHTTP_NO_ADDITIONAL_HEADERS,
		__in      DWORD dwHeadersLength = 0
		);
};
