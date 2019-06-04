#include "StdAfx.h"

_NT_BEGIN

#include "CiclicBuffer.h"

void ZRingBuffer::Init(void* BaseAddress, ULONG Size)
{
	_BaseAddress = BaseAddress, _Size = Size;
}

void ZRingBuffer::Start()
{
	if (GetMinReadBufferSize() + GetMinWriteBufferSize() > _Size + 1)
	{
		__debugbreak();
	}
	_readOffset = 0, _dataSize = 0, _ioCount = 0;
	WriteAsync(0, 0);
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

void ZRingBuffer::ReadAsync(ULONG ReadOffset, ULONG DataSize)
{
	if (CanRead(DataSize))
	{
		StartIo();

		WSABUF wb[2];
		if (!BeginRead(wb, BuildBuffers(wb, ReadOffset, ReadOffset + DataSize)))
		{
			EndRead(0);
		}
	}
}

void ZRingBuffer::WriteAsync(ULONG ReadOffset, ULONG DataSize)
{
	if (CanWrite(DataSize))
	{
		StartIo();

		WSABUF wb[2];
		if (!BeginWrite(wb, BuildBuffers(wb, ReadOffset + DataSize, ReadOffset + _Size)))
		{
			EndWrite(0);
		}
	}
}

void ZRingBuffer::EndWrite(ULONG NumberOfBytesWrite )
{
	if (NumberOfBytesWrite)
	{
		union {
			__int64 v;
			struct {
				ULONG readOffset, dataSize;
			};
		};

		union {
			__int64 v0;
			struct {
				ULONG readOffset0, dataSize0;
			};
		};

		union {
			__int64 v1;
			struct {
				ULONG readOffset1, dataSize1;
			};
		};

		v0 = _v;

		do 
		{
			readOffset1 = readOffset0;
			dataSize1 = dataSize0 + NumberOfBytesWrite;
			v0 = _InterlockedCompareExchange64(&_v, v1, v = v0);
		} while (v0 != v);

		if (!CanRead(dataSize0))
		{
			ReadAsync(readOffset1, dataSize1);
		}

		WriteAsync(readOffset1, dataSize1);
	}

	EndIo();
}

void ZRingBuffer::EndRead(ULONG NumberOfBytesRead )
{
	if (NumberOfBytesRead)
	{
		union {
			__int64 v;
			struct {
				ULONG readOffset, dataSize;
			};
		};

		union {
			__int64 v0;
			struct {
				ULONG readOffset0, dataSize0;
			};
		};

		union {
			__int64 v1;
			struct {
				ULONG readOffset1, dataSize1;
			};
		};

		v0 = _v;

		do 
		{
			readOffset1 = (readOffset0 + NumberOfBytesRead) % _Size;
			dataSize1 = dataSize0 - NumberOfBytesRead;
			v0 = _InterlockedCompareExchange64(&_v, v1, v = v0);
		} while (v0 != v);

		if (!CanWrite(dataSize0))
		{
			WriteAsync(readOffset1, dataSize1);
		}

		ReadAsync(readOffset1, dataSize1);
	}

	EndIo();
}

_NT_END