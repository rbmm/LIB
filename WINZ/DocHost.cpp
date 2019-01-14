#include "StdAfx.h"

_NT_BEGIN

#include "dochost.h"

CDocHost::~CDocHost()
{
	if (m_pActiveObject) m_pActiveObject->Release();
	if (m_pDoc) m_pDoc->Release();
}

CDocHost::CDocHost()
{
	m_hwnd = 0;
	m_pDoc = 0;
	m_pActiveObject = 0;
	m_dwRef = 1;
}

#define ELIF__(itf, This) else if (riid == __uuidof(itf)) { *ppv = static_cast<itf*>(This); }
#define ELIF_(itf, itf_) ELIF__(itf, static_cast<itf_*>(this))
#define ELIF(itf) ELIF__(itf, this)

HRESULT STDMETHODCALLTYPE CDocHost::QueryInterface(REFIID riid, __RPC__deref_out void **ppv)
{
	if (riid == __uuidof(IUnknown))
	{
		*ppv = static_cast<IUnknown*>(static_cast<IDispatch*>(this));
	} 
	ELIF(IDispatch)
		ELIF(IOleClientSite)
		ELIF(IOleInPlaceSite)
		ELIF(IServiceProvider)
		ELIF(IInternetSecurityManager)
		ELIF(IOleCommandTarget)
		ELIF(IOleInPlaceUIWindow)
		ELIF(IDocHostUIHandler)
		ELIF(INewWindowManager)
		ELIF(IHttpSecurity)
		ELIF(IDocHostShowUI)
		ELIF(IHostDialogHelper)
		ELIF(IOleInPlaceFrame)ELIF_(IOleWindow, IOleInPlaceFrame)
	else 
	{
		*ppv = 0;
		return E_NOINTERFACE;
	}
	AddRef();
	return S_OK;
}

DWORD STDMETHODCALLTYPE CDocHost::AddRef()
{
	return InterlockedIncrement(&m_dwRef);
}

DWORD STDMETHODCALLTYPE CDocHost::Release()
{
	DWORD dwRef = InterlockedDecrement(&m_dwRef);
	if (!dwRef)
	{
		delete this;
	}
	return dwRef;
}

HRESULT STDMETHODCALLTYPE CDocHost::QueryService(REFGUID rguid, REFIID riid, void** ppvObj) 
{
	return rguid == riid ? QueryInterface(riid, ppvObj) : E_NOINTERFACE;
}

BOOL CDocHost::CreateHost(HWND hwnd)
{
	if (!LoadTypeInfo())
	{
		return FALSE;
	}

	m_hwnd = hwnd;

	BOOL fOk = FALSE;

	if (!CoCreateInstance(__uuidof(HTMLDocument), 0, CLSCTX_INPROC_SERVER, IID_PPV(m_pDoc)))
	{
		IOleObject* pOleObj;

		if (!m_pDoc->QueryInterface(IID_PPV(pOleObj)))
		{
			if (!pOleObj->SetClientSite(this))
			{
				RECT rcClient={};

				if (fOk = !pOleObj->DoVerb(OLEIVERB_SHOW , NULL, this, 0, hwnd, &rcClient))
				{
					Insert();
				}
			}
			pOleObj->Release();
		}
	}

	return fOk;
}

void CDocHost::DestroyHost()
{
	if (IHTMLDocument2* pDoc = m_pDoc)
	{
		pDoc->AddRef();

		Remove();

		IOleObject* pOleObj;

		if (!pDoc->QueryInterface(IID_PPV(pOleObj)))
		{
			RECT rcClient={};

			pOleObj->DoVerb(OLEIVERB_HIDE , NULL, this, 0, m_hwnd, &rcClient);

			pOleObj->SetClientSite(0);

			pOleObj->Release();
		}

		pDoc->Release();
	}
	m_hwnd = 0;
}

void CDocHost::OnSizeChanged(PRECT prc )
{
	if (m_pActiveObject)
	{
		IOleInPlaceObject* p;
		if (!m_pActiveObject->QueryInterface(IID_PPV(p)))
		{
			p->SetObjectRects(prc, prc);
			p->Release();
		}
	}
}

HRESULT CDocHost::put_URL(LPWSTR url)
{
	return m_pDoc->put_URL(url);
}

#pragma warning(disable : 4100)

BOOL CDocHost::LoadTypeInfo()
{
	return TRUE;
}

ITypeInfo* CDocHost::GetTypeInfo(PVOID* /*ppvInstance*/)
{
	return 0;
}

