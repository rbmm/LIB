#include "stdafx.h"

_NT_BEGIN

#include "rundownV.h"

#define _DEBUG_DELAY_

#ifdef _DEBUG_DELAY_
void DebugDelay()
{
	WCHAR sz[16];
	swprintf_s(sz, _countof(sz), L"[%x]", GetCurrentThreadId());
	MessageBoxW(0, sz, L"DebugDelay", MB_ICONINFORMATION);
}
#else
#define DebugDelay() 
#endif

struct EX_RUNDOWN_BLOCK_WAIT : EX_RUNDOWN_BLOCK 
{
	virtual void OnRundownCompleted()
	{
		if (0 > ZwAlertThreadByThreadId(ThreadId))
		{
			__debugbreak();
		}
	}

	virtual void Wait()
	{
		static const LARGE_INTEGER li = { 0, MINLONG };
		if (0 > ZwWaitForAlertByThreadId(0, const_cast<PLARGE_INTEGER>(&li)))
		{
			__debugbreak();
		}
	}
};

_NODISCARD 
BOOL ExRundownProtection::Acquire()
{
	LONG_PTR Value, NewValue;

	if (0 > (Value = _Value))
	{
		// Rundown isn't active.

		do 
		{
			DebugDelay();

			NewValue = (LONG_PTR)InterlockedCompareExchangePointerNoFence((void**)&_Value, (void*)(Value + 1), (void*)Value);

			if (NewValue == Value) return TRUE;

		} while (0 > (Value = NewValue));
	}

	// Rundown is active

	return FALSE;
}

_NODISCARD 
void ExRundownProtection::Release()
{
	LONG_PTR NewValue;

	union {
		EX_RUNDOWN_BLOCK* Block;
		LONG_PTR Value;
	};

	if (0 > (Value = _Value))
	{
		// Rundown isn't active.

		do 
		{
			DebugDelay();

			NewValue = (LONG_PTR)InterlockedCompareExchangePointerNoFence((void**)&_Value, (void*)(Value - 1), (void*)Value);

			if (NewValue == Value) return ;

		} while (0 > (Value = NewValue));
	}

	// Rundown is active

	if (InterlockedDecrement(&Block->dwCount) == v_complete)
	{
		Block->OnRundownCompleted();
	}
}

void ExRundownProtection::BeginRundown(EX_RUNDOWN_BLOCK* pBlock/* = 0*/)
{
	EX_RUNDOWN_BLOCK_WAIT Block;

	if (!pBlock)
	{
		pBlock = &Block;
		Block.ThreadId = (HANDLE)(ULONG_PTR)GetCurrentThreadId();
	}

	LONG_PTR Value, NewValue;

	if (0 > (Value = _Value))
	{
		// Rundown isn't active.
		do 
		{
			DebugDelay();

			pBlock->dwCount = (ULONG)Value;

			if (!(ULONG)Value)
			{
				pBlock = 0;
			}

			NewValue = (LONG_PTR)InterlockedCompareExchangePointerNoFence((void**)&_Value, pBlock, (void*)Value);

			if (NewValue == Value) 
			{
				if (pBlock) pBlock->Wait();
				return ;
			}

		} while (0 > (Value = NewValue));
	}

	// Rundown is active
	__debugbreak();
}

_NT_END