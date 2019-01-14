#pragma once

class CyclicBufer
{
	union {
		__int64 _value;
		double _d;
		struct {
			ULONG _begin, _data_size;
		};
	};
	char* _buf;
protected:
	static ULONG _buf_size, _dwPageSize;

public:

	ULONG getBufferSize() { return _buf_size; }
	ULONG getPageSize() { return _dwPageSize; }
	ULONG getDataSize() { return _data_size; }
	ULONG getFreeSpace(){ return _buf_size - _data_size; }
	PVOID getBuffer() { return _buf; }

	~CyclicBufer() { if (_buf_size) VirtualFree(_buf, 0, MEM_RELEASE); }

	CyclicBufer() : _buf(0), _value(0) { }

	ULONG Create();

	ULONG GetDataPageCount();

	ULONG BuildSegmentArray(FILE_SEGMENT_ELEMENT aSegmentArray[], ULONG n);

	ULONG RemoveData(ULONG cb, PULONG pOldData = 0);

	ULONG AddData(ULONG cb, PULONG pOldData = 0);

	ULONG get(WSABUF Buffers[2]);

	void Init() { _value = 0; }
};

class __declspec(novtable) CyclicBuferEx : public CyclicBufer
{
public:
	void Start();

	void OnReadEnd(ULONG cb);
	void OnWriteEnd(ULONG cb);
	void BeginReadTo();
	void BeginWriteFrom();

protected:
	virtual void WriteFrom(PFILE_SEGMENT_ELEMENT SegmentArray, ULONG Length) = 0;
	virtual void ReadTo(WSABUF* lpBuffers, DWORD dwBufferCount) = 0;
	virtual ULONG GetReadMinSize() { return 1; }
};
