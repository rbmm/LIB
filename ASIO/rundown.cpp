#include "stdafx.h"

_NT_BEGIN

#include "rundown.h"

BOOL RundownProtection::Acquire()
{
	LONG Value, NewValue;

	if (0 > (Value = _Value))
	{
		do 
		{
			NewValue = InterlockedCompareExchangeNoFence(&_Value, Value + 1, Value);

			if (NewValue == Value) return TRUE;

		} while (0 > (Value = NewValue));
	}

	return FALSE;
}

//  */<memory>*/ bool _Ref_count_base::_Incref_nz()
// increment (*pLock) if not zero, return true if successful
_NODISCARD BOOL ObpLock(PLONG pLock)
{
	LONG Value, NewValue;

	if (Value = *pLock)
	{
		do 
		{
			NewValue = InterlockedCompareExchangeNoFence(pLock, Value + 1, Value);

			if (NewValue == Value) return TRUE;

		} while (Value = NewValue);
	}

	return FALSE;
}

_NT_END