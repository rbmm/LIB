#include "stdafx.h"

_NT_BEGIN

#include "view.h"

void ZDocument::AddView(PLIST_ENTRY entry)
{
	InsertHeadList(&_viewList, entry);
}

ZDocument::~ZDocument()
{
	RemoveEntryList(this);
}

ZDocument::ZDocument()
{
	InitializeListHead(&_viewList);
	InitializeListHead(this);

	if (ZGLOBALS* globals = ZGLOBALS::get())
	{
		InsertHeadList(&globals->_docListHead, this);
	}
}

void ZDocument::OnActivate(BOOL bActivate)
{
	PLIST_ENTRY head = &_viewList, entry = head;

	while ((entry = entry->Flink) != head)
	{
		static_cast<ZView*>(entry)->OnDocumentActivate(bActivate);
	}
}

void ZDocument::DestroyAllViews()
{
	AddRef();

	PLIST_ENTRY head = &_viewList, entry = head->Flink;

	while (entry != head)
	{
		ZView* pView = static_cast<ZView*>(entry);
		entry = entry->Flink;
		pView->DestroyView();
	}

	Release();

	PostThreadMessage(GetCurrentThreadId(), WM_NULL, 0, 0);
}

void ZDocument::UpdateAllViews(ZView* pSender, LPARAM lHint, PVOID pHint)
{
	PLIST_ENTRY head = &_viewList, entry = head;

	while ((entry = entry->Flink) != head)
	{
		ZView* pView = static_cast<ZView*>(entry);
		if (pView != pSender)
		{
			pView->OnUpdate(pSender, lHint, pHint);
		}
	}
}

void ZDocument::EnableCommands(DWORD nCount, WORD cmdIds[])
{
	if (nCount)
	{
		do 
		{
			if (!IsCmdEnabled(*cmdIds)) *cmdIds = 0;
		} while (cmdIds++, --nCount);
	}
}

HRESULT ZDocument::QI(REFIID riid, void **ppvObject)
{
	if (riid == __uuidof(ZDocument))
	{
		*ppvObject = static_cast<ZObject*>(this);
		AddRef();
		return S_OK;
	}

	*ppvObject = 0;

	return E_NOINTERFACE;
}

_NT_END

