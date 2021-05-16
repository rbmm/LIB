#include "stdafx.h"

_NT_BEGIN
//#define _PRINT_CPP_NAMES_
#define DbgPrint /##/

#include "whttp.h"

ULONG WINAPI GetWinHttpErrorString(ULONG Err, PWSTR& MsgBuffer)
{
	NTSTATUS status = RtlGetLastNtStatus();

	if (RtlNtStatusToDosError(status) != Err)
	{
		status = STATUS_UNSUCCESSFUL;

		if (Err != (ULONG)STATUS_UNSUCCESSFUL)
		{
			ULONG r;

			if (r = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
				FORMAT_MESSAGE_FROM_HMODULE, GetModuleHandle(L"winhttp"), Err,
				MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				(PWSTR)&MsgBuffer, 0, NULL))
			{
				return r;
			}

			if (r = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
				FORMAT_MESSAGE_FROM_SYSTEM, 0, Err,
				MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				(PWSTR)&MsgBuffer, 0, NULL))
			{
				return r;
			}
		}
	}

	return FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_HMODULE, GetModuleHandle(L"ntdll"), 
		(ULONG)status,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(PWSTR)&MsgBuffer, 0, NULL);
}

VOID WINAPI LogWinHttpError(__in PCSTR prefix, __in ULONG Err, __in PCSTR Str)
{
	if (!Err)
	{
		return;
	}
	PWSTR MsgBuffer;

	if (GetWinHttpErrorString(Err, MsgBuffer))
	{
		DbgPrint("%s%s: %u %S\n", prefix, Str, Err, MsgBuffer);
		LocalFree(MsgBuffer);
	}
	else
	{
		DbgPrint("%sError %u while formatting message for %d in %s\n", prefix, GetLastError(), Err, Str);
	}

	return;
}

ULONG CHttpConnection::Open(PCWSTR pszAgentW/* = 0*/)
{
	if (HINTERNET hSession = WinHttpOpen(pszAgentW,  
		WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
		WINHTTP_NO_PROXY_NAME, 
		WINHTTP_NO_PROXY_BYPASS, WINHTTP_FLAG_ASYNC))
	{
		_hSession = hSession;

		return NOERROR;
	}

	return GetLastError();
}

ULONG CHttpConnection::Connect(PCWSTR pswzServerName, INTERNET_PORT nServerPort)
{
	if (HINTERNET hConnect = WinHttpConnect(_hSession, pswzServerName, nServerPort, 0))
	{
		set_handle(hConnect);
		return NOERROR;
	}

	return GetLastError();
}

HINTERNET CWinhttpEndpoint::OpenRequest( 
					  IN ULONG dwFlags,
					  IN LPCWSTR pwszVerb,
					  IN LPCWSTR pwszObjectName,
					  IN LPCWSTR pwszVersion,
					  IN LPCWSTR pwszReferrer,
					  IN LPCWSTR * ppwszAcceptTypes
					  )
{
	if (HINTERNET hRequest = WinHttpOpenRequest(_pTarget->get_handle(), pwszVerb, 
		pwszObjectName, pwszVersion, pwszReferrer, ppwszAcceptTypes, dwFlags))
	{
		PVOID Context = this;

		if (WinHttpSetOption(hRequest, WINHTTP_OPTION_CONTEXT_VALUE, &Context, sizeof(Context)))
		{
			AddRef();
			set_handle(hRequest);

			if (WINHTTP_INVALID_STATUS_CALLBACK != WinHttpSetStatusCallback(hRequest, _StatusCallback,
				WINHTTP_CALLBACK_FLAG_ALL_NOTIFICATIONS, NULL ))
			{
				return hRequest;
			}

			set_handle(0);
			Release();
		}

		WinHttpCloseHandle(hRequest);
	}

	return 0;
}

void CWinhttpEndpoint::SendRequest(
				 __in      HINTERNET hRequest,
				 __in_opt  LPVOID lpOptional,
				 __in      ULONG dwOptionalLength,
				 __in      ULONG dwTotalLength,
				 __in_opt  LPCWSTR pwszHeaders,
				 __in      ULONG dwHeadersLength
				 )
{
	if (!WinHttpSendRequest(hRequest, pwszHeaders, dwHeadersLength, lpOptional, dwOptionalLength, dwTotalLength, 0))
	{
		OnHttpError(API_SEND_REQUEST, GetLastError());
	}
}

