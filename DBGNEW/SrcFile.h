#pragma once

#include "../winz/app.h"
#include "memorycache.h"
#include "AsmView.h"
#include "regview.h"
#include "Dll.h"
#include "zdlgs.h"
#include "common.h"
#include "ZDbgThread.h"

struct CV_DebugSFile;

struct Txt_Lines 
{
	DWORD _nLines, _maxLen, _ofs[];
};

class ZSrcView : public ZTxtWnd, public ZView
{
	Txt_Lines* _Lines;
	LINE_INFO* _pLI;
	PVOID _BaseAddress, _DllBase;
	ZSrcView** _ppView;
	DWORD _pcLine, _Line, _nLines, _nLI;

	virtual void DrawLine(HDC hdc, DWORD x, DWORD y, DWORD line, DWORD column, DWORD len);

	virtual void GetVirtualSize(SIZE& N);

	virtual void DrawIndent(HDC hdc, DWORD y, DWORD line);

	virtual PVOID BeginDraw(HDC hdc, PRECT prc);

	virtual int GetIndent();

	virtual ZWnd* getWnd();

	virtual ZView* getView();

	virtual LRESULT WindowProc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam );

	void DbgContinue(BOOL bStepOver);

	DWORD LineToVa(DWORD line, LINE_INFO** ppLI);

public:
	void InvalidateLine(DWORD nLine);

	void Activate();

	NTSTATUS CreateDoc(POBJECT_ATTRIBUTES poa, CV_DebugSFile* pFile);

	ZSrcView(ZSrcView** ppView, ZDbgDoc* pDoc, ZDll* pDLL, CV_DebugSFile* fileId);

	~ZSrcView();

	void GoLine(DWORD nLine);

	void setPC(DWORD nLine);
};

struct ZSrcFile : LIST_ENTRY, UNICODE_STRING 
{
	ZSrcView* _pView;
	CV_DebugSFile* _fileId;
	BOOLEAN _fDontOpen;

	ZSrcFile(CV_DebugSFile* fileId);
	~ZSrcFile();

	ZSrcView* open(ZDbgDoc* pDoc, ZDll* pDLL);
};