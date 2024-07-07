#include "StdAfx.h"

_NT_BEGIN

#include "common.h"
#include "log.h"
#include "DbgPipe.h"

NTSTATUS MySetContextThread ( IN HANDLE ThreadHandle, IN _CONTEXT* Context )
{
	Context->Dr7 |= 0x100;
	Context->Dr6 = 0;
	return ZwSetContextThread(ThreadHandle, Context);
}

NTSTATUS RemoteSetContextThread (CDbgPipe* _pipe, WORD Processor, IN _CONTEXT* Context )
{
	Context->Dr7 |= 0x100;
	Context->Dr6 = 0;
	NTSTATUS status = _pipe->SetContext(Processor, Context);
	if (0 <= status)
	{
		status = _pipe->WriteReadControlSpace(Processor, Context);
	}
	return status;
}

//////////////////////////////////////////////////////////////////////////
// BOL

BOOL ZDbgDoc::isBOL(PCWSTR name)
{
	if (!name)
	{
		return FALSE;
	}

	PLIST_ENTRY head = &_bolListHead, entry = head;

	while ((entry = entry->Flink) != head)
	{
		if (!_wcsicmp(name, static_cast<BOL*>(entry)->_name))
		{
			return TRUE;
		}
	}

	return FALSE;
}

void ZDbgDoc::addBOL(PCWSTR name)
{
	if (!isBOL(name))
	{
		if (BOL* p = new(name) BOL)
		{
			InsertHeadList(&_bolListHead, p);
		}
	}
}

void ZDbgDoc::CreateBOL(PWSTR names)
{
	deleteBOL();
	PWSTR wz = 0;
	for (;;)
	{
		switch (*names++)
		{
		case '\\':
			if (wz)
			{
				names[-1] = 0;
				addBOL(wz);
			}
			wz = names;
			break;
		case 0:
			if (wz)
			{
				addBOL(wz);
			}
			return;
		}
	}
}

STATIC_UNICODE_STRING_(bol);

void ZDbgDoc::LoadBOL()
{
	PVOID stack = alloca(guz);
	DWORD cb = 0, rcb = 1024;
	PKEY_VALUE_PARTIAL_INFORMATION pkvpi = 0;
	NTSTATUS status;
	do 
	{
		if (cb < rcb) cb = RtlPointerToOffset(pkvpi = (PKEY_VALUE_PARTIAL_INFORMATION)alloca(rcb - cb), stack);

		if (0 <= (status = GetValue(&bol, KeyValuePartialInformation, pkvpi, cb, &rcb)))
		{
			cb = pkvpi->DataLength;
			PWSTR sz = (PWSTR)pkvpi->Data;
			if (pkvpi->Type == REG_MULTI_SZ &&
				cb >= 2*sizeof WCHAR &&
				!(cb & 1) &&
				!*(PWSTR)((PUCHAR)sz + cb - sizeof(WCHAR)) &&
				!*(PWSTR)((PUCHAR)sz + cb - 2*sizeof(WCHAR)))
			{
				while (*sz)
				{
					addBOL(sz);
					sz += wcslen(sz) + 1;
				}
			}
		}

	} while (status == STATUS_BUFFER_OVERFLOW);
}

void ZDbgDoc::SetBOLText(HWND hwnd, UINT id)
{
	PLIST_ENTRY head = &_bolListHead, entry = head;

	DWORD len = 0;

	while ((entry = entry->Flink) != head)
	{
		len += (DWORD)wcslen(static_cast<BOL*>(entry)->_name) + 1;
	}

	if (len++)
	{
		PWSTR names = (PWSTR)alloca(len <<= 1), sz = names;

		while ((entry = entry->Flink) != head)
		{
			*sz = '\\';
			sz = xcscpy(sz + 1, static_cast<BOL*>(entry)->_name) - 1;
		}

		SetDlgItemText(hwnd, id, names);
	}
}

void ZDbgDoc::SaveBOL()
{
	PLIST_ENTRY head = &_bolListHead, entry = head;

	DWORD len = 0;

	while ((entry = entry->Flink) != head)
	{
		len += (DWORD)wcslen(static_cast<BOL*>(entry)->_name) + 1;
	}

	if (len++)
	{
		PWSTR names = (PWSTR)alloca(len <<= 1), sz = names;

		while ((entry = entry->Flink) != head)
		{
			sz = xcscpy(sz, static_cast<BOL*>(entry)->_name);
		}

		*sz = 0;
		SetValue(&bol, REG_MULTI_SZ, names, len);
		return;
	}

	ZwDeleteValueKey(_hKey, &bol);
}

void ZDbgDoc::deleteBOL()
{
	PLIST_ENTRY head = &_bolListHead, entry = head->Flink;

	while (entry != head)
	{
		BOL* p = static_cast<BOL*>(entry);

		entry = entry->Flink;

		delete p;
	}

	InitializeListHead(head);
}

//////////////////////////////////////////////////////////////////////////
// ZBreakPoint

void ZDbgDoc::InvalidateVa(INT_PTR Va, ZAsmView::ACTION action)
{
	_pAsm->InvalidateVa(Va, action);
	
	if (ZDll* pDll = getDllByVaNoRef((PVOID)Va))
	{
		CV_DebugSFile* fileId;
		ULONG line = pDll->GetLineByVA((PVOID)Va, &fileId);

		if (line)
		{
			if (ZSrcFile* p = findSrc(fileId))
			{
				if (ZSrcView *pView = p->_pView)
				{
					pView->InvalidateLine(line - 1);
				}
			}
		}
	}
}

ZBreakPoint* ZDbgDoc::getBpByVa(PVOID Va)
{
	PLIST_ENTRY head = &_bpListHead, entry = head;
	while ((entry = entry->Flink) != head)
	{
		ZBreakPoint* pbp = static_cast<ZBreakPoint*>(entry);
		if (pbp->_Va == Va)
		{
			return pbp;
		}
	}
	return 0;
}

void ZDbgDoc::DeleteAllBps()
{
	PLIST_ENTRY head = &_bpListHead, entry = head->Flink;

	while(entry != head)
	{
		ZBreakPoint* pbp = static_cast<ZBreakPoint*>(entry);

		if (pbp->_isActive)
		{
			Write(pbp->_Va, pbp->_opcode);
		}

		InvalidateVa((INT_PTR)pbp->_Va, ZAsmView::BpRemoved);

		entry = entry->Flink;

		delete pbp;
	}

	InitializeListHead(head);
}

BOOL ZDbgDoc::DelBp(PVOID Va)
{
	if (ZBreakPoint* pbp = getBpByVa(Va))
	{
		return DelBp(pbp);
	}

	return FALSE;
}

BOOL ZDbgDoc::DelBp(ZBreakPoint* pbp)
{
	PVOID Va = pbp->_Va;

	RemoveEntryList(pbp);

	if (pbp->_isActive)
	{
		Write(Va, pbp->_opcode);
	}

	delete pbp;

	InvalidateVa((INT_PTR)Va, ZAsmView::BpRemoved);

	UpdateAllViews(_pAsm, BYTE_UPDATED, Va);

	if (_hwndBPs)
	{
		SendMessage(_hwndBPs, ZBPDlg::WM_BPADDDEL, 0, 0);
	}

	return TRUE;
}

BOOL ZDbgDoc::AddBp(PVOID Va)
{
	if (!_IsDebugger || ((INT_PTR)Va <= 0 && !_IsRemoteDebugger)) return FALSE;
	
	UCHAR opcode;

	if (0 > Read(Va, &opcode, sizeof(UCHAR)) || opcode == 0xcc) return FALSE;

	if (ZBreakPoint *pbp = new ZBreakPoint)
	{
		pbp->_Va = Va;
		pbp->_expression = 0;
		pbp->_opcode = opcode;
		pbp->_isActive = TRUE;
		pbp->_isUsed = TRUE;

		if (ZDll* pDll = getDllByVaNoRef(Va))
		{
			pbp->_dllId = pDll->getID();
			pbp->_rva = RtlPointerToOffset(pDll->_BaseOfDll, Va);
		}
		else
		{
			pbp->_dllId = 0;
			pbp->_rva = 0;
		}

		InsertHeadList(&_bpListHead, pbp);

		if (0 <= Write(Va, 0xcc))
		{
			InvalidateVa((INT_PTR)Va, ZAsmView::BpAdded);

			UpdateAllViews(_pAsm, BYTE_UPDATED, Va);

			if (_hwndBPs)
			{
				SendMessage(_hwndBPs, ZBPDlg::WM_BPADDDEL, 0, 0);
			}

			return TRUE;
		}

		RemoveEntryList(pbp);

		delete pbp;

	}

	return FALSE;
}

BOOL ZDbgDoc::ToggleBp(PVOID Va)
{
	return _IsDebugger && (DelBp(Va) || AddBp(Va));
}

void ZDbgDoc::OnLoadUnload(PVOID DllBase, DWORD id, DWORD size, BOOL bLoad)
{
	BOOL f = FALSE;

	PLIST_ENTRY head = &_bpListHead, entry = head;
	while ((entry = entry->Flink) != head)
	{
		ZBreakPoint* pbp = static_cast<ZBreakPoint*>(entry);

		if (pbp->_dllId == id)
		{
			f = TRUE;

			if (bLoad)
			{
				if (!pbp->_Va)
				{
					PVOID Va = RtlOffsetToPointer(DllBase, pbp->_rva);
					pbp->_Va = Va;
					pbp->_isUsed = TRUE;
					if (pbp->_isActive)
					{
						if (0 > Read(Va, &pbp->_opcode, sizeof(UCHAR)) || 0 > Write(Va, 0xcc))
						{
							RemoveEntryList(pbp);
							delete pbp;
						}
					}
				}
			}
			else
			{
				if (RtlPointerToOffset(DllBase, pbp->_Va) < size)
				{
					pbp->_Va = 0;
				}
			}
		}
	}

	if (_hwndBPs && f)
	{
		SendMessage(_hwndBPs, ZBPDlg::WM_BPADDDEL, 0, 0);
	}
}

//BOOL ZDbgDoc::EnableBp(PVOID Va, BOOL bEnable)
//{
//	ZBreakPoint *pbp = getBpByVa(Va);
//
//	return pbp ? EnableBp(pbp, bEnable) : FALSE;
//}

BOOL ZDbgDoc::EnableBp(ZBreakPoint* pbp, BOOL bEnable, BOOL bSendNotify)
{
	BOOL f = FALSE;

	if (PVOID Va = pbp->_Va)
	{
		if (bEnable)
		{
			if (!pbp->_isActive)
			{
				if (0 <= Read(Va, &pbp->_opcode, sizeof(UCHAR)) && 0 <= Write(Va, 0xcc))
				{
					pbp->_isActive = TRUE;
					f = TRUE;
				}
			}
		}
		else
		{
			if (pbp->_isActive)
			{
				if (0 <= Write(Va, pbp->_opcode))
				{
					pbp->_isActive = FALSE;
					f = TRUE;
				}
			}
		}

		if (f)
		{
			InvalidateVa((INT_PTR)Va, bEnable ? ZAsmView::BpActivated : ZAsmView::BpDisabled);

			UpdateAllViews(_pAsm, BYTE_UPDATED, Va);

			if (_hwndBPs && bSendNotify)
			{
				SendMessage(_hwndBPs, ZBPDlg::WM_BPENBDIS, 0, 0);
			}
		}
	}

	return f;
}

void ZDbgDoc::EnableAllBps(BOOL bEnable)
{
	PLIST_ENTRY head = &_bpListHead, entry = head;
	while ((entry = entry->Flink) != head)
	{
		EnableBp(static_cast<ZBreakPoint*>(entry), bEnable, FALSE);
	}
}

void ZDbgDoc::SuspendAllBps(BOOL bSuspend)
{
	PLIST_ENTRY head = &_bpListHead, entry = head;

	while ((entry = entry->Flink) != head)
	{
		ZBreakPoint* pbp = static_cast<ZBreakPoint*>(entry);

		if (PVOID Va = pbp->_Va)
		{
			if (bSuspend)
			{
				if (pbp->_isActive)
				{
					if (0 <= Write(Va, pbp->_opcode))
					{
						pbp->_isActive = FALSE;
						pbp->_isSuspended = TRUE;
					}
				}
			}
			else
			{
				if (pbp->_isSuspended)
				{
					if (0 <= Read(Va, &pbp->_opcode, sizeof(UCHAR)) && 0 <= Write(Va, 0xcc))
					{
						pbp->_isActive = TRUE;
					}
					pbp->_isSuspended = FALSE;
				}
			}
		}
	}
}

BOOL ZDbgDoc::SetBpCondition(PVOID Va, PCWSTR exp, ULONG Len)
{
	if (ZBreakPoint* pbp = getBpByVa(Va))
	{
		if (SetBpCondition(pbp, exp, Len))
		{
			InvalidateVa((INT_PTR)Va, exp ? ZAsmView::BpExpAdded : ZAsmView::BpExpRemoved);

			if (_hwndBPs)
			{
				SendMessage(_hwndBPs, ZBPDlg::WM_BPENBDIS, 0, 0);
			}

			return TRUE;
		}
	}
	
	return FALSE;
}

BOOL ZDbgDoc::SetBpCondition(ZBreakPoint* pbp, PCWSTR exp, ULONG Len)
{
	if (pbp->_expression)
	{
		delete pbp->_expression;
		pbp->_expression = 0;
	}

	if (exp)
	{
		if (pbp->_expression = new WCHAR[Len])
		{
			memcpy(pbp->_expression, exp, Len << 1);
		}
		else
		{
			return FALSE;
		}
	}

	return TRUE;
}

