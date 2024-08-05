#include "stdafx.h"

_NT_BEGIN

#include "pdb.h"
#include "common.h"
#include "udt.h"
#include "../winZ/str.h"

//#define _PRINT_CPP_NAMES_
#include "../inc/asmfunc.h"

size_t __fastcall strnlen(
			   size_t numberOfElements, 
			   const char *str
			   )ASM_FUNCTION;

BOOL g_udtInit, g_spheapInit;

#define _strnstr(a, b, x) strnstr(RtlPointerToOffset(a, b), a, sizeof(x) - 1, x)
#define _strnstrL(a, b, x) strnstr(RtlPointerToOffset(a, b), a, strlen(x), x)
#define _strnchr(a, b, c) strnchr(RtlPointerToOffset(a, b), a, c)
#define _strnstrS(a, b, s, x) strnstr(RtlPointerToOffset(a, b), a, s, x)

#define LP(str) RTL_NUMBER_OF(str) - 1, str
#define PL(str) str, RTL_NUMBER_OF(str) - 1 

__inline bool IsBeginSymbol(char c)
{
	switch (c)
	{
	case ' ':
	case ',':
	case '<':
	case '(':
	case '&':
		return true;
	}
	return false;
}

void SmartRemove(PSTR sz, PCSTR sbstr, ULONG len)
{
	PSTR end = sz + strlen(sz), c, to = 0, q = 0, from = 0, _sz = sz;

	while (c = _strnchr(sz, end, ':'))
	{
		if (*c == ':' && RtlPointerToOffset(sz, c) > len && !memcmp(q = c - len - 1, sbstr, len) && 
			(q == _sz || IsBeginSymbol(q[-1]) ))
		{
			if (to)
			{
				ULONG l = (ULONG)(q - from);
				memcpy(to, from, l);
				to += l;
			}
			else
			{
				to = q;
			}
			from = c + 1;
		}
		sz = c + 1;
	}

	if (to)
	{
		memcpy(to, from, 1 + end - from);
	}
}

BOOL IsOperator(PCSTR name, PCSTR pc)
{
	// c == '<' { <, <=, <<, <<= }
	// c == '>' { ->, ->*, >>=, >> , >, >= }

	for (char c = *--pc; ; c = 0)
	{
		switch (*--pc)
		{
		case '-':
		case '>':
			if (c == '>')
			{
				continue;
			}
			return FALSE;
		case '<':
			if (c == '<')
			{
				continue;
			}
			return FALSE;
		case 'r':
			return pc - 6 > name &&
				pc[-1] == 'o' && 
				pc[-2] == 't' && 
				pc[-3] == 'a' && 
				pc[-4] == 'r' && 
				pc[-5] == 'e' && 
				pc[-6] == 'p' && 
				pc[-7] == 'o' &&
				(pc - 7 == name || pc[-8] < 'A');
		default:
			return FALSE;
		}
	}
}

BOOL IsUnnamedTag(PCSTR name)
{
	switch (*name++)
	{
	case '<': return !strcmp(name, "unnamed-tag>")|| !strcmp(name, "anonymous-tag>");
	case '_': return !strcmp(name, "_unnamed");
	}
	return FALSE;
}

///////////////////////////////////////////////////////////////////////////////////////
// CLineHeap
#include "lineheap.h"

static _lineHeap gsh, gnh;

struct CNS : RTL_AVL_TABLE
{
	PCSTR _name;
	CNS* _next;
	CNS* _child;
	struct ZType* _first;

	void* operator new(size_t cb);

	static RTL_GENERIC_COMPARE_RESULTS NTAPI CompareRoutine(PRTL_AVL_TABLE, PVOID FirstStruct, PVOID SecondStruct);

	static PVOID NTAPI AllocateRoutine(PRTL_AVL_TABLE, CLONG ByteSize);

	ZType* GetType(PCSTR name);

	ZType* InsertType(USHORT leaf, PSTR name);

	CNS* _AddChild(PCSTR name);

	CNS(PCSTR name = 0);

	CNS* FindChild(PCSTR name);

	CNS* AddChild(PCSTR name);
} gns;

struct PRIVATE_UDT_CONTEXT : CNS 
{
	_lineHeap _sh, _nh;

	void* operator new(size_t cb)
	{
		return ::operator new(cb);
	}

	void operator delete (PVOID pv)
	{
		::operator delete(pv);
	}
};

struct _UDT_PARSE_CONTEXT
{
	_lineHeap* _psh;
	_lineHeap* _pnh;
	CNS* _pns;
	ZDbgDoc* _pDoc;
};

typedef RTL_FRAME<_UDT_PARSE_CONTEXT> UDT_PARSE_CONTEXT;

//////////////////////////////////////////////////////////////////////////

char* xstrcpy(char * _Dst, const char * _Src)
{
	char c;
	do *_Dst++ = c = *_Src++; while(c);
	return _Dst - 1;
}

PCSTR GetBasicTypeName(CV_typ_t utype)
{
	switch (utype)
	{
	case T_NOTYPE:
		return "...";
	case T_VOID:
		return "void";
	case T_RCHAR:
	case T_CHAR:
		return "CHAR";
	case T_UCHAR:
		return "UCHAR";
	case T_INT8:
		return "__int8";
	case T_BOOL08:
		return "BOOLEAN";
	case T_WCHAR:
		return "WCHAR";
	case T_UINT8:
		return "unsigned __int8";
	case T_SHORT:
		return "SHORT";
	case T_USHORT:
		return "USHORT";
	case T_INT4:
		return "INT";
	case T_UINT4:
		return "UINT";
	case T_LONG:
		return "LONG";
	case T_ULONG:
		return "ULONG";
	case T_HRESULT:
		return "HRESULT";
	case T_QUAD:
		return "LONGLONG";
	case T_UQUAD:
		return "ULONGLONG";
	case T_REAL32:
		return "float";
	case T_REAL64:
		return "double";
	case T_REAL80:
		return "REAL80";
	case T_REAL128:
		return "REAL128";
	}

	__debugbreak();
	return "??";
}

ULONG GetBasicTypeLen(CV_typ_t utype)
{
	switch (utype)
	{
	case T_NOTYPE:
	case T_VOID:
		return 0;
	case T_RCHAR:
	case T_CHAR:
	case T_UCHAR:
	case T_INT8:
	case T_BOOL08:
		return 1;
	case T_WCHAR:
	case T_UINT8:
	case T_SHORT:
	case T_USHORT:
	case T_CHAR16:
		return 2;
	case T_INT4:
	case T_UINT4:
	case T_LONG:
	case T_ULONG:
	case T_HRESULT:
	case T_REAL32:
	case T_CHAR32:
		return 4;
	case T_QUAD:
	case T_UQUAD:
	case T_REAL64:
		return 8;
	case T_REAL80:
		return 10;
	case T_REAL128:
		return 16;
	}

	return 0;
}

void* ZBase::operator new(size_t cb)
{
	return UDT_PARSE_CONTEXT::get()->_psh->alloc((LONG)cb);
}

void* ZMember::operator new(size_t cb)
{
	return UDT_PARSE_CONTEXT::get()->_psh->alloc((LONG)cb);
}

void* ZType::operator new(size_t cb)
{
	return UDT_PARSE_CONTEXT::get()->_psh->alloc((LONG)cb);
}

