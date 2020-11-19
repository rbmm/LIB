#pragma once

#pragma warning(disable : 4200)

class CDataPacket
{
private:
	LONG m_nRef;
	ULONG m_BufferSize, m_DataSize, m_Pad;
	char m_Data[];

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

	void* operator new(size_t ByteSize, ULONG BufferSize)
	{
		return new(::operator new(ByteSize + BufferSize)) CDataPacket(BufferSize);
	}

	PSTR getFreeBuffer()
	{
		return m_Data + m_DataSize;
	}

	ULONG getFreeSize()
	{
		return m_BufferSize - m_DataSize;
	}

	PSTR getData()
	{
		return m_Data;
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
		PVOID to = m_Data + m_DataSize;
		if (to != pvData) memcpy(to, pvData, cbData);
		return m_DataSize += cbData;
	}

	BOOL formatData(PCSTR fmt, ...)
	{
		int n = _vsnprintf(m_Data + m_DataSize, m_BufferSize - m_DataSize, fmt, (va_list)(&fmt + 1));
		if (0 > n)
		{
			return FALSE;
		}

		m_DataSize += n;
		return TRUE;
	}

	void removeData(ULONG DataSize)
	{
		memcpy(m_Data, m_Data + DataSize, m_DataSize -= DataSize);
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