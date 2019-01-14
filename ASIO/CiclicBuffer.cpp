#include "StdAfx.h"

_NT_BEGIN

#include "CiclicBuffer.h"

ULONG CyclicBufer::_buf_size, CyclicBufer::_dwPageSize;

void CyclicBuferEx::Start() 
{ 
	Init(); 
	WSABUF wb = { _buf_size, (char*)getBuffer() };
	ReadTo(&wb, 1);
}

void CyclicBuferEx::OnReadEnd(ULONG cb)
{
	ULONG old_data_size, new_data_size = AddData(cb, &old_data_size);

	if (new_data_size >= _dwPageSize && old_data_size < _dwPageSize)
	{
		BeginWriteFrom();
	}

	if (GetReadMinSize() <= _buf_size - new_data_size)
	{
		BeginReadTo();
	}
}

void CyclicBuferEx::OnWriteEnd(ULONG cb)
{
	ULONG old_data_size, new_data_size = RemoveData(cb, &old_data_size), max_size = _buf_size - GetReadMinSize();
	
	if (new_data_size <= max_size && old_data_size > max_size)
	{
		BeginReadTo();
	}

	if (new_data_size >= _dwPageSize)
	{
		BeginWriteFrom();
	}
}

void CyclicBuferEx::BeginReadTo()
{
	WSABUF wb[2];
	if (ULONG n = get(wb))
	{
		ReadTo(wb, n);
	}
	else
	{
		__debugbreak();
	}
}

void CyclicBuferEx::BeginWriteFrom()
{
	ULONG n = 1 + _buf_size / _dwPageSize;

	FILE_SEGMENT_ELEMENT* SegmentArray = (FILE_SEGMENT_ELEMENT*)alloca(n * sizeof(FILE_SEGMENT_ELEMENT));

	if (n = BuildSegmentArray(SegmentArray, n))
	{
		WriteFrom(SegmentArray, n);
	}
	else
	{
		__debugbreak();
	}
}

//////////////////////////////////////////////////////////////////////////
//
ULONG CyclicBufer::Create()
{
	if (!_buf_size)
	{
		SYSTEM_INFO si;
		GetNativeSystemInfo(&si);
		_dwPageSize = si.dwPageSize;
		_ReadWriteBarrier();
		_buf_size = si.dwAllocationGranularity;
	}

	if (_buf = (char*)VirtualAlloc(0, _buf_size, MEM_COMMIT, PAGE_READWRITE))
	{
		return NOERROR;
	}

	return GetLastError();
}

ULONG CyclicBufer::GetDataPageCount()
{
	union {
		double d;
		struct {
			ULONG begin, data_size;
		};
	};

	d = _d;

	return data_size / _dwPageSize;
}

ULONG CyclicBufer::BuildSegmentArray(FILE_SEGMENT_ELEMENT aSegmentArray[], ULONG n)
{
	union {
		double d;
		struct {
			ULONG begin, data_size;
		};
	};

	d = _d;

	ULONG buf_size = _buf_size, dwBytes = 0;
	ULONG dwPageSize = _dwPageSize;
	char* begin_buf = _buf, *buf = begin_buf + begin, *end_buf = begin_buf + buf_size;

	if (ULONG k = data_size / dwPageSize)
	{
		if (n < k)
		{
			k = n;
		}

		do 
		{
			dwBytes += dwPageSize;

			aSegmentArray++->Buffer = PtrToPtr64(buf);

			if ((buf += dwPageSize) == end_buf)
			{
				buf = begin_buf;
			}
		} while (--k);

		aSegmentArray->Buffer = 0;
	}

	return dwBytes;
}

ULONG CyclicBufer::RemoveData(ULONG cb, PULONG pOldData)
{
	union {
		__int64 value;
		double d;
		struct {
			ULONG begin, data_size;
		};
	};

	union {
		__int64 value2;
		double d2;
		struct {
			ULONG begin2, data_size2;
		};
	};

	union {
		__int64 new_value;
		double new_d;
		struct {
			ULONG new_begin, new_data_size;
		};
	};

	d = _d;

	for (;;)
	{
		new_value = value;

		new_data_size -= cb;

		if ((LONG)new_data_size < 0) __debugbreak();

		if ((new_begin += cb) >= _buf_size)
		{
			new_begin -= _buf_size;
		}

		value2 = InterlockedCompareExchange64(&_value, new_value, value);

		if (value2 == value)
		{
			if (pOldData)
			{
				*pOldData = data_size;
			}
			return new_data_size;
		}

		value = value2;
	}
}

ULONG CyclicBufer::AddData(ULONG cb, PULONG pOldData)
{
	union {
		__int64 value;
		double d;
		struct {
			ULONG begin, data_size;
		};
	};

	union {
		__int64 value2;
		double d2;
		struct {
			ULONG begin2, data_size2;
		};
	};

	union {
		__int64 new_value;
		double new_d;
		struct {
			ULONG new_begin, new_data_size;
		};
	};

	d = _d;

	for (;;)
	{
		new_value = value;

		new_data_size += cb;

		if (new_data_size > _buf_size) __debugbreak();

		value2 = InterlockedCompareExchange64(&_value, new_value, value);

		if (value2 == value)
		{
			if (pOldData)
			{
				*pOldData = data_size;
			}
			return new_data_size;
		}

		value = value2;
	}
}

ULONG CyclicBufer::get(WSABUF Buffers[2])
{
	union {
		double d;
		struct {
			ULONG begin, data_size;
		};
	};

	d = _d;

	char* buf = _buf;
	ULONG buf_size = _buf_size, n;

	if (buf_size - data_size)
	{
		ULONG end = begin + data_size;

		if (end < buf_size)
		{
			n = 1;
			Buffers[0].buf = buf + end;
			Buffers[0].len = buf_size - end;

			if (begin)
			{
				n = 2;
				Buffers[1].buf = buf;
				Buffers[1].len = begin;
			}
		}
		else
		{
			n = 1;
			end -= buf_size;
			Buffers[0].buf = buf + end;
			Buffers[0].len = begin - end;
		}

		return n;
	}

	return 0;
}

_NT_END