ULONG ZType::getSize()
{
	if (CV_IS_PRIMITIVE((ULONG_PTR)this))
	{
		return GetBasicTypeLen((ULONG)(ULONG_PTR)this);
	}

	switch (_leaf)
	{
	case LF_MODIFIER:
		return _pti->getSize();
	case LF_POINTER:
		return _pAttr.size;
	case LF_ARRAY:
		return _pti->getSize() * _len;
	case LF_ENUM:
	case LF_CLASS:
	case LF_STRUCTURE:
	case LF_UNION:
		return _len;
	}

	__debugbreak();
	return 0;
}

ZType* ZType::getNestedType(PCSTR name)
{
	if (ZMember* member = _member)
	{
		do 
		{
			if (member->_leaf == LF_NESTTYPE)
			{
				if (!strcmp(member->_type->_name, name))
				{
					return member->_type;
				}
			}
			else
			{
				return 0;
			}
		} while (member = member->_next);
	}

	return 0;
}

ZType* ZType::addNestedType(USHORT leaf, PCSTR name)
{
	//if (!strcmp(name, unnamed_tag))
	//{
	//	__nop();
	//}
	ZMember* pmi = new ZMember(LF_NESTTYPE);
	ZType* pti = new ZType(leaf);
	pmi->_type = pti;
	pti->_name = UDT_PARSE_CONTEXT::get()->_pnh->alloc(name);
	pmi->_next = _member;
	pmi->_attr.access = _leaf == LF_CLASS ? CV_private : CV_public;
	_member = pmi;
	return pti;
}

void* CNS::operator new(size_t cb)
{
	return UDT_PARSE_CONTEXT::get()->_psh->alloc((LONG)cb);
}

RTL_GENERIC_COMPARE_RESULTS NTAPI CNS::CompareRoutine(PRTL_AVL_TABLE, PVOID FirstStruct, PVOID SecondStruct)
{
	int i = strcmp(((ZType*)FirstStruct)->_name, ((ZType*)SecondStruct)->_name);
	if (i < 0) return GenericLessThan;
	if (0 < i) return GenericGreaterThan;
	return GenericEqual;
}

PVOID NTAPI CNS::AllocateRoutine(PRTL_AVL_TABLE, CLONG ByteSize)
{
	return UDT_PARSE_CONTEXT::get()->_psh->alloc(ByteSize);
}

ZType* CNS::GetType(PCSTR name)
{
	return (ZType*)RtlLookupElementGenericTableAvl(this, &name);
}

ZType* CNS::InsertType(USHORT leaf, PSTR name)
{
	BOOLEAN NewElement;
	ZType ti(leaf, name), *pti;
	if (pti = (ZType*)RtlInsertElementGenericTableAvl(this, &ti, sizeof(ti), &NewElement))
	{
		if (NewElement)
		{
			pti->_name = UDT_PARSE_CONTEXT::get()->_pnh->alloc(name);
		}
		return pti;
	}

	__debugbreak();
	return 0;
}

CNS* CNS::_AddChild(PCSTR name)
{
	if (name = UDT_PARSE_CONTEXT::get()->_pnh->alloc(name))
	{
		if (CNS* pns = new CNS(name))
		{
			pns->_next = _child;
			_child = pns;
			return pns;
		}
	}

	__debugbreak();
	return 0;
}

CNS::CNS(PCSTR name)
{
	_name = name;
	_next = 0, _child = 0;
	RtlInitializeGenericTableAvl(this, CompareRoutine, AllocateRoutine, 0, 0);
}

CNS* CNS::FindChild(PCSTR name)
{
	if (CNS* pns = _child)
	{
		do 
		{
			if (!strcmp(name, pns->_name))
			{
				return pns;
			}
		} while (pns = pns->_next);
	}

	return 0;
}

CNS* CNS::AddChild(PCSTR name)
{
	CNS* pns = FindChild(name);

	return pns ? pns : _AddChild(name);
}

//////////////////////////////////////////////////////////////////////////////////////
extern volatile const UCHAR guz;

