#pragma once

class __declspec(novtable) ZRingBuffer
{
	PVOID _BaseAddress;
	volatile ULONG _ReadOffset, _WriteOffset;
	ULONG _Size;

	virtual ULONG GetMinWriteBufferSize(){ return 1; }

	virtual ULONG GetMinReadBufferSize(){ return 1; }
	
	virtual BOOL IsAdjacentBuffers(){ return FALSE; }

	// Begins an asynchronous read operation
	virtual void BeginRead(WSABUF* lpBuffers, ULONG dwBufferCount) = 0;

	// Begins an asynchronous write operation
	virtual void BeginWrite(WSABUF* lpBuffers, ULONG dwBufferCount) = 0;

	void ReadAsync(ULONG ReadOffset, ULONG WriteOffset);

	void WriteAsync(ULONG ReadOffset, ULONG WriteOffset);

	ULONG BuildBuffers(WSABUF wb[2], ULONG from, ULONG to);

public:

	ZRingBuffer() : _BaseAddress(0), _Size(0) {}

	void* GetBuffer() { return _BaseAddress; }

	ULONG GetSize() { return _Size; }

	// notifies that asynchronous write completed
	void EndWrite(ULONG NumberOfBytesWrite );

	// notifies that asynchronous read completed
	void EndRead(ULONG NumberOfBytesRead );

	void Init(PVOID BaseAddress, ULONG Size);
	
	void Start();
};
