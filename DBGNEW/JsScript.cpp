#include "StdAfx.h"

_NT_BEGIN

#include "common.h"

JsScript::JsScript()
{
	m_pscParse = 0;
	m_pScript = 0;
	m_dwRef = 1;
}

JsScript::~JsScript()
{
	if (m_pscParse) m_pscParse->Release();
	if (m_pScript) m_pScript->Release();
}

BOOL JsScript::CreateScriptEngine()
{
	CLSID clsid;

	return !CLSIDFromProgID(L"JavaScript", &clsid) &&
		!CoCreateInstance(clsid, 0, CLSCTX_INPROC_SERVER, IID_PPV(m_pScript)) &&
		!m_pScript->QueryInterface(IID_PPV(m_pscParse)) &&
		!m_pScript->SetScriptSite(this) &&
		!m_pscParse->InitNew() &&
		!m_pScript->SetScriptState(SCRIPTSTATE_INITIALIZED) &&
		!m_pScript->AddNamedItem(L"#", SCRIPTITEM_ISVISIBLE|SCRIPTITEM_GLOBALMEMBERS) &&
		!m_pScript->SetScriptState(SCRIPTSTATE_STARTED);
}

void JsScript::Stop()
{
	m_pScript->Close();
	m_pScript->SetScriptSite(0);
}

HRESULT JsScript::printUS(PVOID pv)
{
	if (!_pDoc)
	{
		return S_OK;
	}
#ifdef _WIN64
	if (_pContext->SegCs == 0x23)
	{
		UNICODE_STRING32 us32;
		NTSTATUS status = SymReadMemory(_pDoc, (PVOID)(ULONG)(ULONG_PTR)pv, &us32, sizeof(us32), 0);
		if (0 > status)
		{
			return status;
		}

		if (us32.Length)
		{
			UNICODE_STRING us = { us32.Length, us32.MaximumLength, (PWSTR)alloca(us32.Length) };
			if (0 > (status = SymReadMemory(_pDoc, (PVOID)us32.Buffer, us.Buffer, us.Length, 0)))
			{
				return status;
			}

			_pDoc->printf(prGen, L"%wZ", &us);
		}
	}
	else
#endif
	{
		UNICODE_STRING us;
		NTSTATUS status = SymReadMemory(_pDoc, pv, &us, sizeof(us), 0);
		if (0 > status)
		{
			return status;
		}

		if (us.Length)
		{
			PVOID buf = us.Buffer;
			us.Buffer = (PWSTR)alloca(us.Length);
			if (0 > (status = SymReadMemory(_pDoc, buf, us.Buffer, us.Length, 0)))
			{
				return status;
			}

			_pDoc->printf(prGen, L"%wZ", &us);
		}
	}

	return S_OK;
}

HRESULT JsScript::printOA(PVOID pv)
{
	if (!_pDoc)
	{
		return S_OK;
	}
#ifdef _WIN64
	if (_pContext->SegCs == 0x23)
	{
		OBJECT_ATTRIBUTES32 oa;
		NTSTATUS status = SymReadMemory(_pDoc, (PVOID)(ULONG)(ULONG_PTR)pv, &oa, sizeof(oa), 0);
		if (0 > status)
		{
			return status;
		}

		if (oa.Length != sizeof(oa))
		{
			return STATUS_INFO_LENGTH_MISMATCH;
		}

		if (oa.RootDirectory)
		{
			_pDoc->printf(prGen, L"%x.", oa.RootDirectory);
		}
		return printUS((PVOID)oa.ObjectName);
	}
	else
#endif
	{
		OBJECT_ATTRIBUTES oa;
		NTSTATUS status = SymReadMemory(_pDoc, pv, &oa, sizeof(oa), 0);
		if (0 > status)
		{
			return status;
		}

		if (oa.Length != sizeof(oa))
		{
			return STATUS_INFO_LENGTH_MISMATCH;
		}

		if (oa.RootDirectory)
		{
			_pDoc->printf(prGen, L"%p.", oa.RootDirectory);
		}
		return printUS(oa.ObjectName);
	}
}

