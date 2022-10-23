#include "StdAfx.h"

_NT_BEGIN

#include "common.h"
#include "../inc/md5.h"

Txt_Lines* Parse(PVOID buf, SIZE_T cb)
{
	PVOID end = RtlOffsetToPointer(buf, cb), sz = buf;

	DWORD n = 0;
	do 
	{
		n++;
	} while (sz = _strnchr(sz, end, '\n'));

	if (Txt_Lines* ptl = (Txt_Lines*)new UCHAR[FIELD_OFFSET(Txt_Lines, _ofs[n + 1])])
	{
		ptl->_nLines = n;

		PDWORD pofs = ptl->_ofs;
		DWORD maxLen = 0, Len, ofs, _ofs = 0;
		sz = buf;
		do 
		{
			ofs = RtlPointerToOffset(buf, sz);
			*pofs++ = ofs;
			Len = ofs - _ofs;
			if (maxLen < Len)
			{
				maxLen = Len;
			}
			_ofs = ofs;
		} while (sz = _strnchr(sz, end, '\n'));

		*pofs = (DWORD)cb;
		ptl->_maxLen = maxLen - 2;

		return ptl;
	}

	return 0;
}

DWORD ZSrcView::LineToVa(DWORD line, LINE_INFO** ppLI)
{
	LINE_INFO* pLI = _pLI, *p;
	DWORD a = 0, b = _nLI, o;

	line++;

	do 
	{
		DWORD l = pLI[o = (a + b) >> 1].line;

		if (l == line)
		{
			p = pLI += (a = o);
			while (a--)
			{
				if ((--p)->line != line)
				{
					++p;
					break;
				}
			}
			*ppLI = p;
			b = _nLI;
			while (o++ < b)
			{
				if ((++pLI)->line != line)
				{
					--pLI;
					break;
				}
			}
			return 1 + (ULONG)(pLI - p);
		}

		if (l < line) a = o + 1; else b = o;

	} while (a < b);

	return 0;
}

void ZSrcView::DrawLine(HDC hdc, DWORD x, DWORD y, DWORD line, DWORD column, DWORD len)
{
	//DbgPrint("DL(%u, %u, %u)\n", line, _Line, _pcLine);
	DWORD * pofs = _Lines->_ofs;

	DWORD lineLen = pofs[line+1]-pofs[line], ll = len;

	ZDbgDoc* pDoc = GetDbgDoc();
	LINE_INFO* pLI;
	BOOLEAN bpLine = FALSE;
	DWORD n;

	if (n = LineToVa(line, &pLI))
	{
		do 
		{
			if (ZBreakPoint* pbp = pDoc->getBpByVa(RtlOffsetToPointer(_DllBase, pLI->Rva)))
			{
				if (pbp->_isActive) 
				{
					bpLine = TRUE;
					break;
				}
			}
		} while (--n);
	}

	PWSTR wz = 0;
	n = 0;

	if (column < lineLen && line < _Lines->_nLines)
	{
		PCSTR sz = RtlOffsetToPointer(_BaseAddress, pofs[line]) + column, c = sz;
		len = min(len, lineLen - column);
		DWORD i = len;

		do 
		{
			switch (*c++)
			{
			case '\t':
				n += 4;
				break;
			default: n++;
			}
		} while (--i);

		wz = (PWSTR)alloca(n << 1);
		PWSTR wc = wz;
		i = len;

		do 
		{
			switch (char cz = *sz++)
			{
			case '\t':
				*wc++ = ' ';
				*wc++ = ' ';
				*wc++ = ' ';
				*wc++ = ' ';
				break;
			default: *wc++ = cz;
			}
		} while (--i);
	}

	if (bpLine || line == _pcLine)
	{
		SIZE u;
		GetUnitSize(u);

		RECT rc = { x, y, x + ll * u.cx, y + u.cy };
		DWORD txtclr, bkclr;
		if (bpLine && line == _pcLine)
		{
			bkclr = COLOR_PCBP;
			txtclr = COLOR_TEXT;
		}
		else if (bpLine)
		{
			txtclr = COLOR_NORMAL;
			bkclr = COLOR_BP;
		}
		else
		{
			txtclr = COLOR_TEXT;
			bkclr = COLOR_PC;
		}
		SetBkColor(hdc, bkclr);
		SetTextColor(hdc, txtclr);
		ExtTextOut(hdc, x, y, ETO_OPAQUE|ETO_CLIPPED, &rc, wz, n, 0);
	}
	else if (n)
	{
		SetTextColor(hdc, COLOR_TEXT);
		SetBkColor(hdc, COLOR_NORMAL);
		TextOut(hdc, x, y, wz, n);
	}
}