struct BPDATA 
{
	DWORD _dllId, _rva;
	WORD _ofs;
	WORD _len : 15;
	WORD _isActive : 1;
};

STATIC_UNICODE_STRING_(BPS);

void ZDbgDoc::LoadBps()
{
	PVOID stack = alloca(guz);
	DWORD cb = 0, rcb = 1024;
	PKEY_VALUE_PARTIAL_INFORMATION pkvpi = 0;
	NTSTATUS status;
	do 
	{
		if (cb < rcb) cb = RtlPointerToOffset(pkvpi = (PKEY_VALUE_PARTIAL_INFORMATION)alloca(rcb - cb), stack);

		if (0 <= (status = GetValue(&BPS, KeyValuePartialInformation, pkvpi, cb, &rcb)))
		{
			cb = pkvpi->DataLength;
			if (cb >= sizeof(BPDATA) && cb < 0x10000)
			{
				BPDATA* p = (BPDATA*)pkvpi->Data;
				DWORD maxOfs = cb, minOfs = sizeof(BPDATA), firstOfs = sizeof(BPDATA), curOfs = 0;
				do 
				{
					DWORD ofs = p->_ofs;
					BYTE len = 0;
					
					if (ofs)
					{
						if (minOfs == firstOfs)
						{
							if (ofs % sizeof(BPDATA))
							{
								break;
							}
							minOfs = firstOfs = ofs;
						}

						if (ofs != minOfs || ofs >= maxOfs || !(len = p->_len) || (minOfs += len) > maxOfs)
						{
							break;
						}

						cb -= len;
					}
					else
					{
						if (minOfs == firstOfs)
						{
							if (minOfs > maxOfs)
							{
								break;
							}

							minOfs += sizeof(BPDATA), firstOfs += sizeof(BPDATA);
						}
					}
					
					if ((curOfs += sizeof(BPDATA)) > firstOfs)
					{
						break;
					}

					if (ZBreakPoint* pbp = new ZBreakPoint)
					{
						pbp->_Va = 0;
						pbp->_isUsed = FALSE;
						pbp->_isActive = p->_isActive;
						pbp->_dllId = p->_dllId;
						pbp->_rva = p->_rva;
						pbp->_expression = 0;
						pbp->_opcode = 0xcc;

						if (len)
						{
							int t = MultiByteToWideChar(CP_ACP, 0, (PCSTR)pkvpi->Data + ofs, len, 0, 0);
							if (0 < t)
							{
								if (PWSTR sz = new WCHAR[t + 1])
								{
									MultiByteToWideChar(CP_ACP, 0, (PCSTR)pkvpi->Data + ofs, len, sz, t);
									sz[t] = 0;
									pbp->_expression = sz;
								}
							}
						}

						InsertHeadList(&_bpListHead, pbp);
					}

					p++;

				} while (cb -= sizeof(BPDATA));

				if (cb)
				{
					//DbgBreak();
					PLIST_ENTRY head = &_bpListHead, entry = head->Flink;
					ZBreakPoint* pbp;
					while (entry != head)
					{
						pbp = static_cast<ZBreakPoint*>(entry);

						entry = entry->Flink;

						delete pbp;
					}
				
					InitializeListHead(head);
				}
			}
		}

	} while (status == STATUS_BUFFER_OVERFLOW);
}

void ZDbgDoc::SaveBps()
{
	BOOL IsTerminated = _IsTerminated;

	DWORD ex = 0, cb = 0, ofs, len, f;
	PLIST_ENTRY head = &_bpListHead, entry = head;
	ZBreakPoint* pbp;
	while ((entry = entry->Flink) != head)
	{
		pbp = static_cast<ZBreakPoint*>(entry);

		if (pbp->_isUsed && pbp->_dllId)
		{
			cb += sizeof(BPDATA);
			if (pbp->_expression)
			{
				len = WideCharToMultiByte(CP_ACP, 0, pbp->_expression, MAXDWORD, 0, 0, 0, 0) - 1;
				if (len < MAXSHORT)
				{
					ex += len;
				}
				else
				{
					delete pbp->_expression;
					pbp->_expression = 0;
				}
			}
		}

		if (!IsTerminated && pbp->_isActive && pbp->_Va)
		{
			Write(pbp->_Va, pbp->_opcode);
		}
	}

	if (cb + ex < MAXUSHORT)
	{
		PVOID buf = alloca(cb + ex + 1);
		BPDATA* p = (BPDATA*)buf;
		PSTR sz = RtlOffsetToPointer(buf, cb);
		f = ex + 1;

		head = &_bpListHead, entry = head, ofs = cb;

		while ((entry = entry->Flink) != head)
		{
			pbp = static_cast<ZBreakPoint*>(entry);

			if (pbp->_isUsed && pbp->_dllId)
			{
				p->_isActive = pbp->_isActive;
				p->_dllId = pbp->_dllId;
				p->_rva = pbp->_rva;
				p->_ofs = 0;
				
				if (PCWSTR exp = pbp->_expression)
				{
					p->_ofs = (WORD)ofs;
					len = WideCharToMultiByte(CP_ACP, 0, exp, MAXDWORD, sz, f, 0, 0) - 1;
					p->_len = (USHORT)len;
					sz += len, ofs += len, f -= len;
				}

				p++;
			}
		}

		SetValue(&BPS, REG_BINARY, buf, cb + ex);
	}
	else
	{
		ZwDeleteValueKey(_hKey, &BPS);
	}

	head = &_bpListHead, entry = head->Flink;

	while(entry != head)
	{
		pbp = static_cast<ZBreakPoint*>(entry);

		entry = entry->Flink;

		delete pbp;
	}

	InitializeListHead(head);
}

BOOLEAN ZDbgDoc::IsCurrentThread(ULONG dwThreadId) 
{ 
	return _IsWaitContinue && _pThread && _pThread->getID() == dwThreadId; 
}

ZDbgDoc* ZDbgDoc::find(DWORD dwProcessId)
{
	PLIST_ENTRY head = &ZGLOBALS::get()->_docListHead, entry = head;

	while ((entry = entry->Flink) != head)
	{
		ZDocument* pDoc = static_cast<ZDocument*>(entry);

		ZDbgDoc* pDbg;

		if (!pDoc->QI(IID_PPV(pDbg)))
		{
			if (pDbg->_dwProcessId == dwProcessId)
			{
				return pDbg;
			}
			pDbg->Release();
		}
	}

	return 0;
}

void ZDbgDoc::StopTrace(ZTraceView* pTraceView)
{
	if (_IsInTrace && pTraceView == _pTraceView)
	{
		_IsInTrace = FALSE;
		_pTraceView->SetStatus();
		_pTraceView->Release();
		_pTraceView = 0;
		SuspendAllBps(FALSE);
	}
}

void _cprintf(HWND hwnd, PCWSTR buf)
{
	SendMessage(hwnd, EM_SETSEL, MAXLONG, MAXLONG);
	SendMessage(hwnd, EM_REPLACESEL, 0, (LPARAM)buf);	
}

void ZDbgDoc::cprintf(PR pr, PCWSTR buf)
{
	if (_bittest(&g_printMask, pr) && _hwndLog)
	{
		_cprintf(_hwndLog, buf);	
	}
}

void ZDbgDoc::vprintf(PR pr, PCWSTR format, va_list args)
{
	if (_bittest(&g_printMask, pr) && _hwndLog)
	{
		WCHAR sz[1024];
		_vsnwprintf(sz, RTL_NUMBER_OF(sz) - 1, format, args);
		sz[RTL_NUMBER_OF(sz) - 1] = 0;

		_cprintf(_hwndLog, sz);	
	}
}	

void ZDbgDoc::OnDbgPrint(SIZE_T cch, PVOID pv, BOOL bWideChar)
{
	if (!cch || !_bittest(&g_printMask, prDbgPrint)) return;

	if (cch > MAXUSHORT)
	{
		cch = MAXUSHORT;
	}

	SIZE_T Length, cb;

	if (bWideChar)
	{
		Length = cb = cch * sizeof(WCHAR);
	}
	else
	{
		Length = cch;
		cb = cch * (sizeof(WCHAR) + 1);
	}

	union {
		PVOID buf;
		PSTR psz;
		PWSTR pwz;
	};

	buf = alloca(cb + sizeof(WCHAR));

	if (!bWideChar)
	{
		pwz += cch;
	}

	switch (ZwReadVirtualMemory(_hProcess, pv, buf, Length, &Length))
	{
	case STATUS_SUCCESS:
	case STATUS_PARTIAL_COPY:
		if (Length)
		{
			break;
		}
	default: return;
	}

	ULONG cchWideChar;
	PWSTR wz;

	if (bWideChar)
	{
		cch = Length / sizeof(WCHAR);
	}
	else
	{
		if (cchWideChar = MultiByteToWideChar(CP_ACP, 0, psz, (ULONG)Length, pwz - cch, (ULONG)cch))
		{
			pwz -= cch;
		}
		else
		{
			if (GetLastError() != ERROR_INSUFFICIENT_BUFFER ||
				(cchWideChar = MultiByteToWideChar(CP_ACP, 0, psz, (ULONG)Length, 0, 0)) <= cch)
			{
				return;
			}

			wz = (PWSTR)alloca((cchWideChar - cch)*sizeof(WCHAR));

			if (cchWideChar != (ULONG)MultiByteToWideChar(CP_ACP, 0, psz, (ULONG)Length, wz, cchWideChar))
			{
				return;
			}

			pwz = wz;
		}

		cch = cchWideChar;
	}

	Length = cch, wz = pwz;

	ULONG len = (ULONG)Length;

	WCHAR c;
	BOOL wR = FALSE;
	do 
	{
		switch (c = *wz++)
		{
		case '\r':
			len++;
			wR = TRUE;
			continue;
		case 0:
			if (Length > 1) len += 2;
			break;
		case '\n':
			wR ? len-- : ++len;
			break;
		}

		wR = FALSE;

	} while (--Length);

	if (c)
	{
		len++;
	}

	if (len > cch)
	{
		Length = cch;
		PWSTR _wz = wz = (PWSTR)alloca(len * sizeof(WCHAR));

		wR = FALSE;
		do 
		{
			switch (c = *pwz++)
			{
			case '\r':
				*wz++ = '\r', *wz++ = '\n';
				wR = TRUE;
				continue;
			case '\n':
				if (!wR)
				{
					*wz++ = '\r', *wz++ = '\n';
				}
				break;
			case 0:
				if (Length > 1) 
				{
					*wz++ = '\r', *wz++ = '\n';
					break;
				}
			default:
				*wz++ = c;
			}

			wR = FALSE;
		} while (--Length);

		if (c)
		{
			*wz = 0;
		}

		pwz = _wz;
	}

	_cprintf(_hwndLog, pwz);
}

//////////////////////////////////////////////////////////////////////////
// notify

void ZDbgDoc::AddNotify(ZDetachNotify* p)
{
	InsertHeadList(&_notifyListHead, p);
}

void ZDbgDoc::RemoveNotify(ZDetachNotify* p)
{
	RemoveEntryList(p);
}

void ZDbgDoc::FireNotify()
{
	PLIST_ENTRY head = &_notifyListHead, entry = head->Flink;

	while (entry != head)
	{
		ZDetachNotify* p = static_cast<ZDetachNotify*>(entry);
		entry = entry->Flink;
		p->OnDetach();
	}
}

ZDbgDoc::ZDbgDoc(BOOL IsDebugger)
{
	DbgPrint("++ZDbgDoc<%p>(%p)\n", this);

	_pFC = static_cast<ZMyApp*>(ZGLOBALS::getApp());
	_hProcess = 0;
	_pThread = 0;
	_Flags = 0;
	_pAsm = 0;
	_pReg = 0;
	_hwndLog = 0;
	_pDbgTH = 0;
	_pTraceView = 0;
	_hwndDLLs = 0;
	_hwndBPs = 0;
	_nDllCount = 0;
	_nLoadIndex = 0;
	_hKey = 0;
	_SuspendCount = 0;
	_NtSymbolPath = 0;
	_IsDebugger = IsDebugger;
	_pRemoteData = 0;
	_KernelStackOffset = 0;
	InitializeListHead(&_dllListHead);
	InitializeListHead(&_threadListHead);
	InitializeListHead(&_bpListHead);
	InitializeListHead(&_notifyListHead);
	InitializeListHead(&_bolListHead);
	InitializeListHead(&_srcListHead);
}

ZDbgDoc::~ZDbgDoc()
{
	if (_hProcess)
	{
		__debugbreak();
	}
	DbgPrint("--ZDbgDoc<%p>(%p)\n", this, _dwProcessId);
}

//////////////////////////////////////////////////////////////////////////
// ZSrcFile

void ZDbgDoc::RemoveSrcPC()
{
	PLIST_ENTRY head = &_srcListHead, entry = head;

	while ((entry = entry->Flink) != head)
	{
		if (ZSrcView* pView = static_cast<ZSrcFile*>(entry)->_pView)
		{
			pView->setPC(0);
		}
	}
}

ZSrcFile* ZDbgDoc::findSrc(ZView* pView)
{
	PLIST_ENTRY head = &_srcListHead, entry = head;

	while ((entry = entry->Flink) != head)
	{
		if (static_cast<ZSrcFile*>(entry)->_pView == pView) return static_cast<ZSrcFile*>(entry);
	}

	return 0;
}

