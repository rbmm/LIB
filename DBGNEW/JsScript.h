#pragma once

struct JsScript : IDispatch, IActiveScriptSite
{
	IActiveScript* m_pScript;
	IActiveScriptParse* m_pscParse;
	PCONTEXT _pContext;
	ZDbgDoc* _pDoc;
	void** _pCtx;
	ULONG _ThreadId;
	ULONG _HitCount;
	LONG m_dwRef;

	enum{
		e_reg_base = 0x80000000,
		e_reg_dword = 0x40000000,
		e_reg_word = 0x20000000,
		e_reg_byte = 0x10000000,
		e_print = 1,
		e_add,
		e_sub,
		e_tid,
		e_cnt,
		e_read,
		e_setctx,
		e_getctx,
		e_toPtr,
		e_toInt,
		e_prUS,
		e_prOA
	};

	JsScript();
	~JsScript();

	///////////////////////////////////////////////////////////////
	// IDispatch

	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, __RPC__deref_out void **ppvObject);

	virtual DWORD STDMETHODCALLTYPE AddRef();

	virtual DWORD STDMETHODCALLTYPE Release();

	STDMETHOD(GetTypeInfoCount)(UINT *pctinfo);

	STDMETHOD(GetTypeInfo)( 
		/* [in] */ UINT iTInfo,
		/* [in] */ LCID ,
		/* [out] */ ITypeInfo **ppTInfo);

	STDMETHOD(GetIDsOfNames)( 
		/* [in] */ REFIID ,
		/* [size_is][in] */ LPOLESTR *rgszNames,
		/* [in] */ UINT cNames,
		/* [in] */ LCID ,
		/* [size_is][out] */ DISPID *rgDispId);

	STDMETHOD(Invoke)( 
		/* [in] */ DISPID dispIdMember,
		/* [in] */ REFIID ,
		/* [in] */ LCID ,
		/* [in] */ WORD wFlags,
		/* [out][in] */ DISPPARAMS *pDispParams,
		/* [out] */ VARIANT *pVarResult,
		/* [out] */ EXCEPINFO *pExcepInfo,
		/* [out] */ UINT *puArgErr);

	///////////////////////////////////////////////////////////////
	// IActiveScriptSite

	virtual HRESULT STDMETHODCALLTYPE GetLCID(LCID *plcid);
	virtual HRESULT STDMETHODCALLTYPE GetItemInfo(LPCOLESTR pstrName, DWORD dwReturnMask, IUnknown **ppiunkItem, ITypeInfo **ppti);
	virtual HRESULT STDMETHODCALLTYPE GetDocVersionString(BSTR *pbstrVersion);
	virtual HRESULT STDMETHODCALLTYPE OnScriptTerminate(const VARIANT *, const EXCEPINFO *);
	virtual HRESULT STDMETHODCALLTYPE OnStateChange(SCRIPTSTATE ssScriptState);
	virtual HRESULT STDMETHODCALLTYPE OnScriptError(IActiveScriptError *pscripterror);
	virtual HRESULT STDMETHODCALLTYPE OnEnterScript();
	virtual HRESULT STDMETHODCALLTYPE OnLeaveScript();

	///////////////////////////////////////////////////////////////////////
	//

	static HRESULT _RunScript(PCWSTR script, BOOL *pResult, PCONTEXT pContext, ZDbgDoc* pDoc, ULONG ThreadId, ULONG HitCount, void** pCtx);
	HRESULT RunScript(PCWSTR script, BOOL *pResult, PCONTEXT pContext, ZDbgDoc* pDoc, ULONG ThreadId, ULONG HitCount, void** pCtx);
	BOOL CreateScriptEngine();
	void Stop();
	HRESULT printUS(PVOID pv);
	HRESULT printOA(PVOID pv);
};
