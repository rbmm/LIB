#pragma once

#include "window.h"
#include "app.h"

HRESULT NavigateEx(IHTMLDocument2* pDoc, PCWSTR url, PCWSTR referer = 0);

class __declspec(novtable) ZContainer : public ZWnd,
	public IDispatch,
	IServiceProvider,
	IOleClientSite,
	IOleInPlaceSite, 
	IOleInPlaceFrame,
	IOleCommandTarget,
	IDocHostUIHandler,
	IInternetSecurityManager,
	INewWindowManager,
	IHttpSecurity,
	IDocHostShowUI,
	IHostDialogHelper,
	ZTranslateMsg
{
	HWND m_hwndCtrl;
	IOleInPlaceActiveObject* m_pActiveObject;
	IUnknown* m_pControl;
	ITypeInfo* m_pTI;

	HRESULT CreateTypeInfo();
	BOOL AttachControl(IUnknown* pControl, HWND hwnd, PVOID lpCreateParams);
	void DettachControl(HWND hwnd);

public:
	ZContainer();


protected:
	virtual ~ZContainer();

	virtual BOOL CreateControl(IUnknown** ppControl) = 0;
	virtual BOOL OnControlActivate(IUnknown* pControl, PVOID lpCreateParams);

	virtual HRESULT LoadTypeInfo(ITypeInfo **ppTinfo);
	virtual PVOID GetTypeInfoInstance();
	virtual BOOL GetTypeInfoGuid(GUID& guid);

	IUnknown* getControl() { return m_pControl; }

private:

	virtual LRESULT WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

public:
	////////////////////////////////////////////////////////////////////
	// IUnknown

	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, __RPC__deref_out void **ppvObject);

	virtual DWORD STDMETHODCALLTYPE AddRef();

	virtual DWORD STDMETHODCALLTYPE Release();