ZSrcFile* ZDbgDoc::findSrc(CV_DebugSFile* fileId)
{
	PLIST_ENTRY head = &_srcListHead, entry = head;

	while ((entry = entry->Flink) != head)
	{
		if (static_cast<ZSrcFile*>(entry)->_fileId == fileId) return static_cast<ZSrcFile*>(entry);
	}

	return 0;
}

ZSrcFile* ZDbgDoc::openSrc(CV_DebugSFile* fileId)
{
	if (ZSrcFile* p = new ZSrcFile(fileId))
	{
		InsertHeadList(&_srcListHead, p);
		return p;
	}

	return 0;
}

void ZDbgDoc::Rundown()
{
	AddRef();
	Detach();
	Release();
}

void ZDbgDoc::UpdateThreads(PSYSTEM_PROCESS_INFORMATION pspi)
{
	ULONG NextEntryOffset = 0;

	do 
	{
		(PBYTE&)pspi += NextEntryOffset;

		if (pspi->UniqueProcessId == (HANDLE)(ULONG_PTR)_dwProcessId)
		{
			ZDbgThread * pThread;
			PLIST_ENTRY head = &_threadListHead, entry = head;

			while ((entry = entry->Flink) != head)
			{
				static_cast<ZDbgThread *>(entry)->_state = ZDbgThread::stF4;
			}

			if (ULONG NumberOfThreads = pspi->NumberOfThreads)
			{
				PSYSTEM_EXTENDED_THREAD_INFORMATION TH = pspi->TH;
				do 
				{
					entry = head;
					ULONG dwThreadId = (ULONG)(ULONG_PTR)TH->ClientId.UniqueThread;

					while ((entry = entry->Flink) != head)
					{
						pThread = static_cast<ZDbgThread *>(entry);

						if (pThread->_dwThreadId == dwThreadId)
						{
							pThread->_state = ZDbgThread::stF5;
							break;
						}
					}

					if (entry == head)
					{
						HANDLE hThread;
						if (0 <= MyOpenThread(&hThread, THREAD_ALL_ACCESS, &zoa, &TH->ClientId))
						{
							if (pThread = new ZDbgThread(dwThreadId, hThread, TH->TebAddress))
							{
								InsertHeadList(&_threadListHead, pThread);
								PVOID Win32StartAddress = TH->Win32StartAddress;
								if (!Win32StartAddress)
								{
									Win32StartAddress = TH->StartAddress;
								}
								pThread->_StartAddress = Win32StartAddress;

								WCHAR buf[0x100];
								FormatNameForAddress(Win32StartAddress, buf, _countof(buf));


								_pDbgTH->AddThread(dwThreadId, TH->TebAddress, Win32StartAddress, buf);
							}
							else
							{
								NtClose(hThread);
							}
						}
					}

				} while (TH++, --NumberOfThreads);
			}

			entry = head->Flink;

			while (entry != head)
			{
				pThread = static_cast<ZDbgThread *>(entry);
				entry = entry->Flink;
				if (pThread->_state == ZDbgThread::stF4)
				{
					_pDbgTH->DelThread(pThread->_dwThreadId);
					delete pThread;
				}
			}

			return;
		}
	} while (NextEntryOffset = pspi->NextEntryOffset);
}

void ZDbgDoc::DestroyDbgTH()
{
	if (_pDbgTH)
	{
		DestroyWindow(_pDbgTH->ZWnd::getHWND());
		_pDbgTH->Release();
		_pDbgTH = 0;
	}
}

void ZDbgDoc::Detach()
{
	_IsDetachCalled = TRUE;

	if (_IsDebugger) 
	{
		SaveBOL();
		deleteBOL();
		SaveBps();

		if (!_IsTerminated && !_IsRemoteDebugger) 
		{
			if (_IsWaitContinue)
			{
				ZDbgThread* pThread = _pThread;
				CONTEXT ctx = *_pReg;
				ctx.Dr0 = 0;
				ctx.Dr1 = 0;
				ctx.Dr2 = 0;
				ctx.Dr3 = 0;
				ctx.Dr7 = 0x400;
				MySetContextThread(pThread->_hThread, &ctx);

				CLIENT_ID cid = { (HANDLE)_dwProcessId, (HANDLE)pThread->_dwThreadId };

				DbgUiContinue(&cid, DBG_CONTINUE);

				_IsWaitContinue = 0;
			}

			DoIoControl(IOCTL_SetProtectedProcess);
			NTSTATUS status = DbgUiStopDebugging(_hProcess);
			DoIoControl(IOCTL_DelProtectedProcess);

			if (0 > status)
			{
				return ;
			}

			_IsTerminated = 1;
		}

		_IsDebugger = 0;
	}

	DestroyDbgTH();

	if (_IsInserted)
	{
		ZGLOBALS::getApp()->delWaitObject(this);
		_IsInserted = FALSE;
	}

	if (_hwndDLLs)
	{
		DestroyWindow(_hwndDLLs);
		_hwndDLLs = 0;
	}

	if (_hwndBPs)
	{
		DestroyWindow(_hwndBPs);
		_hwndBPs = 0;
	}

	PLIST_ENTRY head = &_dllListHead, entry = head->Flink;

	while (entry != head)
	{
		ZDll* p = static_cast<ZDll*>(entry);
		entry = entry->Flink;
		p->Unload();
		p->Release();
	}

	InitializeListHead(head);

	head = &_threadListHead, entry = head->Flink;

	while (entry != head)
	{
		ZDbgThread* p = static_cast<ZDbgThread*>(entry);
		entry = entry->Flink;
		delete p;
	}

	InitializeListHead(head);

	if (_pReg)
	{
		DestroyWindow(GetParent(_pReg->getHWND()));
		_pReg->Release();
		_pReg = 0;
	}

	if (_hwndLog)
	{
		cprintf(prGen, L"----- Detach -----");
		_hwndLog = 0;
	}

	FireNotify();

	DestroyAllViews();

	head = &_srcListHead, entry = head->Flink;

	while (entry != head)
	{
		ZSrcFile* p = static_cast<ZSrcFile*>(entry);
		entry = entry->Flink;
		delete p;
	}

	InitializeListHead(head);

	if (_hKey)
	{
		NtClose(_hKey);
		_hKey = 0;
	}

	if (_IsDump)
	{
		if (_pDump)
		{
			_pDump->Release();
			_pDump = 0;
		}

		if (_NtSymbolPath)
		{
			delete [] _NtSymbolPath;
			_NtSymbolPath = 0;
		}

		if (_pUdtCtx)
		{
			DeletePrivateUDTContext(_pUdtCtx);
			_pUdtCtx = 0;
		}

		DestroyDbgTH();
	}
	else if (_IsLocalMemory)
	{
		if (_hProcess)
		{
			NtClose(_hProcess);
			_hProcess = 0;
		}
	}
	else if (_IsRemoteDebugger)
	{
		if (_pipe)
		{
			_pipe->Disconnect();
			_pipe->Release();
			_pipe = 0;
		}

		if (_pUdtCtx)
		{
			DeletePrivateUDTContext(_pUdtCtx);
			_pUdtCtx = 0;
		}

		if (_pRemoteData)
		{
			delete _pRemoteData;
			_pRemoteData = 0;
		}
	}

	ZMemoryCache::Cleanup();
}

//////////////////////////////////////////////////////////////////////////
// ZDbgThread

ZDbgThread* ZDbgDoc::getThreadById(DWORD dwThreadId)
{
	if (_pThread)
	{
		if (_pThread->_dwThreadId == dwThreadId)
		{
			return _pThread;
		}
	}

	PLIST_ENTRY head = &_threadListHead, entry = head;

	while ((entry = entry->Flink) != head)
	{
		ZDbgThread * pThread = static_cast<ZDbgThread *>(entry);

		if (pThread->_dwThreadId == dwThreadId)
		{
			return pThread;
		}
	}

	return 0;
}

void ZDbgDoc::FormatNameForAddress(ZDll* pDll, PVOID Address, PWSTR buf, ULONG cch, BOOL bReparse /*= FALSE*/)
{
	char sz[256];
	PCSTR Name = 0;
	INT_PTR NameVa = 0;
	int d = 0;
	PCSTR name;
	if (pDll->_IsParsed && ( name = pDll->getNameByVa2(Address, &NameVa)))
	{
		if (IS_INTRESOURCE(name))
		{
			CHAR oname[16];
			sprintf_s(oname, _countof(oname), "#%u", (ULONG)(ULONG_PTR)name);
			name = oname;
		}

		Name = unDNameEx(sz, (PCSTR)name, RTL_NUMBER_OF(sz), UNDNAME_DEFAULT);

		WCHAR szdisp[32];

		if (d = RtlPointerToOffset(NameVa, Address))
		{
			swprintf_s(szdisp, _countof(szdisp), L"+%x", d);
		}
		else
		{
			*szdisp = 0;
		}

		if (!bReparse) printf(prThread, L"\t%s!%S%s\r\n", pDll->name(), Name, szdisp);

		if (buf)
		{
			swprintf_s(buf, cch, L"%s!%S%s", pDll->name(), Name, szdisp);
		}
	}
	else
	{
		if (!bReparse) printf(prThread, L"\t%s+%x\r\n", pDll->name(), RtlPointerToOffset(pDll->getBase(), Address));

		if (buf)
		{
			swprintf_s(buf, cch, L"%s+%x", pDll->name(), RtlPointerToOffset(pDll->getBase(), Address));
		}
	}
}

void ZDbgDoc::FormatNameForAddress(PVOID Address, PWSTR buf, ULONG cch)
{
	if (buf)
	{
		*buf = 0;
	}
	if (ZDll* pDll = getDllByVaNoRef(Address, FALSE))
	{
		FormatNameForAddress(pDll, Address, buf, cch);
	}
}

void ZDbgDoc::OnDllParsed(ZDll* pDll)
{
	PLIST_ENTRY head = &_threadListHead, entry = head;

	PVOID _StartAddress = 0;
	while ((entry = entry->Flink) != head)
	{
		ZDbgThread* pThread = static_cast<ZDbgThread*>(entry);

		PVOID StartAddress = pThread->_StartAddress;

		if (pDll->VaInImage(StartAddress))
		{
			WCHAR buf[0x100];
			if (_StartAddress != StartAddress)
			{
				*buf = 0;
				_StartAddress = StartAddress;
				FormatNameForAddress(pDll, StartAddress, buf, _countof(buf), TRUE);
			}
			_pDbgTH->UpdateThread(pThread->_dwThreadId, buf);
		}
	}
}

void ZDbgDoc::OnCreateThread(DWORD dwThreadId, PDBGUI_CREATE_THREAD CreateThreadInfo)
{
	THREAD_BASIC_INFORMATION tbi;

	HANDLE HandleToThread = CreateThreadInfo->HandleToThread;

	if (0 > ZwQueryInformationThread(HandleToThread, ThreadBasicInformation, &tbi, sizeof(tbi), 0))
	{
		tbi.TebBaseAddress = 0;
	}

	if (!CreateThreadInfo->NewThread.StartAddress)
	{
		ZwQueryInformationThread(HandleToThread, ThreadQuerySetWin32StartAddress, 
			&CreateThreadInfo->NewThread.StartAddress, sizeof(CreateThreadInfo->NewThread.StartAddress), 0);
	}

	printf(prThread, L"create thread %x at %p, teb=%p\r\n", dwThreadId, CreateThreadInfo->NewThread.StartAddress, tbi.TebBaseAddress);

	WCHAR buf[0x100];
	FormatNameForAddress(CreateThreadInfo->NewThread.StartAddress, buf, _countof(buf));

	_pDbgTH->AddThread(dwThreadId, tbi.TebBaseAddress, CreateThreadInfo->NewThread.StartAddress, buf);

	if (ZDbgThread* pThread = new ZDbgThread(dwThreadId, HandleToThread, tbi.TebBaseAddress))
	{
		pThread->_StartAddress = CreateThreadInfo->NewThread.StartAddress;

		InsertHeadList(&_threadListHead, pThread);

		if (_SuspendCount)
		{
			DbgPrint("%x>started suspended!\n", dwThreadId);

			if (0 <= ZwSuspendThread(HandleToThread, 0))
			{
				_bittestandset(&pThread->_flags, ZDbgThread::stSuspended);
			}
		}
	}
	else
	{
		NtClose(HandleToThread);
	}
}

void ZDbgDoc::OnExitThread(DWORD dwThreadId, DWORD dwExitCode)
{
	printf(prThread, L"thread %x exit with code 0x%x(%d)\r\n", dwThreadId, dwExitCode, dwExitCode);
	
	_pDbgTH->DelThread(dwThreadId);

	if (ZDbgThread* pThread = getThreadById(dwThreadId))
	{
		delete pThread;

		if (_pThread == pThread)
		{
			_pThread = 0;
		}
	}
}

void ZDbgDoc::SetDbgFlags()
{
	SetDbgFlags(ZGLOBALS::getMainFrame());
}

void ZDbgDoc::SetDbgFlags(ZSDIFrameWnd* pFrame)
{
	pFrame->ZToolBar::IndeterminateCmd(ID_DBGFLAGS, _IsDbgInherit == 0);
}

