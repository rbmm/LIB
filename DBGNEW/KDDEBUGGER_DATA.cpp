#include "stdafx.h"

_NT_BEGIN

void ShowText(PCWSTR caption, PCWSTR text);

extern const CHAR kdd_begin[], kdd_end[];

#include "../winz/str.h"

void Show_KDDEBUGGER_DATA(PBYTE pdd)
{
	ULONG cch = 0x3000;
	if (PWSTR buf = new WCHAR [cch])
	{
		PWSTR pwz = buf;
		PCSTR pa = kdd_begin, pc, pb, pu;

		ULONG vl = 0;

		while (pc = _strnchr(pa, kdd_end, '\n'))
		{
			PCWSTR fmt = 0;
			ULONG len;

			if (pb = _strnchr(pa, pc, ';'))
			{
				if (pu = _strnchr(pa, pb, 'U'))
				{
					switch (*pu++)
					{
					case 'C': // UCHAR
						if (memcmp(pu, "HAR ", 4))
						{
							goto __end;
						}
						vl = sizeof(UCHAR);
						fmt = L"%02x";
						break;

					case 'S': // USHORT
						if (memcmp(pu, "HORT ", 5))
						{
							goto __end;
						}
						vl = sizeof(USHORT);
						fmt = L"%04x";
						break;
					case 'L': // ULONG
						if (memcmp(pu, "ONG", 3))
						{
							goto __end;
						}
						switch (pu[3])
						{
						case ' ':
							vl = sizeof(ULONG);
							fmt = L"%08x";
							break;
						case '6':
							if ('4' == pu[4] && ' ' == pu[5])
							{
								vl = sizeof(ULONG64);
								fmt = L"%016I64x";
								break;
							}
						default:
							goto __end;
						}
						break;
					default: 
						goto __end;
					}
				}
				else
				{
					break;
				}

				--pb;
			}
			else
			{
				pb = pc;
			}

			if (len = MultiByteToWideChar(CP_UTF8, 0, pa, RtlPointerToOffset(pa, pb), pwz, cch))
			{
				pwz += len, cch -= len;
			}
			else
			{
				break;
			}

			if (fmt)
			{
				if (cch < 3)
				{
					break;
				}

				*pwz++ = ' ', *pwz++ = '=', *pwz++ = ' ', cch -= 3;
				ULONG64 u = 0;

				if ((ULONG_PTR)pdd & (vl - 1))
				{
					break;
				}

				switch (vl)
				{
				case sizeof(UCHAR):
					u = *(UCHAR*)pdd;
					break;
				case sizeof(USHORT):
					u = *(USHORT*)pdd;
					break;
				case sizeof(ULONG):
					u = *(ULONG*)pdd;
					break;
				case sizeof(ULONG64):
					u = *(ULONG64*)pdd;
					break;
				default: __debugbreak();
				}

				if (len = swprintf_s(pwz, cch, fmt, u))
				{
					if ((pwz += len, cch -= len) < 2)
					{
						break;
					}
					*pwz++ = '\r', *pwz++ = '\n', cch -= 2;
				}
				else
				{
					break;
				}

				pdd += vl;
			}
			pa = pc;
		}

		if (cch)
		{
			*pwz = 0;
			ShowText(L"KDDEBUGGER_DATA64", buf);
		}
__end:
		delete [] buf;
	}
}

void PrintUTF8(PCSTR pcsz, ULONG len)
{
	if (!IsDebuggerPresent()) return;
	PWSTR pwz = 0;
	ULONG cch = 0;
	while (cch = MultiByteToWideChar(CP_UTF8, 0, pcsz, len, pwz, cch))
	{
		if (pwz)
		{
			ULONG_PTR params[] = { cch, (ULONG_PTR)pwz, len, (ULONG_PTR)pcsz };
			RaiseException(DBG_PRINTEXCEPTION_WIDE_C, 0, _countof(params), params);
			break;
		}

		pwz = (PWSTR)alloca(++cch * sizeof(WCHAR));
	}
}

inline void PrintUTF8(PCSTR pcsz)
{
	if (!IsDebuggerPresent()) return;
	PrintUTF8(pcsz, (ULONG)strlen(pcsz));
}

void PrintUTF8_v(PCSTR format, ...)
{
	if (!IsDebuggerPresent()) return;
	va_list ap;
	va_start(ap, format);

	PSTR buf = 0;
	int len = 0;
	while (0 < (len = _vsnprintf(buf, len, format, ap)))
	{
		if (buf)
		{
			PrintUTF8(buf, len);
			break;
		}

		buf = (PSTR)alloca(++len);
	}
}

_NT_END