#include "stdafx.h"

_NT_BEGIN

#include "dochost.h"

#pragma warning(disable : 4100)

STDMETHODIMP CDocHost::SaveObject(void)
{ 
	return E_NOTIMPL; 
}

STDMETHODIMP CDocHost::GetMoniker(DWORD dwAssign, DWORD dwWhichMoniker, IMoniker **ppmk)
{ 
	*ppmk = 0;
	return E_NOTIMPL; 
}

STDMETHODIMP CDocHost::GetContainer(IOleContainer **ppContainer)
{ 
	*ppContainer = 0;
	return E_NOTIMPL; 
}

STDMETHODIMP CDocHost::ShowObject(void)
{ 
	return S_OK; 
}

STDMETHODIMP CDocHost::OnShowWindow(BOOL fShow)
{ 
	return S_OK; 
}

STDMETHODIMP CDocHost::RequestNewObjectLayout()
{
	return E_NOTIMPL;
}

STDMETHODIMP CDocHost::ShowContextMenu(DWORD dwID, POINT FAR* ppt, IUnknown FAR* pcmdtReserved,
													 IDispatch FAR* pdispReserved)
{	
	//DbgPrint("IDocHostUIHandler::ShowContextMenu()\n");
	return S_OK;
}

STDMETHODIMP CDocHost::GetHostInfo(DOCHOSTUIINFO FAR *pInfo)
{
	pInfo->dwFlags = DOCHOSTUIFLAG_DISABLE_HELP_MENU | 
		DOCHOSTUIFLAG_THEME|
		DOCHOSTUIFLAG_ENABLE_INPLACE_NAVIGATION;
	return S_OK;
}

STDMETHODIMP CDocHost::ShowUI(DWORD dwID, IOleInPlaceActiveObject FAR* pActiveObject,
										 IOleCommandTarget FAR* pCommandTarget,
										 IOleInPlaceFrame  FAR* pFrame,
										 IOleInPlaceUIWindow FAR* pDoc)
{
	return S_OK;
}

STDMETHODIMP CDocHost::HideUI(void)
{
	return S_OK;
}

STDMETHODIMP CDocHost::UpdateUI(void)
{
	return S_OK;
}

STDMETHODIMP CDocHost::OnDocWindowActivate(BOOL fActivate)
{
	return S_OK;
}

STDMETHODIMP CDocHost::OnFrameWindowActivate(BOOL fActivate)
{
	return S_OK;
}

STDMETHODIMP CDocHost::ResizeBorder(LPCRECT prcBorder, IOleInPlaceUIWindow FAR* pUIWindow,
												 BOOL fRameWindow)
{
	return S_OK;
}

STDMETHODIMP CDocHost::TranslateAccelerator(LPMSG lpMsg, const GUID FAR* pguidCmdGroup,
															DWORD nCmdID)
{
	return E_NOTIMPL;
}

STDMETHODIMP CDocHost::GetOptionKeyPath(LPOLESTR FAR* pchKey, DWORD dw)
{	
	*pchKey = 0;
	return E_NOTIMPL;
}

STDMETHODIMP CDocHost::GetDropTarget(IDropTarget* pDropTarget, IDropTarget** ppDropTarget)
{
	return E_NOTIMPL;
}

STDMETHODIMP CDocHost::GetExternal(IDispatch** ppDispatch)
{	
	*ppDispatch = this;
	AddRef();
	return S_OK;
}

STDMETHODIMP CDocHost::TranslateUrl(DWORD dwTranslate, OLECHAR* pchURLIn,
												 OLECHAR** ppchURLOut)
{
	*ppchURLOut = 0;
	return E_NOTIMPL;
}

STDMETHODIMP CDocHost::FilterDataObject(IDataObject* pDO, IDataObject** ppDORet)
{
	*ppDORet = 0;
	return E_NOTIMPL;
}

STDMETHODIMP CDocHost::CanInPlaceActivate()
{
	return S_OK;
}

STDMETHODIMP CDocHost::OnInPlaceActivate()
{		
	return S_OK;
}

STDMETHODIMP CDocHost::OnUIActivate()
{
	return S_OK;
}

STDMETHODIMP CDocHost::GetWindowContext(IOleInPlaceFrame **ppFrame, IOleInPlaceUIWindow **ppDoc,
										LPRECT lprcPosRect, LPRECT lprcClipRect,
										LPOLEINPLACEFRAMEINFO lpFrameInfo)
{
	HWND hwnd = m_hwnd;
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

STDMETHODIMP CDocHost::Scroll(SIZE scrollExtant)
{
	return S_OK;
}

STDMETHODIMP CDocHost::OnUIDeactivate(BOOL fUndoable)
{
	return S_OK;
}

STDMETHODIMP CDocHost::OnInPlaceDeactivate()
{
	return S_OK;
}

STDMETHODIMP CDocHost::DiscardUndoState()
{
	return S_OK;
}

STDMETHODIMP CDocHost::DeactivateAndUndo(void)
{
	return S_OK;
}

STDMETHODIMP CDocHost::OnPosRectChange(LPCRECT lprcPosRect)
{
	return S_OK;
}

STDMETHODIMP CDocHost::GetWindow(HWND* phwnd)
{
	*phwnd = m_hwnd;
	return S_OK;
}

STDMETHODIMP CDocHost::ContextSensitiveHelp(BOOL /*fEnterMode*/)
{
	return S_OK;
}

STDMETHODIMP CDocHost::GetBorder(LPRECT lprectBorder)
{
	return INPLACE_E_NOTOOLSPACE;
}

STDMETHODIMP CDocHost::RequestBorderSpace(LPCBORDERWIDTHS /*pborderwidths*/)
{
	return INPLACE_E_NOTOOLSPACE;
}

STDMETHODIMP CDocHost::SetBorderSpace(LPCBORDERWIDTHS pborderwidths)
{
	return S_OK;
}

STDMETHODIMP CDocHost::SetActiveObject(IOleInPlaceActiveObject* pActiveObject, LPCOLESTR pszObjName)
{
	m_hwndHTML = 0;

	if (pActiveObject) 
	{
		pActiveObject->AddRef();
		pActiveObject->GetWindow(&m_hwndHTML);
	}
	
	pActiveObject = (IOleInPlaceActiveObject*)InterlockedExchangePointer(
		(void**)&m_pActiveObject, (void*)pActiveObject);
	
	if (pActiveObject) pActiveObject->Release();
	
	return S_OK;
}

STDMETHODIMP CDocHost::InsertMenus(HMENU /*hmenuShared*/, LPOLEMENUGROUPWIDTHS /*lpMenuWidths*/)
{
	return S_OK;
}

STDMETHODIMP CDocHost::SetMenu(HMENU /*hmenuShared*/, HOLEMENU /*holemenu*/, HWND /*hwndActiveObject*/)
{
	return S_OK;
}

STDMETHODIMP CDocHost::RemoveMenus(HMENU /*hmenuShared*/)
{
	return S_OK;
}

STDMETHODIMP CDocHost::SetStatusText(LPCOLESTR pszStatusText)
{
	return S_OK;
}

STDMETHODIMP CDocHost::EnableModeless(BOOL )
{
	return S_OK;
}

STDMETHODIMP CDocHost::TranslateAccelerator(LPMSG /*lpMsg*/, WORD /*wID*/)
{
	return S_FALSE;
}

_NT_END