BOOL ZDbgDoc::OnCreateProcess(DWORD dwProcessId, DWORD dwThreadId, PDBGUI_CREATE_PROCESS CreateProcessInfo)
{
	_IsLocalMemory = TRUE;

	NtClose(CreateProcessInfo->HandleToProcess);

	DBGUI_CREATE_THREAD CreateThreadInfo = { CreateProcessInfo->HandleToThread, CreateProcessInfo->NewProcess.InitialThread };
	
	DBGKM_LOAD_DLL LoadDll = { 
		CreateProcessInfo->NewProcess.FileHandle, 
		CreateProcessInfo->NewProcess.BaseOfImage,
		CreateProcessInfo->NewProcess.DebugInfoFileOffset,
		CreateProcessInfo->NewProcess.DebugInfoSize
	};

	_dwProcessId = dwProcessId;

	CLIENT_ID cid = { (HANDLE)dwProcessId };

	if (0 > MyOpenProcess(&_hProcess, PROCESS_ALL_ACCESS_XP, &zoa, &cid))
	{
__fail:
		NtClose(CreateThreadInfo.HandleToThread);
		NtClose(LoadDll.FileHandle);
		return FALSE;
	}

#ifdef _WIN64
	if (0 > ZwQueryInformationProcess(_hProcess, ProcessWow64Information, &_wowPeb, sizeof(_wowPeb), 0))
	{
		goto __fail;
	}
	_IsWow64Process = _wowPeb != 0;
#endif

	BOOLEAN bInherit;
	IsDebugInherit(_hProcess, bInherit);
	_IsDbgInherit = bInherit != 0;
	SetDbgFlags();

	if (!Create())
	{
		goto __fail;
	}

	OnLoadDll(dwThreadId, &LoadDll, TRUE, 0);
	OnCreateThread(dwThreadId, &CreateThreadInfo);

	if (CreateThreadInfo.NewThread.StartAddress)
	{
		WCHAR sz[32];
		swprintf(sz, L"%p; EntryPoint", CreateThreadInfo.NewThread.StartAddress);
		_pAsm->InsertString(sz);
	}

	static PVOID LdrInitializeThunk;
	STATIC_ANSI_STRING(aLdrInitializeThunk, "LdrInitializeThunk");
	if (!LdrInitializeThunk)
	{
		LdrGetProcedureAddress(GetModuleHandle(L"ntdll"), &aLdrInitializeThunk, 0, &LdrInitializeThunk);
	}

	if (LdrInitializeThunk)
	{
		if (ZDbgThread* pThread = getThreadById(dwThreadId))
		{
			CONTEXT ctx = {};
			ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
			ctx.Dr7 = 0x401;
			ctx.Dr0 = (ULONG_PTR)LdrInitializeThunk;
			if (0 <= MySetContextThread(CreateThreadInfo.HandleToThread, &ctx))
			{
				pThread->_Dr[0] = (ULONG_PTR)LdrInitializeThunk;
			}
		}
	}

	return TRUE;
}

void ZDbgDoc::OnExitProcess(DWORD dwExitCode)
{
	printf(prGen, L"process exit with code 0x%x(%d)\r\n", dwExitCode, dwExitCode);

	_IsTerminated = TRUE;

	DbgUiStopDebugging(_hProcess);
	Rundown();
}

void ZDbgDoc::OnRemoteEnd()
{
	printf(prGen, L"remote disconnected\r\n");

	_IsTerminated = TRUE;

	Rundown();
}

BOOL ZDbgDoc::IsRemoteWait()
{ 
	return _pipe && _pipe->CanSend(); 
}

void ZDbgDoc::Break()
{
	if (_IsRemoteDebugger)
	{
		if (!IsRemoteWait()) _pipe->SendBreakIn();
	}
}

PVOID ZDbgDoc::getUdtCtxEx()
{
	if (_IsLocalMemory)
	{
		return 0;
	}

	if (!_IsUdtTry)
	{
		_IsUdtTry = 1;

		if (_pRemoteData)
		{
			if (GLOBALS_EX* globals = static_cast<GLOBALS_EX*>(ZGLOBALS::get()))
			{
				CreatePrivateUDTContext(this, globals->_NtSymbolPath, _pRemoteData->KernBase, &_pUdtCtx);
			}
		}
	}

	return _pUdtCtx;
}
//////////////////////////////////////////////////////////////////////////
//

PCWSTR ZDbgDoc::getNameByID(DWORD id)
{
	PLIST_ENTRY head = &_dllListHead, entry = head;

	while ((entry = entry->Flink) != head)
	{
		ZDll* pDll = static_cast<ZDll*>(entry);

		if (pDll->getID() == id)
		{
			return pDll->_ImageName;
		}
	}

	return 0;
}

ZDll* ZDbgDoc::getDllByVaNoRef(PVOID Va, BOOL bParse /*= TRUE*/)
{
	PLIST_ENTRY head = &_dllListHead, entry = head;

	while ((entry = entry->Flink) != head)
	{
		ZDll* pDll = static_cast<ZDll*>(entry);

		if (pDll->VaInImage(Va))
		{
			if (bParse) pDll->Parse(this);
			return pDll;
		}
	}

	return 0;
}

PVOID ZDbgDoc::getVaByName(PCSTR name)
{
	PCSTR sz;
	ZDll* pDll;
	PLIST_ENTRY head = &_dllListHead, entry = head;

	if (!_strnicmp(name, "api-", 4))
	{
		if (!(sz = strrchr(name, '.')))
		{
			return 0;
		}
		ULONG Ordinal = 0;
		ANSI_STRING aname, *pname = 0;

		if (*++sz == '#')
		{
			if (!(Ordinal = strtoul(sz + 1, (char**)&sz, 10)) || *sz)
			{
				return 0;
			}
		}
		else
		{
			RtlInitString(pname = &aname, sz);
		}

		ULONG cb, len = RtlPointerToOffset(name, sz);
		
		if (0 <= RtlMultiByteToUnicodeSize(&cb, name, len))
		{
			UNICODE_STRING us;
			if (0 <= RtlMultiByteToUnicodeN(us.Buffer = (PWSTR)alloca(cb), us.MaximumLength = (USHORT)cb, &cb, name, len))
			{
				us.Length = (USHORT)cb;

				HMODULE hmod;
				ULONG options = LDR_DONT_RESOLVE_DLL_REFERENCES;

				if (0 <= LdrLoadDll(0, &options, &us, &hmod))
				{
					PVOID pv;
					_LDR_DATA_TABLE_ENTRY* ldte;

					NTSTATUS status = LdrGetProcedureAddress(hmod, pname, Ordinal, &pv);

					if (0 <= status)
					{
						if (0 <= (status = LdrFindEntryForAddress(pv, &ldte)))
						{
							status = STATUS_NOT_FOUND;

							while ((entry = entry->Flink) != head)
							{
								pDll = static_cast<ZDll*>(entry);

								if (PWSTR ImageName = pDll->_ImageName)
								{
									RtlInitUnicodeString(&us, ImageName);

									if (RtlEqualUnicodeString(&us, &ldte->BaseDllName, TRUE))
									{
										status = 0;
										pv = RtlOffsetToPointer(pDll->getBase(), RtlPointerToOffset(ldte->DllBase, pv));
									}
								}
							}
						}
					}

					LdrUnloadDll(hmod);

					return 0 > status ? 0 : pv;
				}
			}
		}

		return 0;
	}

	while ((entry = entry->Flink) != head)
	{
		pDll = static_cast<ZDll*>(entry);

		if (PWSTR ImageName = pDll->_ImageName)
		{
			sz = name;
__loop:
			int c = tolower(*sz++);

			if (!c || c != towlower(*ImageName++))
			{
				continue;
			}

			if (c != '.')
			{
				goto __loop;
			}

			pDll->Parse(this);

			return pDll->getVaByName(sz, this);
		}
	}

	return 0;
}

ZDll* ZDbgDoc::getDllByPathNoRef(PCWSTR path)
{
	ZDll* pDll;
	PLIST_ENTRY head = &_dllListHead, entry = head;
	while ((entry = entry->Flink) != head)
	{
		pDll = static_cast<ZDll*>(entry);
		if (!_wcsicmp(path, pDll->_ImagePath))
		{
			return pDll;
		}
	}

	if (path = wcsrchr(path, OBJ_NAME_PATH_SEPARATOR))
	{
		path++;
		while ((entry = entry->Flink) != head)
		{
			pDll = static_cast<ZDll*>(entry);
			if (!_wcsicmp(path, pDll->_ImageName))
			{
				return pDll;
			}
		}
	}
	return 0;
}

BOOL ZDbgDoc::getDllByVa(ZDll** ppDll, PVOID Va)
{
	if (ZDll* pDll = getDllByVaNoRef(Va))
	{
		pDll->AddRef();
		*ppDll = pDll;
		return TRUE;
	}

	*ppDll = 0;
	return FALSE;
}

PCSTR ZDbgDoc::getNameByVa(PVOID Va)
{
	if (ZDll* pDll = getDllByVaNoRef(Va))
	{
		return pDll->getNameByVa(Va, 0);
	}

	return 0;
}

BOOL ZDbgDoc::GoTo(PVOID Va, BOOL bGotoSrc)
{
	if (_pAsm)
	{
		if (_pAsm->GoTo(_pAsm->getHWND(), (ULONG_PTR)Va))
		{
			if (bGotoSrc) bGotoSrc = _pAsm->GotoSrc((ULONG_PTR)Va);
			if (!bGotoSrc) _pAsm->Activate();
			return TRUE;
		}
	}
	return FALSE;
}

ZDll* ZDbgDoc::getDllByBaseNoRefNoParse(PVOID lpBaseOfDll)
{
	PLIST_ENTRY head = &_dllListHead, entry = head;

	while ((entry = entry->Flink) != head)
	{
		ZDll* pDll = static_cast<ZDll*>(entry);

		if (pDll->_BaseOfDll == lpBaseOfDll)
		{
			return pDll;
		}
	}

	return 0;
}

void ZDbgDoc::OnUnloadDll(PVOID lpBaseOfDll)
{
	if (ZDll* pDll = getDllByBaseNoRefNoParse(lpBaseOfDll))
	{
		_nDllCount--;
		printf(prDLL, L"Unload: base=%p %s\r\n", lpBaseOfDll, pDll->_ImagePath);
		pDll->Unload();
		OnLoadUnload(lpBaseOfDll, pDll->getID(), pDll->getSize(), FALSE);
		_pAsm->OnUnloadDll(pDll);
		UpdateAllViews(0, DLL_UNLOADED, lpBaseOfDll);
		pDll->Release();
		return ;
	}

	printf(prGen, L"Unload: base=%p ?!?\r\n", lpBaseOfDll);
}

STATIC_UNICODE_STRING_(BreakOnLoad);

BOOL ZDbgDoc::Load(PDBGKM_LOAD_DLL LoadDll, BOOL bExe)
{
	BOOL bBreak = FALSE;

	PWSTR lpImageName = (PWSTR)LoadDll->NamePointer;

	PVOID EntryPoint = 0;

	if (bExe && lpImageName)
	{
		UNICODE_STRING us;

		if (us.Buffer = wcsrchr(lpImageName, '\\'))
		{
			us.Buffer++;
		}
		else
		{
			us.Buffer = lpImageName;
		}

		RtlInitUnicodeString(&us, us.Buffer);

		ZGLOBALS::getRegistry()->Create(&_hKey, &us);

		if (_IsDebugger) 
		{
			LoadBps();
			LoadBOL();
		}
	}

	ZDll* p;
	PVOID BaseOfDll = LoadDll->BaseOfDll;

	if (p = getDllByBaseNoRefNoParse(BaseOfDll))
	{
		p->Load(this, LoadDll);
		if (bBreak = isBOL(p->name()))
		{
			ZBreakDll::create(EntryPoint, this, p->path());
		}
	}
	else if (p = new ZDll(++_nLoadIndex))
	{
		if (p->Load(this, LoadDll))
		{
			EntryPoint = p->_EntryPoint;

			printf(prDLL, L"Load: base=%p, size=%08x, ep=%p %s\r\n", BaseOfDll, p->_SizeOfImage, EntryPoint, lpImageName);

			p->AddRef();

			_nDllCount++;
			InsertTailList(&_dllListHead, p);

			OnLoadUnload(BaseOfDll, p->getID(), p->_SizeOfImage, TRUE);
		}
		else
		{
			printf(prGen, L"Load: base=%p, error parsing %s\r\n", BaseOfDll, lpImageName);
		}

		if (bBreak = isBOL(p->name()))
		{
			ZBreakDll::create(EntryPoint, this, p->path());
		}

		p->Release();
	}

	return bBreak;
}

NTSTATUS ZDbgDoc::OnLoadDll(DWORD dwThreadId, PDBGKM_LOAD_DLL LoadDll, BOOL bExe, CONTEXT* ctx)
{
	PVOID NamePointer = LoadDll->NamePointer;

	LoadDll->NamePointer = 0;

	if (HANDLE hFile = LoadDll->FileHandle)
	{
		PVOID stack = alloca(sizeof(WCHAR));
		union {
			PVOID buf;
			POBJECT_NAME_INFORMATION poni;
		};
		DWORD cb = 0, rcb = MAX_PATH * sizeof(WCHAR);
		NTSTATUS status;
		do 
		{
			if (cb < rcb) cb = RtlPointerToOffset(buf = alloca(rcb - cb), stack);

			if (0 <= (status = NtQueryObject(hFile, ObjectNameInformation, buf, cb, &rcb)))
			{
				LoadDll->NamePointer = poni->Name.Buffer;

				*(PWSTR)RtlOffsetToPointer(poni->Name.Buffer, poni->Name.Length) = 0;
			}

		} while (status == STATUS_BUFFER_OVERFLOW || status == STATUS_BUFFER_TOO_SMALL || status == STATUS_INFO_LENGTH_MISMATCH);

		NtClose(hFile);
	}

	if (Load(LoadDll, bExe) && NamePointer && ctx)
	{
		if (ZDbgThread* pThread = getThreadById(dwThreadId))
		{
			ctx->ContextFlags = CONTEXT_CONTROL | CONTEXT_DEBUG_REGISTERS | CONTEXT_INTEGER;
			
			if (0 <= ZwGetContextThread(pThread->_hThread, ctx))
			{
				_pThread = pThread;
				return 0;
			}
		}
	}

	return DBG_CONTINUE;
}

