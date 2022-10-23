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

			if (!wcscmp(szName, L"ofs_ptr"))
			{
				DispId = e_add;
				continue;
			}

			if (!wcscmp(szName, L"ptr_ofs"))
			{
				DispId = e_sub;
				continue;
			}

			if (!wcscmp(szName, L"toPtr"))
			{
				DispId = e_toPtr;
				continue;
			}

			if (!wcscmp(szName, L"toInt"))
			{
				DispId = e_toInt;
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
#define REG_CASE(x, y) case x: DispId = FIELD_OFFSET(CONTEXT, y)|e_reg_base; continue
#define REG_CASE_D(x, y) case x: DispId = FIELD_OFFSET(CONTEXT, y)|e_reg_dword|e_reg_base; continue
#define REG_CASE_W(x, y) case x: DispId = FIELD_OFFSET(CONTEXT, y)|e_reg_word|e_reg_base; continue
#define REG_CASE_B(x, y) case x: DispId = FIELD_OFFSET(CONTEXT, y)|e_reg_byte|e_reg_base; continue

				switch(u.dd)
				{
#ifdef _WIN64
					REG_CASE('xar', Rax);
					REG_CASE('xbr', Rbx);
					REG_CASE('xcr', Rcx);
					REG_CASE('xdr', Rdx);
					REG_CASE('isr', Rsi);
					REG_CASE('idr', Rdi);
					REG_CASE('pbr', Rbp);
					REG_CASE('psr', Rsp);
					REG_CASE('8r', R8);
					REG_CASE('9r', R9);
					REG_CASE('01r', R10);
					REG_CASE('11r', R11);
					REG_CASE('21r', R12);
					REG_CASE('31r', R13);
					REG_CASE('41r', R14);
					REG_CASE('51r', R15);
					REG_CASE('pir', Rip);

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

					REG_CASE('xae', Eax);
					REG_CASE('xbe', Ebx);
					REG_CASE('xce', Ecx);
					REG_CASE('xde', Edx);
					REG_CASE('ise', Esi);
					REG_CASE('ide', Edi);
					REG_CASE('pbe', Ebp);
					REG_CASE('pse', Esp);
					REG_CASE('pie', Eip);

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

	LONG len;

	if (wFlags & DISPATCH_METHOD)
	{
		switch (dispIdMember)
		{
		case e_toPtr:
			if (cArgs != 1)
			{
				return DISP_E_BADPARAMCOUNT;
			}

			if (rgvarg->vt != VT_I4)
			{
				return DISP_E_BADVARTYPE;
			}
			pVarResult->byref = rgvarg->byref;
			pVarResult->vt = VT_USERDEFINED;
			return S_OK;

		case e_toInt:
			if (cArgs != 1)
			{
				return DISP_E_BADPARAMCOUNT;
			}

			if (rgvarg->vt != VT_USERDEFINED)
			{
				return DISP_E_BADVARTYPE;
			}
			pVarResult->byref = rgvarg->byref;
			pVarResult->vt = VT_I4;
			return S_OK;

		case e_setctx:
			if (cArgs != 2)
			{
				return DISP_E_BADPARAMCOUNT;
			}

			if (rgvarg[1].vt != VT_I4)
			{
				return DISP_E_BADVARTYPE;
			}

			if (3 < (cArgs = rgvarg[1].ulVal))
			{
				return E_INVALIDARG;;
			}

			_pCtx[cArgs] = rgvarg->byref;
			pVarResult->vt = VT_EMPTY;
			return S_OK;

		case e_getctx:
			if (cArgs != 1)
			{
				return DISP_E_BADPARAMCOUNT;
			}

			if (rgvarg->vt != VT_I4)
			{
				return DISP_E_BADVARTYPE;
			}

			if (3 < (cArgs = rgvarg->ulVal))
			{
				return E_INVALIDARG;
			}

			pVarResult->byref = _pCtx[cArgs];
			pVarResult->vt = VT_USERDEFINED;
			return S_OK;

		case e_add:
			if (cArgs != 2)
			{
				return DISP_E_BADPARAMCOUNT;
			}

			if (rgvarg[1].vt != VT_USERDEFINED)
			{
				return DISP_E_BADVARTYPE;
			}

			pVarResult->vt = VT_USERDEFINED;
			pVarResult->byref = (PVOID)((INT_PTR)rgvarg[1].byref + rgvarg[0].lVal);

			return S_OK;

		case e_sub:
			if (cArgs != 2)
			{
				return DISP_E_BADPARAMCOUNT;
			}

			if (rgvarg[1].vt != VT_USERDEFINED || rgvarg[0].vt != VT_USERDEFINED)
			{
				return DISP_E_BADVARTYPE;
			}

			pVarResult->vt = VT_I4;
			pVarResult->lVal = RtlPointerToOffset(rgvarg[1].byref, rgvarg[0].byref);

			return S_OK;

		case e_prUS:
			if (cArgs != 1)
			{
				return DISP_E_BADPARAMCOUNT;
			}

			if (rgvarg->vt != VT_USERDEFINED)
			{
				return DISP_E_BADVARTYPE;
			}

			pVarResult->vt = VT_EMPTY;
			return printUS(rgvarg->byref);

		case e_prOA:
			if (cArgs != 1)
			{
				return DISP_E_BADPARAMCOUNT;
			}

			if (rgvarg->vt != VT_USERDEFINED)
			{
				return DISP_E_BADVARTYPE;
			}

			pVarResult->vt = VT_EMPTY;
			return printOA(rgvarg->byref);

		case e_read:
			if (cArgs != 2)
			{
				return DISP_E_BADPARAMCOUNT;
			}

			if (rgvarg->vt != VT_USERDEFINED || rgvarg[1].vt != VT_I4)
			{
				return DISP_E_BADVARTYPE;
			}

			switch (len = rgvarg[1].lVal)
			{
			case 0:
				pVarResult->vt = VT_USERDEFINED;
				len = sizeof(PVOID);
#ifdef _WIN64
				if (_pContext->SegCs == 0x23)
				{
					len = 4;
				}
#endif
				break;
			case -1:
				pVarResult->vt = VT_I1;
				len = 1;
				break;
			case +1:
				pVarResult->vt = VT_UI1;
				break;
			case -2:
				pVarResult->vt = VT_I2;
				len = 2;
				break;
			case +2:
				pVarResult->vt = VT_UI2;
				break;
			case -4:
				pVarResult->vt = VT_I4;
				len = 4;
				break;
			case +4:
				pVarResult->vt = VT_UI4;
				break;
			case -8:
				pVarResult->vt = VT_I8;
				len = 8;
				break;
			case +8:
				pVarResult->vt = VT_UI8;
				break;
			default: return E_INVALIDARG;
			}
#ifdef _WIN64
			if (_pContext->SegCs == 0x23)
			{
				rgvarg->llVal &= MAXULONG;
			}
#endif
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
			pVarResult->vt = VT_UINT;
			pVarResult->ulVal = _HitCount;
			return S_OK;

		case e_tid:
			pVarResult->vt = VT_UINT;
			pVarResult->ulVal = _ThreadId;
			return S_OK;
		}

		if (dispIdMember & e_reg_base)
		{
			pVarResult->byref = *(void**)RtlOffsetToPointer(_pContext, (USHORT)dispIdMember);

			switch (dispIdMember & 0xffff0000)
			{
			case e_reg_base:
				pVarResult->vt = VT_USERDEFINED;
				return S_OK;

			case e_reg_base|e_reg_dword:
				pVarResult->vt = VT_I4;
				return S_OK;

			case e_reg_base|e_reg_word:
				pVarResult->vt = VT_I2;
				return S_OK;

			case e_reg_base|e_reg_byte:
				pVarResult->vt = VT_I1;
				return S_OK;
			}
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