void ZSrcView::GetVirtualSize(SIZE& N)
{
	N.cx = _Lines->_maxLen;
	N.cy = _Lines->_nLines;
}

void ZSrcView::DrawIndent(HDC hdc, DWORD y, DWORD line)
{
	if (line == _Line)
	{
		SIZE u;
		GetUnitSize(u);
		PatBlt(hdc, GetIndent()-14, y, 9, u.cy, BLACKNESS);
	}

	ZDbgDoc* pDoc = GetDbgDoc();
	LINE_INFO* pLI;
	LONG flags = 0;
	enum { bp, act, exp };

	if (DWORD n = LineToVa(line, &pLI))
	{
		do 
		{
			if (ZBreakPoint* pbp = pDoc->getBpByVa(RtlOffsetToPointer(_DllBase, pLI->Rva)))
			{
				_bittestandset(&flags, bp);
				if (pbp->_isActive) _bittestandset(&flags, act);
				if (pbp->_expression) _bittestandset(&flags, exp);
			}
		} while (--n);

		if (_bittest(&flags, bp))
		{
			if (_bittest(&flags, act))
			{
				if (_bittest(&flags, exp))
				{
					n = 1;
				}
				else
				{
					n = 3;
				}
			}
			else
			{
				if (_bittest(&flags, exp))
				{
					n = 0;
				}
				else
				{
					n = 2;
				}
			}
			g_IL20.Draw(hdc, 2, y + 2, n);
		}
	}

	if (line == _pcLine)
	{
		g_IL20.Draw(hdc, 2, y + 2, 4);
	}
}

PVOID ZSrcView::BeginDraw(HDC hdc, PRECT prc)
{
	int z = GetIndent(), x;

	if (prc->right > z)
	{
		FillRect(hdc, prc, (HBRUSH)(1+COLOR_WINDOW));
	}
	else
	{
		RECT rc = *prc;
		x = z - 14;
		if (rc.left < x)
		{
			rc.right = x;
			FillRect(hdc, &rc, (HBRUSH)(1+COLOR_MENUBAR));
			rc.right = prc->right;
		}

		if (rc.right >= x)
		{
			rc.left = x;
			FillRect(hdc, &rc, (HBRUSH)(1+COLOR_WINDOW));
		}
		x = z - 10;
		MoveToEx(hdc, x, prc->top, 0);
		LineTo(hdc, x, prc->bottom);
	}
	return ZTxtWnd::BeginDraw(hdc, prc);
}

void ZSrcView::InvalidateLine(DWORD nLine)
{
	//DbgPrint("InvalidateLine(%u, %u, %u)\n", nLine, _Line, _pcLine);
	POINT nPos;
	GetPos(nPos);
	if ((nLine -= nPos.y) < _nLines)
	{
		SIZE u;
		GetUnitSize(u);
		RECT rc;
		if (GetClientRect(getHWND(), &rc))
		{
			rc.bottom = (rc.top = nLine* u.cy) + u.cy;
			//DbgPrint("InvalidateRect(%u-%u)\n", rc.top, rc.bottom);
			InvalidateRect(getHWND(), &rc, FALSE);
		}
	}
}

void ActivateParentFrame(HWND hwnd);

void ZSrcView::Activate()
{
	ActivateParentFrame(getHWND());
}

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
	IdMax
};

