#pragma once

#pragma warning(disable : 4200)

class CDataPacket
{
private:
	LONG m_nRef;
	ULONG m_BufferSize, m_DataSize, m_Pad;
protected:

	virtual ~CDataPacket() {}

public:
	CDataPacket(ULONG BufferSize) : m_nRef(1), m_BufferSize(BufferSize), m_DataSize(0), m_Pad(0) { }

	void* operator new(size_t , PVOID pv)
	{
		return pv;
	}

	CDataPacket() { }

	void AddRef()
	{
		InterlockedIncrement(&m_nRef);
	}

	void Release()
	{
		if (!InterlockedDecrement(&m_nRef)) delete this;
	}

	void* operator new(size_t ByteSize) = delete;
	void* operator new[](size_t ByteSize) = delete;

	void* operator new(size_t ByteSize, ULONG BufferSize)
	{
		return new(::operator new(ByteSize + BufferSize)) CDataPacket(BufferSize);
	}

	PSTR getFreeBuffer()
	{
		return (char*)(this + 1) + m_DataSize;
	}

	ULONG getFreeSize()
	{
		return m_BufferSize - m_DataSize;
	}

	PSTR getData()
	{
		return (char*)(this + 1);
	}

	ULONG getBufferSize()
	{
		return m_BufferSize;
	}

	ULONG getDataSize()
	{
		return m_DataSize;
	}

	ULONG setDataSize(ULONG DataSize)
	{
		return m_DataSize = DataSize;
	}

	ULONG addData(ULONG DataSize)
	{
		return m_DataSize += DataSize;
	}

	ULONG decData(ULONG DataSize)
	{
		return m_DataSize -= DataSize;
	}

	ULONG addData(const void* pvData, ULONG cbData)
	{
		PVOID to = (char*)(this + 1) + m_DataSize;
		if (to != pvData) memcpy(to, pvData, cbData);
		return m_DataSize += cbData;
	}

	BOOL formatData(PCSTR fmt, va_list args)
	{
		int n = _vsnprintf((char*)(this + 1) + m_DataSize, m_BufferSize - m_DataSize, fmt, args);
		if (0 > n)
		{
			return FALSE;
		}

		m_DataSize += n;
		return TRUE;
	}

	BOOL formatData(PCSTR fmt, ...)
	{
		va_list args;
		va_start(args, fmt);
		BOOL b = formatData(fmt, args);
		va_end(args);
		return b;
	}

	void removeData(ULONG DataSize)
	{
		memcpy((char*)(this + 1), (char*)(this + 1) + DataSize, m_DataSize -= DataSize);
	}

	void reservBuffer(ULONG d)
	{
		m_BufferSize -= d;
	}

	void setPad(ULONG pad)
	{
		m_Pad = pad;
	}

	ULONG getPad()
	{
		return m_Pad;
	}
};