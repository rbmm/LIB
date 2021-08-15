#pragma once

class __declspec(novtable) ZRingBuffer
{
	union {
		__int64 _v;
		struct {
			ULONG _readOffset, _dataSize;
		};
	};
	void* _BaseAddress;
	ULONG _Size;
	volatile LONG _ioCount;

	// must always return the same value
	virtual ULONG GetMinReadBufferSize(){ return 1; }

	// must always return the same value
	virtual ULONG GetMinWriteBufferSize(){ return 1; }

	// if the same memory mapped at
	// [_BaseAddress, _BaseAddress + _Size) and
	// [_BaseAddress + _Size, _BaseAddress + 2* _Size)
	virtual bool IsAdjacentBuffers(){ return false; }

	// Begins an asynchronous read operation
	// return are EndRead will be called
	virtual bool BeginRead(WSABUF* lpBuffers, ULONG dwBufferCount) = 0;

	// Begins an asynchronous write operation
	// return are EndWrite will be called
	virtual bool BeginWrite(WSABUF* lpBuffers, ULONG dwBufferCount) = 0;

	virtual void OnIoStop() = 0;

	void ReadAsync(ULONG ReadOffset, ULONG DataSize);

	void WriteAsync(ULONG ReadOffset, ULONG DataSize);

	ULONG BuildBuffers(WSABUF wb[2], ULONG from, ULONG to);

	bool CanRead(ULONG DataSize)
	{
		return DataSize >= GetMinReadBufferSize();
	}

	bool CanWrite(ULONG DataSize)
	{
		return _Size - DataSize >= GetMinWriteBufferSize();
	}

	LONG StartIo()
	{
		return InterlockedExchangeAddNoFence(&_ioCount, 1);
	}

	void EndIo()
	{
		if (!InterlockedDecrement(&_ioCount)) OnIoStop();
	}

public:

	// notifies that asynchronous write completed
	void EndWrite(ULONG NumberOfBytesWrite );

	// notifies that asynchronous read completed
	void EndRead(ULONG NumberOfBytesRead );

	ZRingBuffer() : _BaseAddress(0), _Size(0) {}

	void* GetBuffer() { return _BaseAddress; }

	ULONG GetSize() { return _Size; }

	void Init(void* BaseAddress, ULONG Size);

	void Start();

	void Stop()
	{
		EndIo();
	}

	// for debug only
	ULONG getIOcount() { return _ioCount; };
};
