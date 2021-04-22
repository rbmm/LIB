#include "stdafx.h"

_NT_BEGIN

#include "rundownV.h"

class SharedData : public ExRundownProtection
{
	HANDLE hEvent = 0, hStartEvent = 0;
	ULONG start_time;
	LONG ActiveThreads = 1;

	void Done()
	{
		if (!InterlockedDecrement(&ActiveThreads))
		{
			SetEvent(hEvent);
		}
	}

	ULONG WorkerThread()
	{
		if (WaitForSingleObject(hStartEvent, INFINITE) != WAIT_OBJECT_0)
		{
			__debugbreak();
		}

		WCHAR sz[16];
		swprintf_s(sz, _countof(sz), L"[%x]", GetCurrentThreadId());

		while (MessageBoxW(0, sz, L"Continue ?", MB_YESNO|MB_ICONQUESTION) == IDYES && Acquire())
		{
			MessageBoxW(0, sz, L"Inside Protection", MB_ICONINFORMATION|MB_OK);
			Release();
		}

		Done();

		return 0;
	}

	static ULONG WINAPI WorkerThreadProc(PVOID pv)
	{
		FreeLibraryAndExitThread((HMODULE)&__ImageBase, reinterpret_cast<SharedData*>(pv)->WorkerThread());
	}

	ULONG Start(PTHREAD_START_ROUTINE lpStartAddress)
	{
		InterlockedIncrementNoFence(&ActiveThreads);

		ULONG dwError;

		HMODULE hModule;

		if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (PCWSTR)lpStartAddress, &hModule))
		{
			if (HANDLE hThread = CreateThread(0, 0, lpStartAddress, this, 0, 0))
			{
				CloseHandle(hThread);

				return NOERROR;
			}

			dwError = GetLastError();

			FreeLibrary(hModule);
		}
		else
		{
			dwError = GetLastError();
		}

		InterlockedDecrementNoFence(&ActiveThreads);

		return dwError;
	}

	ULONG StartN(ULONG n = 2)
	{
		do 
		{
			if (ULONG dwError = Start(WorkerThreadProc))
			{
				return dwError;
			}
		} while (--n);

		return NOERROR;
	}

public:

	void WaitForTask()
	{
		if (InterlockedDecrement(&ActiveThreads))
		{
			if (WaitForSingleObject(hEvent, INFINITE) != WAIT_OBJECT_0)
			{
				__debugbreak();
			}
		}
	}

	ULONG Start()
	{
		ULONG dwError = StartN();

		if (!dwError)
		{
			Init();
		}

		start_time = GetTickCount();

		if (!SetEvent(hStartEvent))
		{
			__debugbreak();
		}

		return dwError;
	}

	~SharedData()
	{
		if (hStartEvent)
		{
			CloseHandle(hStartEvent);
		}
		if (hEvent)
		{
			CloseHandle(hEvent);
		}
	}

	SharedData()
	{
	}

	ULONG Create()
	{
		return (hEvent = CreateEventW(0, TRUE, FALSE, 0)) && 
			(hStartEvent = CreateEventW(0, TRUE, FALSE, 0)) ? NOERROR : GetLastError();
	}
};

void TestSync()
{
	SharedData sd;
	if (!sd.Create())
	{
		sd.Start();
		MessageBoxW(0,0, L"Wait before rundown", MB_ICONWARNING);
		sd.BeginRundown();
		MessageBoxW(0,0, L"rundown completed !", MB_ICONWARNING);
		sd.WaitForTask();
	}
}

void TestAsync()
{
	SharedData sd;
	if (!sd.Create())
	{
		sd.Start();
		MessageBoxW(0,0, L"Wait before rundown", MB_ICONWARNING);

		struct EX_RUNDOWN_BLOCK_NO_WAIT : public EX_RUNDOWN_BLOCK
		{
			virtual void OnRundownCompleted()
			{
				MessageBoxW(0, 0, L"rundown completed !", MB_ICONWARNING);
			}

			virtual void Wait() 
			{
				// never wait
				return ;
			}
		} o;

		sd.BeginRundown(&o);

		sd.WaitForTask();
	}
}

void RundownTest()
{
	TestAsync();
	TestSync();
}

_NT_END