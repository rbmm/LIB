#pragma once

#include "Frame.h"

class ZConsole : public ZFrameWnd, CIcons
{
	HFONT _hFont;
	HGDIOBJ _hBrush;

	void SetFont(HWND hwnd);

	virtual PCUNICODE_STRING getPosName();

	virtual HWND CreateView(HWND hWndParent, int nWidth, int nHeight, PVOID lpCreateParams);

	virtual LRESULT WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	virtual BOOL CanClose();
public:

	struct INID 
	{
		HINSTANCE hInstance;
		LPCWSTR id;
	};

	static void WINAPI _print(PCWSTR buf);

	static void WINAPI vprint(PCWSTR format, va_list args);

	static void print(PCWSTR format, ...);

	inline static HWND g_ConHWND;
};

