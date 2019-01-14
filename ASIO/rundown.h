#pragma once

#include "../inc/mini_yvals.h"

_NODISCARD BOOL ObpLock(PLONG pLock);

class RundownProtection
{
	LONG _Value;

public:

	enum {
		v_complete = 0, v_init = 0x80000000
	};

	_NODISCARD BOOL IsRundownBegin()
	{
		return 0 <= _Value;
	}

	_NODISCARD BOOL Acquire();

	_NODISCARD BOOL Release()
	{
		return InterlockedDecrement(&_Value) == v_complete;
	}

	// if (Acquire()) { Rundown_l(); Release(); }
	void Rundown_l()
	{
		_interlockedbittestandreset(&_Value, 31);
	}

	RundownProtection(LONG Value = v_complete) : _Value(Value)
	{
	}

	void Init()
	{
		if (InterlockedCompareExchange(&_Value, v_init, v_complete) != v_complete)
		{
			__debugbreak();
		}
	}
};

class __declspec(novtable) RUNDOWN_REF : public RundownProtection
{
protected:

	virtual void RundownCompleted() = 0;

public:

	void BeginRundown()
	{
		if (Acquire())
		{
			Rundown_l();
			Release();
		}
	}

	void Release()
	{
		if (RundownProtection::Release())
		{
			RundownCompleted();
		}
	}

	RUNDOWN_REF(LONG Value = RundownProtection::v_init) : RundownProtection(Value) {}
};