void ZDbgDoc::SetModDlg(HWND hwndDLLs)
{
	_hwndDLLs = hwndDLLs;
}

void ZDbgDoc::SetBPDlg(HWND hwndBPs)
{
	_hwndBPs = hwndBPs;
}

//////////////////////////////////////////////////////////////////////////
// Unwind

BOOL ZDbgDoc::DoUnwind(CONTEXT& ctx)
{
#ifdef _WIN64
	LONG_PTR Rip = ctx.Rip;
	bool bFirstTry = 0 > Rip && _IsLocalMemory;

	if (ctx.SegCs == 0x33 || ((_IsDump || _IsRemoteDebugger) && !_IsWow64Process))
	{
__0:
		if (ZDll* pDll = getDllByVaNoRef((PVOID)Rip))
		{
			pDll->CreateRTF(this);

			if (pDll->_RtfPresent)
			{
				return pDll->DoUnwind(Rip, ctx, this);
			}
		}
		else if (bFirstTry)
		{
			bFirstTry = false;

			if (ctx.P2Home)
			{
				ctx.P2Home = 0;
				NTSTATUS status;
				ULONG cb = 0x10000;
				do 
				{
					status = -1;
					if (PVOID pv = new UCHAR[cb])
					{
						if (0 <= (status = NtQuerySystemInformation(SystemModuleInformation, pv, cb, &cb)))
						{
							ctx.P1Home = (ULONG_PTR)pv;
							break;
						}
						delete []pv;
					}
				} while (status == STATUS_INFO_LENGTH_MISMATCH);
			}

			if (ctx.P1Home)
			{
				PRTL_PROCESS_MODULES sp = (PRTL_PROCESS_MODULES)ctx.P1Home;
				if (ULONG NumberOfModules = sp->NumberOfModules)
				{
					PRTL_PROCESS_MODULE_INFORMATION Modules = sp->Modules;
					do 
					{
						if ((ULONG_PTR)(Rip - (ULONG_PTR)Modules->ImageBase) < Modules->ImageSize)
						{
							PWSTR psz = 0;
							PCSTR FullPathName = Modules->FullPathName;
							ULONG cch = 0, cbMultibyte = (ULONG)strlen(FullPathName) + 1;

							while (cch = MultiByteToWideChar(CP_ACP, 0, FullPathName, cbMultibyte, psz, cch))
							{
								if (psz)
								{
									DBGKM_LOAD_DLL LoadDll = { 0, Modules->ImageBase, 0, 0, psz };
									Load(&LoadDll, FALSE);
									goto __0;
								}

								psz = (PWSTR)alloca(cch * sizeof(WCHAR));
							}
							break;
						}
					} while (Modules++, --NumberOfModules);
				}
			}
		}

		return FALSE;
	}
#endif

	struct FRAME_X86
	{
		ULONG Ebp, Eip;
	} frame;

	if (0 <= Read((PVOID)(DWORD)ctx.Xbp, &frame, sizeof(frame)))
	{
		if (ctx.Xbp < frame.Ebp && !(frame.Ebp & 3))
		{
			ctx.Xip = frame.Eip;
			ctx.Xbp = frame.Ebp;
			return TRUE;
		}
	}
	
	return FALSE;
}

void ZDbgDoc::SetAsm(ZAsmView* pAsm)
{
	if (_pAsm != pAsm)
	{
		if (_pAsm)
		{
			_pAsm->Release();
		}

		_pAsm = pAsm;

		if (pAsm)
		{
			pAsm->AddRef();
		}
	}
}

void ZDbgDoc::OnSignal()
{
	_IsTerminated = TRUE;
	Rundown();
}

void ZDbgDoc::OnActivate(BOOL bActivate)
{
	ZSDIFrameWnd* pFrame = ZGLOBALS::getMainFrame();

#ifdef _WIN64
#define PATH_INDEX 6
#else
#define PATH_INDEX 5
#endif

	if (bActivate)
	{
		if (_pAsm) _pAsm->ShowTarget();

		if (_IsDump)
		{
			pFrame->SetStatusText(PATH_INDEX, _NtSymbolPath);
		}
		else
		{
			if (!IsListEmpty(&_dllListHead))
			{
				pFrame->SetStatusText(PATH_INDEX, static_cast<ZDll*>(_dllListHead.Flink)->path());
			}

			WCHAR sz[64];
			swprintf(sz, L"PID=%X(%u)", _dwProcessId, _dwProcessId);
			pFrame->SetStatusText(2, sz);
			swprintf(sz, L"peb=%p", _PebBaseAddress);
			pFrame->SetStatusText(4, sz);
#ifdef _WIN64
			swprintf(sz, L"wow=%p", _wowPeb);
			pFrame->SetStatusText(5, sz);
#endif
		}

		SetDbgFlags(pFrame);
	}
	else
	{
		pFrame->SetStatusText(1, L"");
		pFrame->SetStatusText(2, L"");
		pFrame->SetStatusText(4, L"");
#ifdef _WIN64
		pFrame->SetStatusText(5, L"");
#endif
		pFrame->SetStatusText(PATH_INDEX, L"");
	}

	if (_pReg && IsWaitContinue())
	{
		_pReg->TempHide(bActivate);
	}

	if (_pDbgTH)
	{
		_pDbgTH->TempHide(bActivate);
	}

	ZDocument::OnActivate(bActivate);
}

LRESULT ZDbgDoc::OnCmdMsg(WPARAM wParam, LPARAM lParam)
{
	switch (wParam)
	{
	//case ID_TRACE:DbgContinue(VK_F4);break;
	case ID_DBGTH:
		if (_pDbgTH && !IsDbgWndVisible())
		{
			ShowDbgWnd();
		}
		break;
	case MAKEWPARAM(ID_DISASM, TBN_DROPDOWN):
		if (_pAsm)
		{
			LPNMTOOLBAR lpnmtb = (LPNMTOOLBAR)lParam;
			POINT pt = { lpnmtb->rcButton.left, lpnmtb->rcButton.bottom };
			ClientToScreen(lpnmtb->hdr.hwndFrom, &pt);

			if (HMENU hmenu = LoadMenu((HMODULE)&__ImageBase, MAKEINTRESOURCE(IDR_MENU2)))
			{
				TrackPopupMenu(GetSubMenu(hmenu, 1), 0, pt.x, pt.y, 0, _pAsm->getHWND(), 0);
				DestroyMenu(hmenu);
			}
		}
		break;
	case ID_KILL:
		{
			PCWSTR name = 0;
			if (!IsListEmpty(&_dllListHead))
			{
				name = static_cast<ZDll*>(_dllListHead.Flink)->name();
			}
			
			if (MessageBoxW(ZGLOBALS::getMainHWND(), name, L"Terminate Process ?", MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2) == IDYES)
			{
				NTSTATUS status = ZwTerminateProcess(_hProcess, STATUS_ABANDONED);
				if (0 > status)
				{
					ShowNTStatus(ZGLOBALS::getMainHWND(), status, L"Terminate Process Fail");
				}
				else 
				{
					DbgContinue(VK_F5, 0);
				}
			}
		}
		break;

	case ID_DETACH:
		Rundown();
		break;
	case ID_MEMMAP:
		ShowProcessMemory(_hProcess, _dwProcessId);
		break;
	case ID_MODULES:
		if (!_hwndDLLs)
		{
			if (ZModulesDlg* p = new ZModulesDlg(this))
			{
				p->Create(_nDllCount, &_dllListHead);
				p->Release();
			}
		}
		break;
	case ID_BREAKPOINTS:
		if (_IsDebugger && !_hwndBPs)
		{
			if (ZBPDlg* p = new ZBPDlg(this))
			{
				p->Create();
				p->Release();
			}
		}
		break;
	case ID_BACK:
		if (_pAsm)
		{
			_pAsm->JumpBack();
		}
		break;
	case ID_VMOP:
		ZVmDialog::Create((HANDLE)_dwProcessId);
		break;
	case ID_HANDLES:
		ShowProcessHandles(_dwProcessId);
		break;
	case ID_DBGFLAGS:
		if (0 <= SetDebugInherit(_hProcess, !_IsDbgInherit))
		{
			_IsDbgInherit = !_IsDbgInherit;
		}
		SetDbgFlags();
		break;
	}

	return TBDDRET_DEFAULT;
}

