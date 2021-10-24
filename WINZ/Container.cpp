#include "StdAfx.h"

_NT_BEGIN

#include "Container.h"

#pragma warning(disable : 4100)


struct __declspec(uuid("305104AB-98B5-11CF-BB82-00AA00BDCE0B")) IHTMLPrivateWindow3_7;
struct __declspec(uuid("305104AD-98B5-11CF-BB82-00AA00BDCE0B")) IHTMLPrivateWindow3 : public IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE NavigateEx (IUri *, IUri *, BSTR, BSTR, IBindCtx *, ULONG, ULONG, IDispatch *);
	virtual HRESULT STDMETHODCALLTYPE GetInnerWindowUnknown (IUnknown **);
	virtual HRESULT STDMETHODCALLTYPE OpenEx (IUri *, BSTR, BSTR, BSTR, SHORT, IDispatch *, IHTMLWindow2 **);
	virtual HRESULT STDMETHODCALLTYPE NavigateEx2 (PCWSTR url, PCWSTR, PCWSTR, PCWSTR referer, PCWSTR, PCWSTR, PCWSTR, VARIANT, ULONGLONG, ULONG, ULONG, ULONG, ULONG, ULONG, GUID);
};

extern volatile const UCHAR guz;

HRESULT NavigateEx(IHTMLDocument2* pDoc, PCWSTR url, PCWSTR referer)
{
	HRESULT hr;
	if (!referer)
	{
		hr = E_OUTOFMEMORY;

		if (BSTR bstr = SysAllocString(url))
		{
			hr = pDoc->put_URL(bstr);
			SysFreeString(bstr);
		}
		return hr;
	}

	IHTMLWindow2* pWindow;

	if (!(hr = pDoc->get_parentWindow(&pWindow)))
	{
		IHTMLPrivateWindow3* ppW3;
		if (!(hr = pWindow->QueryInterface(IID_PPV(ppW3))) || 
			!(hr = pWindow->QueryInterface(__uuidof(IHTMLPrivateWindow3_7), (void**)&ppW3)))
		{
			VARIANT u = {};
			GUID guid = {};
			alloca(guz);

			hr = ppW3->NavigateEx2(url, url, 0, referer, 0, 0, 0, u, 0, 0, 0, 0, 0, 0, guid);

			ppW3->Release();
		}
		pWindow->Release();
	}

	return hr;
}

//////////////////////////////////////////////////////////////////////////
// ZContainer

ZContainer::ZContainer()
{
	m_pTI = 0;
	m_pActiveObject = 0;
	m_pControl = 0;
	m_hwndCtrl = 0;
}

ZContainer::~ZContainer()
{
	if (m_pControl)
	{
		m_pControl->Release();
	}

	if (m_pActiveObject)
	{
		m_pActiveObject->Release();
	}

	if (m_pTI)
	{
		m_pTI->Release();
	}
}

//////////////////////////////////////////////////////////////////////////
// IUnknownDbgPrint("QI(%s)\n", #itf);


#define ELIF__(itf, This) else if (riid == __uuidof(itf)) { *ppv = static_cast<itf*>(This); }
#define ELIF_(itf, itf_) ELIF__(itf, static_cast<itf_*>(this))
#define ELIF(itf) ELIF__(itf, this)

HRESULT STDMETHODCALLTYPE ZContainer::QueryInterface(REFIID riid, __RPC__deref_out void **ppv)
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

DWORD STDMETHODCALLTYPE ZContainer::AddRef()
{
	return ZObject::AddRef();
}

DWORD STDMETHODCALLTYPE ZContainer::Release()
{
	return ZObject::Release();
}

//////////////////////////////////////////////////////////////////////////
// IServiceProvider

HRESULT STDMETHODCALLTYPE ZContainer::QueryService(REFGUID rguid, REFIID riid, void** ppvObj) 
{
	return rguid == riid ? QueryInterface(riid, ppvObj) : E_NOINTERFACE;
}

//////////////////////////////////////////////////////////////////////////
// IDispatch