PCWSTR CDocHost::GetUserAgent()
{
	return 0;
}

HRESULT STDMETHODCALLTYPE CDocHost::GetTypeInfoCount( /* [out] */ __RPC__out UINT *pctinfo )
{
	*pctinfo = GetTypeInfo() != 0;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CDocHost::GetTypeInfo( /* [in] */ UINT iTInfo, /* [in] */ LCID lcid, /* [out] */ __RPC__deref_out_opt ITypeInfo **ppTInfo )
{
	*ppTInfo = 0;

	if (iTInfo) return DISP_E_BADINDEX;

	if (ITypeInfo *pTInfo = GetTypeInfo())
	{
		pTInfo->AddRef();
		*ppTInfo = pTInfo;
	}

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CDocHost::GetIDsOfNames( /* [in] */ __RPC__in REFIID riid, /* [size_is][in] */ __RPC__in_ecount_full(cNames) LPOLESTR *rgszNames, /* [range][in] */ __RPC__in_range(0,16384) UINT cNames, /* [in] */ LCID lcid, /* [size_is][out] */ __RPC__out_ecount_full(cNames) DISPID *rgDispId )
{
	if (ITypeInfo *pTInfo = GetTypeInfo())
	{
		//DbgPrint("GetIDsOfNames(%S)\n", *rgszNames);
		return pTInfo->GetIDsOfNames(rgszNames, cNames, rgDispId);
	}

	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CDocHost::Invoke( /* [in] */ DISPID dispIdMember, /* [in] */ REFIID riid, /* [in] */ LCID lcid, /* [in] */ WORD wFlags, /* [out][in] */ DISPPARAMS *pDispParams, /* [out] */ VARIANT *pVarResult, /* [out] */ EXCEPINFO *pExcepInfo, /* [out] */ UINT *puArgErr )
{
	switch (dispIdMember)
	{
	case DISPID_AMBIENT_DLCONTROL:
		pVarResult->vt = VT_I4;
		pVarResult->lVal = DLCTL_DLIMAGES|DLCTL_VIDEOS|DLCTL_BGSOUNDS;
		return S_OK;
	case DISPID_AMBIENT_USERAGENT:
		if (PCWSTR ua = GetUserAgent())
		{
			pVarResult->vt = VT_BSTR;
			pVarResult->bstrVal = SysAllocString(ua);
			return S_OK;
		}
		return E_NOTIMPL;
	}

	PVOID pvInstance;

	if (ITypeInfo *pTInfo = GetTypeInfo(&pvInstance))
	{
		UINT cArgs = pDispParams->cArgs;
		VARIANTARG *rgvarg = pDispParams->rgvarg;
		while (cArgs--)
		{
			if (rgvarg->vt == VT_NULL)
			{
				rgvarg->vt = VT_BSTR;
				rgvarg->bstrVal = 0;
			}
			rgvarg++;
		}
		
		return pTInfo->Invoke(pvInstance, dispIdMember, wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);
	}

	return E_NOTIMPL;
}

BOOL CDocHost::PreTranslateMessage(PMSG lpMsg)
{
	return m_pActiveObject ? !m_pActiveObject->TranslateAccelerator(lpMsg) : FALSE;
}

CDocHost* ZWebFrame::createDocHost()
{
	return new CDocHost;
}

LRESULT ZWebFrame::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
	{
	case WM_NCCREATE:
		if (!(_pDoc = createDocHost())) return FALSE;
		break;

	case WM_NCDESTROY:
		if (_pDoc) _pDoc->Release();
		break;

	case WM_CREATE:

		if (!_pDoc->CreateHost(hwnd)) return -1;

		if (INIT_URL* p = (INIT_URL*)((LPCREATESTRUCT)lParam)->lpCreateParams)
		{
			_pDoc->Navigate(p->url, p->ref);
		}

		break;

	case WM_DESTROY:
		
		_pDoc->DestroyHost();
		break;

	case WM_ERASEBKGND:
		return TRUE;

	case WM_PAINT:
		EmptyPaint(hwnd);
		break;

	case WM_SIZE:
		switch (wParam)
		{
		case SIZE_MAXIMIZED:
		case SIZE_RESTORED:
			RECT rc = { 0, 0, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
			_pDoc->OnSizeChanged(&rc);
			break;
		}
		break;
	}
	
	return ZWnd::WindowProc(hwnd, uMsg, wParam, lParam);
}

void ZWebFrame::Navigate(PWSTR url, PWSTR ref)
{
	_pDoc->Navigate(url, ref);
}

_NT_END
