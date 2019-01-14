#include "stdafx.h"

_NT_BEGIN

#include "layout.h"

BOOL CUILayot::CreateLayout(HWND hwndParent, int xCenter, int yCenter)
{
	HWND hwnd = GetWindow(hwndParent, GW_CHILD);

	while (hwnd)
	{
		if (!AddChild(hwndParent, hwnd, xCenter, yCenter)) return FALSE;

		hwnd = GetWindow(hwnd, GW_HWNDNEXT);
	}

	return TRUE;
}

BOOL CUILayot::AddChild(HWND hwndParent, HWND hwnd, int xCenter, int yCenter)
{
	RECT rc, rcParent;

	if (!GetClientRect(hwndParent, &rcParent) || !GetWindowRect(hwnd, &rc))
	{
		return FALSE;
	}

	MapWindowPoints(HWND_DESKTOP, hwndParent, (POINT*)&rc, 2);

	if (!xCenter)
	{
		xCenter = rcParent.right >> 1;
	}

	if (!yCenter)
	{
		yCenter = rcParent.bottom >> 1;
	}

	DWORD f = 0;

	if (xCenter < rc.right)
	{
		f |= movBx;

		if (xCenter < rc.left)
		{
			f |= movAx;
		}
	}

	if (yCenter < rc.bottom)
	{
		f |= movBy;

		if (yCenter < rc.top)
		{
			f |= movAy;
		}
	}

	if (!f)
	{
		return TRUE;
	}

	if (ENTRY* p = new ENTRY)
	{
		InsertHeadList(this, p);

		p->f = f;
		p->hwnd = hwnd;

		p->left = f & movAx ? rcParent.right - rc.left : rc.left;
		p->right = f & movBx ? rcParent.right - rc.right : rc.right;
		p->top = f & movAy ? rcParent.bottom - rc.top : rc.top;
		p->bottom = f & movBy ? rcParent.bottom - rc.bottom : rc.bottom;

		++nNumWindows;

		return TRUE;
	}

	return FALSE;
}

BOOL CUILayot::RemoveChild(HWND hwnd)
{
	if (ENTRY* entry = get(hwnd))
	{
		RemoveEntryList(entry);

		delete entry;

		--nNumWindows;

		return TRUE;
	}

	return FALSE;
}

CUILayot::CUILayot() : nNumWindows(0)
{
	InitializeListHead(this);
}

CUILayot::~CUILayot()
{
	if (nNumWindows)
	{
		PLIST_ENTRY head = this, entry = head->Flink;

		do 
		{
			ENTRY* p = static_cast<ENTRY*>(entry);

			entry = entry->Flink;

			delete p;

		} while (entry != head);
	}
}

CUILayot::ENTRY* CUILayot::get(HWND hwnd)
{
	PLIST_ENTRY head = this, entry = head;

	while ((entry = entry->Blink) != head) 
	{
		ENTRY* ple = static_cast<ENTRY*>(entry);

		if (ple->hwnd == hwnd) 
		{
			return ple;
		}
	}

	return 0;
}

void CUILayot::Modify2Childs(HWND hwnd1, HWND hwnd2, int d )
{
	ENTRY * ple[2], *p;

	if ((ple[1] = get(hwnd1)) && (ple[0] = get(hwnd2)))
	{
		if (ple[1]->bottom == ple[0]->top && ple[1]->left == ple[0]->left && ple[1]->right == ple[0]->right)
		{
			ple[1]->bottom += d, ple[0]->top += d;
		}
		else if (ple[1]->right == ple[0]->left && ple[1]->top == ple[0]->top && ple[1]->bottom == ple[0]->bottom)
		{
			ple[1]->right += d, ple[0]->left += d;
		}
		else return;

		RECT rc;
		GetClientRect(GetParent(hwnd1), &rc);

		if (HDWP hWinPosInfo = BeginDeferWindowPos(2))
		{
			int i = 2;
			do 
			{
				p = ple[--i];

				if (DWORD f = p->f)
				{
					int Ax, Ay, Bx, By;

					Ax = f & movAx ? rc.right - p->left : p->left;
					Ay = f & movAy ? rc.bottom - p->top : p->top;
					Bx = f & movBx ? rc.right - p->right : p->right;
					By = f & movBy ? rc.bottom - p->bottom : p->bottom;

					hWinPosInfo = DeferWindowPos(hWinPosInfo,
						p->hwnd,
						0,
						Ax,
						Ay,
						Bx - Ax,
						By - Ay,
						SWP_NOACTIVATE|SWP_NOOWNERZORDER
						);
				}
			} while (i);

			EndDeferWindowPos(hWinPosInfo);
		}
	}
}

void CUILayot::Resize(WPARAM wParam, LPARAM lParam)
{
	switch(wParam)
	{
	case SIZE_MAXIMIZED:
	case SIZE_RESTORED:
		Resize(LOWORD(lParam), HIWORD(lParam));
	}
}

void CUILayot::Resize(int cx, int cy)
{
	if (nNumWindows)
	{
		if (HDWP hWinPosInfo = BeginDeferWindowPos(nNumWindows))
		{
			PLIST_ENTRY head = this, entry = head->Flink;

			do 
			{
				ENTRY* p = static_cast<ENTRY*>(entry);

				if (DWORD f = p->f)
				{
					int Ax, Ay, Bx, By;

					Ax = f & movAx ? cx - p->left : p->left;
					Ay = f & movAy ? cy - p->top : p->top;
					Bx = f & movBx ? cx - p->right : p->right;
					By = f & movBy ? cy - p->bottom : p->bottom;

					hWinPosInfo = DeferWindowPos(hWinPosInfo,
						p->hwnd,
						0,
						Ax,
						Ay,
						Bx - Ax,
						By - Ay,
						SWP_NOACTIVATE|SWP_NOOWNERZORDER
						);
				}

			} while ((entry = entry->Flink) != head);

			EndDeferWindowPos(hWinPosInfo);
		}
	}
}

void CUILayot::OnParentNotify(HWND hwndParent, WPARAM wParam, LPARAM lParam)
{
	switch (LOWORD(wParam))
	{
	case WM_CREATE:
		AddChild(hwndParent, (HWND)lParam);
		break;
	case WM_DESTROY:
		RemoveChild((HWND)lParam);
		break;
	}
}

_NT_END