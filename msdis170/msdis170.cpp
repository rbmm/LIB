class __declspec(dllexport) DIS
{
	enum DIST{ arm, cee, ia64, mips, mips16, ppc, ppc2, shcompact, arm2, ia32, ia16, amd64 };
	enum REGA{ eax,ecx,edx,ebx,esp,ebp,esi,edi };
	enum MEMREFT{  };
	enum TRMT{  };
	enum TRMTA { 
		a_switchmode=2,
		a_jmp_u_2=4,
		a_jmp_u_5=5,
		a_jmp_rm=7,
		a_ret=8,
		a_jmp_c_2=10,
		a_jmp_c_6=11,
		a_call=15,
		a_call_rm=17 
	};
	enum OPA{  };
	enum OPREFT{  };

public:

	static DIS * __stdcall PdisNew(DIST){return 0;}
	DIST Dist()const{return arm;} 
	void SetAddr64(bool){}
	void * PvClientSet(void *){return 0;}
	void * PvClient(void)const{return 0;} 
	
	unsigned __int64 Addr()const{return 0;} 
	unsigned __int64 (__stdcall* PfndwgetregSet(unsigned __int64 (__stdcall*)(DIS const *,REGA)))(DIS const *,REGA){return 0;}
	
	size_t  CchFormatInstr(wchar_t *,size_t)const{return 0;}
	size_t  CchFormatAddr(unsigned __int64,wchar_t *,size_t)const{return 0;}

	size_t (__stdcall* PfncchregrelSet(size_t (__stdcall*)(DIS const *,REGA,unsigned long,wchar_t *,size_t,unsigned long *)))(DIS const *,REGA,unsigned long,wchar_t *,size_t,unsigned long *){return 0;}
	size_t (__stdcall* PfncchregSet(size_t (__stdcall*)(DIS const *,REGA,wchar_t *,size_t)))(DIS const *,REGA,wchar_t *,size_t){return 0;}
	size_t (__stdcall* PfncchfixupSet(size_t (__stdcall*)(DIS const *,unsigned __int64,size_t,wchar_t *,size_t,unsigned __int64 *)))(DIS const *,unsigned __int64,size_t,wchar_t *,size_t,unsigned __int64 *){return 0;}
	size_t (__stdcall* PfncchaddrSet(size_t (__stdcall*)(DIS const *,unsigned __int64,wchar_t *,size_t,unsigned __int64 *)))(DIS const *,unsigned __int64,wchar_t *,size_t,unsigned __int64 *){return 0;}
};
