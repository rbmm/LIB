#include "StdAfx.h"

_NT_BEGIN
#include "../winz/Frame.h"
#include "../winz/cursors.h"
#include "resource.h"
#include "eval64.h"
#include "asmview.h"
#include "common.h"
#include "DbgDoc.h"

#pragma warning(disable : 4477)

enum : UINT {
	IdMin = 0x88880000,
	IdShowPc = IdMin,
	IdSetPc,
	IdEnableBp,
	IdDisableBp,
	IdConditionE,
	IdConditionD,
	IdMemoryWindow,
	IdShowReg,
	IdShowDbgWnd,
	IdGotoSrc,
	IdAddWatch,
	IdGoTo,
	IdXrefs,
	IdRefreshCache,
	IdProcessList,
	IdTags,
	IdCopy,
	IdMax
};

extern BOOL g_udtInit;

void ShowXrefs(ZDbgDoc* pDoc, LONG_PTR Va, LONG_PTR ImageBase, PVOID pv, PIMAGE_SECTION_HEADER pish, DWORD NumberOfSections)
{
	DWORD N = 0;
	LONG_PTR xrefs[64];

	do 
	{
		if (!(pish->Characteristics & IMAGE_SCN_MEM_DISCARDABLE))
		{
			DWORD size = min(pish->Misc.VirtualSize, pish->SizeOfRawData);

			PCHAR pb = RtlOffsetToPointer(pv, pish->VirtualAddress);

			if (size >= sizeof(PVOID))
			{
				void **a = (void**)pb, **b = (void**)RtlOffsetToPointer(a, size & ~(sizeof(PVOID)-1));

				while (a < b)
				{
					if (a = findPVOID((DWORD)(b - a), (void**)a, (void*)Va))
					{
						xrefs[N] = ImageBase + RtlPointerToOffset(pv, a);
						if (++N == RTL_NUMBER_OF(xrefs))
						{
							return;
						}
						a++;
					}
					else
					{
						break;
					}
				}
			}

			if (size > sizeof(DWORD) && (pish->Characteristics & IMAGE_SCN_CNT_CODE))
			{
				DWORD cb = size - sizeof(DWORD);

				LONG_PTR lp = ImageBase + pish->VirtualAddress + 1 + sizeof(DWORD);

				do 
				{
					ULONG_PTR d = Va - (lp + *((LONG*)++pb));
					if (d <= sizeof(DWORD))
					{
						xrefs[N] = lp + d;
						if (++N == RTL_NUMBER_OF(xrefs))
						{
							return;
						}
					}
				} while (lp++, --cb);
			}
		}

	} while (pish++, --NumberOfSections);

	if (N)
	{
		ZXrefs::create(pDoc, N, xrefs, Va);
	}
}

BOOL ReadPE(ZDbgDoc* pDoc, PVOID ImageBase, PVOID pv, DWORD cb, PIMAGE_SECTION_HEADER *ppish, DWORD *pNumberOfSections)
{
	if (0 > pDoc->Read(ImageBase, pv, PAGE_SIZE)) return FALSE;

	DWORD e_lfanew = ((PIMAGE_DOS_HEADER)pv)->e_lfanew + sizeof(ULONG);

	if (e_lfanew < sizeof(IMAGE_DOS_HEADER) + sizeof(ULONG))
	{
		return FALSE;
	}

	if (e_lfanew > PAGE_SIZE - sizeof(IMAGE_FILE_HEADER))
	{
		if (
			e_lfanew >= cb - sizeof(IMAGE_FILE_HEADER) 
			||
			0 > pDoc->Read(RtlOffsetToPointer(ImageBase, e_lfanew), RtlOffsetToPointer(pv, e_lfanew), sizeof(IMAGE_FILE_HEADER))
			) 
			return FALSE;
	}

	PIMAGE_FILE_HEADER FileHeader = (PIMAGE_FILE_HEADER)RtlOffsetToPointer(pv, e_lfanew);

	if (DWORD NumberOfSections = FileHeader->NumberOfSections)
	{
		PIMAGE_SECTION_HEADER pish = (PIMAGE_SECTION_HEADER)RtlOffsetToPointer(FileHeader + 1, FileHeader->SizeOfOptionalHeader);
		
		if (pish + NumberOfSections > (PVOID)RtlOffsetToPointer(pv, PAGE_SIZE))
		{
			if (0 > pDoc->Read(RtlOffsetToPointer(ImageBase, RtlPointerToOffset(pv, pish)), pish, NumberOfSections*sizeof(IMAGE_SECTION_HEADER))) return FALSE;
		}

		*ppish = pish;
		*pNumberOfSections = NumberOfSections;

		do 
		{
			if (!(pish->Characteristics & IMAGE_SCN_MEM_DISCARDABLE))
			{
				DWORD size = min(pish->Misc.VirtualSize, pish->SizeOfRawData);
				
				ULONG VirtualAddress = pish->VirtualAddress;
				
				if (
					size < sizeof(PVOID)
					||
					VirtualAddress >= VirtualAddress + size 
					||
					VirtualAddress + size > cb
					||
					0 > pDoc->Read(RtlOffsetToPointer(ImageBase, VirtualAddress), RtlOffsetToPointer(pv, VirtualAddress), size)
					)
				{
					pish->Characteristics = 0;
				}
			}
		} while (++pish, --NumberOfSections);

		return TRUE;
	}
	
	return FALSE;
}

void ShowXrefs(ZDbgDoc* pDoc, PVOID Va)
{
	if (ZDll *p = pDoc->getDllByVaNoRef(Va))
	{
		DWORD cb = p->getSize();
		SIZE_T RegionSize = cb;
		PVOID pv = 0, ImageBase = p->getBase();
		if (0 <= ZwAllocateVirtualMemory(NtCurrentProcess(), &pv, 0, &RegionSize, MEM_COMMIT, PAGE_READWRITE))
		{
			DWORD NumberOfSections;
			PIMAGE_SECTION_HEADER pish;
			if (ReadPE(pDoc, ImageBase, pv, cb, &pish, &NumberOfSections))
			{
				__try
				{
					ShowXrefs(pDoc, (LONG_PTR)Va, (LONG_PTR)ImageBase, pv, pish, NumberOfSections);
				}
				__except(EXCEPTION_EXECUTE_HANDLER)
				{
				}
			}
			ZwFreeVirtualMemory(NtCurrentProcess(), &pv, &RegionSize, MEM_RELEASE);
		}
	}
}

#pragma warning(disable : 4100)
size_t __stdcall ZAsmView::fixupSet(DIS const * pDis, unsigned __int64 Va, size_t len, wchar_t * buf, size_t cb, unsigned __int64 *displacement)
{
	FORMATDATA* pf = (FORMATDATA*)pDis->PvClient();
	if (!pf) return 0;
	pf->fixupLen = len;
	*displacement = 0;

	if (pDis->Dist() != DIS::amd64 && len <= sizeof(PVOID))
	{
		//DbgPrint("%p>fixupSet(%p)| %u\n", pf->pLI->Va, (PVOID)Va, pDis->Trmta());
		switch (pDis->Trmta())
		{
		case DIS::a_jmp_u_2:
		case DIS::a_jmp_u_5:
		case DIS::a_jmp_c_2:
		case DIS::a_jmp_c_6:
		case DIS::a_call:
			break;
		default:
			PVOID pv = (PVOID)Va;
			if (pf->pDll && 0 <= pf->pDoc->Read(pv, &Va, (DWORD)len) && pf->pDll->VaInImage((PVOID)Va))
			{
				return addrSet(pDis, Va, buf, cb, displacement);
			}
		}
	}
	return 0;
}
#pragma warning(default : 4100)