void ZSrcView::DbgContinue(BOOL bStepOver)
{
	if (_pcLine != MAXDWORD)
	{
		ULONG_PTR Va = GetDbgDoc()->getPC();
		LINE_INFO* pLI;
		if (DWORD n = LineToVa(_pcLine, &pLI))
		{
			do 
			{
				union{
					ULONG_PTR ulp;
					ULONG ul;
				} u = {Va - (ULONG_PTR)RtlOffsetToPointer(_DllBase, pLI->Rva)};
				
				if (u.ulp < pLI->len)
				{
					GetDbgDoc()->DbgContinue(Va, pLI->len - u.ul, bStepOver);
					break;
				}
			} while (pLI++, --n);
		}
	}
}

LRESULT ZSrcView::WindowProc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	SIZE u;
	POINT nPos;
	LINE_INFO* pLI;
	ZDbgDoc* pDoc = GetDbgDoc();
	DWORD n, k;

	switch (uMsg)
	{
	case WM_SYSKEYDOWN:
	case WM_KEYDOWN:
		switch(wParam)
		{
		case VK_F4:
			pDoc->DbgContinue(VK_F4);
			return 0;
		case VK_F5:
			pDoc->DbgContinue(VK_F5);
			return 0;
		case VK_F10:
			DbgContinue(TRUE);
		case VK_F11:
			DbgContinue(FALSE);
			return 0;
		case VK_F12:
			pDoc->DbgContinue(VK_F12);
			return 0;
		case VK_F9:
			n = _Line;
			goto __toggle;
		}
		break;
	case WM_LBUTTONDOWN:
		GetUnitSize(u);
		GetPos(nPos);
		n = nPos.y + GET_Y_LPARAM(lParam)/u.cy;
		if (GET_X_LPARAM(lParam) < GetIndent())
		{
__toggle:
			if (n = LineToVa(n, &pLI))
			{
				do 
				{
					pDoc->ToggleBp(RtlOffsetToPointer(_DllBase, pLI++->Rva));
				} while (--n);
			}
		}
		else
		{
			if (_Line != n)
			{
				InvalidateLine(_Line);
				_Line = n;
				InvalidateLine(n);
			}
		}
		break;
	case WM_SIZE:
		GetUnitSize(u);
		_nLines = (u.cy - 1 + GET_Y_LPARAM(lParam))/u.cy;
		break;
	case WM_RBUTTONDOWN:
		if (GET_X_LPARAM(lParam) >= GetIndent())
		{
			if (HMENU hmenu = CreatePopupMenu())
			{
				MENUITEMINFO mii = { sizeof(mii), MIIM_ID|MIIM_STRING|MIIM_BITMAP, 0, 0, IdMemoryWindow, 0, 0, 0, 0, const_cast<PWSTR>(L"Memory"), 0, HBMMENU_CALLBACK};
				InsertMenuItem(hmenu, 0, TRUE, &mii);

				if ((pDoc->IsDebugger() || pDoc->IsDump()) && !pDoc->IsDbgWndVisible())
				{
					mii.wID = IdShowDbgWnd;
					mii.dwTypeData = const_cast<PWSTR>(pDoc->IsDump() ? L"Stack" : L"Threads");
					InsertMenuItem(hmenu, 0, TRUE, &mii);
				}

				if (pDoc->getUdtCtx())
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
					if (_pcLine != MAXDWORD)
					{
						mii.wID = IdShowPc;
						mii.dwTypeData = const_cast<PWSTR>(L"Show PC");
						InsertMenuItem(hmenu, 0, TRUE, &mii);
					}
				}

				if (k = n = LineToVa(_Line, &pLI))
				{
					if (n == 1 && pDoc->IsWaitContinue())
					{
						mii.wID = IdSetPc;
						mii.dwTypeData = const_cast<PWSTR>(L"Set PC");
						InsertMenuItem(hmenu, 0, TRUE, &mii);
					}

					WCHAR sz[17];
					mii.wID = IdMin+100;
					mii.dwTypeData = sz;
					do 
					{
						swprintf(sz, L"%p", RtlOffsetToPointer(_DllBase, pLI++->Rva));
						InsertMenuItem(hmenu, 0, TRUE, &mii);
						mii.wID++;
					} while (--n);
					pLI -= k;
				}

				POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
				ClientToScreen(hwnd, &pt);

				if (n = TrackPopupMenu(hmenu, TPM_RETURNCMD, pt.x, pt.y, 0, hwnd, 0))
				{
					INT_PTR ipc;
					switch (n)
					{
					case IdProcessList:
						pDoc->ShowProcessList();
						break;
					case IdShowDbgWnd:
						pDoc->ShowDbgWnd();
						break;
					case IdShowReg:
						pDoc->ShowReg();
						break;
					case IdMemoryWindow:
						CreateMemoryWindow(pDoc);
						break;
					case IdSetPc:
						pDoc->RemoveSrcPC();
						pDoc->setPC(ipc = (INT_PTR)RtlOffsetToPointer(_DllBase, pLI->Rva));
						pDoc->setAsmPC(ipc);
						break;
					case IdShowPc:
						GoLine(_pcLine);
						break;
					default:
						if ((n -= (IdMin+100)) < k)
						{
							pDoc->GoTo(RtlOffsetToPointer(_DllBase, pLI[n].Rva), FALSE);
						}
					}
				}

				DestroyMenu(hmenu);
			}
		}
		return 0;
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
			if ((pmi->itemID - IdMin) >= (IdMax - IdMin))
			{
				pmi->itemID = IdGotoSrc;
			}
			ImageList_Draw(g_himl16, pmi->itemID - IdMin, pmi->hDC, 1, pmi->rcItem.top, 0);
		}
		return TRUE;
	}
	return ZTxtWnd::WindowProc(hwnd, uMsg, wParam, lParam);
}

