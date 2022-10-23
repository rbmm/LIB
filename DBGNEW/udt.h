#pragma once

struct ZType;
struct ZMember;

struct ZBase 
{
	ZBase* _next;
	ZType* _type;
	ULONG _ofs;
	union {
		unsigned short _value;
		CV_fldattr_t _attr;
		CV_modifier_t _mod;
	};

	void* operator new(size_t cb);
};

struct ZType 
{
	PCSTR _name;
	union {
		ZType* _pti; // for LF_BITFIELD, LF_ARRAY, LF_POINTER, LF_MODIFIER
		ZBase* _first;
		CV_typ_t _base; // for LF_ENUM
	};
	union {
		ZMember* _member;
		PCSTR _classname; // for LF_POINTER ( CV_PTR_MODE_PMEM | CV_PTR_MODE_PMFUNC)
	};
	union {
		LONG _len;
		struct  { // LF_PROCEDURE
			unsigned short  _parmcount;      // number of parameters
			CV_funcattr_t   _funcattr;       // attributes
			CV_call_e   _calltype;           // calling convention
		};
		struct { // LF_BITFIELD
			unsigned char   _length;
			unsigned char   _position;
		};
		CV_Pointer_t  _pAttr; // LF_POINTER
		CV_modifier_t _Modifier; // LF_MODIFIER
	};
	USHORT _leaf, _bProcessed;

	void* operator new(size_t cb);

	ZType(USHORT leaf)
	{
		_leaf = leaf;
	}

	ZType(USHORT leaf, PSTR name)
	{
		RtlZeroMemory(this, sizeof(*this));
		_leaf = leaf, _name = name;
	}

	void Add(ZMember* pmi);
	void AddBaseType(ZType* pti, ULONG ofs, unsigned short value);

	ZType* getNestedType(PCSTR name);
	ZType* addNestedType(USHORT leaf, PCSTR name);

	ULONG getSize();
};

#ifdef _WIN64
C_ASSERT(sizeof(ZType) == 0x20);
#else
C_ASSERT(sizeof(ZType) == 0x14);
#endif

struct ZMember 
{
	PCSTR _name;
	ZMember* _next;
	ZType* _type;
	union {
		LONG _offset;
		LONG _value; // _leaf == LF_ENUMERATE
	};
	CV_fldattr_t _attr;
	USHORT _leaf;

	ZMember(USHORT leaf)
	{
		_leaf = leaf;
	}

	void* operator new(size_t cb);

	PSTR Dump(PSTR lpsz, int level);
};