HRESULT JsScript::RunScript(PCWSTR script, BOOL *pResult, PCONTEXT pContext, ZDbgDoc* pDoc, ULONG ThreadId, ULONG HitCount, void** pCtx)
{
	_ThreadId = ThreadId, _HitCount = HitCount, _pCtx = pCtx, _pContext = pContext, _pDoc = pDoc;
	VARIANT varResult;

	HRESULT hr = m_pscParse->ParseScriptText(script, 0, 0, 0, 0, 0, SCRIPTTEXT_ISEXPRESSION , &varResult, 0);
	if (!hr)
	{
		switch (varResult.vt)
		{
		case VT_BOOL:
			*pResult = varResult.boolVal == VARIANT_TRUE;
			break;
		case VT_EMPTY:
			*pResult = FALSE;
			break;
		default: hr = DISP_E_TYPEMISMATCH;
		}
		VariantClear(&varResult);
	}

	return hr;
}

HRESULT JsScript::_RunScript(PCWSTR script, BOOL *pResult, PCONTEXT pContext, ZDbgDoc* pDoc, ULONG ThreadId, ULONG HitCount, void** pCtx)
{
	if (JsScript* pScript = GLOBALS_EX::_getScript())
	{
		return pScript->RunScript(script, pResult, pContext, pDoc, ThreadId, HitCount, pCtx);
	}

	return E_FAIL;
}

HRESULT STDMETHODCALLTYPE JsScript::QueryInterface(REFIID riid, __RPC__deref_out void **ppv)
{
	if (riid == __uuidof(IUnknown) || riid == __uuidof(IDispatch))
	{
		*ppv = static_cast<IDispatch*>(this);
	}
	else if (riid == __uuidof(IActiveScriptSite))
	{
		*ppv = static_cast<IActiveScriptSite*>(this);
	}
	else 
	{
		*ppv = 0;
		return E_NOINTERFACE;
	}
	AddRef();
	return S_OK;
}

DWORD STDMETHODCALLTYPE JsScript::AddRef()
{
	return InterlockedIncrement(&m_dwRef);
}

DWORD STDMETHODCALLTYPE JsScript::Release()
{
	DWORD n = InterlockedDecrement(&m_dwRef);
	if (!n) delete this;
	return n;
}

STDMETHODIMP JsScript::GetTypeInfoCount(UINT *pctinfo)
{
	*pctinfo = 0;
	return S_OK;
}

STDMETHODIMP JsScript::GetTypeInfo( 
								   /* [in] */ UINT /*iTInfo*/,
								   /* [in] */ LCID ,
								   /* [out] */ ITypeInfo **ppTInfo)
{
	*ppTInfo = 0;
	return DISP_E_BADINDEX;
}

