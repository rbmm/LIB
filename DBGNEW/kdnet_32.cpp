#include "stdafx.h"

_NT_BEGIN

#include "kdnet.h"

KdNetDbg::~KdNetDbg()
{
}


NTSTATUS KdNetDbg::SetContext(unsigned short,CONTEXT *)
{
	return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS KdNetDbg::WriteControlSpace(unsigned short,CONTEXT *)
{
	return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS KdNetDbg::SetBreakPoint(void *,unsigned long *)
{
	return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS KdNetDbg::DeleteBreakpoint(unsigned long)
{
	return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS KdNetDbg::SendBreakIn(void)
{
	return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS KdNetDbg::KdContinue(long)
{
	return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS KdNetDbg::GetContext(unsigned short,CONTEXT *)
{
	return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS KdNetDbg::ReadControlSpace(unsigned short,CONTEXT *)
{
	return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS KdNetDbg::FormatFromAddress(wchar_t *,unsigned long)
{
	return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS KdNetDbg::WriteMemory(void *,void *,unsigned long)
{
	return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS KdNetDbg::ReadRemote(unsigned char *,unsigned char *,unsigned long,unsigned long *)
{
	return STATUS_NOT_IMPLEMENTED;
}

_NT_END