size_t __stdcall ZAsmView::addrSet(DIS const *pDis, unsigned __int64 Va, wchar_t * buf, size_t cb, unsigned __int64 * displacement)
{
	FORMATDATA* pf = (FORMATDATA*)pDis->PvClient();
	if (!pf) return 0;
	//DbgPrint("%p>addrSet(%p | %x)|%u\n", pf->pLI->Va, (PVOID)Va, pf->fixupLen, pDis->Trmta());
	size_t fixupLen = pf->fixupLen;
	pf->fixupLen = 0;
	*displacement = 0;
	BOOL f = TRUE;
	PVOID _Va = (PVOID)Va, __Va = 0;
	ZDbgDoc* pDoc = pf->pDoc;
	LINEINFO* pLI = pf->pLI;
	WORD seg = 0;
	DIS::TRMTA ta = pDis->Trmta();
	
	if (fixupLen == 6)
	{
		seg = (WORD)(Va >> 32);
		Va = (DWORD)Va;
	}
__loop:

	PCSTR Name = pDoc->getNameByVa((PVOID)Va);
	WCHAR szdisp[16];

	if (Name)
	{
		szdisp[0]=0;
__n:
		if (IS_INTRESOURCE(Name))
		{
			char oname[16];
			sprintf(oname, "#%u", (ULONG)(ULONG_PTR)Name);
			Name = oname;
		}
		pLI->jumpEip = (INT_PTR)Va;
		if (__Va) Va = (unsigned __int64)__Va;
		char cc[0x800];
		PCSTR fName = unDNameEx(cc, Name, sizeof(cc), UNDNAME_NAME_ONLY);
		if (strlen(fName) > 0x83)
		{
			strcpy((PSTR)fName + 0x80, "...");
		}
		int len = fixupLen == 6 ? swprintf_s(buf, cb, L"%S%s(%x:%p)", fName, szdisp, seg, (PVOID)(ULONG_PTR)Va) : 
			swprintf_s(buf, cb, L"%S%s(%p)", fName, szdisp, (PVOID)(ULONG_PTR)Va);
		if (len < 0)
		{
			buf[len = (DWORD)cb - 1] = 0;
		}
		else
		{
			buf[0] |= 0x4000, buf[len - (fixupLen == 6 ? 6 + SYM_IN_PTR : 3 + SYM_IN_PTR)] |= 0x8000;
		}
		return len;
	}

	if (f && SYM_IN_PTR + 2 < cb)
	{
		switch (ta)
		{
		case DIS::a_jmp_u_2:
		case DIS::a_jmp_u_5:
		case DIS::a_jmp_c_2:
		case DIS::a_jmp_c_6:
		case DIS::a_loop:
			pLI->jumpEip = (INT_PTR)Va;
			_snwprintf(buf, cb, L"%p %c", (PVOID)Va, (INT_PTR)Va > pLI->Va ? 0x2193 : 0x2191);
			buf[0] |= 0x4000, buf[SYM_IN_PTR - 1] |= 0x8000;
			return SYM_IN_PTR + 2;
		}
	}

	switch (ta)
	{
	case DIS::a_jmp_rm:
	case DIS::a_call_rm:
		if (fixupLen)
		{
			DWORD len = 0;
			PVOID pv = (PVOID)Va;
			Va = 0;
			switch (pDis->Dist())
			{
			case DIS::amd64:
				len = 8;
				break;
			case DIS::ia32:
				len = 4;
				break;
			case DIS::ia16:
				len = 2;
				break;
			}
			if (len && 0 <= pDoc->Read(pv, &Va, len))
			{
				fixupLen = 0, f = FALSE, __Va = pv;
				goto __loop;
			}
		}

		pLI->jumpEip = (INT_PTR)Va;
		int len = _snwprintf(buf, cb, L"%p(%p)", _Va, (PVOID)(ULONG_PTR)Va);
		buf[SYM_IN_PTR + 1] |= 0x4000, buf[2*SYM_IN_PTR+1] |= 0x8000;
		return len;
	}

	if (ZDll* pDll = pDoc->getDllByVaNoRef(_Va))
	{
		if (ta == DIS::a_gen)
		{
			if (Name = pDll->getNameByVa2(_Va, (PINT_PTR)&Va))
			{
				UINT_PTR disp = (UINT_PTR)_Va - (UINT_PTR)Va;
				if (disp < 0x100)
				{
					swprintf(szdisp, L"+0x%x", disp);
					Va = (UINT_PTR)_Va;
					goto __n;
				}
			}
		}

		pLI->jumpEip = (INT_PTR)_Va;

		_snwprintf(buf, cb, L"%p", (PVOID)_Va);
		buf[0] |= 0x4000, buf[SYM_IN_PTR - 1] |= 0x8000;
		return SYM_IN_PTR;
	}

	return 0;
}

void ZAsmView::JumpBack()
{
	if (_iStack)
	{
		if (GoTo(getHWND(), _StackEip[--_iStack & 15]))
		{
			--_iStack;
		}
	}
}

BOOL ZAsmView::VaToLine(ULONG_PTR Address, ZSrcView **ppView, PULONG pLine)
{
	BOOL f = FALSE;

	if (_pDll && !_pDll->get_DontSearchSrc())
	{
		CV_DebugSFile* fileId;
		ULONG Line = _pDll->GetLineByVA((PVOID)Address, &fileId);

		*pLine = Line;

		if (Line)
		{
			ZDbgDoc* pDoc = GetDbgDoc();
			ZSrcFile* p = pDoc->findSrc(fileId);
			ZSrcView *pView = 0;

			if (!p)
			{
				p = pDoc->openSrc(fileId);
			}

			if (p)
			{
				if (pView = p->open(pDoc, _pDll))
				{
					f = TRUE;
				}
			}

			*ppView = pView;
		}
	}

	return f;
}

BOOL ZAsmView::GotoSrc(ULONG_PTR Address)
{
	ULONG line;
	ZSrcView* pView;
	if (VaToLine(Address, &pView, &line))
	{
		pView->GoLine(line);
		pView->Activate();
		return TRUE;
	}
	return FALSE;
}

BOOL ZAsmView::GoTo(HWND hwnd, INT_PTR Address)
{
	if (!_pLI)
	{
		Invalidate(hwnd);
		return FALSE;
	}

	INT_PTR Lo = 0, Hi = 0;

	BOOL fOk = FALSE;

	if (_pDll)
	{
		if (_pDll->InRange((PVOID)Address))
		{
			fOk = _pDll->GetRange((void**)&Lo, (void**)&Hi);//always TRUE
		}
		else
		{
			_pDll->Release();
			_pDll = 0;
		}
	}

	if (!fOk)
	{
		ZDll* pDll;
		if (GetDbgDoc()->getDllByVa(&pDll, (PVOID)Address))
		{
			_pDll = pDll;
			SetTarget(pDll->Is64Bit() ? DIS::amd64 : DIS::ia32);
			fOk = pDll->GetRange((void**)&Lo, (void**)&Hi);
		}
	}

	if (!fOk && !GetDbgDoc()->GetValidRange(Address, Lo, Hi))
	{
		Invalidate(hwnd);
		return FALSE;
	}

	_BaseAddress = Lo;
	_HigestAddress = Hi;
	_ViewSize = (DWORD)(Hi - Lo);

	if (!DisDown(Address, _pLI, _nLines, MAXULONG))
	{
		Invalidate(hwnd);
		return FALSE;
	}

	if (_valid)
	{
		_StackEip[_iStack++ & 15] = _Address;
	}
	_Address = Address;
	_valid = TRUE;

	WCHAR sz[300];
	swprintf(sz, L"[%p, %p) %s", Lo, Hi, _pDll ? _pDll->name() : L"");
	SetWindowText(GetParent(hwnd), sz);

	DeActivateLinkEx(FALSE);

	ZScrollWnd::GoTo(hwnd, 0, RtlPointerToOffset(_BaseAddress, _Address));

	setVA(Address);
	return TRUE;
}

void ZAsmView::OnSize(HWND hwnd, int cx, int cy)
{
	RecalcLayout(hwnd, cy);

	if (_valid)
	{
		ZTxtWnd::OnSize(hwnd, cx, cy);
	}
}