STDMETHODIMP JsScript::GetIDsOfNames( 
									 /* [in] */ REFIID ,
									 /* [size_is][in] */ LPOLESTR *rgszNames,
									 /* [in] */ UINT cNames,
									 /* [in] */ LCID ,
									 /* [size_is][out] */ DISPID *rgDispId)
{
	HRESULT hr = S_OK;

	if (cNames)
	{
		DISPID DispId;
		do 
		{
			PCWSTR szName = *rgszNames++;

			if (!wcscmp(szName, L"Read"))
			{
				DispId = e_read;
				continue;
			}

			if (!wcscmp(szName, L"SetCtx"))
			{
				DispId = e_setctx;
				continue;
			}

			if (!wcscmp(szName, L"GetCtx"))
			{
				DispId = e_getctx;
				continue;
			}

			if (!wcscmp(szName, L"ThreadId"))
			{
				DispId = e_tid;
				continue;
			}

			if (!wcscmp(szName, L"HitCount"))
			{
				DispId = e_cnt;
				continue;
			}

			if (!wcscmp(szName, L"print"))
			{
				DispId = e_print;
				continue;
			}

			if (!wcscmp(szName, L"printOA"))
			{
				DispId = e_prOA;
				continue;
			}

			if (!wcscmp(szName, L"printUS"))
			{
				DispId = e_prUS;
				continue;
			}

			if (!wcscmp(szName, L"Str"))
			{
				DispId = e_toStrW;
				continue;
			}

			if (!wcscmp(szName, L"AStr"))
			{
				DispId = e_toStrA;
				continue;
			}

			if (!wcscmp(szName, L"name"))
			{
				DispId = e_name;
				continue;
			}

			if (!wcscmp(szName, L"strstr"))
			{
				DispId = e_strstr;
				continue;
			}

			if (!wcscmp(szName, L"strcmp"))
			{
				DispId = e_strcmp;
				continue;
			}

			union {
				DWORD dd;
				char regsz[4];
			} u = {};

			int i = 4;
			PSTR str = u.regsz;
			WCHAR c;

			do 
			{
				c = *szName++;
				if (!i || !c || c > 'z')
				{
					break;
				}

				if (c - 'A' <= 'Z' - 'A')
				{
					c |= 0x20;
				}

				*str++ = (char)c;

			} while (i--);

			if (!c)
			{
#define REG_CASE_Q(x, y) case x: DispId = FIELD_OFFSET(CONTEXT, y)|e_reg_qword; continue
#define REG_CASE_D(x, y) case x: DispId = FIELD_OFFSET(CONTEXT, y)|e_reg_dword; continue
#define REG_CASE_W(x, y) case x: DispId = FIELD_OFFSET(CONTEXT, y)|e_reg_word; continue
#define REG_CASE_B(x, y) case x: DispId = FIELD_OFFSET(CONTEXT, y)|e_reg_byte; continue

				switch(u.dd)
				{
#ifdef _WIN64
					REG_CASE_Q('xar', Rax);
					REG_CASE_Q('xbr', Rbx);
					REG_CASE_Q('xcr', Rcx);
					REG_CASE_Q('xdr', Rdx);
					REG_CASE_Q('isr', Rsi);
					REG_CASE_Q('idr', Rdi);
					REG_CASE_Q('pbr', Rbp);
					REG_CASE_Q('psr', Rsp);
					REG_CASE_Q('8r', R8);
					REG_CASE_Q('9r', R9);
					REG_CASE_Q('01r', R10);
					REG_CASE_Q('11r', R11);
					REG_CASE_Q('21r', R12);
					REG_CASE_Q('31r', R13);
					REG_CASE_Q('41r', R14);
					REG_CASE_Q('51r', R15);
					REG_CASE_Q('pir', Rip);

					REG_CASE_D('xae', Rax);
					REG_CASE_D('xbe', Rbx);
					REG_CASE_D('xce', Rcx);
					REG_CASE_D('xde', Rdx);
					REG_CASE_D('ise', Rsi);
					REG_CASE_D('ide', Rdi);
					REG_CASE_D('d8r', R8);
					REG_CASE_D('d9r', R9);
					REG_CASE_D('d01r', R10);
					REG_CASE_D('d11r', R11);
					REG_CASE_D('d21r', R12);
					REG_CASE_D('d31r', R13);
					REG_CASE_D('d41r', R14);
					REG_CASE_D('d51r', R15);

					REG_CASE_W('xa', Rax);
					REG_CASE_W('xb', Rbx);
					REG_CASE_W('xc', Rcx);
					REG_CASE_W('xd', Rdx);
					REG_CASE_W('is', Rsi);
					REG_CASE_W('id', Rdi);
					REG_CASE_W('w8r', R8);
					REG_CASE_W('w9r', R9);
					REG_CASE_W('w01r', R10);
					REG_CASE_W('w11r', R11);
					REG_CASE_W('w21r', R12);
					REG_CASE_W('w31r', R13);
					REG_CASE_W('w41r', R14);
					REG_CASE_W('w51r', R15);

					REG_CASE_B('la', Rax);
					REG_CASE_B('lb', Rbx);
					REG_CASE_B('lc', Rcx);
					REG_CASE_B('ld', Rdx);
					REG_CASE_B('lis', Rsi);
					REG_CASE_B('lid', Rdi);
					REG_CASE_B('l8r', R8);
					REG_CASE_B('l9r', R9);
					REG_CASE_B('l01r', R10);
					REG_CASE_B('l11r', R11);
					REG_CASE_B('l21r', R12);
					REG_CASE_B('l31r', R13);
					REG_CASE_B('l41r', R14);
					REG_CASE_B('l51r', R15);

#else
					REG_CASE_D('xae', Eax);
					REG_CASE_D('xbe', Ebx);
					REG_CASE_D('xce', Ecx);
					REG_CASE_D('xde', Edx);
					REG_CASE_D('ise', Esi);
					REG_CASE_D('ide', Edi);
					REG_CASE_D('pbe', Ebp);
					REG_CASE_D('pse', Esp);
					REG_CASE_D('pie', Eip);

					REG_CASE_W('xa', Eax);
					REG_CASE_W('xb', Ebx);
					REG_CASE_W('xc', Ecx);
					REG_CASE_W('xd', Edx);
					REG_CASE_W('is', Esi);
					REG_CASE_W('id', Edi);

					REG_CASE_B('la', Eax);
					REG_CASE_B('lb', Ebx);
					REG_CASE_B('lc', Ecx);
					REG_CASE_B('ld', Edx);
#endif

					REG_CASE_D('0rd', Dr0);
					REG_CASE_D('1rd', Dr1);
					REG_CASE_D('2rd', Dr2);
					REG_CASE_D('3rd', Dr3);
				}
			}

			hr = DISP_E_UNKNOWNNAME;
			DispId = DISPID_UNKNOWN;

		} while (*rgDispId++ = DispId, --cNames);
	}

	return hr;
}

