#pragma once

class __declspec(novtable) ZCodeExecute
{
public:
	virtual void Execute(LPARAM lParam) = 0;

	enum { WM_EXECUTE = WM_USER + 0x100 };
};