protected:
	//////////////////////////////////////////////////////////////////////
	// IDispatch

	virtual HRESULT STDMETHODCALLTYPE GetTypeInfoCount( 
		/* [out] */ __RPC__out UINT *pctinfo);

	virtual HRESULT STDMETHODCALLTYPE GetTypeInfo( 
		/* [in] */ UINT iTInfo,
		/* [in] */ LCID lcid,
		/* [out] */ __RPC__deref_out_opt ITypeInfo **ppTInfo);

	virtual HRESULT STDMETHODCALLTYPE GetIDsOfNames( 
		/* [in] */ __RPC__in REFIID riid,
		/* [size_is][in] */ __RPC__in_ecount_full(cNames) LPOLESTR *rgszNames,
		/* [range][in] */ __RPC__in_range(0,16384) UINT cNames,
		/* [in] */ LCID lcid,
		/* [size_is][out] */ __RPC__out_ecount_full(cNames) DISPID *rgDispId);

	virtual /* [local] */ HRESULT STDMETHODCALLTYPE Invoke( 
		/* [in] */ DISPID dispIdMember,
		/* [in] */ REFIID riid,
		/* [in] */ LCID lcid,
		/* [in] */ WORD wFlags,
		/* [out][in] */ DISPPARAMS *pDispParams,
		/* [out] */ VARIANT *pVarResult,
		/* [out] */ EXCEPINFO *pExcepInfo,
		/* [out] */ UINT *puArgErr);

	//////////////////////////////////////////////////////////////////////////
	// IServiceProvider

	STDMETHOD(QueryService)(REFGUID rguid, REFIID riid, void** ppvObj);

	//////////////////////////////////////////////////////////////////////////
	// INewWindowManager

	virtual HRESULT STDMETHODCALLTYPE EvaluateNewWindow( 
		/* [string][in] */ __RPC__in_string LPCWSTR pszUrl,
		/* [string][in] */ __RPC__in_string LPCWSTR pszName,
		/* [string][in] */ __RPC__in_string LPCWSTR pszUrlContext,
		/* [string][in] */ __RPC__in_string LPCWSTR pszFeatures,
		/* [in] */ BOOL fReplace,
		/* [in] */ DWORD dwFlags,
		/* [in] */ DWORD dwUserActionTime);

	//////////////////////////////////////////////////////////////////////////
	// IOleCommandTarget

	virtual /* [input_sync] */ HRESULT STDMETHODCALLTYPE QueryStatus( 
		/* [unique][in] */ __RPC__in_opt const GUID *pguidCmdGroup,
		/* [in] */ ULONG cCmds,
		/* [out][in][size_is] */ __RPC__inout_ecount_full(cCmds) OLECMD prgCmds[  ],
		/* [unique][out][in] */ __RPC__inout_opt OLECMDTEXT *pCmdText);

	virtual HRESULT STDMETHODCALLTYPE Exec( 
		/* [unique][in] */ __RPC__in_opt const GUID *pguidCmdGroup,
		/* [in] */ DWORD nCmdID,
		/* [in] */ DWORD nCmdexecopt,
		/* [unique][in] */ __RPC__in_opt VARIANT *pvaIn,
		/* [unique][out][in] */ __RPC__inout_opt VARIANT *pvaOut);

	//////////////////////////////////////////////////////////////////////////
	// HostUI

	STDMETHOD(GetWindow)(HWND* phwnd);
	STDMETHOD(SaveObject)(void);	
	STDMETHOD(GetMoniker)(DWORD dwAssign, DWORD dwWhichMoniker, IMoniker **ppmk);	
	STDMETHOD(GetContainer)(IOleContainer **ppContainer);	
	STDMETHOD(ShowObject)(void);	
	STDMETHOD(OnShowWindow)(BOOL fShow);
	STDMETHOD(RequestNewObjectLayout)(void);
	STDMETHOD(CanInPlaceActivate)(void);
	STDMETHOD(OnInPlaceActivate)(void);
	STDMETHOD(OnUIActivate)(void);
	STDMETHOD(GetWindowContext)(IOleInPlaceFrame **ppFrame, IOleInPlaceUIWindow **ppDoc, LPRECT lprcPosRect, LPRECT lprcClipRect, LPOLEINPLACEFRAMEINFO lpFrameInfo);
	STDMETHOD(Scroll)(SIZE scrollExtant);
	STDMETHOD(OnUIDeactivate)(BOOL fUndoable);
	STDMETHOD(OnInPlaceDeactivate)(void);
	STDMETHOD(DiscardUndoState)(void);
	STDMETHOD(DeactivateAndUndo)(void);
	STDMETHOD(OnPosRectChange)(LPCRECT lprcPosRect);
	STDMETHOD(ShowContextMenu)(DWORD dwID, POINT FAR* ppt, IUnknown FAR* pcmdtReserved, IDispatch FAR* pdispReserved);
	STDMETHOD(GetHostInfo)(DOCHOSTUIINFO FAR *pInfo);
	STDMETHOD(ShowUI)(DWORD dwID, IOleInPlaceActiveObject FAR* pActiveObject, IOleCommandTarget FAR* pCommandTarget, IOleInPlaceFrame  FAR* pFrame, IOleInPlaceUIWindow FAR* pDoc);
	STDMETHOD(HideUI)(void);
	STDMETHOD(UpdateUI)(void);
	STDMETHOD(OnDocWindowActivate)(BOOL fActivate);	
	STDMETHOD(OnFrameWindowActivate)(BOOL fActivate);	
	STDMETHOD(ResizeBorder)(LPCRECT prcBorder, IOleInPlaceUIWindow FAR* pUIWindow, BOOL fRameWindow);
	STDMETHOD(TranslateAccelerator)(LPMSG lpMsg, const GUID FAR* pguidCmdGroup, DWORD nCmdID);	
	STDMETHOD(GetOptionKeyPath)(LPOLESTR FAR* pchKey, DWORD dw);
	STDMETHOD(GetDropTarget)(IDropTarget* pDropTarget, IDropTarget** ppDropTarget);	
	STDMETHOD(GetExternal)(IDispatch** ppDispatch);
	STDMETHOD(TranslateUrl)(DWORD dwTranslate, OLECHAR* pchURLIn, OLECHAR** ppchURLOut);
	STDMETHOD(FilterDataObject)(IDataObject* pDO, IDataObject** ppDORet);	
	STDMETHOD(ContextSensitiveHelp)(BOOL /*fEnterMode*/);
	STDMETHOD(GetBorder)(LPRECT lprectBorder);
	STDMETHOD(RequestBorderSpace)(LPCBORDERWIDTHS /*pborderwidths*/);
	STDMETHOD(SetBorderSpace)(LPCBORDERWIDTHS pborderwidths);
	STDMETHOD(SetActiveObject)(IOleInPlaceActiveObject* pActiveObject, LPCOLESTR pszObjName);
	STDMETHOD(InsertMenus)(HMENU /*hmenuShared*/, LPOLEMENUGROUPWIDTHS /*lpMenuWidths*/);
	STDMETHOD(SetMenu)(HMENU /*hmenuShared*/, HOLEMENU /*holemenu*/, HWND /*hwndActiveObject*/);
	STDMETHOD(RemoveMenus)(HMENU /*hmenuShared*/);
	STDMETHOD(SetStatusText)(LPCOLESTR pszStatusText);
	STDMETHOD(EnableModeless)(BOOL /*fEnable*/);
	STDMETHOD(TranslateAccelerator)(LPMSG /*lpMsg*/, WORD /*wID*/);

	//////////////////////////////////////////////////////////////////////////
	// IInternetSecurityManager

	STDMETHOD(SetSecuritySite)( /* [unique][in] */ IInternetSecurityMgrSite *pSite);
	STDMETHOD(GetSecuritySite)( /* [out] */ IInternetSecurityMgrSite **ppSite);
	STDMETHOD(MapUrlToZone)(/* [in] */ LPCWSTR pwszUrl, /* [out] */ DWORD *pdwZone, /* [in] */ DWORD dwFlags);
	STDMETHOD(GetSecurityId)( /* [in] */ LPCWSTR pwszUrl,/* [size_is][out] */ BYTE *pbSecurityId,/* [out][in] */ DWORD *pcbSecurityId,/* [in] */ DWORD_PTR dwReserved);
	STDMETHOD(ProcessUrlAction)( /* [in] */ LPCWSTR pwszUrl,/* [in] */ DWORD dwAction,/* [size_is][out] */ BYTE *pPolicy,/* [in] */ DWORD cbPolicy,/* [in] */ BYTE *pContext,/* [in] */ DWORD cbContext,/* [in] */ DWORD dwFlags,/* [in] */ DWORD dwReserved);
	STDMETHOD(QueryCustomPolicy)(/* [in] */ LPCWSTR pwszUrl,/* [in] */ REFGUID guidKey,/* [size_is][size_is][out] */ BYTE **ppPolicy,/* [out] */ DWORD *pcbPolicy,/* [in] */ BYTE *pContext,/* [in] */ DWORD cbContext,/* [in] */ DWORD dwReserved);
	STDMETHOD(SetZoneMapping)(/* [in] */ DWORD dwZone,/* [in] */ LPCWSTR lpszPattern,/* [in] */ DWORD dwFlags);
	STDMETHOD(GetZoneMappings)(/* [in] */ DWORD dwZone,/* [out] */ IEnumString **ppenumString,/* [in] */ DWORD dwFlags);

	//////////////////////////////////////////////////////////////////////////
	// IHttpSecurity

	virtual HRESULT STDMETHODCALLTYPE GetWindow( 
		/* [in] */ REFGUID rguidReason,
		/* [out] */ HWND *phwnd);

	virtual HRESULT STDMETHODCALLTYPE OnSecurityProblem( 
		/* [in] */ DWORD dwProblem);

	///////////////////////////////////////////////////////////////////////////
	// IDocHostShowUI

	virtual HRESULT STDMETHODCALLTYPE ShowMessage( 
		/* [in] */ HWND hwnd,
		/* [in] */ 
		__in __nullterminated  LPOLESTR lpstrText,
		/* [in] */ 
		__in __nullterminated  LPOLESTR lpstrCaption,
		/* [in] */ DWORD dwType,
		/* [in] */ 
		__in __nullterminated  LPOLESTR lpstrHelpFile,
		/* [in] */ DWORD dwHelpContext,
		/* [out] */ LRESULT *plResult);

	virtual HRESULT STDMETHODCALLTYPE ShowHelp( 
		/* [in] */ HWND hwnd,
		/* [in] */ LPOLESTR pszHelpFile,
		/* [in] */ UINT uCommand,
		/* [in] */ DWORD dwData,
		/* [in] */ POINT ptMouse,
		/* [out] */ IDispatch *pDispatchObjectHit);

	///////////////////////////////////////////////////////////////////////////
	// IHostDialogHelper

	virtual HRESULT STDMETHODCALLTYPE ShowHTMLDialog( 
		HWND hwndParent,
		IMoniker *pMk,
		VARIANT *pvarArgIn,
		WCHAR *pchOptions,
		VARIANT *pvarArgOut,
		IUnknown *punkHost);

	//////////////////////////////////////////////////////////////////////////
	//

	virtual BOOL PreTranslateMessage(PMSG lpMsg);


	//////////////////////////////////////////////////////////////////////////
	//

	void OnSizeChanged(PRECT prc );
};