int ZSrcView::GetIndent()
{
	return 38;
}

enum CV_SourceChksum_t : BYTE {
	CHKSUM_TYPE_NONE,        // indicates no checksum is available
	CHKSUM_TYPE_MD5,
	CHKSUM_TYPE_SHA1,
	CHKSUM_TYPE_SHA_256,
};

NTSTATUS IsChecksumOk(PCWSTR pszAlgId, PBYTE pb, ULONG cb, PBYTE pbHash, ULONG cbHash)
{
	NTSTATUS status;
	BCRYPT_HASH_HANDLE hHash = 0;
	BCRYPT_ALG_HANDLE hAlgorithm;

	if (0 <= (status = BCryptOpenAlgorithmProvider(&hAlgorithm, pszAlgId, 0, 0)))
	{
		ULONG HashLength = 0, cbResult = 0;
		if (0 <= (status = BCryptGetProperty(hAlgorithm, BCRYPT_HASH_LENGTH, (PBYTE)&HashLength, sizeof(HashLength), &cbResult, 0)))
		{
			status = cbHash == HashLength ? BCryptCreateHash(hAlgorithm, &hHash, 0, 0, 0, 0, 0) : STATUS_INVALID_IMAGE_HASH;
		}

		BCryptCloseAlgorithmProvider(hAlgorithm, 0);

		if (0 <= status)
		{
			PBYTE Hash = (PBYTE)alloca(cbHash);

			if (0 <= (status = BCryptHashData(hHash, pb, cb, 0)) &&
				0 <= (status = BCryptFinishHash(hHash, Hash, cbHash, 0)))
			{
				status = memcmp(Hash, pbHash, cbHash) ? STATUS_IMAGE_CHECKSUM_MISMATCH : STATUS_SUCCESS;
			}

			BCryptDestroyHash(hHash);
		}
	}

	return status;
}

CV_SourceChksum_t GetFileChecksum(_In_ CV_DebugSFile* pFile, _Out_ PBYTE* Checksum, _Out_ ULONG* cbChecksum);