HRESULT STDMETHODCALLTYPE ZContainer::GetTypeInfoCount( /* [out] */ __RPC__out UINT *pctinfo )
{
	*pctinfo = GetTypeInfoInstance() != nullptr;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE ZContainer::GetTypeInfo( /* [in] */ UINT iTInfo, /* [in] */ LCID /*lcid*/, /* [out] */ __RPC__deref_out_opt ITypeInfo **ppTInfo )
{
	*ppTInfo = 0;

	if (iTInfo) return DISP_E_BADINDEX;

	if (HRESULT hr = CreateTypeInfo())
	{
		return hr;
	}

	m_pTI->AddRef();
	*ppTInfo = m_pTI;

	return S_OK;
}

HRESULT STDMETHODCALLTYPE ZContainer::GetIDsOfNames( /* [in] */ __RPC__in REFIID /*riid*/, /* [size_is][in] */ __RPC__in_ecount_full(cNames) LPOLESTR *rgszNames, /* [range][in] */ __RPC__in_range(0,16384) UINT cNames, /* [in] */ LCID /*lcid*/, /* [size_is][out] */ __RPC__out_ecount_full(cNames) DISPID *rgDispId )
{
	if (HRESULT hr = CreateTypeInfo())
	{
		return hr;
	}

	return m_pTI->GetIDsOfNames(rgszNames, cNames, rgDispId);
}

HRESULT STDMETHODCALLTYPE ZContainer::Invoke( /* [in] */ DISPID dispIdMember, /* [in] */ REFIID /*riid*/, /* [in] */ LCID /*lcid*/, /* [in] */ WORD wFlags, /* [out][in] */ DISPPARAMS *pDispParams, /* [out] */ VARIANT *pVarResult, /* [out] */ EXCEPINFO *pExcepInfo, /* [out] */ UINT *puArgErr )
{
	if (HRESULT hr = CreateTypeInfo())
	{
		return hr;
	}

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

	return m_pTI->Invoke(GetTypeInfoInstance(), dispIdMember, wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);
}

//////////////////////////////////////////////////////////////////////////
//

STDMETHODIMP ZContainer::SaveObject(void)
{ 
	return E_NOTIMPL; 
}

STDMETHODIMP ZContainer::GetMoniker(DWORD dwAssign, DWORD dwWhichMoniker, IMoniker **ppmk)
{ 
	*ppmk = 0;
	return E_NOTIMPL; 
}

STDMETHODIMP ZContainer::GetContainer(IOleContainer **ppContainer)
{ 
	*ppContainer = 0;
	return E_NOTIMPL; 
}

STDMETHODIMP ZContainer::ShowObject(void)
{ 
	return S_OK; 
}

STDMETHODIMP ZContainer::OnShowWindow(BOOL fShow)
{ 
	return S_OK; 
}

STDMETHODIMP ZContainer::RequestNewObjectLayout()
{
	return E_NOTIMPL;
}

STDMETHODIMP ZContainer::ShowContextMenu(DWORD dwID, POINT * ppt, IUnknown * pcmdtReserved,
								  IDispatch * pdispReserved)
{	
	//DbgPrint("IDocHostUIHandler::ShowContextMenu()\n");
	return S_OK;
}

STDMETHODIMP ZContainer::GetHostInfo(DOCHOSTUIINFO *pInfo)
{
	pInfo->dwFlags = DOCHOSTUIFLAG_DISABLE_HELP_MENU | 
		DOCHOSTUIFLAG_THEME|
		DOCHOSTUIFLAG_ENABLE_INPLACE_NAVIGATION;
	return S_OK;
}

STDMETHODIMP ZContainer::ShowUI(DWORD dwID, IOleInPlaceActiveObject * pActiveObject,
						 IOleCommandTarget * pCommandTarget,
						 IOleInPlaceFrame * pFrame,
						 IOleInPlaceUIWindow * pDoc)
{
	return S_OK;
}

STDMETHODIMP ZContainer::HideUI(void)
{
	return S_OK;
}

STDMETHODIMP ZContainer::UpdateUI(void)
{
	return S_OK;
}

STDMETHODIMP ZContainer::OnDocWindowActivate(BOOL fActivate)
{
	return S_OK;
}

STDMETHODIMP ZContainer::OnFrameWindowActivate(BOOL fActivate)
{
	return S_OK;
}

STDMETHODIMP ZContainer::ResizeBorder(LPCRECT prcBorder, IOleInPlaceUIWindow * pUIWindow, BOOL fRameWindow)
{
	return S_OK;
}

STDMETHODIMP ZContainer::TranslateAccelerator(LPMSG lpMsg, const GUID * pguidCmdGroup, DWORD nCmdID)
{
	return E_NOTIMPL;
}

STDMETHODIMP ZContainer::GetOptionKeyPath(LPOLESTR * pchKey, DWORD dw)
{	
	*pchKey = 0;
	return E_NOTIMPL;
}

STDMETHODIMP ZContainer::GetDropTarget(IDropTarget* pDropTarget, IDropTarget** ppDropTarget)
{
	return E_NOTIMPL;
}

STDMETHODIMP ZContainer::GetExternal(IDispatch** ppDispatch)
{	
	*ppDispatch = this;
	AddRef();
	return S_OK;
}

STDMETHODIMP ZContainer::TranslateUrl(DWORD dwTranslate, OLECHAR* pchURLIn, OLECHAR** ppchURLOut)
{
	*ppchURLOut = 0;
	return E_NOTIMPL;
}

STDMETHODIMP ZContainer::FilterDataObject(IDataObject* pDO, IDataObject** ppDORet)
{
	*ppDORet = 0;
	return E_NOTIMPL;
}

STDMETHODIMP ZContainer::CanInPlaceActivate()
{
	return S_OK;
}

STDMETHODIMP ZContainer::OnInPlaceActivate()
{		
	return S_OK;
}

STDMETHODIMP ZContainer::OnUIActivate()
{
	return S_OK;
}

STDMETHODIMP ZContainer::GetWindowContext(IOleInPlaceFrame **ppFrame, IOleInPlaceUIWindow **ppDoc,
								   LPRECT lprcPosRect, LPRECT lprcClipRect,
								   LPOLEINPLACEFRAMEINFO lpFrameInfo)
{
	HWND hwnd;
	GetWindow(&hwnd);
	GetClientRect(hwnd, lprcPosRect);
	GetClientRect(hwnd, lprcClipRect);

	*ppFrame = this, AddRef();
	*ppDoc = this, AddRef();
	if (lpFrameInfo && (lpFrameInfo->cb == sizeof OLEINPLACEFRAMEINFO)){
		lpFrameInfo->fMDIApp = FALSE;
		lpFrameInfo->hwndFrame = hwnd;
		lpFrameInfo->haccel = 0;
		lpFrameInfo->cAccelEntries = 0;
	}
	return S_OK;
}

STDMETHODIMP ZContainer::Scroll(SIZE scrollExtant)
{
	return S_OK;
}

STDMETHODIMP ZContainer::OnUIDeactivate(BOOL fUndoable)
{
	return S_OK;
}

STDMETHODIMP ZContainer::OnInPlaceDeactivate()
{
	return S_OK;
}

STDMETHODIMP ZContainer::DiscardUndoState()
{
	return S_OK;
}

STDMETHODIMP ZContainer::DeactivateAndUndo(void)
{
	return S_OK;
}

STDMETHODIMP ZContainer::OnPosRectChange(LPCRECT lprcPosRect)
{
	return S_OK;
}

STDMETHODIMP ZContainer::GetWindow(HWND* phwnd)
{
	*phwnd = getHWND();
	return S_OK;
}

STDMETHODIMP ZContainer::ContextSensitiveHelp(BOOL /*fEnterMode*/)
{
	return S_OK;
}

STDMETHODIMP ZContainer::GetBorder(LPRECT lprectBorder)
{
	return INPLACE_E_NOTOOLSPACE;
}

STDMETHODIMP ZContainer::RequestBorderSpace(LPCBORDERWIDTHS /*pborderwidths*/)
{
	return INPLACE_E_NOTOOLSPACE;
}

STDMETHODIMP ZContainer::SetBorderSpace(LPCBORDERWIDTHS pborderwidths)
{
	return S_OK;
}

STDMETHODIMP ZContainer::SetActiveObject(IOleInPlaceActiveObject* pActiveObject, LPCOLESTR pszObjName)
{
	m_hwndCtrl = 0;

	if (pActiveObject) 
	{
		pActiveObject->AddRef();
		pActiveObject->GetWindow(&m_hwndCtrl);
	}

	pActiveObject = (IOleInPlaceActiveObject*)InterlockedExchangePointer(
		(void**)&m_pActiveObject, (void*)pActiveObject);

	if (pActiveObject) pActiveObject->Release();

	return S_OK;
}

STDMETHODIMP ZContainer::InsertMenus(HMENU /*hmenuShared*/, LPOLEMENUGROUPWIDTHS /*lpMenuWidths*/)
{
	return S_OK;
}

STDMETHODIMP ZContainer::SetMenu(HMENU /*hmenuShared*/, HOLEMENU /*holemenu*/, HWND /*hwndActiveObject*/)
{
	return S_OK;
}

STDMETHODIMP ZContainer::RemoveMenus(HMENU /*hmenuShared*/)
{
	return S_OK;
}

STDMETHODIMP ZContainer::SetStatusText(LPCOLESTR pszStatusText)
{
	return S_OK;
}

STDMETHODIMP ZContainer::EnableModeless(BOOL )
{
	return S_OK;
}

STDMETHODIMP ZContainer::TranslateAccelerator(LPMSG /*lpMsg*/, WORD /*wID*/)
{
	return S_FALSE;
}

//////////////////////////////////////////////////////////////////////////
// INewWindowManager

HRESULT STDMETHODCALLTYPE ZContainer::EvaluateNewWindow( 
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

//////////////////////////////////////////////////////////////////////////
// IOleCommandTarget

HRESULT STDMETHODCALLTYPE ZContainer::QueryStatus( 
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

HRESULT STDMETHODCALLTYPE ZContainer::Exec( 
									/* [unique][in] */ __RPC__in_opt const GUID *pguidCmdGroup,
									/* [in] */ DWORD nCmdID,
									/* [in] */ DWORD nCmdexecopt,
									/* [unique][in] */ __RPC__in_opt VARIANT *pvaIn,
									/* [unique][out][in] */ __RPC__inout_opt VARIANT *pvaOut)
{
	return S_OK;
}

//////////////////////////////////////////////////////////////////////////
//

STDMETHODIMP ZContainer::SetSecuritySite(IInternetSecurityMgrSite *pSite)
{
	return E_NOTIMPL;
}

STDMETHODIMP ZContainer::GetSecuritySite(IInternetSecurityMgrSite **ppSite)
{
	*ppSite = 0;
	return E_NOTIMPL;
}

STDMETHODIMP ZContainer::MapUrlToZone(LPCWSTR pwszUrl, DWORD *pdwZone, DWORD dwFlags)
{
	*pdwZone = URLZONE_TRUSTED;
	return S_OK;
}

STDMETHODIMP ZContainer::GetSecurityId(LPCWSTR pwszUrl, BYTE *pbSecurityId, DWORD *pcbSecurityId, DWORD_PTR dwReserved)
{
	return INET_E_DEFAULT_ACTION;
}

STDMETHODIMP ZContainer::ProcessUrlAction( 
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

STDMETHODIMP ZContainer::QueryCustomPolicy( 
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

STDMETHODIMP ZContainer::SetZoneMapping( 
								 /* [in] */ DWORD dwZone,
								 /* [in] */ LPCWSTR lpszPattern,
								 /* [in] */ DWORD dwFlags)
{
	return S_OK;
}

STDMETHODIMP ZContainer::GetZoneMappings( DWORD dwZone, IEnumString **ppenumString, DWORD dwFlags)
{
	*ppenumString = 0;
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE ZContainer::OnSecurityProblem( DWORD dwProblem)
{
	return RPC_E_RETRY;
}

HRESULT STDMETHODCALLTYPE ZContainer::GetWindow( REFGUID /*rguidReason*/, HWND *phwnd)
{
	return GetWindow(phwnd);
}

HRESULT STDMETHODCALLTYPE ZContainer::ShowHTMLDialog( HWND hwndParent, IMoniker *pMk, VARIANT *pvarArgIn, WCHAR *pchOptions, VARIANT *pvarArgOut, IUnknown *punkHost )
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE ZContainer::ShowMessage( /* [in] */ HWND hwnd, /* [in] */ __in __nullterminated LPOLESTR lpstrText, /* [in] */ __in __nullterminated LPOLESTR lpstrCaption, /* [in] */ DWORD dwType, /* [in] */ __in __nullterminated LPOLESTR lpstrHelpFile, /* [in] */ DWORD dwHelpContext, /* [out] */ LRESULT *plResult )
{
	*plResult = MessageBox(hwnd, lpstrText, lpstrCaption, 0);
	return S_OK;
}

HRESULT STDMETHODCALLTYPE ZContainer::ShowHelp( /* [in] */ HWND hwnd, /* [in] */ __in __nullterminated LPOLESTR pszHelpFile, /* [in] */ UINT uCommand, /* [in] */ DWORD dwData, /* [in] */ POINT ptMouse, /* [out] */ IDispatch *pDispatchObjectHit )
{
	return S_OK;
}

//////////////////////////////////////////////////////////////////////////
//

BOOL ZContainer::OnControlActivate(IUnknown* /*pControl*/, PVOID /*lpCreateParams*/)
{
	return TRUE;
}

BOOL ZContainer::AttachControl(IUnknown* pControl, HWND hwnd, PVOID lpCreateParams)
{
	pControl->AddRef();

	if (_InterlockedCompareExchangePointer((void**)&m_pControl, (void*)pControl, 0))
	{
		pControl->Release();

		return FALSE;
	}

	BOOL fOk = FALSE;

	IOleObject* pOleObj;

	if (!pControl->QueryInterface(IID_PPV_ARGS(&pOleObj)))
	{
		if (!pOleObj->SetClientSite(this))
		{
			RECT rcClient = {};

			if (!pOleObj->DoVerb(OLEIVERB_SHOW, NULL, this, 0, hwnd, &rcClient))
			{
				fOk = OnControlActivate(pControl, lpCreateParams);
			}
		}

		pOleObj->Release();
	}

	return fOk;
}

void ZContainer::DettachControl(IUnknown* pControl, HWND hwnd)
{
	IOleObject* pOleObj;

	if (!pControl->QueryInterface(IID_PPV_ARGS(&pOleObj)))
	{
		RECT rcClient = {};

		pOleObj->DoVerb(OLEIVERB_HIDE , NULL, this, 0, hwnd, &rcClient);

		pOleObj->SetClientSite(0);

		pOleObj->Release();
	}

}

void ZContainer::DettachControl(HWND hwnd)
{
	if (IUnknown* pControl = (IUnknown*)_InterlockedExchangePointer((void**)&m_pControl, 0))
	{
		DettachControl(pControl, hwnd);
		pControl->Release();
	}
}

//////////////////////////////////////////////////////////////////////////
// TypeInfo

HRESULT ZContainer::CreateTypeInfo()
{
	if (!m_pTI)
	{
		ITypeInfo *pTInfo;

		if (HRESULT hr = LoadTypeInfo(&pTInfo))
		{
			return hr;
		}

		if (_InterlockedCompareExchangePointer((void**)&m_pTI, (void*)pTInfo, 0))
		{
			pTInfo->Release();
		}
	}

	return S_OK;
}

HRESULT ZContainer::LoadTypeInfo(ITypeInfo **ppTinfo)
{
	HRESULT hr = E_NOTIMPL;

	GUID guid;

	if (GetTypeInfoGuid(guid))
	{
		_LDR_DATA_TABLE_ENTRY* ldte;

		if (0 <= (hr = LdrFindEntryForAddress(&__ImageBase, &ldte)))
		{
			ITypeLib* pTL;
			if (S_OK == (hr = LoadTypeLibEx(ldte->FullDllName.Buffer, REGKIND_NONE, &pTL)))
			{
				hr = pTL->GetTypeInfoOfGuid(guid, ppTinfo);
				pTL->Release();
			}
		}
	}

	return hr;
}

BOOL ZContainer::GetTypeInfoGuid(GUID& /*guid*/)
{
	return FALSE;
}

PVOID ZContainer::GetTypeInfoInstance()
{
	return nullptr;
}

BOOL ZContainer::PreTranslateMessage(PMSG lpMsg)
{
	if (!m_pActiveObject || lpMsg->message != WM_KEYDOWN)
	{
		return FALSE;
	}

	HWND hwnd = lpMsg->hwnd, hWnd = getHWND();
	
	while (hwnd)
	{
		if (hwnd == hWnd)
		{
			return S_OK == m_pActiveObject->TranslateAccelerator(lpMsg);
		}
		hwnd = GetParent(hwnd);
	}

	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
//

void ZContainer::OnSizeChanged(PRECT prc )
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

LRESULT ZContainer::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_DESTROY:
		ZTranslateMsg::Remove();
		DettachControl(hwnd);
		break;

	case WM_CREATE:
		{
			BOOL fOk = FALSE;
			IUnknown* pControl;
			if (CreateControl(&pControl))
			{
				fOk = AttachControl(pControl, hwnd, ((LPCREATESTRUCT)lParam)->lpCreateParams);
				pControl->Release();
			}

			if (!fOk)
			{
				return -1;
			}
			ZTranslateMsg::Insert();
		}
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
			OnSizeChanged(&rc);
			break;
		}
		break;
	}

	return ZWnd::WindowProc(hwnd, uMsg, wParam, lParam);
}

//////////////////////////////////////////////////////////////////////////
// ZHtmlContainer

BOOL ZHtmlContainer::CreateControl(IUnknown** ppControl)
{
	return S_OK == CoCreateInstance(__uuidof(HTMLDocument), 0, CLSCTX_INPROC_SERVER, __uuidof(IHTMLDocument2), (void**)ppControl);
}

BOOL ZHtmlContainer::OnControlActivate(IUnknown* pControl, PVOID lpCreateParams)
{
	return !NavigateEx((IHTMLDocument2*)pControl, ((URL_REF*)lpCreateParams)->url, ((URL_REF*)lpCreateParams)->referer);
}

HRESULT STDMETHODCALLTYPE ZHtmlContainer::EvaluateNewWindow( 
	/* [string][in] */ __RPC__in_string LPCWSTR pszUrl,
	/* [string][in] */ __RPC__in_string LPCWSTR /*pszName*/,
	/* [string][in] */ __RPC__in_string LPCWSTR pszUrlContext,
	/* [string][in] */ __RPC__in_string LPCWSTR /*pszFeatures*/,
	/* [in] */ BOOL /*fReplace*/,
	/* [in] */ DWORD /*dwFlags*/,
	/* [in] */ DWORD /*dwUserActionTime*/)
{
	NavigateEx((IHTMLDocument2*)getControl(), pszUrl, pszUrlContext); 
	return S_FALSE;
}

HRESULT ZHtmlContainer::Navigate(PCWSTR url, PCWSTR referer)
{
	return NavigateEx((IHTMLDocument2*)getControl(), url, referer); 
}

PCWSTR ZHtmlContainer::GetUserAgent()
{
	return nullptr;
}

HRESULT STDMETHODCALLTYPE ZHtmlContainer::Invoke( /* [in] */ DISPID dispIdMember, /* [in] */ REFIID riid, /* [in] */ LCID lcid, /* [in] */ WORD wFlags, /* [out][in] */ DISPPARAMS *pDispParams, /* [out] */ VARIANT *pVarResult, /* [out] */ EXCEPINFO *pExcepInfo, /* [out] */ UINT *puArgErr )
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

	return ZContainer::Invoke(dispIdMember, riid, lcid, wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);
}

_NT_END