void ZAsmView::OnUpdate(ZView* /*pSender*/, LPARAM lHint, PVOID pHint)
{
	if(!_valid) return;

	if (lHint == BYTE_UPDATED)
	{
		if ((INT_PTR)pHint >= _Address)
		{
			LINEINFO* pLI = _pLI;
			DWORD n = _nLines;

			do 
			{
				if (!(pLI->Flags & fIsNameLine) && (ULONG_PTR)((INT_PTR)pHint - pLI->Va) < pLI->len)
				{
					if (!(pLI->Flags & fFinalLine))
					{
						DisDown(pLI->Va, pLI, n, MAXULONG);
						InvalidateRect(getHWND(), 0, TRUE);
					}
					return;
				}
			} while (pLI++, --n);
		}
	}
}

DWORD ZAsmView::DisUp(INT_PTR Va, LINEINFO* pLI, DWORD n, ULONG _index)
{
	DWORD cb = min((DWORD)(Va - _BaseAddress), 32 + (n << 4));

	if (!cb)
	{
		return 0;
	}

	DWORD ex = min((DWORD)(_HigestAddress - Va), 16);

	ZDbgDoc* pmc = GetDbgDoc();
	
	PBYTE buf = (PBYTE)alloca(cb+ex);

	ZDll* pDll = _pDll;
	DIS* pDisasm = _pDisasm;

	DWORD index = MAXDWORD, k = 0;
	INT_PTR NameVa = MAXINT_PTR, Address = Va;
	PCSTR Name = 0;

	Va -= cb;

	if (0 > pmc->Read((PVOID)Va, buf, cb+ex)) 
	{
		Invalidate(getHWND());
		return 0;
	}

	if (pDll)
	{
		Name = pDll->getNameByVaEx((PVOID)Va, &NameVa, &index);
	}

	LINEINFO* pli = pLI;
	DWORD fLoop = 0;

	do 
	{
		pLI = pli + k;

		while (NameVa <= Va)
		{
			if (NameVa == Va)
			{
				if (index == _index)
				{
					goto __done;
				}
				pLI->Va = Va;
				pLI->len = 0;
				pLI->Flags = fIsNameLine;
				pLI->Index = index;

				if (++k == n)
				{
					fLoop = n;
					k = 0;
				}

				pLI = pli + k;
			} 

			Name = pDll->getNameByIndex(++index, &NameVa, 0);
		}

		if (Va == Address)
		{
			break;
		}

		pLI->Flags = 0;

		if (ZBreakPoint* pbp = pmc->getBpByVa((PVOID)Va))
		{
			UCHAR fl = fIsBpLine;

			if (pbp->_isActive)
			{
				*buf = pbp->_opcode;
			}
			else
			{
				fl |= fIsBpDisable;
			}

			if (pbp->_expression)
			{
				fl |= fIsBpExp;
			}

			pLI->Flags = fl;
		}

		UINT len = pDisasm->CbDisassemble((ULONG_PTR)Va, buf, 16);

		if (!len)
		{
			len = 1;
			pLI->Flags = fInvalidLine;
		}

		memcpy(pLI->buf, buf, len);

		pLI->Va = Va;
		pLI->len = (BYTE)len;

		Va += len, buf += len, cb -= len;
	
		if (++k == n)
		{
			fLoop = n;
			k = 0;
		}

	} while (Va <= Address);

__done:
	if (Va != Address)
	{
		DisDown(Va, _pLI, _nLines, index);
		return MAKELONG(fLoop + k, 1);
	}

	return fLoop + k;
}

DWORD ZAsmView::DisDown(INT_PTR Va, LINEINFO* pLI, DWORD n, ULONG index)
{
	ZDbgDoc* pmc = GetDbgDoc();

	RtlZeroMemory(pLI, n * sizeof(LINEINFO));

	DWORD cb = min((DWORD)(_HigestAddress - Va), n << 4);

	PBYTE buf = (PBYTE)alloca(cb);

	if (0 > pmc->Read((PVOID)Va, buf, cb)) 
	{
		Invalidate(getHWND());
		return 0;
	}

	ZDll* pDll = _pDll;
	DIS* pDisasm = _pDisasm;
	
	INT_PTR NameVa = MAXINT_PTR;
	PCSTR Name = 0;

	if (pDll)
	{
		Name = index == MAXULONG ? pDll->getNameByVaEx((PVOID)Va, &NameVa, &index) : pDll->getNameByIndex(index, &NameVa, 0);
	}

	int nLines = 0;

	do 
	{
		while (NameVa <= Va)
		{
			if (NameVa == Va)
			{
				pLI->Flags = fIsNameLine;
				pLI->Va = Va;
				pLI->Index = index;
				pLI++->len = 0;
				nLines++;

				if (!--n)
				{
					return nLines;
				}
			}

			Name = pDll->getNameByIndex(++index, &NameVa, 0);
		}

		pLI->Flags = 0;

		if (ZBreakPoint* pbp = pmc->getBpByVa((PVOID)Va))
		{
			UCHAR fl = fIsBpLine;

			if (pbp->_isActive)
			{
				*buf = pbp->_opcode;
			}
			else
			{
				fl |= fIsBpDisable;
			}

			if (pbp->_expression)
			{
				fl |= fIsBpExp;
			}

			pLI->Flags = fl;
		}

		UINT len = pDisasm->CbDisassemble((ULONG_PTR)Va, buf, 16);

		if (cb < len)
		{
			do 
			{
				pLI->Flags = fInvalidLine|fFinalLine;
				pLI++->len = 0;
			} while (--n);

			return nLines;
		}
	
		if (!len)
		{
			len = 1;
			pLI->Flags = fInvalidLine;
		}

		memcpy(pLI->buf, buf, len);
		
		pLI->Va = Va;
		pLI++->len = (BYTE)len;

		Va += len, buf += len, cb -= len;

	} while (nLines++, --n);

	return nLines;
}

int ZAsmView::ScrollLines(int nBar, int nLines, int& nPos)
{
	DeActivateLinkEx();

	if (nBar == SB_HORZ || !_valid || !nLines)
	{
		return ZScrollWnd::ScrollLines(nBar, nLines, nPos);
	}
	
	LINEINFO* pLI = (LINEINFO*)alloca(abs(nLines) * sizeof(LINEINFO)), *pli;

	if (0 < nLines)
	{
		pli = _pLI + _nLines - 1;
		if (pli->Flags & fFinalLine)
		{
			nLines = 0;
		}
		else if (nLines = DisDown(pli->Va + pli->len, pLI, nLines, pli->Flags & fIsNameLine ? pli->Index+1 : MAXULONG))
		{
			memcpy(_pLI, _pLI + nLines, (_nLines - nLines) * sizeof(LINEINFO));
			memcpy(_pLI + _nLines - nLines, pLI, nLines * sizeof(LINEINFO));
		}
	}
	else
	{
		DWORD n = -nLines, k;
		BOOL fInvalidate = FALSE;

		if (k = DisUp(_Address, pLI, n, _pLI->Flags & fIsNameLine ? _pLI->Index : MAXULONG))
		{
			fInvalidate = HIWORD(k), k = LOWORD(k);

			if (k < n)
			{
				memmove(_pLI + k, _pLI, (_nLines - k) * sizeof(LINEINFO));
				memcpy(_pLI, pLI, k * sizeof(LINEINFO));
			}
			else
			{
				k -= n;
				memmove(_pLI + n, _pLI, (_nLines - n) * sizeof(LINEINFO));
				memcpy(_pLI, pLI + k, (n - k) * sizeof(LINEINFO));
				if (k) memcpy(_pLI + n - k, pLI, k * sizeof(LINEINFO));
				k = n;
			}
		}

		nLines = fInvalidate ? 0 : -(int)k;
	}

	_Address = _pLI->Va;

	nPos = (int)(_Address - _BaseAddress);

	return nLines;
}

int ZAsmView::NewPos(int nBar, int nPos, BOOL bTrack)
{
	DeActivateLinkEx();

	if (nBar == SB_HORZ || !_valid)
	{
		return nPos;
	}

	INT_PTR Address = _BaseAddress + nPos;

	if (!bTrack)
	{
		Address = AdjustAddress(Address);
	}

	if (_Address != Address)
	{
		DisDown(Address, _pLI, _nLines, MAXULONG);
		_Address = Address;
	}

	return (int)(Address - _BaseAddress);
}