class ZHtmlContainer : public ZContainer
{
public:
	struct URL_REF
	{
		PCWSTR url, referer;
	};

	static ZWnd* WINAPI create()
	{
		return new ZHtmlContainer;
	}

	HRESULT Navigate(PCWSTR url, PCWSTR referer = 0);

protected:
	virtual PCWSTR GetUserAgent();

	virtual /* [local] */ HRESULT STDMETHODCALLTYPE Invoke( 
		/* [in] */ DISPID dispIdMember,
		/* [in] */ REFIID riid,
		/* [in] */ LCID lcid,
		/* [in] */ WORD wFlags,
		/* [out][in] */ DISPPARAMS *pDispParams,
		/* [out] */ VARIANT *pVarResult,
		/* [out] */ EXCEPINFO *pExcepInfo,
		/* [out] */ UINT *puArgErr);

	virtual BOOL CreateControl(IUnknown** ppControl);

	virtual BOOL OnControlActivate(IUnknown* pControl, PVOID lpCreateParams);

	HRESULT STDMETHODCALLTYPE EvaluateNewWindow( 
		/* [string][in] */ __RPC__in_string LPCWSTR pszUrl,
		/* [string][in] */ __RPC__in_string LPCWSTR /*pszName*/,
		/* [string][in] */ __RPC__in_string LPCWSTR pszUrlContext,
		/* [string][in] */ __RPC__in_string LPCWSTR /*pszFeatures*/,
		/* [in] */ BOOL /*fReplace*/,
		/* [in] */ DWORD /*dwFlags*/,
		/* [in] */ DWORD /*dwUserActionTime*/);
};
