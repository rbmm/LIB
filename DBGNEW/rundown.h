#pragma once

class __declspec(novtable) RUNDOWN_REF
{
	LONG _LockCount;

protected:
	virtual void RundownCompleted() = 0;
public:
	RUNDOWN_REF()
	{
		_LockCount = 1;
	}
	BOOL AcquireRundownProtection();
	void ReleaseRundownProtection();
};