BOOL ZAsmView::RecalcLayout(HWND hwnd, int cy)
{
	if (!cy)
	{
		Invalidate(hwnd);
		return FALSE;
	}

	SIZE u;
	GetUnitSize(u);

	DWORD nLines = (cy + u.cy - 1) / u.cy;

	if (nLines != _nLines)
	{
		if (LINEINFO* pLI = new LINEINFO[nLines])
		{
			if (_pLI)
			{
				memcpy(pLI, _pLI, min(nLines, _nLines)* sizeof(LINEINFO));
				delete [] _pLI;
			}

			_pLI = pLI;

			if (_valid && nLines > _nLines)
			{
				pLI += _nLines;

				LONG Flags = pLI[-1].Flags;
				DWORD n = nLines - _nLines;

				if (Flags & fFinalLine)
				{
					do 
					{
						pLI->Flags = fInvalidLine|fFinalLine;
						pLI++->len = 0;
					} while (--n);
				}
				else
				{
					DisDown(pLI[-1].Va + pLI[-1].len, pLI, n, Flags & fIsNameLine ? pLI[-1].Index+1 : MAXULONG);
				}
			}

			_nLines = nLines;
		}
		else
		{
			Invalidate(hwnd);
			return FALSE;
		}
	}

	return TRUE;
}

BOOL ZAsmView::CanCloseFrame()
{
	return FALSE;
}

INT_PTR ZAsmView::AdjustAddress(INT_PTR Address)
{
	DWORD cb = min((DWORD)(Address - _BaseAddress), 64);

	if (!cb) return Address;

	DWORD ex = min((DWORD)(_HigestAddress - Address), 16);

	ZDbgDoc* pmc = GetDbgDoc();

	PBYTE buf = (PBYTE)alloca(cb+ex);

	INT_PTR Va = Address - cb;

	if (0 > pmc->Read((PVOID)Va, buf, cb+ex)) 
	{
		Invalidate(getHWND());
		return 0;
	}

	DIS* pDisasm = _pDisasm;
	do 
	{
		if (*buf == 0xcc)
		{
			if (ZBreakPoint* pbp = pmc->getBpByVa((PVOID)Va))
			{
				if (pbp->_isActive)
				{
					*buf = pbp->_opcode;
				}
			}
		}

		UINT len = pDisasm->CbDisassemble((ULONG_PTR)Va, buf, 16);

		if (!len)
		{
			len = 1;
		}
		Va += len, buf += len;
	
	} while (Va < Address);

	return Va;
}

int ZAsmView::GetIndent()
{
	return 38;
}

void ZAsmView::OnMouseMove(int x, int y)
{
	int i = GetIndent();
	SIZE v;
	GetUnitSize(v);
	if (y < 0) y = 0;
	
	if (x < i)
	{
		x = 0;
	}
	else
	{
		POINT pos;
		GetPos(pos);
		x = pos.x + (x - i) / v.cx;
	}

	y /= v.cy;

	if ((_xMouse != x || _yMouse != y) && ((ULONG)y < _nLines))
	{
		LINEINFO* pLI = _pLI + y;
		BOOL bLinkActive = (pLI->Flags & fLinkLine) && (pLI->aLink <= (BYTE)x) && ((BYTE)x < pLI->zLink);

		if (_bLinkActive && (!bLinkActive || _yMouse != y))
		{
			ActivateLink(FALSE);
		}

		_xMouse = (short)x, _yMouse = (short)y;

		if (bLinkActive != _bLinkActive)
		{
			ActivateLink(TRUE);
		}
	}
}

void ZAsmView::EndDraw(HDC hdc, PVOID pv)
{
	if (_bDrawActive)
	{
		_bDrawActive = FALSE;
		if (_valid)
		{
			POINT pt;
			GetCursorPos(&pt);
			ScreenToClient(getHWND(), &pt);
			OnMouseMove(pt.x, pt.y);
		}
	}
	ZTxtWnd::EndDraw(hdc, pv);
}

void ZAsmView::DrawIndent(HDC hdc, DWORD y, DWORD line)
{
	if (_valid)
	{
		LINEINFO* pLI = &_pLI[((_BaseAddress + line) - _Address)];

		DWORD Flags = pLI->Flags;

		if (!(Flags & fIsNameLine))
		{
			SIZE u;
			GetUnitSize(u);

			if (pLI->Va == _Va)
			{
				PatBlt(hdc, GetIndent()-14, y, 9, u.cy, BLACKNESS);
			}

			int i = -1;

			switch (Flags & (fIsBpLine|fIsBpDisable|fIsBpExp))
			{
			case fIsBpLine|fIsBpDisable|fIsBpExp:
				i = 0;
				break;
			case fIsBpLine|fIsBpExp:
				i = 1;
				break;
			case fIsBpLine|fIsBpDisable:
				i = 2;
				break;
			case fIsBpLine:
				i = 3;
				break;
			}

			if (0 <= i)
			{
				g_IL20.Draw(hdc, 2, y + 2, i);
			}

			if (pLI->Va == _pcVa)
			{
				g_IL20.Draw(hdc, 2, y + 2, 4);
			}
		}
	}
}

PVOID ZAsmView::BeginDraw(HDC hdc, PRECT prc)
{
	int z = GetIndent(), x;

	if (prc->right > z)
	{
		FillRect(hdc, prc, (HBRUSH)(1 + COLOR_WINDOW));
		_bDrawActive = TRUE;
	}
	else
	{
		RECT rc = *prc;
		x = z - 14;
		if (rc.left < x)
		{
			rc.right = x;
			FillRect(hdc, &rc, (HBRUSH)(1 + COLOR_MENUBAR));
			rc.right = prc->right;
		}

		if (rc.right >= x)
		{
			rc.left = x;
			FillRect(hdc, &rc, (HBRUSH)(1 + COLOR_WINDOW));
		}
		x = z - 10;
		MoveToEx(hdc, x, prc->top, 0);
		LineTo(hdc, x, prc->bottom);
	}

	return ZTxtWnd::BeginDraw(hdc, prc);
}

void ZAsmView::GetVirtualSize(SIZE& N)
{
	N.cx = 256;
	N.cy = _ViewSize;
}

ZAsmView::ZAsmView(ZDocument* pDocument) : ZAddressView(pDocument)
{
	DbgPrint("++ZAsmView<%p>\n", this);
	_pDll = 0;
	_pDisasm = 0;
	_iDist = DIS::invalid;
	_nLines = 0;
	_pcVa = 0;
	_Va = 0;
	_pLI = 0;
	_bLinkActive = FALSE;
	_bTrackActive = FALSE;
	_bDrawActive = FALSE;
	_xMouse = 0, _yMouse = 0;
	_iStack = 0;
}

ZAsmView::~ZAsmView()
{
	if (_pDll) _pDll->Release();

	if (_pLI) delete [] _pLI;

	if (_pDisasm) delete _pDisasm;
	
	DbgPrint("--ZAsmView<%p>\n", this);
}

void ZAsmView::OnUnloadDll(ZDll* pDll)
{
	if (_pDll == pDll)
	{
		pDll->Release();
		_pDll = 0;
		Invalidate(getHWND());
	}
}

void ZAsmView::ShowTarget()
{
	PCWSTR name = L"";
	switch (_iDist)
	{
	case DIS::amd64:
		name = L"amd64";
		break;
	case DIS::ia32:
		name = L"ia32";
		break;
	case DIS::ia16:
		name = L"ia16";
		break;
	}
	ZGLOBALS::getMainFrame()->SetStatusText(1, name);
}

