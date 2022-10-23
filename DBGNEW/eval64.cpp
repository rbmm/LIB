#include "stdafx.h"

_NT_BEGIN

#include "eval64.h"

UINT_PTR strtoui64(const char * sz, const char ** psz)
{
	UINT_PTR r = 0;

	for (;;)
	{
		UCHAR c = *sz;

		if (c == '`')
		{
			sz++;
			continue;
		}

		if ((UCHAR)(c - '0') <= (UCHAR)('9' - '0'))
		{
			c -= '0';
		}
		else if ((UCHAR)(c - 'a') <= (UCHAR)('f' - 'a'))
		{
			c -= 'a' - 10;
		}
		else if ((UCHAR)(c - 'A') <= (UCHAR)('F' - 'A'))
		{
			c -= 'A' - 10;
		}
		else
		{
			break;
		}

		r = (r << 4) + c, sz++;
	}

	if (psz) *psz = sz;

	return r;
}

BOOL CEvalutor64::PreEval(PSTR sz)
{
	char c, *pc;

	pc = sz;
	while(c = *pc++) if (c < 0) return FALSE;

	int i = e_c_dim - 1;
	do {

		PCSTR tok = Convert(i).tok;
		int len = (int)strlen(tok);
		c = Convert(i).c, pc = sz;		
		while(pc = strstr(pc, tok)) memset(pc, ' ', len), *pc = c;

	} while(i--);

	pc = sz;
	
	do 
	{
		if ((c = *pc++) != ' ') *sz++ = c;
	} while (c);

	return TRUE;
}

BOOL CEvalutor64::Evalute(PCSTR sz, INT_PTR& res) 
{	
	int len = (int)strlen(sz) + 1;
	PSTR buf = (PSTR)alloca(len + 1);
	memcpy(buf, sz, len);

	m_k = 0x40; 
	__try
	{
		if (PreEval(buf) && Calc(buf))
		{
			res = m_res;
			return TRUE;
		}
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
	}

	return FALSE;	
}

