#pragma once

#include "window.h"

class ZView;

class WINZ_API Z_INTERFACE("DA039C7B-4B82-4deb-8ADD-E09C73631B34") ZDocument : public ZObject, LIST_ENTRY
{
protected:
	LIST_ENTRY _viewList;

	virtual BOOL IsCmdEnabled(WORD cmd) = 0;

	virtual ~ZDocument();

	void DestroyAllViews();

public:
	
	virtual HRESULT QI(REFIID riid, void **ppvObject);

	virtual void OnActivate(BOOL bActivate);

	virtual LRESULT OnCmdMsg(WPARAM wParam, LPARAM lParam) = 0;

	void EnableCommands(DWORD nCount, WORD cmdIds[]);

	void AddView(PLIST_ENTRY entry);

	void UpdateAllViews(ZView* pSender, LPARAM lHint, PVOID pHint);

	ZDocument();
};