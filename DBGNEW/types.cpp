#include "stdafx.h"
#include "..\ntver\nt_ver.h"

_NT_BEGIN

#include "types.h"

extern volatile const UCHAR guz;

ZOBJECT_ALL_TYPES_INFORMATION g_AOTI;

int ZOBJECT_ALL_TYPES_INFORMATION::TypeIndexToIndex(ULONG TypeIndex)
{
	if (ULONG NumberOfTypes = _NumberOfTypes)
	{
		POBJECT_TYPE_INFORMATION TypeInformation = _TypeInformation;

		ULONG index = TypeIndex - _FirstTypeIndex;

		if (index < NumberOfTypes && TypeInformation[index].TypeIndex == TypeIndex)
		{
			return index;
		}

		index = 0;

		do 
		{
			if (TypeInformation->TypeIndex == TypeIndex)
			{
				return index;
			}
		} while (TypeInformation++, index++, --NumberOfTypes);
	}

	return -1;
}

const OBJECT_TYPE_INFORMATION* ZOBJECT_ALL_TYPES_INFORMATION::operator[](PCUNICODE_STRING TypeName)
{
	if (ULONG NumberOfTypes = _NumberOfTypes)
	{
		POBJECT_TYPE_INFORMATION TypeInformation = _TypeInformation;

		do 
		{
			if (RtlEqualUnicodeString(TypeName, &TypeInformation->TypeName, TRUE))
			{
				return TypeInformation;
			}
		} while (TypeInformation++, -- NumberOfTypes);
	}

	return 0;
}

NTSTATUS ZOBJECT_ALL_TYPES_INFORMATION::Init(POBJECT_TYPES_INFORMATION pTypes, ULONG cb)
{
	if (ULONG NumberOfTypes = pTypes->NumberOfTypes)
	{
		union {
			PVOID pv;
			ULONG_PTR up;
			POBJECT_TYPE_INFORMATION TypeInformation;
		};

		if (pv = new UCHAR[cb])
		{
			_NumberOfTypes = NumberOfTypes;
			_TypeInformation = TypeInformation;

			ULONG Index = 0;

			union {
				ULONG_PTR uptr;
				POBJECT_TYPE_INFORMATION pti;
			};

			union {
				PWSTR buf;
				PBYTE pb;
				PVOID pv;
			};

			pti = pTypes->TypeInformation;
			pv = TypeInformation + NumberOfTypes;

			UCHAR TypeIndex = 2;

			if (g_nt_ver.Version < _WIN32_WINNT_WINBLUE)
			{
				pti->TypeIndex = 2;
			}

			_FirstTypeIndex = pti->TypeIndex;

			do 
			{
				if (g_nt_ver.Version < _WIN32_WINNT_WINBLUE)
				{
					pti->TypeIndex = TypeIndex++;
				}


				ULONG Length = pti->TypeName.Length, MaximumLength = pti->TypeName.MaximumLength;
				memcpy(buf, pti->TypeName.Buffer, Length);

				*TypeInformation = *pti;
				TypeInformation++->TypeName.Buffer = buf;
				pb += Length;
				uptr += (sizeof(OBJECT_TYPE_INFORMATION) + MaximumLength + 
					__alignof(OBJECT_TYPE_INFORMATION) - 1) & ~ (__alignof(OBJECT_TYPE_INFORMATION)-1);

			} while (Index++, --NumberOfTypes);

			return STATUS_SUCCESS;
		}

		return STATUS_NO_MEMORY;
	}

	return STATUS_NOT_FOUND;
}

NTSTATUS ZOBJECT_ALL_TYPES_INFORMATION::Init()
{
	NTSTATUS status;

	ULONG cb = 0x4000;

	do 
	{
		status = STATUS_INSUFFICIENT_RESOURCES;

		if (PVOID buf = new UCHAR[cb])
		{
			if (0 <= (status = NtQueryObject(0, ObjectAllTypeInformation, buf, cb, &cb)))
			{
				status = Init((POBJECT_TYPES_INFORMATION)buf, cb);
			}
			delete [] buf;
		}

	} while (status == STATUS_INFO_LENGTH_MISMATCH);

	return status;
}

_NT_END