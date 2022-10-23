#pragma once

#include "../winZ/document.h"
#include "../winz/view.h"
#include "../asio/pipe.h"
#include "DbgPipe.h"

class CMDbgDoc : public ZDocument
{
	CDbgPipe* m_pipe;
	USHORT m_MachineType;
	WORD m_Processor;
	union {
		ULONG m_flags;
		struct {
			ULONG m_Wow : 1;
		};
	};
	union {
		CONTEXT m_ctx;
		WOW64_CONTEXT m_wctx;
	};

	virtual BOOL IsCmdEnabled(WORD /*cmd*/)
	{
		return FALSE;
	}

	virtual LRESULT OnCmdMsg(WPARAM /*wParam*/, LPARAM /*lParam*/)
	{
		return 0;
	}

	~CMDbgDoc()
	{
		DbgPrint("%s<%p>\n", __FUNCTION__, this);
		m_pipe->Release();
	}
public:

	CMDbgDoc(CDbgPipe* pipe)
	{
		m_pipe = pipe;
		pipe->AddRef();
		DbgPrint("%s<%p>\n", __FUNCTION__, this);
	}

	WORD get_CurrentProcessor() {
		return m_Processor;
	}

	void StopDebugging()
	{
		__debugbreak();
		m_pipe->Disconnect();
	}

	BOOL OnRemoteStart(_DBGKD_GET_VERSION* GetVersion);

	void OnRemoteEnd()
	{
		DestroyAllViews();
	}

	void Break()
	{
		m_pipe->SendBreakIn();
	}

	void Continue(NTSTATUS status)
	{
		m_pipe->KdContinue(status);
	}

	void OnException(DBGKD_WAIT_STATE_CHANGE* pwsc);

	NTSTATUS GetContext(WORD Processor, PVOID ctx, ULONG cb, PULONG rcb);
	NTSTATUS SetContext(WORD Processor, PVOID ctx, ULONG cb);
	NTSTATUS SetContext();
};