void CWinhttpEndpoint::Query(HINTERNET hRequest)
{
	DbgPrint("[%x]%s<%p>\n", GetCurrentThreadId(), __FUNCTION__, this);

	if (_cbBuffer == _cbData)
	{
		OnHttpError(API_READ_DATA, ERROR_INSUFFICIENT_BUFFER);
	}
	else if (ULONG dwError = BOOL_TO_ERROR(WinHttpQueryDataAvailable(hRequest, 0)))
	{
		OnHttpError(API_QUERY_DATA_AVAILABLE, dwError);
	}
}

void CWinhttpEndpoint::LogHttpError(DWORD_PTR dwResult, ULONG dwError )
{
	PCSTR msg = "REQUEST_COMPLETE";

	switch (dwResult)
	{
	case API_QUERY_DATA_AVAILABLE: msg = "DATA_AVAILABLE";
		break;
	case API_READ_DATA: msg = "READ_DATA";
		break;
	case API_SEND_REQUEST: msg = "SEND_REQUEST";
		break;
	case API_WRITE_DATA: msg = "WRITE_DATA";
		break;
	case API_RECEIVE_RESPONSE: msg = "RECEIVE_RESPONSE";
		break;
	}

	LogWinHttpError("", dwError, msg);
}

void CWinhttpEndpoint::OnHttpError(DWORD_PTR dwResult, ULONG dwError )
{
	LogHttpError(dwResult, dwError);
	Close();
}

void CWinhttpEndpoint::OnSecureFailure( ULONG /*flags*/ /* WINHTTP_CALLBACK_STATUS_FLAG_ */)
{
	DbgPrint("OnSecureFailure[%x]\n", flags);
	//Close();
}

BOOL CWinhttpEndpoint::StatusCallback(
									 __in  HINTERNET hRequest,
									 __in  ULONG dwInternetStatus,
									 __in  LPVOID lpvStatusInformation,
									 __in  ULONG dwStatusInformationLength
									 )
{
	CPP_FUNCTION;

	ULONG dwError;

	//DbgPrint("%x>CB(%x, %p, %x)\n", GetCurrentThreadId(), dwInternetStatus, lpvStatusInformation, dwStatusInformationLength);

	switch (dwInternetStatus)
	{
	case WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING:
		Release();
		return TRUE;

	case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE:
		if (dwError = BOOL_TO_ERROR(WinHttpReceiveResponse(hRequest, NULL)))
		{
			OnHttpError(API_RECEIVE_RESPONSE, dwError);
		}
		break;

	case WINHTTP_CALLBACK_STATUS_SECURE_FAILURE:
		OnSecureFailure(*(ULONG*)lpvStatusInformation);
		break;

	case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR:
		OnHttpError(reinterpret_cast<WINHTTP_ASYNC_RESULT*>(lpvStatusInformation)->dwResult,
			reinterpret_cast<WINHTTP_ASYNC_RESULT*>(lpvStatusInformation)->dwError);
		break;

	case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE:
		if (OnConnect(hRequest))
		{
			Query(hRequest);
		}
		else
		{
			Close();
		}
		break;

	case WINHTTP_CALLBACK_STATUS_READ_COMPLETE:
		dwStatusInformationLength 
			? (OnRead(_lpBuffer + _cbData, dwStatusInformationLength), Query(hRequest)) : (OnDisconnect(), Close());
		break;

	case WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE:
		dwStatusInformationLength = *(ULONG*)lpvStatusInformation;
		ULONG dwNumberOfBytesRead = _cbBuffer - _cbData;
		DbgPrint("DATA_AVAILABLE:%x - %x\n", dwStatusInformationLength, dwNumberOfBytesRead);

		if (!dwStatusInformationLength)
		{
			OnDisconnect(), Close();
			break;
		}
		if (dwError = BOOL_TO_ERROR(WinHttpReadData(hRequest, 
			_lpBuffer + _cbData, min(dwNumberOfBytesRead, dwStatusInformationLength), 0)))
		{
			OnHttpError(API_READ_DATA, dwError);
		}
		break;
	}

	return FALSE;
}

ULONG CWinhttpEndpoint::InitReadBuffer(ULONG cbBuffer)
{
	if (_lpBuffer = new UCHAR[cbBuffer])
	{
		_cbBuffer = cbBuffer;
		return NOERROR;
	}

	return GetLastError();
}

_NT_END