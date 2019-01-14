#include "stdafx.h"

_NT_BEGIN
#include "dochost.h"

#pragma warning(disable : 4100)

GUID IID_NULL;

HRESULT STDMETHODCALLTYPE CDocHost::EvaluateNewWindow( 
	/* [string][in] */ __RPC__in_string LPCWSTR pszUrl,
	/* [string][in] */ __RPC__in_string LPCWSTR pszName,
	/* [string][in] */ __RPC__in_string LPCWSTR pszUrlContext,
	/* [string][in] */ __RPC__in_string LPCWSTR pszFeatures,
	/* [in] */ BOOL fReplace,
	/* [in] */ DWORD dwFlags,
	/* [in] */ DWORD dwUserActionTime)
{
	return S_FALSE;
}

HRESULT STDMETHODCALLTYPE CDocHost::QueryStatus( 
	/* [unique][in] */ __RPC__in_opt const GUID *pguidCmdGroup,
	/* [in] */ ULONG cCmds,
	/* [out][in][size_is] */ __RPC__inout_ecount_full(cCmds) OLECMD prgCmds[  ],
	/* [unique][out][in] */ __RPC__inout_opt OLECMDTEXT *pCmdText)
{
	if (!pguidCmdGroup)
	{
		while(cCmds--)
		{
			switch (prgCmds->cmdID)
			{
			case OLECMDID_SETPROGRESSTEXT:
				prgCmds->cmdf = OLECMDF_ENABLED|OLECMDF_SUPPORTED;             
				break;
			}
			prgCmds++;
		}
	}
	return S_OK;
}

#define DOCHOST_DOCCANNAVIGATE	0
#define DOCHOST_URL	11
#define SHDVID_SETURL 67
#define SHDVID_SETWINDOW 63
#define SHDVID_LOADDONE 140
#define SHDVID_PARSEDONE 103

struct __declspec(uuid("DE4BA900-59CA-11CF-9592-444553540000")) CGID_MSHTML;
struct __declspec(uuid("000214d4-0000-0000-c000-000000000046")) CGID_DocHostCmdPriv;
struct __declspec(uuid("f38bc242-b950-11d1-8918-00c04fc2c836")) CGID_DocHostCommandHandler;
struct __declspec(uuid("30d02401-6a81-11d0-8274-00c04fd5ae38")) CGID_SearchBand;

HRESULT STDMETHODCALLTYPE CDocHost::Exec( 
									   /* [unique][in] */ __RPC__in_opt const GUID *pguidCmdGroup,
									   /* [in] */ DWORD nCmdID,
									   /* [in] */ DWORD nCmdexecopt,
									   /* [unique][in] */ __RPC__in_opt VARIANT *pvaIn,
									   /* [unique][out][in] */ __RPC__inout_opt VARIANT *pvaOut)
{
	return S_OK;
}

_NT_END
