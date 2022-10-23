#pragma once

class ZOBJECT_ALL_TYPES_INFORMATION
{
	POBJECT_TYPE_INFORMATION _TypeInformation = 0;
	ULONG _NumberOfTypes = 0, _FirstTypeIndex = 0;

	NTSTATUS Init(POBJECT_TYPES_INFORMATION pTypes, ULONG cb);

public:

	ULONG count()
	{
		return _NumberOfTypes;
	}

	ULONG maxIndex()
	{
		return _NumberOfTypes + _FirstTypeIndex;
	}

	operator const OBJECT_TYPE_INFORMATION*()
	{
		return _TypeInformation;
	}

	const OBJECT_TYPE_INFORMATION* operator[](ULONG Index)
	{
		return Index < _NumberOfTypes ? _TypeInformation + Index : 0;
	}

	int TypeIndexToIndex(ULONG TypeIndex);

	const OBJECT_TYPE_INFORMATION* operator[](PCUNICODE_STRING TypeName);

	NTSTATUS Init();

	~ZOBJECT_ALL_TYPES_INFORMATION()
	{
		if (_TypeInformation)
		{
			delete [] _TypeInformation;
		}
	}
};

extern ZOBJECT_ALL_TYPES_INFORMATION g_AOTI;