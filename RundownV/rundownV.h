#pragma once

struct __declspec(novtable) EX_RUNDOWN_BLOCK 
{
	union {
		HANDLE hEvent;
		HANDLE ThreadId;
	};
	LONG dwCount;

	virtual void OnRundownCompleted() = 0;
	virtual void Wait() = 0;
};

class ExRundownProtection
{
	LONG_PTR _Value;

public:

	enum : LONG_PTR {
		v_complete = 0, v_init = MINLONG_PTR
	};

	_NODISCARD BOOL IsRundownBegin()
	{
		return 0 <= _Value;
	}

	_NODISCARD BOOL Acquire();

	_NODISCARD void Release();

	void BeginRundown(EX_RUNDOWN_BLOCK* pBlock = 0);

	ExRundownProtection(LONG_PTR Value = v_complete) : _Value(Value)
	{
	}

	void Init()
	{
		if (InterlockedCompareExchangePointer((void**)&_Value, (void*)v_init, (void*)v_complete) != (void*)v_complete)
		{
			__debugbreak();
		}
	}
};
