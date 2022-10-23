#pragma once

class CEvalutor64
{
public:
	typedef BOOL (CALLBACK * RESOLVENAME)(PVOID pUser, PCSTR name, INT_PTR& res);
	typedef BOOL (CALLBACK * READMEM)(PVOID pUser, PVOID Va, PVOID buf, DWORD cb);

	CEvalutor64(PCONTEXT cntx, READMEM rm, RESOLVENAME pfn, PVOID pUser) 
		: m_cntx(cntx) , m_pUser(pUser), m_pfn(pfn), m_rm(rm) { }

	BOOL Evalute(PCSTR sz, INT_PTR& res);		

private:

	typedef INT_PTR (* fn_b_op)(INT_PTR a,INT_PTR b);
	typedef INT_PTR (* fn_u_op)(INT_PTR a);

	struct b_op_arr{fn_b_op	pfn;char c;};
	struct u_op_arr{fn_u_op	pfn;char c;};

	BOOL Calc(LPSTR sz);

	enum { 
#ifdef _WIN64
		e_c_dim = 12,
#else
		e_c_dim = 11,
#endif
		e_b_dim = 18, 
		e_u_dim = 4 
	};
	struct name_to_char{PCSTR tok;char c;};

	static BOOL PreEval(PSTR sz);

	static name_to_char& Convert(int i)
	{
		static name_to_char arr[] =
		{
			{"==", -0x49}, {"!=", -0x41}, {"<<", -0x42},
			{">>", -0x43}, {"<=", -0x44}, {">=", -0x45},
			{"||", -0x4a}, {"&&", -0x4b}, 
			{"byte", -0x46}, 
			{"word", -0x47}, 
			{"dword", -0x48},
#ifdef _WIN64
			{"qword", -0x4c}
#endif
		};
		return arr[i];
	}

	INT_PTR				m_buf[0x40];
	RESOLVENAME			m_pfn;
	READMEM				m_rm;
	PVOID				m_pUser;
	PCONTEXT			m_cntx;
	INT_PTR				m_res;
	char				m_k;

	static INT_PTR fn2Land(INT_PTR a, INT_PTR b){ return a && b; }
	static INT_PTR fn2_Lor(INT_PTR a, INT_PTR b){ return a || b; }
	static INT_PTR fn1_bnt(INT_PTR a)      { return !a; }
	static INT_PTR fn2_equ(INT_PTR a,INT_PTR b){ return a == b; }
	static INT_PTR fn2_nqu(INT_PTR a,INT_PTR b){ return a != b; }
	static INT_PTR fn2_lqu(INT_PTR a,INT_PTR b){ return a < b;  }
	static INT_PTR fn2_gqu(INT_PTR a,INT_PTR b){ return a > b;  }
	static INT_PTR fn2_leu(INT_PTR a,INT_PTR b){ return a <= b; }
	static INT_PTR fn2_geu(INT_PTR a,INT_PTR b){ return a >= b; }
	static INT_PTR fn2_add(INT_PTR a,INT_PTR b){ return a + b;  }
	static INT_PTR fn2_sub(INT_PTR a,INT_PTR b){ return a - b;  }
	static INT_PTR fn2_mul(INT_PTR a,INT_PTR b){ return a * b;  }
	static INT_PTR fn2_div(INT_PTR a,INT_PTR b){ return a / b;  }
	static INT_PTR fn2_dv2(INT_PTR a,INT_PTR b){ return a % b;  }
	static INT_PTR fn2_lsh(INT_PTR a,INT_PTR b){ return (UINT_PTR)a << b; }
	static INT_PTR fn2_rsh(INT_PTR a,INT_PTR b){ return (UINT_PTR)a >> b; }
	static INT_PTR fn2_xor(INT_PTR a,INT_PTR b){ return a ^ b; }
	static INT_PTR fn2_and(INT_PTR a,INT_PTR b){ return a & b; }
	static INT_PTR fn2__or(INT_PTR a,INT_PTR b){ return a | b; }
	static INT_PTR fn1_add(INT_PTR a)      { return +a; }
	static INT_PTR fn1_sub(INT_PTR a)      { return -a; }
	static INT_PTR fn1_not(INT_PTR a)      { return ~a; }

	static b_op_arr& Get_b_arr(int i)
	{
		static b_op_arr b_arr[] = 
		{
			{fn2_equ, -0x49},{fn2_nqu, -0x41},{fn2_lqu, '<'}, {fn2_gqu, '>'},
			{fn2_leu, -0x44},{fn2_geu, -0x45},{fn2_add, '+'}, {fn2_sub, '-'},
			{fn2_mul, '*'}, {fn2_div, '/'}, {fn2_lsh, -0x42}, {fn2_rsh, -0x43},
			{fn2_xor, '^'}, {fn2_dv2, '%'}, {fn2_and, '&'}, {fn2__or, '|'},
			{fn2Land, -0x4b}, {fn2_Lor, -0x4a}
		};

		return b_arr[i];
	}

	static u_op_arr& Get_u_arr(int i)
	{
		static u_op_arr u_arr[] = 
		{
			{fn1_add, '+'}, {fn1_sub, '-'}, {fn1_bnt,'!'}, {fn1_not,'~'}
		};
		return u_arr[i];
	}
};