BOOL ZAsmView::SetTarget(DIS::DIST i)
{
	if (_iDist == i || _iDist == (DIS::DIST)-1)
	{
		return TRUE;
	}

	if (_pDisasm)
	{
		delete _pDisasm;
		_pDisasm = 0;
		_iDist = DIS::invalid;
	}

	if (i == DIS::invalid)
	{
		return TRUE;
	}

	if (DIS * pDisasm = DIS::PdisNew(i))
	{
		_iDist = i;
		_pDisasm = pDisasm;
		pDisasm->PfncchaddrSet(addrSet);
		pDisasm->PfncchfixupSet(fixupSet);

		if (DWORD n = _nLines)
		{
			LINEINFO* pLI = _pLI;
			do 
			{
				pLI->Flags = fInvalidLine;
			} while (pLI++, -- n);
		}

		ShowTarget();

		return TRUE;
	}

	return FALSE;
}

void ZAsmView::OnMenuItemSelect(HMENU hmenu, DWORD id, HWND hwnd)
{
	ZDbgDoc* pDoc = GetDbgDoc();

	DIS::DIST i = DIS::invalid;

	switch (id)
	{
	case IdRefreshCache:
		pDoc->Invalidate();
		return;
	case IdXrefs:
		ShowXrefs(pDoc, (PVOID)_Va);
		return;
	case IdGoTo:
		{
			MENUITEMINFO mii = { sizeof(mii), MIIM_DATA };
			if (GetMenuItemInfo(hmenu, IdGoTo, FALSE, &mii))
			{
				GoTo(hwnd, mii.dwItemData);
			}
		}
		return;
	case IdProcessList:
		pDoc->ShowProcessList();
		return;
	case IdAddWatch:
		AddWatch(pDoc);
		return;
	case IdGotoSrc:
		GotoSrc(_Va);
		return;
	case IdShowDbgWnd:
		pDoc->ShowDbgWnd();
		return;
	case IdShowReg:
		pDoc->ShowReg();
		return;
	case IdMemoryWindow:
		CreateMemoryWindow(pDoc);
		return;
	case IdSetPc:
		pDoc->setPC(_Va);
		setPC(_Va);
		return;
	case IdShowPc:
		GoTo(hwnd, pDoc->getPC());
		return;
	case IdCopy:
		if (ULONG nLines = _nLines)
		{
			if (OpenClipboard(getHWND()))
			{
				EmptyClipboard();
				enum { maxCCh = 0x1000 };
				if (HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, maxCCh*sizeof(WCHAR)))
				{
					PWSTR psz = (PWSTR)GlobalLock(hg), end = psz + maxCCh - 3;
					ULONG k = 0;
					do 
					{
						ULONG cch = (ULONG)(end - psz);
						if (cch < 256)
						{
							break;
						}
						psz = FormatLine(k++, psz, cch);
						*psz++ = '\r', *psz++ = '\n', *psz = 0;

					} while (--nLines);

					GlobalUnlock(hg);
					if (!SetClipboardData(CF_UNICODETEXT, hg)) GlobalFree(hg);

				}
				CloseClipboard();
			}
		}
		return;
	case ID_1_AMD64:
		i = DIS::amd64;
		break;
	case ID_1_IA16:
		i = DIS::ia16;
		break;
	case ID_1_IA32:
		i = DIS::ia32;
		break;
	default: return;
	}

	SetTarget(i);
	if (_valid)
	{
		DeActivateLinkEx();
		DisDown(_Address, _pLI, _nLines, _pLI->Flags & fIsNameLine ? _pLI->Index : MAXULONG);
		InvalidateRect(hwnd, 0, TRUE);
	}
}

HMENU ZAsmView::GetContextMenu(HMENU& rhsubmenu)
{
	rhsubmenu = 0;

	ZDbgDoc* pDoc = GetDbgDoc();

	if (pDoc->IsRemoteDebugger())
	{
		if (!pDoc->IsRemoteWait())
		{
			return 0;
		}
		pDoc->getUdtCtxEx();
	}

	if (HMENU hmenu = CreatePopupMenu())
	{
		MENUITEMINFO mii = { sizeof(mii), MIIM_ID|MIIM_STRING|MIIM_BITMAP|MIIM_DATA , 0, 0, IdMemoryWindow, 0, 0, 0, 0, const_cast<PWSTR>(L"Memory"), 0, HBMMENU_CALLBACK};
		InsertMenuItem(hmenu, 0, TRUE, &mii);

		if (!pDoc->IsDump())
		{
			mii.wID = IdRefreshCache;
			mii.dwTypeData = const_cast<PWSTR>(L"Refresh Cache");
			InsertMenuItem(hmenu, 0, TRUE, &mii);
		}
		//else
		//{
		//	mii.wID = IdTags;
		//	mii.dwTypeData = L"Blobs";
		//	InsertMenuItem(hmenu, 0, TRUE, &mii);
		//}

		if (pDoc->IsDbgWndExist() && !pDoc->IsDbgWndVisible())
		{
			mii.wID = IdShowDbgWnd;
			mii.dwTypeData = const_cast<PWSTR>(pDoc->IsDump() ? L"Stack" : L"Threads");
			InsertMenuItem(hmenu, 0, TRUE, &mii);
		}

		if (pDoc->IsLocalMemory() ? g_udtInit : pDoc->getUdtCtx() != 0)
		{
			mii.wID = IdAddWatch;
			mii.dwTypeData = const_cast<PWSTR>(L"Add Watch");
			InsertMenuItem(hmenu, 0, TRUE, &mii);
		}

		if (pDoc->getUdtCtx() && pDoc->IsDump())
		{
			mii.wID = IdProcessList;
			mii.dwTypeData = const_cast<PWSTR>(L"Processes");
			InsertMenuItem(hmenu, 0, TRUE, &mii);
		}

		if (pDoc->IsDump() || pDoc->IsWaitContinue())
		{
			if (!pDoc->IsRegVisible())
			{
				mii.wID = IdShowReg;
				mii.dwTypeData = const_cast<PWSTR>(L"Registry");
				InsertMenuItem(hmenu, 0, TRUE, &mii);
			}
		}

		if (pDoc->IsWaitContinue())
		{
			if (_valid)
			{
				mii.wID = IdSetPc;
				mii.dwTypeData = const_cast<PWSTR>(L"Set PC");
				InsertMenuItem(hmenu, 0, TRUE, &mii);
			}

			mii.wID = IdShowPc;
			mii.dwTypeData = const_cast<PWSTR>(L"Show PC");
			InsertMenuItem(hmenu, 0, TRUE, &mii);
		}

		if (_valid)
		{
			mii.wID = IdCopy;
			mii.dwTypeData = const_cast<PWSTR>(L"Copy");
			InsertMenuItem(hmenu, 0, TRUE, &mii);
		}

		if (_valid && _pDll)
		{
			CV_DebugSFile* fileId;
			ULONG line = _pDll->GetLineByVA((PVOID)_Va, &fileId);

			if (line)
			{
				PCSTR name = _pDll->GetFileName(fileId), c = strrchr(name, '\\');
				if (c)
				{
					name = c + 1;
				}

				int len = MultiByteToWideChar(CP_UTF8, 0, name, MAXDWORD, 0, 0);

				if (0 < len)
				{
					MultiByteToWideChar(CP_UTF8, 0, name, MAXDWORD, mii.dwTypeData = (PWSTR)alloca((24+len) << 1), len);
					swprintf(mii.dwTypeData + len - 1, L"(%u)", line);
					mii.wID = IdGotoSrc;
					InsertMenuItem(hmenu, 0, TRUE, &mii);
				}
			}

			if (!pDoc->IsRemoteDebugger()) 
			{
				mii.wID = IdXrefs;
				mii.dwTypeData = const_cast<PWSTR>(L"Show xrefs");
				InsertMenuItem(hmenu, 0, TRUE, &mii);
			}

			if (PCSTR name = _pDll->getNameByVa2((PVOID)_Va, (PINT_PTR)&mii.dwItemData))
			{
				if (IS_INTRESOURCE(name))
				{
					char oname[16];
					sprintf(oname, "#%u", (ULONG)(ULONG_PTR)name);
					name = oname;
				}

				char undName[256];
				name = unDNameEx(undName, name, RTL_NUMBER_OF(undName), UNDNAME_NAME_ONLY);
				mii.wID = IdGoTo;
				swprintf(mii.dwTypeData = (PWSTR)alloca((strlen(name) + 10) << 1), L"%S+%x", name, RtlPointerToOffset(mii.dwItemData, _Va));
				
				InsertMenuItem(hmenu, 0, TRUE, &mii);
			}
		}

		rhsubmenu = hmenu;

		return hmenu;
	}
	return 0;
}

