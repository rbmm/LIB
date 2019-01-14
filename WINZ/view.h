#pragma once

#include "document.h"

class WINZ_API Z_INTERFACE("E9B9E2B2-BF48-49f7-BE99-77BCDD674CA0") ZView : LIST_ENTRY
{
protected:
	ZDocument* _pDocument;

public:

	virtual void DestroyView();

	virtual void OnUpdate(ZView* pSender, LPARAM lHint, PVOID pHint);

	virtual void OnDocumentActivate(BOOL bActivate);

	virtual ZWnd* getWnd() = 0;

	void Detach();

	ZView(ZDocument* pDocument);

	virtual ~ZView();

	ZDocument* getDocument() { return _pDocument; };

	static BOOL CanClose(HWND hwnd);
	static ZView* get(HWND hwnd);
};