void FillCB(CNS* pns, HWND hwnd)
{
	PVOID RestartKey = 0;
	DWORD cb = 0, rcb;
	PWSTR sz = 0, wz;
	PVOID stack = alloca(guz);

	while (ZType* pti = (ZType*)RtlEnumerateGenericTableWithoutSplayingAvl(pns, &RestartKey))
	{
		switch (pti->_leaf)
		{
		case LF_CLASS:
		case LF_UNION:
			break;
		default:
			DbgBreak();
		}

		if (PCSTR name = pti->_name)
		{
			if (cb < (rcb = (1 + (DWORD)strlen(name)) << 1))
			{
				cb = RtlPointerToOffset(sz = (PWSTR)alloca(rcb - cb), stack);
			}

			wz = sz;
			char c;
			do 
			{
				*wz++ = c = *name++;
			} while (c);

			int i = ComboBox_AddString(hwnd, sz);
			ComboBox_SetItemData(hwnd, i, pti);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////
void ZType::Add(ZMember* pmi)
{
	ZMember** pp = &_member, *p;
	while(p = *pp)
	{
		pp = &p->_next;
	}

	*pp = pmi;
	pmi->_next = 0;
}

void ZType::AddBaseType(ZType* pti, ULONG ofs, unsigned short value)
{
	if (ZBase* pb = new ZBase)
	{
		pb->_next = 0;
		pb->_type = pti;
		pb->_ofs = ofs;
		pb->_value = value;

		ZBase** pp = &_first, *p;

		while (p = *pp)
		{
			pp = &p->_next;
		}

		*pp = pb;
	}
}

//////////////////////////////////////////////////////////////////////////
STATIC_ASTRING(tmptst, "PARAM_DESCRIPTION");

ZType* getType(USHORT leaf, CV_prop_t property, PSTR name)
{
	//if (!memcmp(name, tmptst, sizeof(tmptst)-1))
	//{
	//	__nop();
	//}

	int lock = 0;

	PSTR c = name; 
	CNS* pns = UDT_PARSE_CONTEXT::get()->_pns, *p;
	ZType* pti = 0;

	for(;;) 
	{
		switch (*c++)
		{
		case ' ':
			if (lock)
			{
				continue;
			}
			if (!IsOperator(name, c))
			{
				__debugbreak();
			}
		case 0:
			if (lock)
			{
				__debugbreak();
			}

			if (IsUnnamedTag(name))
			{
				return new ZType(leaf);
			}

			if (property.isnested)
			{
				if (!pti || !(pti = pti->getNestedType(name)))
				{
					__debugbreak();
				}
			}
			else
			{
				if (!(pti = pns->GetType(name)))
				{
					__debugbreak();
				}
			}
			return pti;

		case '<':
			if (!IsOperator(name, c))
			{
				lock++;
			}
			continue;

		case '>':
			if (!IsOperator(name, c))
			{
				if (!lock--)
				{
					__debugbreak();
				}
			}
			continue;

		case ':':
			if (!lock)
			{
				if (*c != ':')
				{
					__debugbreak();
				}

				c[-1] = 0;

				SmartRemove(c, name, (ULONG)strlen(name));

				if (pti)
				{
					if (!(pti = pti->getNestedType(name)))
					{
						__debugbreak();
					}
				}
				else
				{
					if (p = pns->FindChild(name))
					{
						pns = p;
					}
					else
					{
						if (property.isnested)
						{
							if (!(pti = pns->GetType(name)))
							{
								__debugbreak();
							}
						}
						else
						{
							__debugbreak();
						}
					}
				}

				c[-1] = ':';

				name = ++c;
			}
			continue;
		}
	}
}

void InsertNestedUDT(USHORT leaf, CV_prop_t property, PSTR name, ULONG len)
{
	if ((property.value & (prop_isnested|prop_scoped)) == prop_isnested)
	{
		//if (!strcmp(name, "_userSTGMEDIUM::_STGMEDIUM_UNION"))__debugbreak();
		int lock = 0;

		PSTR c = name; 
		CNS* pns = UDT_PARSE_CONTEXT::get()->_pns, *p;
		ZType* pti = 0, *pt;

		for(;;) 
		{
			switch (*c++)
			{
			case ' ':
				if (lock)
				{
					continue;
				}
				if (!IsOperator(name, c))
				{
					__debugbreak();
				}

			case 0:
				if (lock || !pti)
				{
					__debugbreak();
				}

				if (!IsUnnamedTag(name))
				{
					if (pt = pti->getNestedType(name))
					{
						pt->_leaf = leaf;
						if (!property.fwdref)
						{
							pt->_len = len;
						}
					}
					else
					{
						if (pti = pti->addNestedType(leaf, name))
						{
							if (!property.fwdref)
							{
								pti->_len = len;
							}
						}
						else
						{
							__debugbreak();
						}
					}
				}
				return;

			case '<':
				if (!IsOperator(name, c))
				{
					lock++;
				}
				continue;

			case '>':
				if (!IsOperator(name, c))
				{
					if (!lock--)
					{
						__debugbreak();
					}
				}
				continue;

			case ':':
				if (!lock)
				{
					if (*c != ':')
					{
						__debugbreak();
					}

					c[-1] = 0;

					SmartRemove(c, name, (ULONG)strlen(name));

					if (pti)
					{
						if (pt = pti->getNestedType(name))
						{
							pti = pt;
						}
						else
						{
							if (!(pti = pti->addNestedType(0, name)))// leaf is unknown!!
							{
								__debugbreak();
							}
						}
					}
					else
					{
						if (p = pns->FindChild(name))
						{
							pns = p;
						}
						else
						{
							if (!(pti = pns->GetType(name)))
							{
								//__debugbreak();
								pti = pns->InsertType(LF_CLASS, name);// ???
							}
						}
					}

					c[-1] = ':';

					name = ++c;
				}
				continue;
			}
		}
	}
}

void InsertUDT(USHORT leaf, CV_prop_t property, PSTR name, ULONG len)
{
	if (!(property.value & (prop_isnested|prop_scoped)))
	{
		int lock = 0;
		ZType* pti = 0;

		PSTR c = name; 
		CNS* pns = UDT_PARSE_CONTEXT::get()->_pns;
		for(;;) 
		{
			switch (*c++)
			{
			case ' ':
				if (lock)
				{
					continue;
				}
				if (!IsOperator(name, c))
				{
					__debugbreak();
				}

			case 0:
				if (!IsUnnamedTag(name))
				{
					if (lock || !(pti = pns->InsertType(leaf, name)))
					{
						__debugbreak();
					}
					if (!property.fwdref)
					{
						pti->_len = len;
					}
				}
				return;

			case '<':
				if (!IsOperator(name, c))
				{
					lock++;
				}
				continue;

			case '>':
				if (!IsOperator(name, c))
				{
					if (!lock--)
					{
						__debugbreak();
					}
				}
				continue;

			case ':':
				if (!lock)
				{
					if (*c != ':')
					{
						__debugbreak();
					}

					c[-1] = 0;

					pns = pns->AddChild(name);

					SmartRemove(c, name, (ULONG)strlen(name));

					c[-1] = ':';

					name = ++c;
				}
				continue;
			}
		}
	}
}

PSTR GetLastNameComponent(PSTR name)
{
	int lock = 0;

	PSTR c = name; 
	for(;;) 
	{
		switch (*c++)
		{
		case 0:
			if (lock)
			{
				__debugbreak();
			}
			return name;

		case '<':
			if (!IsOperator(name, c))
			{
				lock++;
			}
			continue;

		case '>':
			if (!IsOperator(name, c))
			{
				if (!lock--)
				{
					__debugbreak();
				}
			}
			continue;

		case ':':
			if (!lock)
			{
				if (*c != ':')
				{
					__debugbreak();
				}

				SmartRemove(c, name, (ULONG)strlen(name));
				name = ++c;
			}
			continue;
		}
	}
}

ULONG IsNameOk(lfRecord* plr, PCSTR name, ULONG len)
{
	ULONG size = RtlPointerToOffset(plr, name);
	return size < len && (size += (ULONG)strnlen(len - size, name)) < len ? 1 + size : 0;
}

BOOL FillOfs(TYPTYPE* ptr, LONG cb, ULONG dTypes, PULONG pOfs)
{
	ULONG ofs = sizeof(USHORT);

	do 
	{
		if (0 > (cb -= sizeof(USHORT)))
		{
			return FALSE;
		}

		ULONG len = ptr->len;
		LONG length;

		if (len < sizeof(USHORT) || (0 > (cb -= len)))
		{
			return FALSE;
		}

		*pOfs++ = ofs, ofs += len + sizeof(USHORT);

		lfRecord* plr = &ptr->u;

		PSTR name;

		switch (plr->leaf)
		{
		case LF_UNION:
			if (
				len <= sizeof(lfUnion) 
				|| 
				!plr->Union.length_name.value(&length, &name) 
				||
				!IsNameOk(plr, name, len)
				)
			{
				return FALSE;
			}
			InsertUDT(LF_UNION, plr->Union.property, name, length);
			break;

		case LF_CLASS:
		case LF_STRUCTURE:
			if (
				len <= sizeof(lfClass) 
				|| 
				!plr->Class.length_name.value(&length, &name) 
				||
				!IsNameOk(plr, name, len)
				)
			{
				return FALSE;
			}
			InsertUDT(LF_CLASS, plr->Class.property, name, length);

			break;
		}

		ptr = ptr->Next();

	} while (--dTypes);

	return TRUE;
}

class TiEnum
{
	PVOID _Base;
	PULONG _pOfs;
	ULONG _tiMin, _dTypes, _dwPointerSize;
	BOOL _Is64Bit;

public:

	TiEnum(PVOID Base, PULONG pOfs, ULONG tiMin, ULONG dTypes, BOOL Is64Bit)
	{
		_Base = Base, _pOfs = pOfs, _tiMin = tiMin, _dTypes = dTypes, _Is64Bit = Is64Bit;
		_dwPointerSize = Is64Bit ? 8 : 4;
	}

	BOOL ProcessFieldList(ZType* pType, CV_typ_t field);

	void Round2();
	void Round3();
	ZType* CreateUDT(lfRecord* plr, BOOL bRecursive);
	ZType* _CreateUDT(CV_typ_t utype);

	lfRecord* IndexToType(CV_typ_t utype)
	{
		if ((utype -= _tiMin) >= _dTypes)
		{
			__debugbreak();
			return FALSE;
		}

		return (lfRecord*)RtlOffsetToPointer(_Base, _pOfs[utype]);
	}
};

void TiEnum::Round2()
{
	CV_typ_t index = _dTypes;
	PVOID Base = _Base;
	PULONG pOfs = _pOfs;
	LONG len;
	PSTR name = 0;
	do 
	{
		lfRecord* plr = (lfRecord*)RtlOffsetToPointer(Base, pOfs[--index]);

		switch (plr->leaf)
		{
		case LF_UNION:
			plr->Union.length_name.value(&len, &name);
			InsertNestedUDT(LF_UNION, plr->Union.property, name, len);
			break;

		case LF_CLASS:
		case LF_STRUCTURE:
			plr->Class.length_name.value(&len, &name);
			InsertNestedUDT(LF_CLASS, plr->Class.property, name, len);
			break;
		}
	} while (index);
}

void TiEnum::Round3()
{
	PVOID Base = _Base;
	PULONG pOfs = _pOfs;

	CV_typ_t index = _dTypes;
	do 
	{
		CreateUDT((lfRecord*)RtlOffsetToPointer(Base, pOfs[--index]), FALSE);
	} while (index);
}

ZType* TiEnum::_CreateUDT(CV_typ_t utype)
{
	if (CV_IS_PRIMITIVE(utype))
	{
		if (utype == T_PVOID)
		{
			utype = _Is64Bit ? T_64PVOID : T_32PVOID;
		}
		ULONG len = 0;
		switch (utype >> 8)
		{
		case 0:
			break;
		case 0x04:
			len = 4;
			break;
		case 0x06:
			len = 8;
			break;
		default:
			__debugbreak();
			return 0;
		}

		switch (utype &= 0xff)
		{
		case T_NOTYPE:
		case T_CHAR:
		case T_UCHAR:
		case T_RCHAR:
		case T_WCHAR:
		case T_INT8:
		case T_UINT8:
		case T_INT4:
		case T_UINT4:
		case T_LONG:
		case T_ULONG:
		case T_SHORT:
		case T_USHORT:
		case T_VOID:
		case T_HRESULT:
		case T_QUAD:
		case T_UQUAD:
		case T_BOOL08:
		case T_REAL32:
		case T_REAL64:
		case T_REAL80:
		case T_REAL128:
			break;
		default:__debugbreak();
		}

		if (len)
		{
			ZType* pType = new ZType(LF_POINTER);
			pType->_pti = (ZType*)(utype & 0xff);
			pType->_pAttr.value = 0;
			//pType->_pAttr.ptrmode = CV_PTR_MODE_PTR; // CV_PTR_MODE_PTR == 0
			pType->_pAttr.ptrtype = _Is64Bit ? CV_PTR_64 : CV_PTR_NEAR32;
			pType->_pAttr.size = len;
			return pType;
		}

		return (ZType*)(ULONG_PTR)utype;
	}
	else
	{
		return CreateUDT(IndexToType(utype), TRUE);
	}
}

ZType* TiEnum::CreateUDT(lfRecord* plr, BOOL bRecursive)
{
	ZType* pti;
	ULONG cb;
	PSTR name;
	LONG len;

	switch (plr->leaf)
	{
	case LF_MODIFIER:
		if (bRecursive)
		{
			if (*((PUSHORT)plr - 1) < sizeof(lfModifier))
			{
				__debugbreak();
			}

			return _CreateUDT(plr->Modifier.type);
		}
		break;

	case LF_PROCEDURE:
		if (bRecursive)
		{
			return _CreateUDT(T_VOID);
		}
		break;

	case LF_POINTER:
		if (bRecursive)
		{
			if (*((PUSHORT)plr - 1) < sizeof(lfPointerBody))
			{
				__debugbreak();
			}

			switch (plr->Pointer.attr.ptrmode)
			{
			case CV_PTR_MODE_PTR:
			case CV_PTR_MODE_REF:
				if (!plr->Pointer.attr.size)
				{
					switch (plr->Pointer.attr.ptrtype)
					{
					case CV_PTR_NEAR32:
						plr->Pointer.attr.size = 4;
						break;
					case CV_PTR_64:
						plr->Pointer.attr.size = 8;
						break;
					}
				}

				if (plr->Pointer.attr.size == _dwPointerSize)
				{
					pti = new ZType(LF_POINTER);
					pti->_pAttr = plr->Pointer.attr;
					pti->_pti = _CreateUDT(plr->Pointer.utype);
					return pti;
				}
				else
				{
					plr->Pointer.attr.size = 0;
				}
				break;
			}
		}
		return 0;

	case LF_BITFIELD:
		if (bRecursive)
		{
			if (*((PUSHORT)plr - 1) < sizeof(lfBitfield))
			{
				__debugbreak();
			}
			pti = new ZType(LF_BITFIELD);
			pti->_length = plr->Bitfield.length;
			pti->_position = plr->Bitfield.position;
			pti->_pti = _CreateUDT(plr->Bitfield.type);
			return pti;
		}
		break;

	case LF_ARRAY:
		if (bRecursive)
		{
			cb = *((PUSHORT)plr - 1);
			if (
				cb < sizeof(lfArray) ||
				!plr->Array.length_name.value(&len, &name) ||
				!IsNameOk(plr, name, cb)
				)
			{
				__debugbreak();
				return 0;
			}

			if (pti = new ZType(LF_ARRAY))
			{
				if (pti->_pti = _CreateUDT(plr->Array.elemtype))
				{
					if (len)
					{
						ULONG size = pti->_pti->getSize();
						if (len % size)
						{
							__debugbreak();
							return 0;
						}
						pti->_len = len / size;
					}
					else
					{
						pti->_len = 0;
					}
					return pti;
				}
			}
		}
		return 0;

	case LF_ENUM:
		if (bRecursive)
		{
			return _CreateUDT(plr->Enum.utype);
		}
		break;

	case LF_UNION:
		if (!plr->Union.property.scoped)
		{
			if (
				!plr->Union.length_name.value(&len, &name)
				||
				!(pti = getType(LF_UNION, plr->Union.property, name)) 
				|| 
				pti->_leaf != LF_UNION
				)
			{
				__debugbreak();
				return 0;
			}

			if (!pti->_bProcessed && !plr->Union.property.fwdref)
			{
				pti->_bProcessed = TRUE;
				pti->_len = len;
				ProcessFieldList(pti, plr->Union.field);
			}

			return pti;
		}
		break;

	case LF_CLASS:
	case LF_STRUCTURE:
		if (!plr->Class.length_name.value(&len, &name))
		{
			__debugbreak();
		}

		if (plr->Class.property.scoped)
		{
			if (bRecursive)
			{
				pti = new ZType(plr->leaf);
				pti->_name = UDT_PARSE_CONTEXT::get()->_pnh->alloc(GetLastNameComponent(name));
				return pti;
			}
		}
		else
		{
			if (!(pti = getType(LF_CLASS, plr->Class.property, name)) || pti->_leaf != plr->leaf)
			{
				switch (pti->_leaf)
				{
				case LF_CLASS:
				case LF_STRUCTURE:
					break;
				default:
					__debugbreak();
					return 0;
				}
			}

			if (!pti->_bProcessed && !plr->Class.property.fwdref)
			{
				//if (pti->_name && !strcmp(pti->_name, "_LIST_ENTRY"))__debugbreak();//$$$
				pti->_bProcessed = TRUE;
				pti->_len = len;
				ProcessFieldList(pti, plr->Class.field);
			}

			return pti;
		}
		break;
	}

	//if (bRecursive)
	//{
	//	__debugbreak();
	//}

	return 0;
}

BOOL TiEnum::ProcessFieldList(ZType* pType, CV_typ_t field)
{
	if (!field)
	{
		return TRUE;
	}

	lfRecord* plr = IndexToType(field);

	LONG len = *((PUSHORT)plr - 1);

	if (0 > (len -= sizeof(lfFieldList)))
	{
		__debugbreak();
		return FALSE;
	}

	if (plr->leaf != LF_FIELDLIST)
	{
		__debugbreak();
		return FALSE;
	}

	if (!len)
	{
		return TRUE;
	}

	PUCHAR pc = plr->FieldList.data;
	PSTR name;
	LONG offset, value;

	ULONG cb;

	do 
	{
		if (*pc >= LF_PAD0)
		{
			pc++, len--;
			continue;
		}

		plr = (lfRecord*)pc;
		ZMember* pmi;

		switch (plr->leaf)
		{
		case LF_VBCLASS:
		case LF_IVBCLASS:
			if (
				len <= sizeof(lfVBClass)
				||
				!plr->VBClass.get_end(&value, &offset, &name)
				||
				(ULONG)len <= (cb = RtlPointerToOffset(plr, name))
				)
			{
				__debugbreak();
				return FALSE;
			}
			pc = (PUCHAR)RtlOffsetToPointer(plr, cb);
			len -= cb;

			break;

		case LF_BCLASS:
			if (
				len <= sizeof(lfBClass)
				||
				!plr->BClass.offset.value(&value, &name)
				||
				(ULONG)len <= (cb = RtlPointerToOffset(plr, name))
				)
			{
				__debugbreak();
				return FALSE;
			}
			pc = (PUCHAR)RtlOffsetToPointer(plr, cb);
			len -= cb;

			break;

		case LF_ENUMERATE:
			if (
				len <= sizeof(lfEnumerate)
				||
				!plr->Enumerate.value_name.value(&value, &name)
				||
				!(cb = IsNameOk(plr, name, len))
				)
			{
				__debugbreak();
				return FALSE;
			}
			pc = (PUCHAR)RtlOffsetToPointer(plr, cb);
			len -= cb;

			break;

		case LF_MEMBER:
			if (
				len <= sizeof(lfMember)
				||
				!plr->Member.offset_name.value(&value, &name)
				||
				!(cb = IsNameOk(plr, name, len))
				)
			{
				__debugbreak();
				return FALSE;
			}
			pc = (PUCHAR)RtlOffsetToPointer(plr, cb);
			len -= cb;

			pmi = new ZMember(LF_MEMBER);
			pmi->_offset = value;
			pmi->_attr = plr->Member.attr;
			pmi->_name = UDT_PARSE_CONTEXT::get()->_pnh->alloc(name);
			pmi->_type = _CreateUDT(plr->Member.index);
			pType->Add(pmi);
			break;

		case LF_STMEMBER:
			name = plr->StMember.name;
			if (
				len <= sizeof(lfSTMember)
				||
				!(cb = IsNameOk(plr, name, len))
				)
			{
				__debugbreak();
				return FALSE;
			}
			pc = (PUCHAR)RtlOffsetToPointer(plr, cb);
			len -= cb;

			break;

		case LF_METHOD:
			name = plr->Method.name;
			if (
				len <= sizeof(lfMethod)
				||
				!(cb = IsNameOk(plr, name, len))
				)
			{
				__debugbreak();
				return FALSE;
			}
			pc = (PUCHAR)RtlOffsetToPointer(plr, cb);
			len -= cb;

			break;

		case LF_ONEMETHOD:
			if (
				len <= sizeof(lfOneMethod)
				||
				!(cb = IsNameOk(plr, name = plr->OneMethod.get_name(&offset), len))
				)
			{
				__debugbreak();
				return FALSE;
			}
			pc = (PUCHAR)RtlOffsetToPointer(plr, cb);
			len -= cb;

			break;

		case LF_NESTTYPE:
		case LF_NESTTYPEEX:
			if (
				len <= sizeof(lfNestTypeEx)
				||
				!(cb = IsNameOk(plr, name = plr->NestType.name, len))
				)
			{
				__debugbreak();
				return FALSE;
			}
			pc = (PUCHAR)RtlOffsetToPointer(plr, cb);
			len -= cb;

			break;

		case LF_VFUNCTAB:
			if (0 > (len -= sizeof(lfVFuncTab)))
			{
				__debugbreak();
				return FALSE;
			}
			pc = (PUCHAR)plr + sizeof(lfVFuncTab);
			break;

		case LF_INDEX:
			if (0 > (len -= sizeof(lfIndex)))
			{
				__debugbreak();
				return FALSE;
			}
			pc = (PUCHAR)plr + sizeof(lfIndex);
			if (!ProcessFieldList(pType, plr->Index.index))
			{
				return FALSE;
			}
			break;

		default:
			__debugbreak();
		}

	} while (len);

	return TRUE;
}


BOOL TPIDumpTypes (TPIHDR* pHdr, LONG cb, BOOL Is64Bit)
{
	LONG cbHdr = pHdr->cbHdr;

	if (cb < sizeof(cbHdr))
	{
		return FALSE;
	}

	ULONG tiMin = pHdr->tiMin;

	CV_typ_t dTypes = pHdr->tiMax - tiMin;

	if (!dTypes || tiMin < CV_FIRST_NONPRIM)
	{
		return FALSE;
	}

	if (PULONG pOfs = new ULONG[dTypes])
	{
		PVOID pData = RtlOffsetToPointer(pHdr, cbHdr);
		BOOL fOk = FALSE;
		__try
		{
			if (FillOfs((TYPTYPE*)pData, cb - cbHdr, dTypes, pOfs))
			{
				TiEnum te(pData, pOfs, tiMin, dTypes, Is64Bit);
				te.Round2();
				te.Round3();
				fOk = TRUE;
			}
		}
		__except(EXCEPTION_EXECUTE_HANDLER)
		{
		}
		delete pOfs;

		return fOk;
	}

	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
//
#define _DBG_

struct AaT
{
	PCSTR _name;
	PUCHAR _Address;
	ZType* _pType;
	PUCHAR _buf;// for LF_POINTER
};

struct SpecialHeap 
{
	PVOID _BaseAddress;
	PSINGLE_LIST_ENTRY _pFirst;
#ifdef _DBG_
	ULONG _n, _N, _Max;
#endif

	AaT* alloc()
	{
		if (PSINGLE_LIST_ENTRY p = _pFirst)
		{
#ifdef _DBG_
			if (_Max < (_N - --_n))
			{
				_Max = _N - _n;
			}
#endif
			_pFirst = p->Next;
			__stosp((PULONG_PTR)p, 0, sizeof(AaT)/sizeof(ULONG_PTR));
			return (AaT*)p;
		}

		return 0;
	}

	void free(PSINGLE_LIST_ENTRY p)
	{
		p->Next = _pFirst;
		_pFirst = p;
#ifdef _DBG_
		++_n;
#endif
	}

	BOOL Init(SIZE_T RegionSize)
	{
		if (0 > ZwAllocateVirtualMemory(NtCurrentProcess(), &_BaseAddress, 0, &RegionSize, MEM_COMMIT, PAGE_READWRITE))
		{
			return FALSE;
		}

		DWORD n = (DWORD)RegionSize / sizeof(AaT);
		_pFirst = (PSINGLE_LIST_ENTRY)_BaseAddress;
		PSINGLE_LIST_ENTRY p = _pFirst;
#ifdef _DBG_
		_n = n, _N = n, _Max = 0;
#endif
		do 
		{
			p = p->Next = (PSINGLE_LIST_ENTRY)RtlOffsetToPointer(p, sizeof(AaT));
		} while (--n);

		return TRUE;
	}

	SpecialHeap()
	{
		_BaseAddress = 0;
		g_spheapInit = g_spheap.Init(0x100000);
	}

	~SpecialHeap()
	{
		if (_BaseAddress)
		{
#ifdef _DBG_
			if (_n != _N)
			{
				__debugbreak();
			}
			DbgPrint("PeekUsage %u/%u\n", _Max, _N);
#endif
			SIZE_T RegionSize = 0;
			ZwFreeVirtualMemory(NtCurrentProcess(), &_BaseAddress, &RegionSize, MEM_RELEASE);
		}
	}
} g_spheap;

void FreeUdtData(AaT* p)
{
	if (p)
	{
		if (p->_buf)// pointer node
		{
			delete p->_buf;
		}
		g_spheap.free((PSINGLE_LIST_ENTRY)p);
	}
}

STATIC_ASTRING(emptyname, "");

void GetNodeText(LPTVITEM item)
{
	PCSTR name = ((AaT*)item->lParam)->_name;
	PUCHAR Address = ((AaT*)item->lParam)->_Address;

	union {
		ZType* pti;
		CV_typ_t utype;
	};

	pti = ((AaT*)item->lParam)->_pType;

	int index = -1;

	if (IS_INTRESOURCE(name))// LF_ARRAY ?
	{
		index = (ULONG)(ULONG_PTR)name;
		name = 0;
	}

	USHORT leaf = CV_IS_PRIMITIVE((ULONG_PTR)pti) ? 0 : pti->_leaf;

	if (item->mask & TVIF_CHILDREN)
	{
		switch (leaf)
		{
		case LF_POINTER:
			item->cChildren = pti->_pAttr.size && pti->_pti->getSize();
			break;
		case LF_ARRAY:
			item->cChildren = pti->_len != 0;
			break;
		case LF_CLASS:
		case LF_UNION:
			item->cChildren = pti->_member != 0;
			break;
		default: 
			item->cChildren = 0;
		}
	}

	if (item->mask & TVIF_TEXT)
	{
		PWSTR pszText = item->pszText;
		int cchTextMax = item->cchTextMax;

		if (!cchTextMax)
		{
			return ;
		}
		pszText[--cchTextMax] = 0;

		int len = 0;

		if (name) // !pointer node
		{
			len = _snwprintf(pszText, cchTextMax, L"%S", name);
		} 
		else if (0 <= index)
		{
			len = _snwprintf(pszText, cchTextMax, L"[0x%x]", index);
		}

		if (len < 0)
		{
			return;
		}

		cchTextMax -= len, pszText += len;

		union {
			UCHAR u1;
			USHORT u2;
			ULONG u4;
			ULONGLONG u8;
			PVOID ptr;
		};

		ULONGLONG mask;
		unsigned char position, length;
		PCWSTR format;

		switch (leaf)
		{
		case LF_POINTER:
			format = L" = 0x%p  // * ";
			if (name == emptyname) format += 3; // skip " = "
			ptr = 0;
			memcpy(&ptr, Address, pti->_pAttr.size);
			len = _snwprintf(pszText, cchTextMax, format, ptr);
			goto ar;
		case LF_CLASS:
		case LF_UNION:
			if ((len = _snwprintf(pszText, cchTextMax, L"  // ")) < 0)
			{
				return;
			}

			cchTextMax -= len, pszText += len;
cu:
			_snwprintf(pszText, cchTextMax, pti->_name ? L"%S {}" : L"{}", pti->_name);
			return;
		case LF_ARRAY:
			len = _snwprintf(pszText, cchTextMax, pti->_len ? L"[0x%x]  // " : L"[]  // ", pti->_len);
ar:
			if (len < 0)
			{
				return;
			}

			cchTextMax -= len, pszText += len;
			pti = pti->_pti;
			if (CV_IS_PRIMITIVE((ULONG_PTR)pti))
			{
				_snwprintf(pszText, cchTextMax, L"%S", GetBasicTypeName(utype));
			}
			else
			{
				switch (pti->_leaf)
				{
				case LF_CLASS:
				case LF_UNION:
					goto cu;
				case LF_POINTER:
					_snwprintf(pszText, cchTextMax, L"*");
					break;
				case LF_ARRAY:
					_snwprintf(pszText, cchTextMax, pti->_len ? L"[0x%x]" : L"[]", pti->_len);
					break;
				}
			}
			return;

		case LF_BITFIELD:
			position = pti->_position, length = pti->_length;
			mask = ((1ULL << length) - 1) << position;
			pti = pti->_pti;

			if (CV_IS_PRIMITIVE((ULONG_PTR)pti))
			{
				len = GetBasicTypeLen(utype);

				u8 = 0, __movsb(&u1, Address, len);
				mask &= u8;
				u8 >>= position;
				u8 &= (1ULL << length) - 1;

				_snwprintf(pszText, cchTextMax, L" = 0x%I64x  // %S : %u [0x%I64x]", u8, GetBasicTypeName(utype), length, mask);
			}
			break;

		default:
			if (CV_IS_PRIMITIVE((ULONG_PTR)pti))
			{
				u8 = 0, format = 0;

				if (len = GetBasicTypeLen(utype))
				{
					__movsb(&u1, Address, len);
				}

				switch (utype)
				{
				case T_PWCHAR:
					_snwprintf(pszText, cchTextMax, L"\"%.128s\"", (PWSTR)Address);
					return;
				case T_PCHAR:
					_snwprintf(pszText, cchTextMax, L"\"%.128S\"", (PSTR)Address);
					return;

				case T_RCHAR:
				case T_UCHAR:
				case T_CHAR:
				case T_WCHAR:
					if (0 <= index || name == emptyname)
					{
						if (u1)
						{
							format = L" = '%c' [0x%x]  // %S";
							if (name == emptyname) format += 3; // skip " = "
							_snwprintf(pszText, cchTextMax, format, u2, u2, GetBasicTypeName(utype));
						}
						else
						{
							format = L" = 0x%x  // %S";
							if (name == emptyname) format += 3; // skip " = "
							_snwprintf(pszText, cchTextMax, format, u2, GetBasicTypeName(utype));
						}
						return;
					}
				default:
					switch (len)
					{
					case 1:
						format = L" = 0x%02I64x  // %S";
						break;
					case 2:
						format = L" = 0x%04I64x  // %S";
						break;
					case 4:
						format = L" = 0x%08I64x  // %S";
						break;
					case 8:
						format = L" = 0x%016I64x  // %S";
						break;
					}
				}

				if (format)
				{
					if (name == emptyname) format += 3; // skip " = "
					_snwprintf(pszText, cchTextMax, format, u8, GetBasicTypeName(utype));
				}
			}
		}
	}
}

BOOL isTextAscii(PCHAR pc, int len)
{
	if (!*pc) return FALSE;
	char c;
	do 
	{
		if (!(c = *pc++)) return TRUE;
		if (c < ' ') return FALSE;
	} while (--len);
	return TRUE;
}

BOOL isTextUnicode(const void* pBuffer, int len)
{
	INT f;
	if (PWSTR pz = wtrnchr(len, pBuffer, 0))
	{
		len = RtlPointerToOffset(pBuffer, pz);
	}
	else
	{
		len <<= 1;
	}
	return *(PWSTR)pBuffer && IsTextUnicode(pBuffer, len, &(f = IS_TEXT_UNICODE_STATISTICS));
}

void CheckForString(HWND hwndTV, LPTVINSERTSTRUCT tv, ZType* pti, PUCHAR Address, int n)
{
	TYPE_ENUM_e t;  

	switch ((ULONG_PTR)pti)
	{
	case T_WCHAR:
		if (isTextUnicode(Address, n))
		{
			t = T_PWCHAR;
			break;
		}
		return;
	case T_CHAR:
	case T_RCHAR:
	case T_UCHAR:
		if (isTextAscii((PSTR)Address, n))
		{
			t = T_PCHAR;
			break;
		}
	default: return;
	}

	if (AaT *pnew = g_spheap.alloc())
	{
		pnew->_Address = Address;
		pnew->_name = emptyname;
		pnew->_pType = (ZType*)t;
		tv->item.lParam = (LPARAM)pnew;

		if (!TreeView_InsertItem(hwndTV, tv))
		{
			delete pnew;
		}
	}
}

BOOL ExpandUDT(ZDbgDoc* pDoc, HWND hwndTV, HTREEITEM hParent, AaT* p)
{
	ZType* pti = p->_pType;
	PUCHAR Address = p->_Address;

	TVINSERTSTRUCT tv;
	tv.hParent = hParent;
	tv.hInsertAfter = TVI_LAST;
	tv.item.mask = TVIF_TEXT|TVIF_PARAM|TVIF_CHILDREN;
	tv.item.cChildren = I_CHILDRENCALLBACK;
	tv.item.pszText = LPSTR_TEXTCALLBACK;

	AaT *pnew;

	switch (pti->_leaf)
	{
	case LF_ARRAY:
		if (ULONG n = pti->_len)
		{
			if (n > 256)
			{
				n = 256;
			}

			pti = pti->_pti;

			CheckForString(hwndTV, &tv, pti, Address, n);

			int k = 0, s = pti->getSize();
			do 
			{
				if (pnew = g_spheap.alloc())
				{
					pnew->_Address = Address;
					pnew->_name = (PCSTR)(k++);
					pnew->_pType = pti;
					tv.item.lParam = (LPARAM)pnew;

					if (!TreeView_InsertItem(hwndTV, &tv))
					{
						delete pnew;
						break;
					}
					Address += s;
				}
			} while (--n);

			return FALSE;
		}
		break;

	case LF_CLASS:
	case LF_UNION:
		if (ZMember* member = pti->_member)
		{
			do 
			{
				if (pnew = g_spheap.alloc())
				{
					pnew->_Address = Address + member->_offset;
					pnew->_name = member->_name;
					pnew->_pType = member->_type;
					tv.item.lParam = (LPARAM)pnew;

					if (!TreeView_InsertItem(hwndTV, &tv))
					{
						delete pnew;
						break;
					}
				}
			} while (member = member->_next);

			return FALSE;
		}
		break;

	case LF_POINTER:
		if (ULONG ptr_size = pti->_pAttr.size)
		{
			pti = pti->_pti;

			ULONG size = pti->getSize();

			// special case for string
			switch ((ULONG_PTR)pti)
			{
			case T_WCHAR:
				size = 128*sizeof(WCHAR);
				break;
			case T_CHAR:
			case T_RCHAR:
			case T_UCHAR:
				size = 128;
				break;
			}

			if (size)
			{
				if (size <= MAXUSHORT)
				{
					if (PUCHAR pc = new UCHAR[size])
					{
						PVOID ptr = 0;
						memcpy(&ptr, Address, ptr_size);
						if (0 <= pDoc->Read(ptr, pc, size))
						{
							p->_buf = pc;

							CheckForString(hwndTV, &tv, pti, pc, size >> 1);

							if (CV_IS_PRIMITIVE((ULONG_PTR)pti) || pti->_leaf == LF_POINTER)
							{
								if (pnew = g_spheap.alloc())
								{
									pnew->_Address = pc;
									pnew->_name = emptyname;
									pnew->_pType = pti;
									tv.item.lParam = (LPARAM)pnew;

									if (TreeView_InsertItem(hwndTV, &tv))
									{
										return FALSE;
									}
									delete pnew;
								}
								return TRUE;
							}

							AaT aat = { 0, pc, pti };
							return ExpandUDT(pDoc, hwndTV, hParent, &aat);
						}
						delete pc;
					}
				}
			}
		}
		break;
	}

	return TRUE;
}

PVOID RootExpand(HWND hwndTV, PVOID Address, ZType* pType)
{
	if (AaT *pnew = g_spheap.alloc())
	{
		union {
			ZType* pti;
			PUCHAR pc;
		};

		if (pc = new UCHAR[sizeof(ZType)])
		{
			pti->_leaf = LF_POINTER;
			pti->_pti = pType;
			pti->_pAttr.size = sizeof(PVOID);
			pti->_classname = (PCSTR)Address;

			pnew->_Address = (PUCHAR)&pti->_classname;
			pnew->_name = emptyname;
			pnew->_pType = pti;

			TVINSERTSTRUCT tv;
			tv.hParent = TVI_ROOT;
			tv.hInsertAfter = TVI_LAST;
			tv.item.mask = TVIF_TEXT|TVIF_PARAM|TVIF_CHILDREN;
			tv.item.cChildren = I_CHILDRENCALLBACK;
			tv.item.pszText = LPSTR_TEXTCALLBACK;
			tv.item.lParam = (LPARAM)pnew;

			if (TreeView_InsertItem(hwndTV, &tv))
			{
				return pc;
			}

			delete pc;
		}

		delete pnew;
	}

	return 0;
}

BOOL TPIDumpTypesAndSave(TPIHDR* pHdr, LONG cb, BOOL Is64Bit)
{
	_UDT_PARSE_CONTEXT* pupc = UDT_PARSE_CONTEXT::get();
	
	if (!pupc->_psh->Create(0x2000000, sizeof(PVOID)) || !pupc->_pnh->Create(0x1000000, sizeof(char))) return FALSE;

	BOOL b = TPIDumpTypes(pHdr, cb, Is64Bit) && g_spheap.Init(0x100000);

	pupc->_pnh->Compact(), pupc->_psh->Compact();

	if (b)
	{
		if (ZType* pti = pupc->_pns->GetType("_UNICODE_STRING"))
		{
			if (!CV_IS_PRIMITIVE((ULONG_PTR)pti) && pti->_leaf == LF_CLASS)
			{
				if (ZMember* pmem = pti->_member)
				{
					do 
					{
						if (pmem->_name && !strcmp(pmem->_name, "Buffer"))
						{
							pti = pmem->_type;
							if (!CV_IS_PRIMITIVE((ULONG_PTR)pti) && pti->_leaf == LF_POINTER)
							{
								pti->_pti = (ZType*)T_WCHAR;
							}
						}
					} while (pmem = pmem->_next);
				}
			}
		}
	}

	return b;
}

BOOL GetStruct(PVOID UdtCtx, PCSTR TypeName, struct ZType** ppti, ULONG* pSize)
{
	if (ZType* pti = reinterpret_cast<PRIVATE_UDT_CONTEXT*>(UdtCtx)->GetType(TypeName))
	{
		if (!CV_IS_PRIMITIVE((ULONG_PTR)pti) && pti->_leaf == LF_CLASS)
		{
			*ppti = pti;
			if (pSize)
			{
				*pSize = pti->_len;
			}
			return TRUE;
		}
	}

	return FALSE;
}

BOOL GetFieldOffset(struct ZType* pti, PCSTR FieldName, ULONG& ofs)
{
	if (ZMember* pmem = pti->_member)
	{
		do 
		{
			if (pmem->_name && !strcmp(pmem->_name, FieldName))
			{
				ofs = pmem->_offset;
				return TRUE;
			}
		} while (pmem = pmem->_next);
	}

	return FALSE;
}

PVOID gLocalKernelImageBase;
ULONG gLocalKernelImageSize;

PVOID GetKernelBase()
{
	PVOID ImageBase = 0;
	NTSTATUS status;
	DWORD cb = 0x10000;

	union {
		PRTL_PROCESS_MODULES buffer;
		PVOID buf;
	};

	do 
	{
		status = STATUS_INSUFFICIENT_RESOURCES;

		if (buf = LocalAlloc(0, cb))
		{
			if (0 <= (status = ZwQuerySystemInformation(SystemModuleInformation, buf, cb, &cb)))
			{
				if (buffer->NumberOfModules)
				{
					gLocalKernelImageSize = buffer->Modules->ImageSize;

					ImageBase = buffer->Modules->ImageBase;

					gLocalKernelImageBase = ImageBase;
				}
			}
			LocalFree(buf);
		}

	} while (status == STATUS_INFO_LENGTH_MISMATCH);

	return ImageBase;
}

NTSTATUS OpenPdb(PdbReader* pdb, ZDbgDoc* pDoc, PVOID ImageBase, DWORD VirtualAddress, DWORD Size, PCWSTR PePath, PBOOL pbSystem);

BOOL LoadNtSymbols(ZDbgDoc* pDoc, PVOID ImageBase)
{
	union{
		IMAGE_DOS_HEADER idh;
		IMAGE_NT_HEADERS inth;
		IMAGE_NT_HEADERS32 inth32;
		IMAGE_NT_HEADERS64 inth64;
	};

	DWORD e_lfanew;

	BOOL Is64Bit;
	USHORT Machine, Magic;

	if (pDoc)
	{
		if (pDoc->Is64BitProcess())
		{
			Machine = IMAGE_FILE_MACHINE_AMD64;
			Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
			Is64Bit = TRUE;
		}
		else
		{
			Machine = IMAGE_FILE_MACHINE_I386;
			Magic = IMAGE_NT_OPTIONAL_HDR32_MAGIC;
			Is64Bit = FALSE;
		}
	}
	else
	{
#ifdef _WIN64
		Machine = IMAGE_FILE_MACHINE_AMD64;
		Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
		Is64Bit = TRUE;
#else
		Machine = IMAGE_FILE_MACHINE_I386;
		Magic = IMAGE_NT_OPTIONAL_HDR32_MAGIC;
		Is64Bit = FALSE;
#endif
	}

	if (
		0 > SymReadMemory(pDoc, ImageBase, &idh, sizeof(idh)) || 
		idh.e_magic != IMAGE_DOS_SIGNATURE ||
		0 > SymReadMemory(pDoc, RtlOffsetToPointer(ImageBase, e_lfanew = idh.e_lfanew), &inth, sizeof(inth)) ||
		inth.Signature != IMAGE_NT_SIGNATURE ||
		inth.FileHeader.Machine != Machine ||
		inth.OptionalHeader.Magic != Magic
		)
	{
		return FALSE;
	}

	ULONG VirtualAddress, Size;

	if (Is64Bit)
	{
		if (inth64.OptionalHeader.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_DEBUG)
		{
			return FALSE;
		}

		Size = inth64.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].Size;
		VirtualAddress = inth64.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].VirtualAddress;
	}
	else
	{
		if (inth32.OptionalHeader.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_DEBUG)
		{
			return FALSE;
		}

		Size = inth32.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].Size;
		VirtualAddress = inth32.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].VirtualAddress;
	}

	BOOL bSystem;
	PdbReader pdb;
	BOOL fOk = FALSE;

	if (0 <= OpenPdb(&pdb, pDoc, ImageBase, VirtualAddress, Size, 0, &bSystem))
	{
		PVOID pv;
		ULONG cb;

		if (0 <= pdb.getStream(PDB_STREAM_TPI, &pv, &cb))
		{
			fOk = TPIDumpTypesAndSave((TPIHDR*)pv, cb, Is64Bit);
			pdb.FreeStream(pv);
		}
	}

	return fOk;
}