NTSTATUS IsFileChecksumOk(CV_DebugSFile* pFile, PVOID pvData, ULONG cbData)
{
	PBYTE Checksum;
	ULONG cbHash;
	PCWSTR pszAlgId;

	switch (GetFileChecksum(pFile, &Checksum, &cbHash))
	{
	case CHKSUM_TYPE_MD5:
		pszAlgId = BCRYPT_MD5_ALGORITHM;
		break;
	case CHKSUM_TYPE_SHA1:
		pszAlgId = BCRYPT_SHA1_ALGORITHM;
		break;
	case CHKSUM_TYPE_SHA_256:
		pszAlgId = BCRYPT_SHA256_ALGORITHM;
		break;
	default:
		return STATUS_IMAGE_CHECKSUM_MISMATCH;
	}

	return IsChecksumOk(pszAlgId, (PBYTE)pvData, cbData, Checksum, cbHash);
}


NTSTATUS ZSrcView::CreateDoc(POBJECT_ATTRIBUTES poa, CV_DebugSFile* pFile)
{
	HANDLE hFile, hSection = 0;
	IO_STATUS_BLOCK iosb;
	NTSTATUS status;

	if (!_pLI)
	{
		return STATUS_UNSUCCESSFUL;
	}

	if (0 <= (status = ZwOpenFile(&hFile, FILE_GENERIC_READ, poa, &iosb, FILE_SHARE_READ, FILE_SYNCHRONOUS_IO_NONALERT)))
	{
		FILE_STANDARD_INFORMATION fsi;

		if (0 <= (status = ZwQueryInformationFile(hFile, &iosb, &fsi, sizeof(fsi), FileStandardInformation)))
		{
			if (fsi.EndOfFile.HighPart)
			{
				status = STATUS_FILE_TOO_LARGE;
			}
			else if (!fsi.EndOfFile.LowPart)
			{
				status = STATUS_MAPPED_FILE_SIZE_ZERO;
			}
			else
			{
				status = ZwCreateSection(&hSection, SECTION_MAP_READ, 0, 0, PAGE_READONLY, SEC_COMMIT, hFile);
			}
		}

		NtClose(hFile);

		if (0 <= status)
		{
			PVOID BaseAddress = 0;
			SIZE_T ViewSize = 0;

			status = ZwMapViewOfSection(hSection, NtCurrentProcess(), 
				&BaseAddress, 0, 0, 0, &ViewSize, ViewUnmap, 0, PAGE_READONLY);

			NtClose(hSection);

			if (0 <= status)
			{
				if (0 > (status = IsFileChecksumOk(pFile, BaseAddress, fsi.EndOfFile.LowPart)))
				{
					if (MessageBoxW(ZGLOBALS::getMainHWND(),
						L"source file mismatch! use it anyway ?", poa->ObjectName->Buffer,
						MB_ICONWARNING | MB_YESNO) == IDYES)
					{
						status = STATUS_SUCCESS;
					}
				}

				if (0 <= status)
				{
					if ((_Lines = Parse(BaseAddress, fsi.EndOfFile.LowPart)))
					{
						_BaseAddress = BaseAddress;
					}
					else
					{
						status = STATUS_INSUFFICIENT_RESOURCES;
					}
				}

				if (0 > status)
				{
					ZwUnmapViewOfSection(NtCurrentProcess(), BaseAddress);
				}
			}
		}
	}

	return status;
}

ZSrcView::ZSrcView(ZSrcView** ppView, ZDbgDoc* pDoc, ZDll* pDLL, CV_DebugSFile* fileId) : ZView(pDoc)
{
	_Lines = 0;
	_pcLine = MAXDWORD;
	_Line = 0;
	_pLI = pDLL->GetSrcLines(fileId, &_nLI);
	_DllBase = pDLL->getBase();
	_BaseAddress = 0;
	_ppView = ppView;
	*ppView = this;
}

ZSrcView::~ZSrcView()
{
	*_ppView = 0;

	if (_pLI)
	{
		delete [] _pLI;
	}

	if (_Lines)
	{
		delete [] _Lines;
	}

	if (_BaseAddress)
	{
		ZwUnmapViewOfSection(NtCurrentProcess(), _BaseAddress);
	}
}

void ZSrcView::GoLine(DWORD nLine)
{
	if (_Line != --nLine)
	{
		DWORD l = _Line;
		_Line = nLine;
		GoTo(getHWND(), 0, nLine ? nLine - 1 : 0);
		InvalidateLine(l);
	}
}

