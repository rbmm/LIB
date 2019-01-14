#include "StdAfx.h"

_NT_BEGIN

#include "dochost.h"

#pragma warning(disable : 4100)

STDMETHODIMP CDocHost::SetSecuritySite( 
						   /* [unique][in] */ IInternetSecurityMgrSite *pSite)
{
	return E_NOTIMPL;
}

STDMETHODIMP CDocHost::GetSecuritySite( 
						   /* [out] */ IInternetSecurityMgrSite **ppSite)
{
	*ppSite = 0;
	return E_NOTIMPL;
}

STDMETHODIMP CDocHost::MapUrlToZone( 
						/* [in] */ LPCWSTR pwszUrl,
						/* [out] */ DWORD *pdwZone,
						/* [in] */ DWORD dwFlags)
{
	*pdwZone = URLZONE_TRUSTED;
	return S_OK;
}

STDMETHODIMP CDocHost::GetSecurityId( 
						 /* [in] */ LPCWSTR pwszUrl,
						 /* [size_is][out] */ BYTE *pbSecurityId,
						 /* [out][in] */ DWORD *pcbSecurityId,
						 /* [in] */ DWORD_PTR dwReserved)
{
	return INET_E_DEFAULT_ACTION;
}

STDMETHODIMP CDocHost::ProcessUrlAction( 
							/* [in] */ LPCWSTR pwszUrl,
							/* [in] */ DWORD dwAction,
							/* [size_is][out] */ BYTE *pPolicy,
							/* [in] */ DWORD cbPolicy,
							/* [in] */ BYTE *pContext,
							/* [in] */ DWORD cbContext,
							/* [in] */ DWORD dwFlags,
							/* [in] */ DWORD dwReserved)
{
	if (cbPolicy != sizeof(DWORD)) return INET_E_DEFAULT_ACTION;
	
	//DbgPrint("\r\n%p:ProcessUrlAction(%x, %S)", this, dwAction, pwszUrl);
	
	*(PDWORD)pPolicy = URLPOLICY_ALLOW;

	switch(dwAction)
	{
	case URLACTION_JAVA_PERMISSIONS:
		*(PDWORD)pPolicy = URLPOLICY_JAVA_LOW;
		break;
	}

	return S_OK;
}

STDMETHODIMP CDocHost::QueryCustomPolicy( 
							 /* [in] */ LPCWSTR pwszUrl,
							 /* [in] */ REFGUID guidKey,
							 /* [size_is][size_is][out] */ BYTE **ppPolicy,
							 /* [out] */ DWORD *pcbPolicy,
							 /* [in] */ BYTE *pContext,
							 /* [in] */ DWORD cbContext,
							 /* [in] */ DWORD dwReserved)
{
	*pcbPolicy = 0;
	return S_OK;
}

STDMETHODIMP CDocHost::SetZoneMapping( 
						  /* [in] */ DWORD dwZone,
						  /* [in] */ LPCWSTR lpszPattern,
						  /* [in] */ DWORD dwFlags)
{
	return S_OK;
}

STDMETHODIMP CDocHost::GetZoneMappings( 
						   /* [in] */ DWORD dwZone,
						   /* [out] */ IEnumString **ppenumString,
						   /* [in] */ DWORD dwFlags)
{
	*ppenumString = 0;
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CDocHost::OnSecurityProblem( 
	/* [in] */ DWORD dwProblem)
{
	return RPC_E_RETRY;
}

HRESULT STDMETHODCALLTYPE CDocHost::GetWindow( 
	/* [in] */ REFGUID rguidReason,
	/* [out] */ HWND *phwnd)
{
	*phwnd = m_hwnd;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CDocHost::ShowHTMLDialog( HWND hwndParent, IMoniker *pMk, VARIANT *pvarArgIn, WCHAR *pchOptions, VARIANT *pvarArgOut, IUnknown *punkHost )
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CDocHost::ShowMessage( /* [in] */ HWND hwnd, /* [in] */ __in __nullterminated LPOLESTR lpstrText, /* [in] */ __in __nullterminated LPOLESTR lpstrCaption, /* [in] */ DWORD dwType, /* [in] */ __in __nullterminated LPOLESTR lpstrHelpFile, /* [in] */ DWORD dwHelpContext, /* [out] */ LRESULT *plResult )
{
	*plResult = MessageBox(hwnd, lpstrText, lpstrCaption, 0);
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CDocHost::ShowHelp( /* [in] */ HWND hwnd, /* [in] */ __in __nullterminated LPOLESTR pszHelpFile, /* [in] */ UINT uCommand, /* [in] */ DWORD dwData, /* [in] */ POINT ptMouse, /* [out] */ IDispatch *pDispatchObjectHit )
{
	return S_OK;
}

_NT_END