STDMETHODIMP JsScript::Invoke( 
							  /* [in] */ DISPID dispIdMember,
							  /* [in] */ REFIID ,
							  /* [in] */ LCID ,
							  /* [in] */ WORD wFlags,
							  /* [out][in] */ DISPPARAMS *pDispParams,
							  /* [out] */ VARIANT *pVarResult,
							  /* [out] */ EXCEPINFO * /*pExcepInfo*/,
							  /* [out] */ UINT * /*puArgErr*/)
{
	if (pDispParams->cNamedArgs)
	{
		return DISP_E_NONAMEDARGS;
	}

	UINT cArgs = pDispParams->cArgs;
	VARIANT *rgvarg = pDispParams->rgvarg;

	union {
		ULONG len;
		SIZE_T rcb;
	};

	union {
		CHAR astr[0x200];
		WCHAR str[0x100];
	};

	PWSTR psz1, psz2;

	if (wFlags & DISPATCH_METHOD)
	{
		switch (dispIdMember)
		{
		case e_strstr:
			if (cArgs != 2)
			{
				return DISP_E_BADPARAMCOUNT;
			}

			if (rgvarg[0].vt != VT_BSTR || rgvarg[1].vt != VT_BSTR)
			{
				return DISP_E_BADVARTYPE;
			}

			pVarResult->intVal = -1;
			pVarResult->vt = VT_I4;

			if ((psz1 = rgvarg[1].bstrVal) && (psz2 = rgvarg[0].bstrVal))
			{
				if (psz2 = wcsstr(psz1, psz2))
				{
					pVarResult->intVal = RtlPointerToOffset(psz1, psz2) >> 1;
				}
			}

			return S_OK;

		case e_strcmp:
			if (cArgs != 2)
			{
				return DISP_E_BADPARAMCOUNT;
			}

			if (rgvarg[0].vt != VT_BSTR || rgvarg[1].vt != VT_BSTR)
			{
				return DISP_E_BADVARTYPE;
			}

			pVarResult->vt = VT_I4;
			pVarResult->intVal = 0;

			if ((psz1 = rgvarg[1].bstrVal) && (psz2 = rgvarg[0].bstrVal))
			{
				pVarResult->intVal = wcscmp(psz1, psz2);
			}

			return S_OK;

		case e_name:
			if (cArgs != 1)
			{
				return DISP_E_BADPARAMCOUNT;
			}

			if (_pDoc)
			{
				PCSTR Name = 0;
				INT_PTR NameVa = 0;
				PCWSTR szDllName = L"";
				PVOID Va = rgvarg->byref;

				if (ZDll* pDll = _pDoc->getDllByVaNoRef(Va))
				{
					szDllName = pDll->name();
					if (PCSTR name = pDll->getNameByVa2(Va, &NameVa))
					{
						if (IS_INTRESOURCE(name))
						{
							char oname[16];
							sprintf(oname, "#%u", (ULONG)(ULONG_PTR)name);
							name = oname;
						}

						Name = unDNameEx(astr, name, RTL_NUMBER_OF(astr), UNDNAME_DEFAULT);
					}
					else
					{
						NameVa = (INT_PTR)pDll->getBase();
						Name = "";
					}

					if (Name)
					{
						int d = RtlPointerToOffset(NameVa, Va);

						PWSTR psz = 0;
						cArgs = 0;
						while (0 < (INT)(cArgs = _snwprintf(psz, cArgs, L"%s!%S%c+ %x", szDllName, Name, d ? ' ' : 0, d)))
						{
							if (psz)
							{
								pVarResult->bstrVal = psz;
								pVarResult->vt = VT_BSTR;
								return S_OK;
							}

							if (!(psz = SysAllocStringLen(0, cArgs)))
							{
								return E_OUTOFMEMORY;
							}
						}

						if (psz)
						{
							SysFreeString(psz);
						}
					}
				}
			}
			else
			{
				pVarResult->bstrVal = 0;
				pVarResult->vt = VT_BSTR;
				return S_OK;
			}

			return E_FAIL;

		case e_toStrA:
			if (cArgs != 1)
			{
				return DISP_E_BADPARAMCOUNT;
			}

			if (!_pDoc)
			{
				return S_OK;
			}

			switch (SymReadMemory(_pDoc, rgvarg->byref, astr, sizeof(astr), &rcb))
			{
			case STATUS_SUCCESS:
			case STATUS_PARTIAL_COPY:
				if (rcb)
				{
					astr[rcb-1]=0;
					PWSTR psz = 0;
					cArgs = 0;
					while (cArgs = MultiByteToWideChar(CP_UTF8, 0, astr, MAXULONG, psz, cArgs))
					{
						if (psz)
						{
							pVarResult->bstrVal = psz;
							pVarResult->vt = VT_BSTR;
							return S_OK;
						}

						if (!(psz = SysAllocStringLen(0, cArgs)))
						{
							return E_OUTOFMEMORY;
						}
					}

					if (psz)
					{
						SysFreeString(psz);
					}
				}
			}

			return E_FAIL;

		case e_toStrW:
			if (cArgs != 1)
			{
				return DISP_E_BADPARAMCOUNT;
			}

			if (rgvarg->ulVal & 1)
			{
				return E_INVALIDARG;
			}

			if (!_pDoc)
			{
				pVarResult->bstrVal = 0;
				pVarResult->vt = VT_BSTR;
				return S_OK;
			}

			switch (SymReadMemory(_pDoc, rgvarg->byref, str, sizeof(str), &rcb))
			{
			case STATUS_SUCCESS:
			case STATUS_PARTIAL_COPY:
				if (rcb >>= 1)
				{
					str[rcb-1]=0;
					pVarResult->bstrVal = SysAllocString(str);
					pVarResult->vt = VT_BSTR;
					return S_OK;
				}
			}
			return E_FAIL;

		case e_setctx:
			if (cArgs != 2)
			{
				return DISP_E_BADPARAMCOUNT;
			}

			if (3 < (cArgs = rgvarg[1].ulVal))
			{
				return E_INVALIDARG;
			}

			_pCtx[cArgs] = rgvarg->byref;
			pVarResult->vt = VT_EMPTY;
			return S_OK;

		case e_getctx:
			if (cArgs != 1)
			{
				return DISP_E_BADPARAMCOUNT;
			}

			if (3 < (cArgs = rgvarg->ulVal))
			{
				return E_INVALIDARG;
			}

			pVarResult->byref = _pCtx[cArgs];
			pVarResult->vt = VT_I4;
			return S_OK;

		case e_prUS:
			if (cArgs != 1)
			{
				return DISP_E_BADPARAMCOUNT;
			}

			pVarResult->vt = VT_EMPTY;
			return printUS(rgvarg->byref);

		case e_prOA:
			if (cArgs != 1)
			{
				return DISP_E_BADPARAMCOUNT;
			}

			pVarResult->vt = VT_EMPTY;
			return printOA(rgvarg->byref);

		case e_read:
			if (cArgs != 2)
			{
				return DISP_E_BADPARAMCOUNT;
			}

			switch (len = rgvarg[1].lVal)
			{
			case +1:
			case +2:
			case +4:
			case +8:
				break;
			default: return E_INVALIDARG;
			}
#ifdef _WIN64
			if (_pContext->SegCs == 0x23)
			{
				rgvarg->llVal &= MAXULONG;
			}
#endif
			pVarResult->vt = VT_I4;
			pVarResult->llVal = 0;
			return _pDoc ? SymReadMemory(_pDoc, rgvarg->byref, &pVarResult->llVal, len, 0) : S_OK;

		case e_print:

			if (cArgs-- < 1)
			{
				return DISP_E_BADPARAMCOUNT;
			}

			if (rgvarg[cArgs].vt != VT_BSTR)
			{
				return DISP_E_BADVARTYPE;
			}

			if (_pDoc)
			{
				void** argv = 0;

				if (cArgs)
				{
					argv = (void**)alloca(cArgs * sizeof(PVOID)) + cArgs;
					do 
					{
						*--argv = rgvarg++->byref;
					} while (--cArgs);
				}

				__try {
					_pDoc->vprintf(prGen, rgvarg->bstrVal, (va_list)argv);
				}
				__except(EXCEPTION_EXECUTE_HANDLER){
					return GetExceptionCode();
				}
			}

			pVarResult->vt = VT_EMPTY;
			return S_OK;
		}
	}
	else if (wFlags & DISPATCH_PROPERTYGET)
	{
		if (cArgs)
		{
			return DISP_E_BADPARAMCOUNT;
		}

		switch (dispIdMember)
		{
		case e_cnt:
			pVarResult->vt = VT_UI4;
			pVarResult->ulVal = _HitCount;
			return S_OK;

		case e_tid:
			pVarResult->vt = VT_UI4;
			pVarResult->ulVal = _ThreadId;
			return S_OK;
		}

		if ((dispIdMember & (e_reg_qword|e_reg_dword|e_reg_word|e_reg_byte)) &&
			(USHORT)dispIdMember < sizeof(CONTEXT) && !((USHORT)dispIdMember & (__alignof(ULONG_PTR) - 1)))
		{
			ULONG_PTR value = *(ULONG_PTR*)RtlOffsetToPointer(_pContext, (USHORT)dispIdMember);
			pVarResult->vt = VT_I4;

			switch (dispIdMember & 0xffff0000)
			{
			case e_reg_byte:
				value &= 0xFF;
				break;
			case e_reg_word:
				value &= 0xFFFF;
				break;
			case e_reg_dword:
				value &= 0xFFFFFFFF;
			case e_reg_qword:
				break;
			default: return E_INVALIDARG;
			}
			pVarResult->byref = (PVOID)value;
			return S_OK;
		}
	}

	return DISP_E_MEMBERNOTFOUND;
}