void ZSrcView::setPC(DWORD nLine)
{
	//DbgPrint("setPC(%u,%u,%u)\n", nLine, _Line, _pcLine);
	ULONG pcLine = _pcLine;
	if (!nLine)
	{
		if (pcLine != MAXDWORD)
		{
			_pcLine = MAXDWORD;
			InvalidateLine(pcLine);
		}
		return;
	}
	if (pcLine != --nLine)
	{
		DWORD l = _Line, k = _Line;
		_pcLine = nLine;
		_Line = nLine;
		POINT nPos;
		GetPos(nPos);
		if (nLine - nPos.y < _nLines - 1)
		{
			InvalidateLine(nLine);
		}
		else
		{
			GoTo(getHWND(), 0, nLine ? nLine - 1 : 0);
		}
		InvalidateLine(k);
		if (l != k) InvalidateLine(l);
	}
}

ZWnd* ZSrcView::getWnd()
{
	return this;
}

ZView* ZSrcView::getView()
{
	return this;
}

struct OPEN_SRC
{
	POBJECT_ATTRIBUTES poa;
	ZSrcView** ppView;
	ZDll* pDLL;
	ZDbgDoc* pDoc;
	CV_DebugSFile* fileId;
};

class ZSrcFrame : public ZMDIChildFrame
{
	virtual HWND CreateView(HWND hwnd, int nWidth, int nHeight, PVOID lpCreateParams)
	{
		HWND hwndClient = 0;
		if (ZSrcView* p = new ZSrcView(reinterpret_cast<OPEN_SRC*>(lpCreateParams)->ppView, 
			reinterpret_cast<OPEN_SRC*>(lpCreateParams)->pDoc, 
			reinterpret_cast<OPEN_SRC*>(lpCreateParams)->pDLL, reinterpret_cast<OPEN_SRC*>(lpCreateParams)->fileId))
		{
			if (0 <= p->CreateDoc(reinterpret_cast<OPEN_SRC*>(lpCreateParams)->poa, reinterpret_cast<OPEN_SRC*>(lpCreateParams)->fileId))
			{
				hwndClient = p->ZWnd::Create(0, 0, WS_CHILD|WS_VISIBLE|WS_VSCROLL|WS_HSCROLL, 0, 0, nWidth, nHeight, hwnd, 0, 0);
			}
			p->Release();
		}
		return hwndClient;
	}
};

ZSrcFile::ZSrcFile(CV_DebugSFile* fileId)
{
	_pView = 0;
	_fDontOpen = FALSE;
	_fileId = fileId;
	InitializeListHead(this);
	RtlInitUnicodeString(this, 0);
}

ZSrcFile::~ZSrcFile()
{
	RemoveEntryList(this);
	RtlFreeUnicodeString(this);
}

ULONG FindPrefix(PWSTR wz1, PWSTR wz2)
{
	_wcslwr(wz1), _wcslwr(wz2);

	ULONG len1 = (ULONG)wcslen(wz1), len2 = (ULONG)wcslen(wz2), len;

	PWSTR sz1 = wz1 + len1, psep = 0;
	PWSTR sz2 = wz2 + len2, psep2 = 0;

	if (!(len = min(len1, len2)))
	{
		return 0;
	}

	do 
	{
		WCHAR c = *--sz1;
		if (c != *--sz2)
		{
			if (psep)
			{
				*psep2 = 0;
				return (ULONG)(psep - wz1);
			}
			break;
		}
		if (c == '\\')
		{
			psep = sz1, psep2 = sz2;
		}
	} while (--len);

	return 0;
}