BOOL CreatePrivateUDTContext(ZDbgDoc* pDoc, PCWSTR NtSymbolPath, PVOID KernelBase)
{
	GLOBALS_EX globals;
	globals.SetPathNoReg(NtSymbolPath);

	__try
	{
		return LoadNtSymbols(pDoc, KernelBase);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
	}

	return FALSE;
}

void SymbolsThread(PCWSTR NtSymbolPath)
{
	if (PVOID KernelBase = GetKernelBase())
	{
		UDT_PARSE_CONTEXT upc;
		upc._pnh = &gnh;
		upc._psh = &gsh;
		upc._pns = &gns;

		g_udtInit = CreatePrivateUDTContext(0, NtSymbolPath, KernelBase);
	}

	ExitThread(0);
}

BOOL CreatePrivateUDTContext(ZDbgDoc* pDoc, PCWSTR NtSymbolPath, PVOID KernelBase, void** ppv)
{
	if (PRIVATE_UDT_CONTEXT* pc = new PRIVATE_UDT_CONTEXT)
	{
		UDT_PARSE_CONTEXT upc;
		upc._pnh = &pc->_nh;
		upc._psh = &pc->_sh;
		upc._pns = pc;

		if (CreatePrivateUDTContext(pDoc, NtSymbolPath, KernelBase))
		{
			*ppv = pc;
			return TRUE;
		}

		delete pc;
	}

	return FALSE;
}

void DeletePrivateUDTContext(PVOID pv)
{
	delete (PRIVATE_UDT_CONTEXT*)pv;
}

void FillCB(HWND hwnd, PVOID UdtCtx)
{
	FillCB(UdtCtx ? reinterpret_cast<PRIVATE_UDT_CONTEXT*>(UdtCtx) : &gns, hwnd);
}

ZType* FindType(CNS* pns, PCSTR name)
{
	BYTE buf[sizeof(ZType)];
	reinterpret_cast<ZType*>(buf)->_name = name;
	return (ZType*)RtlLookupElementGenericTableAvl(pns, buf);
}

ZType* FindType(PVOID UdtCtx, PCSTR name)
{
	return FindType(UdtCtx ? reinterpret_cast<PRIVATE_UDT_CONTEXT*>(UdtCtx) : &gns, name);
}

_NT_END