BOOL ZAsmView::FormatLineForDrag(DWORD line, PSTR buf, DWORD cb)
{
	if (!_valid || _bLinkActive || _nLines <= line || cb < 5 + SYM_IN_PTR)
	{
		return FALSE;
	}

	LINEINFO* pLI = &_pLI[line];

	if (pLI->Flags & (fIsNameLine|fFinalLine))
	{
		return FALSE;
	}

	INT_PTR Va = pLI->Va;

	buf += sprintf(buf, " %p   ", Va); 
	cb -= 4 + SYM_IN_PTR;

	if (pLI->Flags & fInvalidLine)
	{
		return TRUE;
	}

	WCHAR bufw[256];

	FORMATDATA fd = { _pDll, GetDbgDoc(), pLI };
	_pDisasm->CbDisassemble((ULONG_PTR)Va, pLI->buf, 16);
	_pDisasm->PvClientSet(&fd);
	DWORD len = (DWORD)_pDisasm->CchFormatInstr(bufw, RTL_NUMBER_OF(bufw));
	
	return 0 <= RtlUnicodeToMultiByteN(buf, cb, 0, bufw, len<<1);
}

void ZAsmView::OnIndentDown(int line)
{
	LINEINFO* pLI = &_pLI[line];
	if (!(pLI->Flags & (fInvalidLine|fIsNameLine)))
	{
		GetDbgDoc()->ToggleBp((PVOID)pLI->Va);
	}
}

void ZAsmView::OnLButtonDown(HWND hwnd, int /*x*/, int y)
{
	if (_valid)
	{
		SIZE u;
		GetUnitSize(u);
		LINEINFO* pLI = &_pLI[y / u.cy];
		if (_bLinkActive)
		{
			if (pLI->Flags & fLinkLine)
			{
				GoTo(hwnd, pLI->jumpEip);
			}
			else
			{
				//DbgBreak();
			}
		}
		else if (!(pLI->Flags & (fFinalLine|fIsNameLine)))
		{
			setVA(pLI->Va);
		}
	}
}

void ZAsmView::InvalidateVa(INT_PTR Va, ACTION action)
{
	if (_valid)
	{
		DWORD n = _nLines;
		LINEINFO* pLI = _pLI;

		do 
		{
			if (pLI->Va == Va && !(pLI->Flags & (fFinalLine|fIsNameLine)))
			{
				switch (action)
				{
				case BpActivated:
					pLI->Flags &= ~fIsBpDisable;
					break;
				case BpDisabled:
					pLI->Flags |= fIsBpDisable;
					break;
				case BpRemoved:
					pLI->Flags &= ~(fIsBpLine|fIsBpExp|fIsBpDisable);
					break;
				case BpAdded:
					pLI->Flags |= fIsBpLine;
					break;
				case BpExpAdded:
					pLI->Flags |= fIsBpExp;
					break;
				case BpExpRemoved:
					pLI->Flags &= ~fIsBpExp;
					break;
				}

				SIZE u;
				GetUnitSize(u);
				RECT rc;
				if (GetClientRect(getHWND(), &rc))
				{
					rc.top = (_nLines - n) *u.cy, rc.bottom = rc.top + u.cy;
					switch (action)
					{
					case actMarker:
						rc.right = GetIndent();
						break;
					}
					InvalidateRect(getHWND(), &rc, TRUE);
				}
				break;
			}
		} while (pLI++, --n);
	}
}

void ZAsmView::setPC(INT_PTR Va)
{
	_setPC(Va);

	if (Va)
	{
		HWND hwnd = static_cast<ZMDIFrameWnd*>(ZGLOBALS::getMainFrame())->GetActive();
		ZSrcView* pView;
		DWORD Line;
		if (VaToLine(Va, &pView, &Line))
		{
			pView->setPC(Line);
			if (hwnd != GetParent(getHWND()) && hwnd != GetParent(pView->getHWND()))
			{
				pView->Activate();
			}
		}
		else if (hwnd != GetParent(getHWND()))
		{
			Activate();
		}
	}
}

void ZAsmView::_setPC(INT_PTR Va)
{
	if (_pcVa != Va)
	{
		if (_pcVa)
		{
			InvalidateVa(_pcVa, actPC);
		}

		_pcVa = Va;

		if (Va)
		{
			if (Va != _Va)
			{
				if (_Va)
				{
					InvalidateVa(_Va, actMarker);
				}

				_Va = Va;
			}

			if (DWORD n = _nLines - 1)
			{
				LINEINFO* pLI = _pLI;
				do 
				{
					if (pLI->Va == Va && !(pLI->Flags & (fIsNameLine|fInvalidLine)))
					{
						SIZE u;
						GetUnitSize(u);
						RECT rc;
						if (GetClientRect(getHWND(), &rc))
						{
							rc.top = (_nLines - n - 1)*u.cy, rc.bottom = rc.top + u.cy;
							InvalidateRect(getHWND(), &rc, TRUE);
						}
						return;
					}
				} while (pLI++, -- n);
			}

			DIS::DIST iDist = _iDist;
			_iDist = (DIS::DIST)-1;
			GoTo(getHWND(), Va);
			_iDist = iDist;
		}
	}
}

void ZAsmView::setVA(INT_PTR Va)
{
	if (Va != _Va)
	{
		if (_Va)
		{
			InvalidateVa(_Va, actMarker);
		}

		_Va = Va;

		if (Va)
		{
			InvalidateVa(Va, actMarker);
		}
	}
}

PWSTR ZAsmView::FormatLine(DWORD line, PWSTR buf, ULONG cch)
{
	LINEINFO* pLI = &_pLI[line];

	LONG Flags = pLI->Flags;

	if (_pDll && (Flags & fIsNameLine))
	{
		char ss[512];
		buf[1] = ' ';
		ULONG flags;
		INT_PTR Va;
		PCSTR Name = _pDll->getNameByIndex(pLI->Index, &Va, &flags);
		switch (flags)
		{
		case FLAG_RVAEXPORT:
			buf[0] = L'·';
			break;
		case FLAG_NOEXPORT:
			buf[0] = ' ';
			break;
		default: buf[0] = '*';
		}

		if (IS_INTRESOURCE(Name))
		{
			char oname[16];
			sprintf(oname, "#%u", (ULONG)(ULONG_PTR)Name);
			Name = oname;
		}

		PCSTR c = unDNameEx(ss, Name, RTL_NUMBER_OF(ss) - 2, UNDNAME_DEFAULT);
		ULONG BytesInUnicodeString;
		if (0 <= RtlMultiByteToUnicodeN(buf + 2, (cch - 2)*sizeof(WCHAR), &BytesInUnicodeString, c, 1 + (ULONG)strlen(c)))
		{
			return buf + 1 + BytesInUnicodeString / sizeof(WCHAR);
		}

		return buf;
	}

	if (!pLI->len)
	{
		*buf = 0;
		return 0;
	}

	INT_PTR Va = pLI->Va;

	int len = swprintf_s(buf, cch, L" %p   ", Va);

	if (0 > len)
	{
		return buf;
	}
	
	buf += len, cch -= len;

	if (Flags & fInvalidLine)
	{
		len = swprintf_s(buf, cch, L"DB          %02X", pLI->buf[0]);
		return 0 < len ? buf + len : buf;
	}

	FORMATDATA fd = { _pDll, GetDbgDoc(), pLI };
	_pDisasm->CbDisassemble((ULONG_PTR)Va, pLI->buf, 16);
	_pDisasm->PvClientSet(&fd);
	_pDisasm->CchFormatInstr(buf, cch);

	while (WCHAR c = *buf)
	{
		if (c & 0xc000)
		{
			*buf &= 0x3fff;
		}
		buf++;
	}

	return buf;
}