BOOL ZDbgDoc::IsCmdEnabled(WORD cmd)
{
	switch (cmd)
	{
	case ID_DISASM:
		return _pAsm != 0;
	case ID_BACK:
		return _pAsm && _pAsm->canBack();
	case ID_MODULES:
		return !_hwndDLLs;
	case ID_BREAKPOINTS:
		return _IsDebugger && !_hwndBPs;
	case ID_DBGTH:
		return _pDbgTH && !IsWindowVisible(_pDbgTH->ZWnd::getHWND());
	case ID_KILL:
	case ID_DBGFLAGS:
		return _IsDebugger & !_IsRemoteDebugger;
	case ID_DETACH:
		return !_IsInTrace;
	//case ID_TRACE:return _IsWaitContinue && !_IsInTrace;
	case ID_VMOP:
	case ID_HANDLES:
	case ID_MEMMAP:
		return TRUE;
	}
	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
// DEBUG_EVENT

NTSTATUS OnFatal(HANDLE hProcess)
{
	ZwTerminateProcess(hProcess, STATUS_ABANDONED);
	return DBG_EXCEPTION_NOT_HANDLED;
}

BOOL ZDbgDoc::_DbgContinue(CONTEXT& ctx, ULONG_PTR Va, int len, BOOL bStepOver)
{
	DIS* pDisasm = _pAsm->_pDisasm;

	ZDbgThread* pThread = _pThread;

	pThread->_len = len;
	pThread->_bStepOver = bStepOver;
	pThread->_Va = Va;

	PUCHAR buf = (PUCHAR)alloca(len);
	
	if (0 > Read((PVOID)Va, buf, len)) return _DbgContinue(ctx, VK_F11);

	DWORD cb;

	if (!ctx.Dr6 && *buf == 0xcc)
	{
		return _DbgContinue(ctx, VK_F11);
	}

	do 
	{
		if (*buf == 0xcc)
		{
			pThread->_Va = 0;
			return _DbgContinue(ctx, VK_F5);
		}

		if (!(cb = pDisasm->CbDisassemble(Va, buf, len)))
		{
			return _DbgContinue(ctx, VK_F11);
		}

		switch (pDisasm->Trmta())
		{
		case DIS::a_call:
		case DIS::a_call_rm:
			if (!bStepOver)
			{
				break;
			}
		case DIS::a_gen:
		case DIS::a_int:
		case DIS::a_div:
			continue;
		}

		break;

	} while (Va += cb, buf += cb, 0 < (len -= cb));

	if (pThread->_Va == Va)
	{
		return _DbgContinue(ctx, VK_F11);
	}

	if (!len)
	{
		pThread->_Va = 0;
	}

	return _DbgContinue(ctx, VK_F10, Va);
}

void ZDbgDoc::DbgContinue(int key, INT_PTR Va)
{
	if (_IsWaitContinue)
	{
		CONTEXT ctx = *_pReg;
		ctx.Dr6 = 0;
		_DbgContinue(ctx, key, Va);
	}
}

void ZDbgDoc::DbgContinue(ULONG_PTR Va, int len, BOOL bStepOver)
{
	if (_IsWaitContinue)
	{
		CONTEXT ctx = *_pReg;
		ctx.Dr6 = 0;
		_DbgContinue(ctx, Va, len, bStepOver);
	}
}

void ZDbgDoc::SuspendOrResumeAllThreads(BOOL bSuspend, ZDbgThread* pCurrentThread)
{
	//DbgPrint("%s all by %x\n", bSuspend ? "Suspend":"Resume", pCurrentThread->_dwThreadId);

	if (_IsRemoteDebugger)
	{
		return ;
	}
	NTSTATUS  (*fn)(HANDLE hThread, PULONG SuspendCount);

	if (bSuspend)
	{
		if (_SuspendCount++)
		{
			DbgPrint("_SuspendCount++==%x\n", _SuspendCount);
			return;
		}
		fn = ZwSuspendThread;
	}
	else
	{
		if (--_SuspendCount)
		{
			DbgPrint("--_SuspendCount==%x\n", _SuspendCount);

			if (!_bittestandset(&pCurrentThread->_flags, ZDbgThread::stSuspended))
			{
				DbgPrint("ZwSuspendThread(%x)\n", pCurrentThread->_dwThreadId);
				if (0 > ZwSuspendThread(pCurrentThread->_hThread, 0))
				{
					_bittestandreset(&pCurrentThread->_flags, ZDbgThread::stSuspended);
				}
			}
			return ;
		}
		fn = ZwResumeThread;
	}

	PLIST_ENTRY head = &_threadListHead, entry = head;

	while ((entry = entry->Flink) != head)
	{
		ZDbgThread* pThread = static_cast<ZDbgThread*>(entry);

		if (pThread != pCurrentThread)
		{
			NTSTATUS status = fn(pThread->_hThread, 0);
			//DbgPrint("%s(%x)=%x\n", bSuspend ? "Suspend" : "Resume", pThread->_dwThreadId, status);
			if (0 <= status)
			{
				if (bSuspend)
				{
					_bittestandset(&pThread->_flags, ZDbgThread::stSuspended);
				}
				else
				{
					_bittestandreset(&pThread->_flags, ZDbgThread::stSuspended);
				}
			}
		}
	}
}

BOOL ZDbgDoc::_DbgContinue(CONTEXT& ctx, int key, INT_PTR Va)
{
	ZDbgThread* pThread = _pThread;
	NTSTATUS status = DBG_CONTINUE;

	switch (key)
	{
	case VK_F10:
		ctx.Dr0 = Va;
		ctx.Dr7 &= 0xfff0fffc;
		ctx.Dr7 |= 0x401;
	case VK_F5:
		pThread->_state = ZDbgThread::stF5;
		break;
	case VK_F11:
		ctx.EFlags |= TRACE_FLAG;
		pThread->_state = ZDbgThread::stF11;
		break;
	case VK_F4:
		if (_IsInTrace || !_IsLocalMemory)
		{
			return FALSE;
		}

		if (ZTraceView* pTraceView = new ZTraceView(this, pThread->_dwThreadId, &ctx))
		{
			ULONG cx = GetSystemMetrics(SM_CXSCREEN), cy = GetSystemMetrics(SM_CYSCREEN);

			if (pTraceView->ZFrameMultiWnd::Create(0, L"Trace", WS_POPUP|WS_VISIBLE|WS_CAPTION |
				WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_THICKFRAME, 
				cx >> 3, cy >> 3, cx>>1, cy>>1, ZGLOBALS::getMainHWND(), 0, 0))
			{
				_pTraceView = pTraceView;
				pTraceView->AddFirstReport();
				SuspendAllBps(TRUE);
				_IsInTrace = TRUE;
			}
			else
			{
				pTraceView->Release();
				return FALSE;
			}
		}
		break;
	case VK_F12:
		status = DBG_EXCEPTION_NOT_HANDLED;
		goto __F12;
	default: return FALSE;
	}

	ctx.Dr7 |= 1;
	ctx.Dr6 = 0;
	ctx.EFlags |= RESUME_FLAG;

	if (ZBreakPoint* pbp = getBpByVa((PVOID)ctx.Xip))
	{
		if (pbp->_isActive)
		{
			if (SuspendBp(pbp))
			{
				ctx.EFlags |= TRACE_FLAG;
				_bittestandset(&pThread->_flags, ZDbgThread::stBP);
				pThread->_pbpVa = (PVOID)ctx.Xip;
				SuspendOrResumeAllThreads(TRUE, pThread);
			}
			else
			{
				return FALSE;
			}
		}
	}

	if (0 > (_IsRemoteDebugger ? RemoteSetContextThread(_pipe, (WORD)pThread->_dwThreadId, &ctx) : MySetContextThread(pThread->_hThread, &ctx)))
	{
		OnFatal(_hProcess);
		return FALSE;
	}

	memcpy(pThread->_Dr, &ctx.Dr0, sizeof(pThread->_Dr));
__F12:
	CLIENT_ID cid = { (HANDLE)_dwProcessId, (HANDLE)pThread->_dwThreadId };

	if (0 <= (_IsRemoteDebugger ? _pipe->KdContinue(status) : DbgUiContinue(&cid, status)))
	{
		_IsWaitContinue = FALSE;
		//_pThread = 0;

		ZGLOBALS::getMainFrame()->SetStatusText(3, L"");
		_pAsm->setPC(0);
		RemoveSrcPC();
		_pReg->SetDisabled();

		return TRUE;
	}
	
	return FALSE;
}

void ZDbgDoc::ShowFrameContext(ULONG i)
{
	if (_pReg)
	{
		CONTEXT ctx = *_pDump->GetContextRecord();

		while (i-- && DoUnwind(ctx)) ;

		_pReg->SetContext(&ctx);
	}
}

BOOL ZDbgDoc::IsRegVisible()
{
	return _pReg ? IsWindowVisible(_pReg->getHWND()) : TRUE;
}

BOOL ZDbgDoc::IsDbgWndVisible()
{
	return _pDbgTH ? IsWindowVisible(_pDbgTH->ZWnd::getHWND()) : TRUE;
}

void ZDbgDoc::ShowReg()
{
	if (_pReg) ShowWindow(GetParent(_pReg->getHWND()), SW_SHOW);
}

void ZDbgDoc::ShowDbgWnd()
{
	if (_pDbgTH) ShowWindow(_pDbgTH->ZWnd::getHWND(), SW_SHOW);
}

INT_PTR ZDbgDoc::getPC()
{
	return _IsWaitContinue ? _pReg->Xip : 0;
}

void ZDbgDoc::setPC(INT_PTR Va)
{
	if (_IsWaitContinue)
	{
		CONTEXT ctx = *_pReg;
		ctx.Xip = Va;
		_pReg->SetContext(&ctx);
	}
}

void ZDbgDoc::PrintException(DWORD dwThreadId, NTSTATUS ExceptionCode, PVOID ExceptionAddress, ULONG NumberParameters, PULONG_PTR ExceptionInformation, PCSTR Chance)
{
	WCHAR buf[256], *sz;
	sz = buf + swprintf(buf, L"%x>OnException %x(%S) at %p [", dwThreadId, ExceptionCode, Chance, ExceptionAddress);
	if (NumberParameters)
	{
		do 
		{
			sz += swprintf(sz, L" %p,", (void*)*ExceptionInformation++);
		} while (--NumberParameters);
		sz--;
	}

	sz[0] = ']', sz[1] = '\r', sz[2] = '\n', sz[3] = 0;

	cprintf(prException, buf);
}

BOOL ZDbgDoc::ResumeBp(ZBreakPoint* pbp)
{
	if (--pbp->_SuspendCount)
	{
		DbgPrint("ResumeBp(SuspendCount==%x)\n", pbp->_SuspendCount);
		return TRUE;
	}

	if (!pbp->_isActive)
	{
		DbgPrint("ResumeBp:bp<%p> already disabled\n", pbp->_Va);
		return TRUE;
	}

	pbp->_isActive = FALSE;

	if (EnableBp(pbp, TRUE, FALSE))
	{
		return TRUE;
	}

	pbp->_isActive = TRUE;
	++pbp->_SuspendCount;

	return FALSE;
}

BOOL ZDbgDoc::SuspendBp(ZBreakPoint* pbp)
{
	if (pbp->_SuspendCount++)
	{
		DbgPrint("SuspendBp(SuspendCount==%x)\n", pbp->_SuspendCount);
		return TRUE;
	}

	if (!pbp->_isActive)
	{
		DbgPrint("SuspendBp:bp<%p> already disabled\n", pbp->_Va);
		return TRUE;
	}

	if (EnableBp(pbp, FALSE, FALSE))
	{
		pbp->_isActive = TRUE;
		return TRUE;
	}

	pbp->_SuspendCount--;

	return FALSE;
}

NTSTATUS ZDbgDoc::OnRemoteException(ZDbgThread* pThread, DBGKD_WAIT_STATE_CHANGE* pwsc, CONTEXT& ctx)
{
	if (!Is64BitProcess())
	{
		pwsc->Exception.ExceptionRecord.ExceptionAddress &= MAXDWORD;
	}

	WORD Processor = pwsc->Processor;

	NTSTATUS status = RemoteGetContext(Processor, &ctx);
	if (0 > status)
	{
		return status;
	}

	ZBreakPoint* bp;
	ZDbgThread::STATE state = pThread->_state;

	ULONG ExceptionCode = pwsc->Exception.ExceptionRecord.ExceptionCode;
	PVOID ExceptionAddress = (PVOID)(ULONG_PTR)pwsc->Exception.ExceptionRecord.ExceptionAddress;

	switch (ExceptionCode)
	{
	default:
		PrintException(pwsc->Processor, ExceptionCode, 
			ExceptionAddress, pwsc->Exception.ExceptionRecord.NumberParameters, 
			(PULONG_PTR)pwsc->Exception.ExceptionRecord.ExceptionInformation, 
			pwsc->Exception.FirstChance ? "First" : "Last");

	case STATUS_BREAKPOINT:
		if (bp = getBpByVa(ExceptionAddress))
		{
			if (!bp->_isActive)
			{
				ctx.Xip = (ULONG_PTR)ExceptionAddress;
				return DBG_CONTINUE;
			}

			bp->_HitCount++;

			if (bp->_expression && state == ZDbgThread::stF5)
			{
				BOOL b;//dwThreadId
				if (JsScript::_RunScript(bp->_expression, &b, &ctx, this, Processor, bp->_HitCount, pThread->_Ctx))
				{
					MessageBox(ZGLOBALS::getMainHWND(), L"", L"Script Error", MB_ICONHAND);
					return 0;
				}

				if (!b && SuspendBp(bp))
				{
					ctx.EFlags |= TRACE_FLAG;
					pThread->_pbpVa = bp->_Va;
					_bittestandset(&pThread->_flags, ZDbgThread::stBP);
					if (0 > RemoteSetContextThread(_pipe, Processor, &ctx))
					{
						return OnFatal(_hProcess);
					}
					return DBG_CONTINUE;
				}
			}
		}
		else
		{
			ctx.Xip++;//
		}
		break;

	case STATUS_SINGLE_STEP:

		if (pThread->_Va)
		{
			if (_bittestandreset(&pThread->_flags, ZDbgThread::stBP))
			{
				if (bp = getBpByVa(pThread->_pbpVa))
				{
					ResumeBp(bp);
				}
			}

			if ((ULONG_PTR)ExceptionAddress - pThread->_Va < pThread->_len)
			{
				_DbgContinue(ctx, (ULONG_PTR)ExceptionAddress, 
					RtlPointerToOffset(ExceptionAddress, pThread->_Va + pThread->_len), pThread->_bStepOver);
				return DBG_CONTINUE;
			}
			else
			{
				pThread->_Va = 0;
				return 0;
			}
		}

		PLONG Dr6 = (PLONG)&ctx.Dr6;

		if (_bittest(Dr6, 14))
		{
			if (_bittestandreset(&pThread->_flags, ZDbgThread::stBP))
			{
				if (bp = getBpByVa(pThread->_pbpVa))
				{
					ResumeBp(bp);
				}

				if (state == ZDbgThread::stF5)
				{
					return DBG_CONTINUE;
				}
			}
			if (state == ZDbgThread::stF11)
			{
				return 0;
			}
		}
		else
		{
			int i = 0;
			PULONG_PTR Dr = (PULONG_PTR)&ctx.Dr0, _Dr = pThread->_Dr;
			do 
			{
				if (_bittest(Dr6, i))
				{
					if (_Dr[i] == Dr[i])
					{
						return 0;
					}
				}
			} while (++i < 4);
		}
		break;

	}
	// ++ wow 32-64

	if (pwsc->Exception.FirstChance && !_pFC->StopOnFC(ExceptionCode))
	{
		return DBG_EXCEPTION_NOT_HANDLED;
	}

	return 0;
}

NTSTATUS ZDbgDoc::RemoteGetContext(WORD Processor, CONTEXT* ctx)
{
	if (!_pipe->CanSend())
	{
		return STATUS_INVALID_PIPE_STATE;
	}

	NTSTATUS status = _pipe->GetContext(Processor, ctx);
	if (0 > status)
	{
		return status;
	}

	return _pipe->ReadReadControlSpace(Processor, ctx);
}

NTSTATUS ZDbgDoc::OnRemoteLoadUnload(DBGKD_WAIT_STATE_CHANGE* pwsc, CONTEXT& ctx)
{
	ULONG_PTR BaseOfDll = (ULONG_PTR)pwsc->LoadSymbols.BaseOfDll;

	if (!Is64BitProcess())
	{
		BaseOfDll &= MAXDWORD;
	}

	if (pwsc->LoadSymbols.UnloadSymbols)
	{
		OnUnloadDll((PVOID)BaseOfDll);
		return DBG_CONTINUE;
	}

	DBGKM_LOAD_DLL lddi = {
		0, (PVOID)BaseOfDll, 
		pwsc->LoadSymbols.CheckSum, // DebugInfoFileOffset
		pwsc->LoadSymbols.SizeOfImage, //DebugInfoSize
		alloca(pwsc->LoadSymbols.PathNameLength* sizeof(WCHAR))
	};

	MultiByteToWideChar(CP_UTF8, 0, pwsc->Name, pwsc->LoadSymbols.PathNameLength, 
		(PWSTR)lddi.NamePointer, pwsc->LoadSymbols.PathNameLength);

	if (!Load(&lddi, _pRemoteData->KernBase == (PVOID)BaseOfDll))
	{
		return DBG_CONTINUE;// break on load
	}

	NTSTATUS status = RemoteGetContext(pwsc->Processor, &ctx);
	if (0 > status)
	{
		return status;
	}
	return 0;
}

NTSTATUS ZDbgDoc::OnWaitStateChange(DBGKD_WAIT_STATE_CHANGE* pwsc)
{
	if (!Is64BitProcess())
	{
		pwsc->Thread &= MAXDWORD;
		pwsc->ProgramCounter &= MAXDWORD;
	}

	WORD Processor = pwsc->Processor;

	ZDbgThread* pThread = getThreadById(Processor);

	if (!pThread)
	{
		if (pThread = new ZDbgThread(Processor, 0, 0))
		{
			InsertHeadList(&_threadListHead, pThread);
		}
		else
		{
			return DBG_EXCEPTION_NOT_HANDLED;
		}

		_pDbgTH->AddThread(Processor, 0, 0, 0);//!! kpcrb,t
	}

	pThread->_lpThreadLocalBase = (PVOID)(ULONG_PTR)pwsc->Thread;

	CONTEXT ctx{};

	NTSTATUS status = DBG_CONTINUE;

	switch (pwsc->ApiNumber)
	{
	case DbgKdExceptionStateChange:
		status = OnRemoteException(pThread, pwsc, ctx);
		break;
	case DbgKdLoadSymbolsStateChange:
		status = OnRemoteLoadUnload(pwsc, ctx);
		break;
	}

	if (status)
	{
		return status;
	}

	WCHAR sz[16];
	swprintf(sz, L"TID=%I64x", pwsc->Thread);
	_ZGLOBALS* globals = ZGLOBALS::get();
	globals->MainFrame->SetStatusText(3, sz);
	SetForegroundWindow(globals->hwndMain);

	_pThread = pThread;
	_pReg->SetContext(&ctx);
	_IsWaitContinue = TRUE;
	_pAsm->setPC(ctx.Xip);
	UpdateAllViews(0, ALL_UPDATED, 0);

	_pDbgTH->ShowStackTrace(ctx);

	return 0;
}

NTSTATUS ZDbgDoc::OnException(DWORD dwThreadId, PEXCEPTION_RECORD ExceptionRecord, BOOL dwFirstChance, CONTEXT* ctx)
{
	NTSTATUS ExceptionCode = ExceptionRecord->ExceptionCode;

	BOOL bWideChar = FALSE;

	switch (ExceptionCode)
	{
	case DBG_PRINTEXCEPTION_WIDE_C:
		bWideChar = TRUE;
	case DBG_PRINTEXCEPTION_C:
		if (ExceptionRecord->NumberParameters >= 2)
		{
			OnDbgPrint(ExceptionRecord->ExceptionInformation[0],
				(PVOID)ExceptionRecord->ExceptionInformation[1], bWideChar);
		}
		return DBG_CONTINUE;
	}

	ZDbgThread* pThread = getThreadById(dwThreadId);

	if (!pThread)
	{
		return OnFatal(_hProcess);
	}

	_pThread = pThread;

	ctx->ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_DEBUG_REGISTERS;

	HANDLE hThread = pThread->_hThread;

	if (0 > ZwGetContextThread(hThread, ctx))
	{
		return OnFatal(_hProcess);
	}

	PVOID ExceptionAddress = ExceptionRecord->ExceptionAddress;

	ZDbgThread::STATE state = pThread->_state;

	if (!dwFirstChance)
	{
		PrintException(dwThreadId, ExceptionCode, ExceptionAddress, ExceptionRecord->NumberParameters, ExceptionRecord->ExceptionInformation, "Last");
		return 0;
	}

	if (_IsInTrace && _pTraceView->getID() == dwThreadId)
	{
		switch (ExceptionCode)
		{
		default: 
			_pTraceView->SetExeceptionAddress(ExceptionAddress);
			ctx->EFlags &= ~RESUME_FLAG;
			ctx->ContextFlags = CONTEXT_CONTROL;
			MySetContextThread(hThread, ctx);
			return DBG_EXCEPTION_NOT_HANDLED;
		case STATUS_WX86_SINGLE_STEP:
		case STATUS_SINGLE_STEP:
			if (ExceptionCode = _pTraceView->OnException(ctx))
			{
				if (0 > MySetContextThread(hThread, ctx))
				{
					return OnFatal(_hProcess);
				}
			}
			else
			{
				ctx->EFlags &= ~TRACE_FLAG;

				StopTrace(_pTraceView);
			}
			return ExceptionCode;
		}
	}

	ZBreakPoint* bp;

	switch (ExceptionCode)
	{
	case STATUS_WX86_BREAKPOINT:
		ExceptionCode = STATUS_BREAKPOINT;
	case STATUS_BREAKPOINT:

		if (_bittestandreset(&pThread->_flags, ZDbgThread::stSuspended))
		{
			DbgPrint("%x>suspended !!\n", pThread->_dwThreadId);
			if (0 > ZwResumeThread(pThread->_hThread, 0))
			{
				_bittestandset(&pThread->_flags, ZDbgThread::stSuspended);
			}
		}

		if (bp = getBpByVa(ExceptionAddress))
		{
			if (!bp->_isActive)
			{
				ctx->Xip = (ULONG_PTR)ExceptionAddress;
				return DBG_CONTINUE;
			}

			ctx->Xip = (ULONG_PTR)ExceptionAddress;
			bp->_HitCount++;

			if (bp->_expression && state == ZDbgThread::stF5)
			{
				BOOL b;
				if (JsScript::_RunScript(bp->_expression, &b, ctx, this, dwThreadId, bp->_HitCount, pThread->_Ctx))
				{
					MessageBox(ZGLOBALS::getMainHWND(), L"", L"Script Error", MB_ICONHAND);
					return 0;
				}

				if (!b && SuspendBp(bp))
				{
					ctx->EFlags |= TRACE_FLAG;
					pThread->_pbpVa = bp->_Va;
					_bittestandset(&pThread->_flags, ZDbgThread::stBP);
					SuspendOrResumeAllThreads(TRUE, pThread);//
					if (0 > MySetContextThread(hThread, ctx))
					{
						return OnFatal(_hProcess);
					}
					return DBG_CONTINUE;
				}
			}

			return 0;
		}
		break;//not our bp
	
	case STATUS_WX86_SINGLE_STEP:
		ExceptionCode = STATUS_SINGLE_STEP;
	case STATUS_SINGLE_STEP:

		if (pThread->_Va)
		{
			if (_bittestandreset(&pThread->_flags, ZDbgThread::stBP))
			{
				if (bp = getBpByVa(pThread->_pbpVa))
				{
					ResumeBp(bp);
				}
				SuspendOrResumeAllThreads(FALSE, pThread);
			}

			if ((ULONG_PTR)ExceptionAddress - pThread->_Va < pThread->_len)
			{
				_DbgContinue(*ctx, (ULONG_PTR)ExceptionAddress, RtlPointerToOffset(ExceptionAddress, pThread->_Va + pThread->_len), pThread->_bStepOver);
				return DBG_CONTINUE;
			}
			else
			{
				pThread->_Va = 0;
				return 0;
			}
		}

		PLONG Dr6 = (PLONG)&ctx->Dr6;

		if (_bittest(Dr6, 14))
		{
			if (_bittestandreset(&pThread->_flags, ZDbgThread::stBP))
			{
				if (bp = getBpByVa(pThread->_pbpVa))
				{
					ResumeBp(bp);
				}

				SuspendOrResumeAllThreads(FALSE, pThread);//

				if (state == ZDbgThread::stF5)
				{
					return DBG_CONTINUE;
				}
			}
			if (state == ZDbgThread::stF11)
			{
				return 0;
			}
		}
		else
		{
			int i = 0;
			PULONG_PTR Dr = &ctx->Dr0, _Dr = pThread->_Dr;
			do 
			{
				if (_bittest(Dr6, i))
				{
					if (_Dr[i] == Dr[i])
					{
						return 0;
					}
				}
			} while (++i < 4);
		}
		break;
	}

	PrintException(dwThreadId, ExceptionCode, ExceptionAddress, ExceptionRecord->NumberParameters, ExceptionRecord->ExceptionInformation, "First");

	return _pFC->StopOnFC(ExceptionCode) ? 0 : DBG_EXCEPTION_NOT_HANDLED;
}

void ZDbgDoc::OnDebugEvent(DBGUI_WAIT_STATE_CHANGE& StateChange)
{
	ZMemoryCache::Invalidate();

	NTSTATUS status = DBG_CONTINUE;
	CONTEXT ctx{};

	ULONG dwThreadId = PtrToUlong(StateChange.AppClientId.UniqueThread);

	switch(StateChange.NewState) 
	{
	case DbgExceptionStateChange:
	case DbgBreakpointStateChange:
	case DbgSingleStepStateChange:

		status = OnException(dwThreadId, 
			&StateChange.Exception.ExceptionRecord, StateChange.Exception.FirstChance, &ctx);			
		break;

	case DbgCreateProcessStateChange:
		if (!OnCreateProcess(PtrToUlong(StateChange.AppClientId.UniqueProcess), 
			dwThreadId, &StateChange.CreateProcessInfo))
		{
			Rundown();
			return;
		}
		break;
	case DbgCreateThreadStateChange:
		OnCreateThread(dwThreadId, &StateChange.CreateThread);			
		break;
	case DbgExitProcessStateChange:
		OnExitProcess(StateChange.ExitProcess.ExitStatus);			
		break;
	case DbgExitThreadStateChange:
		OnExitThread(dwThreadId, StateChange.ExitThread.ExitStatus);			
		break;
	case DbgLoadDllStateChange:
		status = OnLoadDll(dwThreadId, &StateChange.LoadDll, FALSE, &ctx);
		break;
	case DbgUnloadDllStateChange:
		OnUnloadDll(StateChange.UnloadDll.BaseAddress);			
		break;
	//default: DbgBreak();
	}

	if (status) 
	{
		DbgUiContinue(&StateChange.AppClientId, status);
		
		//_pThread = 0;

		if (StateChange.NewState != DbgExitProcessStateChange)
		{
			_pAsm->setPC(0);
			RemoveSrcPC();
		}
	}
	else 
	{
#ifdef _WIN64
		switch (ctx.SegCs)
		{
		case 0x33:
			_pAsm->SetTarget(DIS::amd64);
			break;
		case 0x23:
			_pAsm->SetTarget(DIS::ia32);
			break;
		}
#endif

		WCHAR sz[16];
		swprintf(sz, L"TID=%x", dwThreadId);
		_ZGLOBALS* globals = ZGLOBALS::get();
		globals->MainFrame->SetStatusText(3, sz);
		SetForegroundWindow(globals->hwndMain);

		ctx.Dr7 &= 0xfff0fffc;
		ctx.Dr0 = 0;

		_pThread->_Va = 0;
		_pReg->SetContext(&ctx);
		_IsWaitContinue = TRUE;
		_pAsm->setPC(ctx.Xip);
		UpdateAllViews(0, ALL_UPDATED, 0);

		_pDbgTH->ShowStackTrace(ctx);
	}
}

DWORD ZDbgDoc::getThreadId() 
{ 
	return _pThread && _IsWaitContinue ? _pThread->getID() : _pDbgTH ? _pDbgTH->GetCurrentThreadId() : 0; 
}

NTSTATUS ZDbgDoc::Write(PVOID RemoteAddress, UCHAR c)
{
	if (_IsDump)
	{
		return STATUS_UNSUCCESSFUL;
	}

	if (_IsRemoteDebugger)
	{
		NTSTATUS status = _pipe->CanSend() ? _pipe->WriteMemory(RemoteAddress, &c, sizeof(c)) : -1;
		if (0 <= status)
		{
			WriteToCache(RemoteAddress, c);
		}
		return status;
	}

	return ZMemoryCache::Write(RemoteAddress, c);
}

NTSTATUS ZDbgDoc::Read(PVOID RemoteAddress, PVOID buf, DWORD cb, PSIZE_T pcb)
{
	if (_IsDump)
	{
		if (pcb) *pcb = 0;
		return _pDump->ReadVirtual(RemoteAddress, buf, cb, pcb);
	}

	if (_IsRemoteDebugger)
	{
		if (pcb) *pcb = 0;
		return _pipe->CanSend() ? _pipe->ReadRemote((PBYTE)RemoteAddress, (PBYTE)buf, cb, pcb) : -1;
	}

	return ZMemoryCache::Read(RemoteAddress, buf, cb, pcb);
}

BOOL ZDbgDoc::Create()
{
	WCHAR sz[17];
	ZSDIFrameWnd* frame = ZGLOBALS::getMainFrame();
#ifdef _WIN64
	if (_wowPeb)
	{
		swprintf(sz, L"%p", _wowPeb);
		frame->SetStatusText(5, sz);
	}
#endif

	if (_IsLocalMemory)
	{
		_PebBaseAddress = 0;
		PROCESS_BASIC_INFORMATION pbi;
		if (0 <= ZwQueryInformationProcess(_hProcess, ProcessBasicInformation, &pbi, sizeof(pbi), 0))
		{
			_PebBaseAddress = pbi.PebBaseAddress;
			swprintf(sz, L"%p", pbi.PebBaseAddress);
			frame->SetStatusText(4, sz);
		}
	}

	return ZMemoryCache::Create() && (_hwndLog = CreateLogView(this, _dwProcessId)) &&
		CreateAsmWindow(this) && _pAsm && _pAsm->SetTarget(
#ifdef _WIN64
		_IsWow64Process ? DIS::ia32 : DIS::amd64
#else
		DIS::ia32
#endif
		) && CreateDbgTH(&_pDbgTH, this) && (!_IsDebugger || createRegView(&_pReg, _dwProcessId));
}

BOOL ZDbgDoc::GetValidRange(INT_PTR Address, INT_PTR& rLo, INT_PTR& rHi)
{
	if (!_IsLocalMemory)
	{
		INT_PTR V, Max, Min;
#ifdef _WIN64
		if (_IsWow64Process)
		{
			if ((int)Address < 0)
			{
				Max = 0xFFFFFFFF, Min = MINLONG;
			}
			else
			{
				Max = 0x7FFFFFFF, Min = 0;
			}
		}
		else
#endif
		{
			if (Address < 0)
			{
				Max = -1, Min = MINLONG_PTR;
			}
			else
			{
				Max = MAXLONG_PTR, Min = 0;
			}
		}
		V = (Max - Address) & ~(PAGE_SIZE - 1);
		rHi = Address + min (V, 0x20000000);
		V = (Address - Min) & ~(PAGE_SIZE - 1);
		rLo = Address - min (V, 0x20000000);
		return TRUE;
	}

	return NT::GetValidRange(_hProcess, Address, rLo, rHi);
}

//////////////////////////////////////////////////////////////////////////
// Dump

void ZDbgDoc::LoadKernelModules()
{
	HWND hwndLog = _hwndLog;
	IMemoryDump* pDump = _pDump;

	PVOID PsLoadedModuleListAddr = pDump->PsLoadedModuleList();

	if ((LONG_PTR)PsLoadedModuleListAddr >= 0 || ((LONG_PTR)PsLoadedModuleListAddr & (__alignof(LIST_ENTRY) - 1)))
	{
		lprintf(hwndLog, L"PsLoadedModuleList=%p invalid!\r\n", PsLoadedModuleListAddr);
		return;
	}

	_LDR_DATA_TABLE_ENTRY ldte;

	if (0 > pDump->ReadVirtual(PsLoadedModuleListAddr, &ldte.InLoadOrderLinks, sizeof(ldte.InLoadOrderLinks)))
	{
		lprintf(hwndLog, L"Fail read PsLoadedModuleList=%p\r\n", PsLoadedModuleListAddr);
		return;
	}

	DBGKM_LOAD_DLL LoadDll = { };
	PVOID stack = alloca(guz);
	SIZE_T cb = 0, rcb = 0;
	ULONG max = 256;

	while((PVOID)(ULONG_PTR)ldte.InLoadOrderLinks.Flink != PsLoadedModuleListAddr)
	{
		if (!max--)
		{
			lprintf(hwndLog, L"more than 256 modules found while walk PsLoadedModuleList\r\n");
			return;
		}

		if (0 > pDump->ReadVirtual((PVOID)(ULONG_PTR)ldte.InLoadOrderLinks.Flink, &ldte, sizeof ldte, 0))
		{
			lprintf(hwndLog, L"Fail read at location %p while walk PsLoadedModuleList\r\n", 
				(PVOID)(ULONG_PTR)ldte.InLoadOrderLinks.Flink);
			return;
		}

		if (cb < (rcb = ldte.FullDllName.Length + sizeof WCHAR))
		{
			cb = RtlPointerToOffset(LoadDll.NamePointer = alloca(rcb - cb), stack);
		}

		if (!pDump->ReadVirtual((PVOID)(ULONG_PTR)ldte.FullDllName.Buffer, LoadDll.NamePointer, ldte.FullDllName.Length, &rcb))
		{
			*(PWSTR)RtlOffsetToPointer(LoadDll.NamePointer, ldte.FullDllName.Length) = 0;
			LoadDll.BaseOfDll = (PVOID)(ULONG_PTR)ldte.DllBase;
			Load(&LoadDll, FALSE);
		}
	}
}

#ifdef _WIN64

void ZDbgDoc::LoadKernelModulesWow()
{
	HWND hwndLog = _hwndLog;
	IMemoryDump* pDump = _pDump;

	PVOID PsLoadedModuleListAddr = pDump->PsLoadedModuleList();

	if ((LONG)(LONG_PTR)PsLoadedModuleListAddr >= 0 || ((LONG_PTR)PsLoadedModuleListAddr & (__alignof(LIST_ENTRY32) - 1)))
	{
		lprintf(hwndLog, L"PsLoadedModuleList=%p invalid!\r\n", PsLoadedModuleListAddr);
		return;
	}

	LDR_DATA_TABLE_ENTRY32 ldte;

	if (0 > pDump->ReadVirtual(PsLoadedModuleListAddr, &ldte.InLoadOrderLinks, sizeof(ldte.InLoadOrderLinks)))
	{
		lprintf(hwndLog, L"Fail read PsLoadedModuleList=%p\r\n", PsLoadedModuleListAddr);
		return;
	}

	DBGKM_LOAD_DLL LoadDll = { };
	PVOID stack = alloca(guz);
	SIZE_T cb = 0, rcb = 0;
	ULONG max = 256;

	while((PVOID)(ULONG_PTR)ldte.InLoadOrderLinks.Flink != PsLoadedModuleListAddr)
	{
		if (!max--)
		{
			lprintf(hwndLog, L"more than 256 modules found while walk PsLoadedModuleList\r\n");
			return;
		}

		if (0 > pDump->ReadVirtual((PVOID)(ULONG_PTR)ldte.InLoadOrderLinks.Flink, &ldte, sizeof ldte, 0))
		{
			lprintf(hwndLog, L"Fail read at location %p while walk PsLoadedModuleList\r\n", 
				(PVOID)(ULONG_PTR)ldte.InLoadOrderLinks.Flink);
			return;
		}

		if (cb < (rcb = ldte.FullDllName.Length + sizeof WCHAR))
		{
			cb = RtlPointerToOffset(LoadDll.NamePointer = alloca(rcb - cb), stack);
		}

		if (!pDump->ReadVirtual((PVOID)(ULONG_PTR)ldte.FullDllName.Buffer, LoadDll.NamePointer, ldte.FullDllName.Length, &rcb))
		{
			*(PWSTR)RtlOffsetToPointer(LoadDll.NamePointer, ldte.FullDllName.Length) = 0;
			LoadDll.BaseOfDll = (PVOID)(ULONG_PTR)ldte.DllBase;;
			Load(&LoadDll, FALSE);
		}
	}
}

#endif

NTSTATUS ZDbgDoc::OpenDump(PCWSTR FileName, PWSTR NtSymbolPath)
{
	_IsDump = TRUE;

	static ULONG s_dumpIndex;
	_dwProcessId = ++s_dumpIndex;

	size_t len = wcslen(NtSymbolPath) + 1;

	if (!(_NtSymbolPath = new WCHAR[len]) ||
		!(_hwndLog = CreateLogView(this, _dwProcessId)))
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	memcpy(_NtSymbolPath, NtSymbolPath, len << 1);

	NTSTATUS status = STATUS_UNSUCCESSFUL;

	UNICODE_STRING ObjectName;
	OBJECT_ATTRIBUTES oa = { sizeof(oa), 0, &ObjectName };

	if (RtlDosPathNameToNtPathName_U(FileName, &ObjectName, 0, 0))
	{
		status = NT::OpenDump(_hwndLog, &oa, &_pDump);
		RtlFreeUnicodeString(&ObjectName);

		if (0 > status)
		{
			return status;
		}

		return OpenDumpComplete();
	}

	return STATUS_OBJECT_PATH_INVALID;
}

NTSTATUS ZDbgDoc::OpenDumpComplete()
{
#ifdef _WIN64
	_IsWow64Process = !_pDump->Is64Bit();
	_wowPeb = 0;
#endif

	if (!CreateAsmWindow(this) || !_pAsm)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	_pAsm->SetTarget(
#ifdef _WIN64
		_IsWow64Process ? DIS::ia32 : DIS::amd64
#else
		DIS::ia32
#endif
		);

	LONG printMask = g_printMask;
	g_printMask = 0;
#ifdef _WIN64
	if (_IsWow64Process)
	{
		LoadKernelModulesWow();
	}
	else
#endif
	{
		LoadKernelModules();
	}

	g_printMask = printMask;

	HWND hwndLog = _hwndLog;
	IMemoryDump* pDump = _pDump;

	pDump->DumpDebuggerData(hwndLog);

	pDump->UpdateContext();

	lprintf(hwndLog, L"//++++++ crash context:\r\n");
	pDump->DumpContext(hwndLog);
	lprintf(hwndLog, L"//------ crash context\r\n");

	lprintf(hwndLog, L"\r\n--------tags---------\r\n");
	pDump->EnumTags(hwndLog);

	if (CreateDbgTH(&_pDbgTH, this))
	{
		CONTEXT ctx = *pDump->GetContextRecord();
#ifdef _WIN64
		ctx.P1Home = 0;
		ctx.P2Home = 0;
#endif
		_pDbgTH->ShowStackTrace(ctx);
	}

	CreatePrivateUDTContext(this, _NtSymbolPath, pDump->get_KernBase(), &_pUdtCtx);

	if (createRegView(&_pReg, _dwProcessId))
	{
		_pReg->SetContext(pDump->GetContextRecord());
	}

	return STATUS_SUCCESS;
}

void FreePM(_In_ PRTL_PROCESS_MODULES mods);
NTSTATUS QueryPM(_In_ HANDLE dwProcessId, _Out_ PRTL_PROCESS_MODULES* pmods);

NTSTATUS ZDbgDoc::Attach(DWORD dwProcessId)
{
	_IsLocalMemory = TRUE, _IsAttached = TRUE;

	_dwProcessId = dwProcessId;
	
	CLIENT_ID cid = { (HANDLE)dwProcessId };

	NTSTATUS status = MyOpenProcess(&_hProcess, PROCESS_ALL_ACCESS_XP, &zoa, &cid);

	if (0 > status)
	{
		return status;
	}

#ifdef _WIN64
	if (0 > (status = ZwQueryInformationProcess(_hProcess, ProcessWow64Information, &_wowPeb, sizeof(_wowPeb), 0)))
	{
		return status;
	}
	_IsWow64Process = _wowPeb != 0;

#endif

	if (!Create())
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	PRTL_PROCESS_MODULES psmi;

	if (0 <= (status = QueryPM(cid.UniqueProcess, &psmi)))
	{
		if (DWORD Num = psmi->NumberOfModules)
		{
			printf(prGen, L"Attached to %x\r\n", _dwProcessId);

			_ZGLOBALS* globals = ZGLOBALS::get();

			if (globals->AppEx->addWaitObject(this, _hProcess))
			{
				_IsInserted = TRUE;
			}

			PRTL_PROCESS_MODULE_INFORMATION smi = psmi->Modules;

			WCHAR ImageName[256];
			DBGKM_LOAD_DLL LoadDll = { 0, 0, 0, 0, ImageName };
			do
			{
				LoadDll.BaseOfDll = smi->ImageBase;
				MultiByteToWideChar(CP_ACP, 0, smi->FullPathName, MAXDWORD, ImageName, RTL_NUMBER_OF(ImageName));

				Load(&LoadDll, FALSE);

			} while(smi++, --Num);

			ULONG cb = 0x80000;

			do 
			{
				status = STATUS_INSUFFICIENT_RESOURCES;
				if (PVOID buf = new UCHAR[cb])
				{
					if (0 <= (status = NtQuerySystemInformation(SystemExtendedProcessInformation, buf, cb, &cb)))
					{
						UpdateThreads((PSYSTEM_PROCESS_INFORMATION)buf);
					}
					delete [] buf;
				}
			} while (status == STATUS_INFO_LENGTH_MISMATCH);

		}

		FreePM(psmi);
	}

	return status;
}

BOOL ZDbgDoc::OnRemoteStart(CDbgPipe* pipe, _DBGKD_GET_VERSION* GetVersion)
{
	_IsRemoteDebugger = TRUE;

#ifdef _WIN64
	_wowPeb = 0;
#endif

	switch (GetVersion->MachineType)
	{
	case IMAGE_FILE_MACHINE_I386:
#ifdef _WIN64
		_IsWow64Process = 1;
	case IMAGE_FILE_MACHINE_AMD64:
#endif
		break;
	default:
		return FALSE;
	}

	if (_pRemoteData = new RemoteData)
	{
		_pRemoteData->KernBase = (PVOID)(ULONG_PTR)GetVersion->KernBase;
		_pRemoteData->PsLoadedModuleList = (PVOID)(ULONG_PTR)GetVersion->PsLoadedModuleList;
	}
	else
	{
		return FALSE;
	}

	_pipe = pipe, pipe->AddRef();

	static LONG sid;
	
	_dwProcessId = InterlockedIncrement(&sid);

	if (!Create())
	{
		return FALSE;
	}

	printf(prGen, L"GetVersionApi(%x.%x m=%x kb=%I64x pslml=%I64x)\r\n", 
		GetVersion->MajorVersion, GetVersion->MinorVersion, GetVersion->MachineType,
		GetVersion->KernBase, GetVersion->PsLoadedModuleList);

	union {
		LIST_ENTRY64 le64;
		LIST_ENTRY32 le32;
	};

	if (0 > pipe->ReadRemote((PBYTE)GetVersion->DebuggerDataList, (PBYTE)&le64, sizeof(le64)))
	{
		return FALSE;
	}

	KDDEBUGGER_DATA64 dbgdata;
	if (0 > pipe->ReadRemote(_IsWow64Process ? (PBYTE)le32.Flink : (PBYTE)le64.Flink, (PBYTE)&dbgdata, sizeof(dbgdata)))
	{
		return FALSE;
	}

	printf(prGen, 
		L"PsLoadedModuleList=%p\r\n"
		L"PsActiveProcessHead=%p\r\n"
		L"NtBuildLab=%p\r\n"
		L"KiProcessorBlock=%p\r\n", 
		dbgdata.PsLoadedModuleList, dbgdata.PsActiveProcessHead, dbgdata.NtBuildLab, dbgdata.KiProcessorBlock);
	return TRUE;
}

HRESULT ZDbgDoc::QI(REFIID riid, void **ppvObject)
{
	if (riid == __uuidof(ZDbgDoc))
	{
		*ppvObject = static_cast<ZObject*>(this);
		AddRef();
		return S_OK;
	}

	return ZDocument::QI(riid, ppvObject);
}

_NT_END