ZSrcView* ZSrcFile::open(ZDbgDoc* pDoc, ZDll* pDLL)
{
	struct PAREP
	{
		USHORT len;
		WCHAR path[];
	};

	C_ASSERT(sizeof(PAREP)==sizeof(USHORT));

	if (_pView)
	{
		return _pView;
	}

	if (_fDontOpen)
	{
		return 0;
	}

	PCSTR name = pDLL->GetFileName(_fileId);
	int len = MultiByteToWideChar(CP_UTF8, 0, name, MAXDWORD, 0, 0);

	if (0 > len)
	{
		_fDontOpen = TRUE;
		return 0;
	}

	PWSTR sz = (PWSTR)alloca(len << 1), path = sz;
	MultiByteToWideChar(CP_UTF8, 0, name, MAXDWORD, sz, len);

	BOOL bQueryRegistry = TRUE;

	OBJECT_ATTRIBUTES oa = { sizeof(oa), 0, this };

__loop:

	if (RtlDosPathNameToNtPathName_U(sz, this, &sz, 0))
	{
		FILE_BASIC_INFORMATION fbi;

		if (0 > ZwQueryAttributesFile(&oa, &fbi))
		{
			UNICODE_STRING ukn;
			WCHAR kn[16];
			swprintf(kn, L"%08x", pDLL->getID());
			RtlInitUnicodeString(&ukn, kn);
			ZRegistry* reg = ZGLOBALS::getRegistry();

			union {
				PVOID buf;
				PKEY_VALUE_PARTIAL_INFORMATION pkvpi;
				PAREP* p;
			};
			NTSTATUS status;

			if (bQueryRegistry)
			{
				bQueryRegistry = FALSE;

				PVOID stack = alloca(guz);

				ULONG cb = 0, rcb = 256;
				do 
				{
					if (cb < rcb)
					{
						cb = RtlPointerToOffset(buf = alloca(rcb - cb), stack);
					}

					if (0 <= (status = reg->GetValue(&ukn, KeyValuePartialInformation, pkvpi, cb, &rcb)))
					{
						PWSTR end;
						if (
							pkvpi->Type == REG_BINARY && 
							(cb = pkvpi->DataLength) && 
							!(cb & 1) && 
							cb < MAXUSHORT &&
							!*(end = (PWSTR)RtlOffsetToPointer(pkvpi->Data, cb - sizeof(WCHAR)))
							)
						{
							p = (PAREP*)pkvpi->Data;
							cb = p->len;
							if (cb < wcslen(path) && path[cb] == '\\')
							{
								wcscpy(end, path + cb);

								RtlFreeUnicodeString(this);
								sz = p->path;
								goto __loop;
							}
						}
					}

				} while (status == STATUS_BUFFER_OVERFLOW);
			}

			ZSrcPathDlg::ZZ px = { path, pDLL->name() };

			if (ZSrcPathDlg* pq = new ZSrcPathDlg)
			{
				INT_PTR r = pq->DoModal((HINSTANCE)&__ImageBase, MAKEINTRESOURCE(IDD_DIALOG16), ZGLOBALS::getMainHWND(), (LPARAM)&px);
				pq->Release();

				if (r != -1)
				{
					if (px.bNotLoad)
					{
						pDLL->set_DontSearchSrc(TRUE);
					}
					else if (px.path)
					{
						BOOL f = FALSE;

						if (ULONG n = FindPrefix(path, px.path))
						{
							f = TRUE;
							ULONG s = ((ULONG)wcslen(px.path) + 1) << 1;
							p = (PAREP*)alloca(sizeof(PAREP) + s);
							wcscpy(p->path, px.path);
							p->len = (USHORT)n;
							reg->SetValue(&ukn, REG_BINARY, p, sizeof(PAREP) + s);
							wcscat(p->path, path + n);
						}
						delete [] px.path;

						if (f)
						{
							RtlFreeUnicodeString(this);
							sz = p->path;
							goto __loop;
						}
					}
				}
			}
			_fDontOpen = TRUE;
			RtlFreeUnicodeString(this);
			return 0;
		}

		if (ZSrcFrame* p = new ZSrcFrame)
		{
			OPEN_SRC os = { &oa, &_pView, pDLL, pDoc, _fileId };
			p->Create(sz, &os);
			if (!_pView)
			{
				_fDontOpen = TRUE;
			}
			p->Release();
		}

		return _pView;
	}

	_fDontOpen = TRUE;
	return 0;
}

_NT_END