void ZAsmView::DrawLine(HDC hdc, DWORD x, DWORD y, DWORD line, DWORD column, DWORD len)
{
	if (!_valid)
	{
		return ;
	}
	
	WCHAR buf[0x400], *sz;
	LINEINFO* pLI = &_pLI[((_BaseAddress + line) - _Address)];

	LONG Flags = pLI->Flags;

	PWSTR szLink[2]={}, link = 0;
	DWORD lenLink[2] = {};
	DWORD iLink = 0;
	DWORD color = COLOR_NORMAL, txtcolor = COLOR_TEXT;

	if (_pDll && (Flags & fIsNameLine))
	{
		txtcolor = COLOR_NORMAL, color = COLOR_NAME;

		char ss[0x400];
		buf[1] = ' ';
		ULONG flags;
		INT_PTR Va;
		PCSTR Name = _pDll->getNameByIndex(pLI->Index, &Va, &flags);
		switch (flags)
		{
		case FLAG_RVAEXPORT:
			buf[0] = L'·';
			break;
		case FLAG_NOEXPORT:
			buf[0] = ' ';
			break;
		default:buf[0] = '*';
		}

		if (IS_INTRESOURCE(Name))
		{
			char oname[16];
			sprintf(oname, "#%u", (ULONG)(ULONG_PTR)Name);
			Name = oname;
		}

		PCSTR c = unDNameEx(ss, Name, RTL_NUMBER_OF(ss) - 2, UNDNAME_DEFAULT);
		RtlMultiByteToUnicodeN(buf + 2, sizeof(buf) - 2*sizeof(WCHAR), 0, c, 1 + (ULONG)strlen(c));
	}
	else if (pLI->len)
	{
		INT_PTR Va = pLI->Va;

		if (Va == _pcVa)
		{
			color = COLOR_PC;
		}

		sz = buf + swprintf(buf, L" %p   ", Va);

		if (Flags & fInvalidLine)
		{
			swprintf(sz, L"DB          %02X", pLI->buf[0]);
		}
		else
		{
			if ((Flags & (fIsBpLine|fIsBpDisable)) == fIsBpLine)
			{
				color = color == COLOR_NORMAL ? COLOR_BP : COLOR_PCBP;
				txtcolor = COLOR_NORMAL;
			}

			FORMATDATA fd = { _pDll, GetDbgDoc(), pLI };
			_pDisasm->CbDisassemble((ULONG_PTR)Va, pLI->buf, 16);
			_pDisasm->PvClientSet(&fd);
			_pDisasm->CchFormatInstr(sz, RTL_NUMBER_OF(buf) - 4 - SYM_IN_PTR);

			WCHAR c;
			do 
			{
				c = *sz;

				if (c & 0x4000)
				{
					*sz &= ~0x4000;
					link = sz;
				}

				if ((c & 0x8000) && link)
				{
					*sz &= ~0x8000;
					szLink[iLink] = link;
					lenLink[iLink] = 1 + (DWORD)(sz - link);

					pLI->aLink = (BYTE)(link - buf);
					pLI->zLink = (BYTE)(sz + 1 - buf);

					if (++iLink == RTL_NUMBER_OF(szLink))
					{
						break;
					}
				}
			} while (++sz, c);

			if (iLink)
			{
				pLI->Flags |= fLinkLine;
			}
		}
	}
	else
	{
		*buf = 0;
	}

	DWORD n = (DWORD)wcslen(buf);
	SIZE u;
	GetUnitSize(u);

	RECT rc = { x, y, x + len * u.cx, y + u.cy };

	//if (COLOR_NORMAL==color && (line&1))color = RGB(0xc0,0xc0,0xc0);

	SetTextColor(hdc, txtcolor);
	SetBkColor(hdc, color);

	if (column > n)
	{
		ExtTextOutW(hdc, x, y, ETO_OPAQUE|ETO_CLIPPED, &rc, L"", 0, 0);
		return ;
	}

	if ((n -= column) || color != COLOR_NORMAL)
	{
		ExtTextOutW(hdc, x, y, ETO_OPAQUE|ETO_CLIPPED, &rc, sz = buf + column, min(n, len), 0);

		if (iLink)
		{
			SetTextColor(hdc, COLOR_NAME);
			do 
			{
				int cx = (DWORD)((link = szLink[--iLink]) - sz) * u.cx;
				ExtTextOutW(hdc, x + cx, y, ETO_CLIPPED, &rc, link, lenLink[iLink], 0);
			} while (iLink);
		}
	}
}

STATIC_UNICODE_STRING_(Disasm);

PCUNICODE_STRING ZAsmView::getRegName()
{
	return &Disasm;
}

void ZAsmView::DrawLink(int iLine, BOOL fActivate)
{
	WCHAR buf[256], *sz;
	LINEINFO* pLI = &_pLI[iLine];
	
	FORMATDATA fd = { _pDll, GetDbgDoc(), pLI };

	_pDisasm->CbDisassemble((ULONG_PTR)pLI->Va, pLI->buf, 16);
	_pDisasm->PvClientSet(&fd);
	_pDisasm->CchFormatInstr(buf, RTL_NUMBER_OF(buf));

	int i = pLI->aLink, j = pLI->zLink;
	buf[j - SYM_IN_PTR - 5] &= ~0x8000, buf[j - SYM_IN_PTR -4] = 0;
	sz = buf + i - SYM_IN_PTR -4;
	sz[0] &= ~0x4000;

	if (HDC hdc = GetDC(getHWND()))
	{
		POINT pos;
		GetPos(pos);

		if (0 < (j -= pos.x))
		{
			if (0 > (i -= pos.x))
			{
				sz -= i;
				i = 0;
			}
			SIZE u;
			GetUnitSize(u);

			HGDIOBJ o;

			DWORD color = pLI->Va == _pcVa ? COLOR_PC : COLOR_NORMAL, txtcolor = fActivate ? COLOR_ACTIVE_LINK : COLOR_NAME;

			if ((pLI->Flags & (fIsBpLine|fIsBpDisable)) == fIsBpLine)
			{
				color = color == COLOR_NORMAL ? COLOR_BP : COLOR_PCBP;

				if (fActivate)
				{
					txtcolor = COLOR_ACTIVE_LINK2;
				}
			}

			SetBkColor(hdc, color);
			SetTextColor(hdc, txtcolor);
			int x = GetIndent() + i * u.cx;

			ZFont* font = ZGLOBALS::getFont();
			o = SelectObject(hdc, fActivate ? font->_getFont() : font->getFont());
			TextOutW(hdc, x, iLine * u.cy, sz, j - i);
			SelectObject(hdc, o);
		}

		ReleaseDC(getHWND(), hdc);
	}
}

void ZAsmView::ActivateLink(BOOL fActivate, BOOL fDraw)
{
	if (fActivate)
	{
		_bLinkActive = TRUE;

		DrawLink(_yMouse, TRUE);
		//DbgPrint("+++++(%u,%u)+++++\n", _xMouse, _yMouse);
		SetCursor(CCursorCashe::GetCursor(CCursorCashe::HAND));
		if (!_bTrackActive)
		{
			TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, getHWND() };
			_bTrackActive = TrackMouseEvent(&tme) != 0;
		}
	}
	else if (_bLinkActive)
	{
		_bLinkActive = FALSE;
		RECT rc;
		POINT pt;
		GetClientRect(getHWND(), &rc);
		GetCursorPos(&pt);
		ScreenToClient(getHWND(), &pt);
		if (PtInRect(&rc, pt))
		{
			SetCursor(CCursorCashe::GetCursor(CCursorCashe::IBEAM));
		}
		if (fDraw) DrawLink(_yMouse, FALSE);
		//DbgPrint("-----(%u,%u)-----\n", _xMouse, _yMouse);
	}
}

