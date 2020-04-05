#pragma once

#include "mini_yvals.h"

enum RundownState {
	v_complete = 0, v_init = 0x80000000
};

template<typename T>
class RundownProtection
{
	LONG _Value;

public:

	_NODISCARD BOOL IsRundownBegin()
	{
		return 0 <= _Value;
	}

	_NODISCARD BOOL AcquireRP()
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

	void ReleaseRP()
	{
		if (InterlockedDecrement(&_Value) == v_complete)
		{
			static_cast<T*>(this)->RundownCompleted();
		}
	}

	void Rundown_l()
	{
		_interlockedbittestandreset(&_Value, 31);
	}

	void Rundown()
	{
		if (AcquireRP())
		{
			Rundown_l();
			ReleaseRP();
		}
	}

	RundownProtection(RundownState Value = v_complete) : _Value(Value)
	{
	}

	void Init()
	{
		_Value = v_init;
	}
};