HRESULT STDMETHODCALLTYPE JsScript::GetLCID(LCID *plcid)
{
	*plcid = LANG_NEUTRAL;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE JsScript::GetItemInfo(LPCOLESTR pstrName, DWORD dwReturnMask, IUnknown **ppiunkItem, ITypeInfo **ppti)
{
	if (SCRIPTINFO_IUNKNOWN & dwReturnMask) *ppiunkItem=0;
	if (SCRIPTINFO_ITYPEINFO & dwReturnMask) *ppti = 0;

	if (!wcscmp(L"#", pstrName))
	{
		if (SCRIPTINFO_IUNKNOWN & dwReturnMask)
		{
			QueryInterface(__uuidof(IUnknown), (void**)ppiunkItem);
		}

		if (SCRIPTINFO_ITYPEINFO & dwReturnMask)
		{
			*ppti = 0;
		}
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE JsScript::GetDocVersionString(BSTR *pbstrVersion)
{
	*pbstrVersion=0;
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE JsScript::OnScriptTerminate(const VARIANT *, const EXCEPINFO *)
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE JsScript::OnStateChange(SCRIPTSTATE /*ssScriptState*/)
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE JsScript::OnScriptError(IActiveScriptError *pscripterror)
{
	DWORD a=0, b=0;
	LONG c=0;
	if (!pscripterror->GetSourcePosition(&a, &b, &c))
	{
		DbgPrint("\r\n///////////////////////////////////////////////////\r\n");
		DbgPrint("ScriptError at line = %u charposition = %u", b+1, c+1);
		DbgPrint("\r\n///////////////////////////////////////////////////\r\n");
	}

	return S_OK;
}

HRESULT STDMETHODCALLTYPE JsScript::OnEnterScript()
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE JsScript::OnLeaveScript()
{
	return S_OK;
}

_NT_END