inline void ZAsmView::DeActivateLinkEx(BOOL fDraw)
{
	ActivateLink(FALSE, fDraw);
	_xMouse = 0, _yMouse = 0;
}

LRESULT ZAsmView::WindowProc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	ZDbgDoc* pDoc = GetDbgDoc();
	
	switch (uMsg)
	{
	case WM_COMMAND:
		{
			DIS::DIST i = DIS::invalid;
			switch (wParam)
			{
			case ID_1_AMD64:
				i = DIS::amd64;
				break;
			case ID_1_IA32:
				i = DIS::ia32;
				break;
			case ID_1_IA16:
				i = DIS::ia16;
				break;
			}
			if (i != DIS::invalid)
			{
				SetTarget(i);
				GoTo(hwnd, _Address);
			}
		}
		break;
	case WM_CREATE:
		pDoc->SetAsm(this);
		break;
	
	case WM_DESTROY:
		pDoc->SetAsm(0);
		break;

	case WM_SYSKEYDOWN:
	case WM_KEYDOWN:
		switch(wParam)
		{
		case VK_PAUSE:
			pDoc->Break();
			break;
		case VK_F4:
			pDoc->DbgContinue(VK_F4);
			return 0;
		case VK_F5:
			pDoc->DbgContinue(VK_F5);
			return 0;
		case VK_F10:
			if (INT_PTR Va = pDoc->getPC())
			{
				UCHAR buf[16];
				SIZE_T cb;
				if (0 <= pDoc->Read((PVOID)Va, buf, 16, &cb))
				{
					if (ZBreakPoint* pbp = pDoc->getBpByVa((PVOID)Va))
					{
						if (pbp->_isActive)
						{
							buf[0] = pbp->_opcode;
						}
					}
					if (cb = _pDisasm->CbDisassemble((ULONG_PTR)Va, buf, cb))
					{
						switch (_pDisasm->Trmta())
						{
						case DIS::a_call:
						case DIS::a_call_rm:
__f10:
							pDoc->DbgContinue(VK_F10, Va + cb);
							return 0;
						default:
							WCHAR sz[64];
							_pDisasm->PvClientSet(0);
							_pDisasm->CchFormatInstr(sz, RTL_NUMBER_OF(sz));
							if (sz[0] == 'r' && sz[1] == 'e' && sz[2] == 'p')
							{
								goto __f10;
							}
						}
					}
				}
			}
		case VK_F11:
			pDoc->DbgContinue(VK_F11);
			return 0;
		case VK_F12:
			pDoc->DbgContinue(VK_F12);
			return 0;
		case VK_F9:
			if (_valid && pDoc->_IsDebugger)
			{
				INT_PTR Va = _Va;
				int n = _nLines;
				LINEINFO* pLi = _pLI;
				do 
				{
					if (!(pLi->Flags & (fIsNameLine|fInvalidLine)) && (pLi->Va == Va))
					{
						pDoc->ToggleBp((PVOID)Va);
						return 0;
					}
				} while (pLi++, --n);
			}
			return 0;
		}
		break;

	case WM_SETCURSOR:
		if (_bLinkActive)
		{
			SetCursor(CCursorCashe::GetCursor(CCursorCashe::HAND));
			return TRUE;
		}
		break;

	case WM_MOUSELEAVE:
		_bTrackActive = FALSE;
		DeActivateLinkEx();
		break;

	case WM_MOUSEMOVE:
		if (_valid && !_capture)
		{
			OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		}
		break;

	case WM_NCLBUTTONDOWN:
		if (_valid)
		{
			POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
			if (ScreenToClient(hwnd, &pt) && pt.x < GetIndent())
			{
				SIZE u;
				GetUnitSize(u);
				OnIndentDown(pt.y / u.cy);
			}
		}
		break;
	case WM_NCRBUTTONDOWN:
		if (_valid)
		{
			POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
			if (ScreenToClient(hwnd, &pt) && pt.x < GetIndent())
			{
				SIZE u;
				GetUnitSize(u);
				LINEINFO* pLI = &_pLI[pt.y / u.cy];
				DWORD flags = pLI->Flags;
				if ((flags & (fIsBpLine|fInvalidLine)) == fIsBpLine)
				{
					if (HMENU hmenu = CreatePopupMenu())
					{
						MENUITEMINFO mii = { sizeof(mii), MIIM_ID|MIIM_STRING|MIIM_BITMAP, 0, 0, 0, 0, 0, 0, 0, const_cast<PWSTR>(L"Condition"), 0, HBMMENU_CALLBACK};
						
						if (flags & fIsBpDisable)
						{
							mii.wID = IdConditionD;
						}
						else
						{
							mii.wID = IdConditionE;
						}

						InsertMenuItem(hmenu, 0, TRUE, &mii);

						if (flags & fIsBpDisable)
						{
							mii.wID = IdEnableBp;
							mii.dwTypeData = const_cast<PWSTR>(L"Enable Bp");
						}
						else
						{
							mii.wID = IdDisableBp;
							mii.dwTypeData = const_cast<PWSTR>(L"Disable Bp");
						}

						InsertMenuItem(hmenu, 0, TRUE, &mii);

						if (DWORD id = TrackPopupMenu(hmenu, TPM_RETURNCMD, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), 0, hwnd, 0))
						{
							if (ZBreakPoint* pbp = pDoc->getBpByVa((PVOID)pLI->Va))
							{
								switch (id)
								{
								case IdEnableBp:
								case IdDisableBp:
									pDoc->EnableBp(pbp, id == IdEnableBp, TRUE);
									break;
								case IdConditionE:
								case IdConditionD:
									if (ZBpExp* p = new ZBpExp(pDoc, (PVOID)pLI->Va))
									{
										p->Create(pbp->_expression);
										p->Release();
									}
									break;
								}
							}
						}

						DestroyMenu(hmenu);
					}
				}
			}
		}
		break;
	case WM_MEASUREITEM:
		if (!wParam)
		{
			PMEASUREITEMSTRUCT pmi = (PMEASUREITEMSTRUCT)lParam;
			pmi->itemHeight = 16;
			pmi->itemWidth = 4;
		}
		return TRUE;
	case WM_DRAWITEM:
		if (!wParam)
		{
			PDRAWITEMSTRUCT pmi = (PDRAWITEMSTRUCT)lParam;
			if ((pmi->itemID -= IdMin) < (IdMax - IdMin))
			{
				ImageList_Draw(g_himl16, pmi->itemID, pmi->hDC, 1, pmi->rcItem.top, 0);
			}
		}
		return TRUE;
	}

	return ZAddressView::WindowProc(hwnd, uMsg, wParam, lParam);
}

void ActivateParentFrame(HWND hwnd)
{
	hwnd = GetParent(hwnd);
	ZMDIChildFrame::_Activate(GetParent(hwnd), hwnd);
	SetFocus(hwnd);
}

void ZAsmView::Activate()
{
	ActivateParentFrame(getHWND());
}

HRESULT ZAsmView::QI(REFIID riid, void **ppvObject)
{
	if (riid == __uuidof(ZAsmView))
	{
		*ppvObject = static_cast<ZObject*>(this);
		AddRef();
		return S_OK;
	}

	return ZTxtWnd::QI(riid, ppvObject);
}

class ZAsmFrame : public ZMDIChildMultiFrame
{
	virtual BOOL CreateClient(HWND hwnd, int nWidth, int nHeight, PVOID lpCreateParams)
	{
		if (hwnd = ZAddressCreateClient(hwnd, nWidth, nHeight, ZAsmView::createObject, (ZDocument*)lpCreateParams))
		{
			_hwndView = hwnd;
			return TRUE;
		}
		return FALSE;
	}
};

HWND CreateAsmWindow(ZDbgDoc* pDocument)
{
	HWND hwnd = 0;

	if (ZAsmFrame* p = new ZAsmFrame)
	{
		WCHAR title[64];
		swprintf(title, L"%X Asm", (DWORD)pDocument->getId());
		hwnd = p->Create(title, pDocument);

		p->Release();
	}

	return hwnd;
}

_NT_END