BOOL CEvalutor64::Calc(PSTR sz)
{	
	char *p, *q, c;
	int i;
	DWORD cb;
	INT_PTR res;

	if (!*sz) return FALSE;

	while (p = strrchr(sz, '(')) 
	{
		q = strchr(p + 1,')');
		if (!q) return FALSE;
		*q++ = 0;
		if (!Calc(p + 1) || !m_k) return FALSE;
		*p++ = -m_k, m_buf[--m_k] = m_res;
		strcpy(p, q);
	}

	if (READMEM rm = m_rm)
	{
		while (p = strrchr(sz,'[')) 
		{
			if (p == sz) return FALSE;
			q = strchr(p + 1, ']');
			if (!q) return FALSE;
			switch(p[-1])
			{
			case -0x46: cb = 1;break;
			case -0x47: cb = 2;break;
			case -0x48: cb = 4;break;
#ifdef _WIN64
			case -0x4c: cb = 8;break;
#endif

			default: return FALSE;
			}
			*q++ = 0;
			if (!Calc(p + 1) || !m_k || !rm(m_pUser, (PVOID)m_res, &res, cb)) return FALSE;
			p[-1] = -m_k, m_buf[--m_k] = res;
			strcpy(p, q);
		}
	}

	for(i = 0; i < e_b_dim; i++)
	{
		if (p = strchr(sz + 1, Get_b_arr(i).c))
		{
			*p++ = 0;
			if (!Calc(sz)) return FALSE;
			res = m_res;
			if (!Calc(p)) return FALSE;
			m_res = Get_b_arr(i).pfn(res, m_res);
			return TRUE;
		}
	}

	for(i = 0; i < e_u_dim; i++)
	{
		if (*sz == Get_u_arr(i).c)
		{
			if (!Calc(sz + 1)) return FALSE;
			m_res = Get_u_arr(i).pfn(m_res);
			return TRUE;
		}
	}

	if (!*sz) return FALSE;

	if ((0 < (c = -*sz)) && (c <= RTL_NUMBER_OF(m_buf)))
	{
		if (!sz[1])
		{
			m_res = m_buf[c - 1];
			return TRUE;
		}
	}

	m_res = strtoui64(sz, (PCSTR*)&p);

	if (!*p) return TRUE;

	_CONTEXT* cntx = m_cntx;

	INT_PTR i64 = 0;

	if (cntx)
	{
		p = sz;
		union{
			DWORD dd;
			char regsz[8];
		} u = {};
		q = u.regsz;
		i = 4;
		do 
		{
			c = (char)tolower(*p++);
			if (!c || (c == ' ')) {
				break;
			}
			*q++ = c;
		} while(i--);

		if (0 <= i)
		{
			BOOL reg = TRUE;
			switch(u.dd)
			{
#ifdef _WIN64
			case 'xar': i64 = cntx->Rax; break;
			case 'xbr': i64 = cntx->Rbx; break;
			case 'xcr': i64 = cntx->Rcx; break;
			case 'xdr': i64 = cntx->Rdx; break;
			case 'isr': i64 = cntx->Rsi; break;
			case 'idr': i64 = cntx->Rdi; break;
			case 'pbr': i64 = cntx->Rbp; break;
			case 'psr': i64 = cntx->Rsp; break;
			case '8r' : i64 = cntx->R8 ; break;
			case '9r' : i64 = cntx->R9 ; break;
			case '01r': i64 = cntx->R10; break;
			case '11r': i64 = cntx->R11; break;
			case '21r': i64 = cntx->R12; break;
			case '31r': i64 = cntx->R13; break;
			case '41r': i64 = cntx->R14; break;
			case '51r': i64 = cntx->R15; break;
			case 'pir': i64 = cntx->Rip; break;
			case 'xae': i64 = (DWORD)cntx->Rax; break;
			case 'xbe': i64 = (DWORD)cntx->Rbx; break;
			case 'xce': i64 = (DWORD)cntx->Rcx; break;
			case 'xde': i64 = (DWORD)cntx->Rdx; break;
			case 'ise': i64 = (DWORD)cntx->Rsi; break;
			case 'ide': i64 = (DWORD)cntx->Rdi; break;
			case 'pbe': i64 = (DWORD)cntx->Rbp; break;
			case 'pse': i64 = (DWORD)cntx->Rsp; break;
			case 'pie': i64 = (DWORD)cntx->Rip; break;
			case 'd8r' : i64 = (DWORD)cntx->R8 ; break;
			case 'd9r' : i64 = (DWORD)cntx->R9 ; break;
			case 'd01r': i64 = (DWORD)cntx->R10; break;
			case 'd11r': i64 = (DWORD)cntx->R11; break;
			case 'd21r': i64 = (DWORD)cntx->R12; break;
			case 'd31r': i64 = (DWORD)cntx->R13; break;
			case 'd41r': i64 = (DWORD)cntx->R14; break;
			case 'd51r': i64 = (DWORD)cntx->R15; break;
#else
			case 'xae': i64 = cntx->Eax; break;
			case 'xbe': i64 = cntx->Ebx; break;
			case 'xce': i64 = cntx->Ecx; break;
			case 'xde': i64 = cntx->Edx; break;
			case 'ise': i64 = cntx->Esi; break;
			case 'ide': i64 = cntx->Edi; break;
			case 'pbe': i64 = cntx->Ebp; break;
			case 'pse': i64 = cntx->Esp; break;
			case 'pie': i64 = cntx->Eip; break;
#endif
			case '0rd': i64 = cntx->Dr0; break;
			case '1rd': i64 = cntx->Dr1; break;
			case '2rd': i64 = cntx->Dr2; break;
			case '3rd': i64 = cntx->Dr3; break;
			default: reg = FALSE;
			}

			if (reg)
			{
				m_res = i64;
				return TRUE;
			}
		} 
	}

	return m_pfn && m_pfn(m_pUser, sz, m_res);
}

_NT_END