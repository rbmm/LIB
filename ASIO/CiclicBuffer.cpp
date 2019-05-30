#include "StdAfx.h"

_NT_BEGIN

#include "CiclicBuffer.h"

void ZRingBuffer::Init(PVOID BaseAddress, ULONG Size)
{
	_BaseAddress = BaseAddress, _Size = Size;
	_ReadOffset = 0, _WriteOffset = 0;
}

void ZRingBuffer::Start()
{
	if (GetMinReadBufferSize() + GetMinWriteBufferSize() > _Size)
	{
		__debugbreak();
	}
	_ReadOffset = 0, _WriteOffset = 0;
	WriteAsync(0, 0);
}

void ZRingBuffer::ReadAsync(ULONG ReadOffset, ULONG WriteOffset)
{
	if (WriteOffset - ReadOffset < GetMinReadBufferSize())
	{
		return;
	}

	WSABUF wb[2];

	BeginRead(wb, BuildBuffers(wb, ReadOffset, WriteOffset));
}

void ZRingBuffer::WriteAsync(ULONG ReadOffset, ULONG WriteOffset)
{
	ULONG capacity = _Size - 1;

	if (capacity - (WriteOffset - ReadOffset) < GetMinWriteBufferSize())
	{
		return;
	}

	WSABUF wb[2];

	BeginWrite(wb, BuildBuffers(wb, WriteOffset, ReadOffset + capacity));
}

ULONG ZRingBuffer::BuildBuffers(WSABUF wb[2], ULONG from, ULONG to)
{
	ULONG Size = _Size;

	if (IsAdjacentBuffers())
	{
		wb[0].buf = (char*)_BaseAddress + (from % Size);
		wb[0].len = to - from;
		return 1;
	}

	from %= Size, to %= Size;

	wb[0].buf = (char*)_BaseAddress + from;

	if (from < to)
	{
		wb[0].len = to - from;
		return 1;
	}
	else
	{
		wb[0].len = Size - from;

		if (to)
		{
			wb[1].len = to;
			wb[1].buf = (char*)_BaseAddress;
			return 2;
		}

		return 1;
	}
}

void ZRingBuffer::EndWrite(ULONG NumberOfBytesWrite )
{
	ULONG ReadOffset = _ReadOffset;// memory_order_acquire
	ULONG WriteOffset_0 = _WriteOffset;
	ULONG WriteOffset_1 = WriteOffset_0 + NumberOfBytesWrite;
	_WriteOffset = WriteOffset_1; // memory_order_release

	if (WriteOffset_0 - ReadOffset < GetMinReadBufferSize())
	{
		ReadAsync(ReadOffset, WriteOffset_1);
	}

	WriteAsync(ReadOffset, WriteOffset_1);
}

void ZRingBuffer::EndRead(ULONG NumberOfBytesRead )
{
	ULONG WriteOffset = _WriteOffset; // memory_order_acquire 
	ULONG ReadOffset_0 = _ReadOffset;
	ULONG ReadOffset_1 = ReadOffset_0 + NumberOfBytesRead;
	_ReadOffset = ReadOffset_1; // memory_order_release 

	if ((_Size - 1) - (WriteOffset - ReadOffset_0) < GetMinWriteBufferSize())
	{
		WriteAsync(ReadOffset_1, WriteOffset);
	}

	ReadAsync(ReadOffset_1, WriteOffset);
}

_NT_END