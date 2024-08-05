#include "stdafx.h"

_NT_BEGIN

#include "Dll.h"
#include "DbgDoc.h"
#include "common.h"
#include "PDB.h"

#ifdef _WIN64
#include "../inc/rtf.h"
#endif

size_t __fastcall strnlen(_In_ size_t numberOfElements, _In_ const char *str);

template<typename T>
BOOL IsValidSymbol(T* ps, ULONG cb)
{
	if (FIELD_OFFSET(T, name) < cb)
	{
		ULONG len = sizeof(USHORT) + static_cast<SYM_HEADER*>(ps)->len;
		if (FIELD_OFFSET(T, name) < len && len <= cb)
		{
			len -= FIELD_OFFSET(T, name);
			if (strnlen(len, ps->name) < len)
			{
				return TRUE;
			}
		}
	}

	return FALSE;
}

PSTR xcscpy(PSTR dst, PCSTR src)
{
	CHAR c;

	do 
	{
		*dst++ = c = *src++;
	} while (c);

	return dst;
}

PWSTR xcscpy(PWSTR dst, PCWSTR src)
{
	WCHAR c;

	do 
	{
		*dst++ = c = *src++;
	} while (c);

	return dst;
}

#define r_rva(r) ((r) & 0x0FFFFFFF)

int __cdecl RVAOFS::compare_nf(RVAOFS& a, RVAOFS& b)
{
	ULONG a_rva = r_rva(a.rva), b_rva = r_rva(b.rva);
	if (a_rva < b_rva) return -1;
	if (a_rva > b_rva) return +1;
	if (a.ofs < b.ofs) return +1;
	if (a.ofs > b.ofs) return -1;
	return 0;
}

int __cdecl RVAOFS::compare(RVAOFS& a, RVAOFS& b)
{
	ULONG a_rva = a.rva, b_rva = b.rva;
	if (a_rva < b_rva) return -1;
	if (a_rva > b_rva) return +1;
	if (a.ofs < b.ofs) return +1;
	if (a.ofs > b.ofs) return -1;
	return 0;
}

int __cdecl LINE_INFO::compare(LINE_INFO& a, LINE_INFO& b)
{
	if (a.line < b.line) return -1;
	if (a.line > b.line) return +1;
	return 0;
}

ZDll::ZDll(DWORD index)
{
#ifdef _WIN64
	_prtf = 0;
#endif

	_pSymbols = 0;
	_pSC = 0;
	_pMI = 0;
	_pFileNames = 0;
	_pForwards = 0;
	_EntryPoint = 0;
	_BaseOfDll = 0;
	_ImageName = 0;
	_ImagePath = 0;
	_SizeOfImage = 0;
	_Flags = 0;
	_index = index;
	InitializeListHead(this);
}

ZDll::~ZDll()
{
	union {
		PVOID pv;
		RVAOFS *pSymbols;
	};
#ifdef _WIN64
	if (_RtfPresent)
	{
		delete [] _prtf;
	}
#endif

	DeleteLineInfo();

	if (pSymbols = _pSymbols) delete [](pSymbols - 1);

	if (pv = _pForwards) delete [] pv;

	if (pv = _ImagePath)
	{
		delete [] pv;
	}

	RemoveEntryList(this);
}

int ZDll::IsRvaExported(ULONG rva, PCSTR name)
{
	if (RVAOFS *pSymbols = _pSymbols)
	{
		ULONG a = 0, b = _nSymbols, o, r;

		if (pSymbols->rva > rva)
		{
			return 0;
		}

		do 
		{
			o = (a + b) >> 1;

			RVAOFS* p = pSymbols + o, *q;
			r = p->rva;

			if (r == rva)
			{
				if (!((o = p->ofs) & FLAG_ORDINAL) && !strcmp(RtlOffsetToPointer(pSymbols, o), name))
				{
					return +1;
				}

				q = p;

				while ((++q)->rva == rva)
				{
					if (!((o = q->ofs) & FLAG_ORDINAL) && !strcmp(RtlOffsetToPointer(pSymbols, o), name))
					{
						return +1;
					}
				}

				q = p;

				while ((--q)->rva == rva)
				{
					if (!((o = q->ofs) & FLAG_ORDINAL) && !strcmp(RtlOffsetToPointer(pSymbols, o), name))
					{
						return +1;
					}
				}

				return -1;
			}

			if (r < rva)
			{
				a = o + 1;
			}
			else
			{
				b = o;
			}

		} while (a < b);
	}

	return 0;
}

void ZDll::Unload() 
{ 
	_IsUnloaded = 1; 
	RemoveEntryList(this);
	InitializeListHead(this);
}

BOOL ZDll::Load(ZDbgDoc* pDoc, PDBGKM_LOAD_DLL LoadDll)
{
	_dllId = 0;
	_BaseOfDll = LoadDll->BaseOfDll;

	DWORD hash = 0;

	if (_ImagePath)
	{
		delete [] _ImagePath;
		_ImagePath = 0;
		_ImageName = 0;
	}

	if (PWSTR lpImageName = (PWSTR)LoadDll->NamePointer)
	{
		if (PWSTR ImagePath = new WCHAR[1 + wcslen(lpImageName)])
		{
			_ImagePath = ImagePath, _ImageName = ImagePath;
			WCHAR c;
			do 
			{
				c = *lpImageName++, *ImagePath++ = c;

				if (c == '\\')
				{
					_ImageName = ImagePath;
				}
			} while (c);

			UNICODE_STRING str;
			RtlInitUnicodeString(&str, _ImageName);
			RtlHashUnicodeString(&str, TRUE, HASH_STRING_ALGORITHM_X65599, &hash);
		}
	}

	if (pDoc->IsRemoteDebugger())
	{
		_dllId = LoadDll->DebugInfoFileOffset; // CheckSum
		_SizeOfImage = LoadDll->DebugInfoSize; // SizeOfImage
		_EntryPoint = 0;
		_Is64Bit = pDoc->Is64BitProcess();
		_tmpIddSize = 0;//$$$
		_tmpExpRVA = 0;//$$$
#ifdef _WIN64
		_tmpRtfRVA = 0;
#endif
		return TRUE;
	}

	return Load2(pDoc, hash);
}

BOOL ZDll::Load2(ZDbgDoc* pDoc, DWORD hash)
{
	union {
		IMAGE_DOS_HEADER idh;
		IMAGE_NT_HEADERS32 inth32;
		IMAGE_NT_HEADERS64 inth64;
	};

	DWORD e_lfanew;

	if (
		0 > pDoc->Read(_BaseOfDll, &idh, sizeof(idh)) || 
		idh.e_magic != IMAGE_DOS_SIGNATURE ||
		0 > pDoc->Read(RtlOffsetToPointer(_BaseOfDll, e_lfanew = idh.e_lfanew), &inth64, sizeof(inth64)) ||
		inth64.Signature != IMAGE_NT_SIGNATURE
		)
	{
		return FALSE;
	}

	if (!_dllId) _dllId = hash + inth64.FileHeader.TimeDateStamp;

	switch (inth64.FileHeader.Machine)
	{
	case IMAGE_FILE_MACHINE_I386:

		if (inth32.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC)
		{
			return FALSE;
		}

		_SizeOfImage = inth32.OptionalHeader.SizeOfImage;

		if (inth32.OptionalHeader.AddressOfEntryPoint)
		{
			_EntryPoint = RtlOffsetToPointer(_BaseOfDll, inth32.OptionalHeader.AddressOfEntryPoint);
		}

		if (inth32.OptionalHeader.ImageBase != (ULONG_PTR)_BaseOfDll)
		{
			_NotAtBase = TRUE;
		}

		_tmpIddSize = inth32.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].Size;
		_tmpIddRva = inth32.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].VirtualAddress;

		_tmpExpSize = inth32.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;
		_tmpExpRVA = inth32.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
		break;

#ifdef _WIN64
	case IMAGE_FILE_MACHINE_AMD64:

		_Is64Bit = TRUE;

		if (inth64.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
		{
			return FALSE;
		}

		_SizeOfImage = inth64.OptionalHeader.SizeOfImage;

		if (inth64.OptionalHeader.AddressOfEntryPoint)
		{
			_EntryPoint = RtlOffsetToPointer(_BaseOfDll, inth64.OptionalHeader.AddressOfEntryPoint);
		}

		if (inth64.OptionalHeader.ImageBase != (ULONG_PTR)_BaseOfDll)
		{
			_NotAtBase = TRUE;
		}

		_tmpIddSize = inth64.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].Size;
		_tmpIddRva = inth64.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].VirtualAddress;

		_tmpExpSize = inth64.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;
		_tmpExpRVA = inth64.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;

		_tmpRtfRVA = inth64.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION].VirtualAddress;
		_tmpRtfSize = inth64.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION].Size;

		break;
#endif

	default: return FALSE;
	}

	return TRUE;
}

NTSTATUS OpenPdb(PdbReader* pdb, ZDbgDoc* pDoc, PVOID ImageBase, DWORD VirtualAddress, DWORD Size, PCWSTR PePath, PBOOL pbSystem)
{
	if (!VirtualAddress || !Size || (Size % sizeof IMAGE_DEBUG_DIRECTORY) || Size > MAXUSHORT)
	{
		return STATUS_NOT_FOUND;
	}

	PIMAGE_DEBUG_DIRECTORY pidd = (PIMAGE_DEBUG_DIRECTORY)alloca(Size);

	NTSTATUS status = SymReadMemory(pDoc, RtlOffsetToPointer(ImageBase, VirtualAddress), pidd, Size, 0);

	if (0 > status)
	{
		return status;
	}

	do 
	{
		if (pidd->Type == IMAGE_DEBUG_TYPE_CODEVIEW && pidd->SizeOfData > sizeof(CV_INFO_PDB))
		{
			DWORD SizeOfData = pidd->SizeOfData, AddressOfRawData = pidd->AddressOfRawData;

			if (AddressOfRawData && SizeOfData > sizeof(CV_INFO_PDB))
			{
				CV_INFO_PDB* lpcvh;
				if (SizeOfData > Size)
				{
					lpcvh = (CV_INFO_PDB*)alloca(SizeOfData - Size);
				}
				else
				{
					lpcvh = (CV_INFO_PDB*)pidd;
				}

				if (0 > (status = SymReadMemory(pDoc, RtlOffsetToPointer(ImageBase, AddressOfRawData), lpcvh, SizeOfData, 0)))
				{
					return status;
				}

				*RtlOffsetToPointer(lpcvh, SizeOfData - 1) = 0;

				PCWSTR NtSymbolPath = 0;
				if (pDoc)
				{
					NtSymbolPath = pDoc->NtSymbolPath();
				}

				if (!NtSymbolPath)
				{
					NtSymbolPath = static_cast<GLOBALS_EX*>(ZGLOBALS::get())->getPath();
				}
				return pdb->Open(lpcvh, NtSymbolPath, PePath, pbSystem);
			}
			break;
		}

	} while (pidd++, Size -= sizeof IMAGE_DEBUG_DIRECTORY);

	return STATUS_NOT_FOUND;
}

void ZDll::Parse(ZDbgDoc* pDoc)
{
	if (!_IsParsed)
	{
		if (pDoc->IsRemoteDebugger() && !pDoc->IsRemoteWait()) return;

		_IsParsed = TRUE;

		if (pDoc->IsRemoteDebugger())
		{
			if (!Load2(pDoc, 0))
			{
				return;
			}
		}

		ULONG expLen = 0;
		ProcessExport(pDoc, _BaseOfDll, &expLen);
		BOOL bSystem;

		PdbReader pdb;

		NTSTATUS status = OpenPdb(&pdb, pDoc, _BaseOfDll, _tmpIddRva, _tmpIddSize, _ImagePath, &bSystem);

		_tmpIddRva = 0, _tmpIddSize = 0;

		if (0 <= status)
		{
			if (bSystem)
			{
				_IsSystem = 1;
			}

			LoadPublicSymbols(&pdb, pdb.getPublicSymbolsStreamIndex(), expLen);

			if (GetNtMod() == _BaseOfDll)
			{
				if (!LdrpDispatchUserCallTarget)
				{
					LdrpDispatchUserCallTarget = getVaByName("LdrpDispatchUserCallTarget", pDoc);
				}

				if (!LdrpDispatchUserCallTargetES)
				{
					LdrpDispatchUserCallTargetES = getVaByName("LdrpDispatchUserCallTargetES", pDoc);
				}
			}

			if ((!_IsSystem || 0 > (INT_PTR)_BaseOfDll) && !pdb.IsOmapExist())
			{
				if (0 > LoadModuleInfo(&pdb) || 0 > LoadSC(&pdb))
				{
					DeleteLineInfo();
				}
			}
		}

		pDoc->OnDllParsed(this);
	}
}

void ZDll::ProcessExport(PIMAGE_EXPORT_DIRECTORY pied, DWORD ExpSize, DWORD ExpRVA, PULONG pLen)
{
	DWORD Ordinal;

	DWORD NumberOfFunctions = pied->NumberOfFunctions;

	if (!NumberOfFunctions || NumberOfFunctions >= (ExpSize >> 2))
	{
		return ;
	}

	DWORD AddressOfFunctions = pied->AddressOfFunctions - ExpRVA;

	Ordinal = AddressOfFunctions + (NumberOfFunctions << 2);

	if (AddressOfFunctions >= Ordinal || Ordinal > ExpSize)
	{
		return ;
	}

	DWORD NumberOfNames = pied->NumberOfNames;
	DWORD AddressOfNames = pied->AddressOfNames - ExpRVA;
	DWORD AddressOfNameOrdinals = pied->AddressOfNameOrdinals - ExpRVA;

	if (NumberOfNames)
	{
		Ordinal = AddressOfNames + (NumberOfNames << 2);

		if (AddressOfNames >= Ordinal || Ordinal > ExpSize)
		{
			return ;
		}

		Ordinal = AddressOfNameOrdinals + (NumberOfNames << 1);

		if (AddressOfNameOrdinals >= Ordinal || Ordinal > ExpSize)
		{
			return ;
		}
	}

	PSTR sz, name;
	Ordinal = pied->Base;
	PDWORD pAddressOfFunctions = (PDWORD)RtlOffsetToPointer(pied, AddressOfFunctions);
	PVOID pv;
	RVAOFS *pRO = (RVAOFS*)alloca(NumberOfFunctions * sizeof(RVAOFS)), *pd = pRO, *pSymbols;
	DWORD i = NumberOfFunctions, rva, len = 0, flen = 0, nForward = 0, ofs, k = 0;

	ULONG SizeOfImage = _SizeOfImage, NotInImage = 0;
	do 
	{
		pd->rva = rva = *pAddressOfFunctions++;
		pd->ofs = Ordinal|FLAG_ORDINAL;

		if (rva >= SizeOfImage)
		{
			if (rva & FLAG_FORWARD)
			{
				NotInImage++;
				pd->rva &= ~FLAG_FORWARD;
				if (!(rva & FLAG_NOEXPORT))
				{
					pd->rva |= FLAG_NOEXPORT;
					pd->ofs |= FLAG_NOEXPORT;
				}
			}
		}
		else
		{
			if ((rva -= ExpRVA) < ExpSize)
			{
				if (rva && (sz = strnchr(ExpSize - rva, name = RtlOffsetToPointer(pied, rva), 0)))
				{
					pd->rva = rva|FLAG_FORWARD;
					flen += RtlPointerToOffset(name, sz);
					nForward++;
				}
				else
				{
					pd->rva = 0;
				}
			}

			if (!pd->rva)
			{
				k++;
			}
		}

	} while (pd++, Ordinal++, --i);

	if (NumberOfNames)
	{
		PDWORD pAddressOfNames = (PDWORD)RtlOffsetToPointer(pied, AddressOfNames);
		PWORD pAddressOfNameOrdinals = (PWORD)RtlOffsetToPointer(pied, AddressOfNameOrdinals);

		do
		{
			WORD o = *pAddressOfNameOrdinals++;
			rva = *pAddressOfNames++ - ExpRVA;
			if (o < NumberOfFunctions && rva < ExpSize)
			{
				if (sz = strnchr(ExpSize - rva, name = RtlOffsetToPointer(pied, rva), 0))
				{
					//if (!strcmp(name, "NtOpenKey"))
					//{
					//	__nop();
					//}
					pd = pRO + o;
					pd->ofs = rva;
					i = RtlPointerToOffset(name, sz);
					if (pd->rva & FLAG_FORWARD)
					{
						flen += i;
					}
					else
					{
						len += i;
					}
				}
			}
			//else
			//{
			//		DbgBreak();
			//}
		} while (--NumberOfNames);
	}

	qsort(pRO, NumberOfFunctions, sizeof(RVAOFS), QSORTFN(RVAOFS::compare));

	NumberOfFunctions -= k, pRO += k;

	if (nForward)
	{
		pd = pRO + (NumberOfFunctions -= nForward);

		if (pv = new CHAR[nForward * sizeof(RVAOFS) + flen] )
		{
			_pForwards = pSymbols = (RVAOFS*)pv;
			_nForwards = nForward;
			name = (PSTR)(pSymbols + nForward);

			do 
			{
				if ((ofs = pd->ofs) & FLAG_ORDINAL)
				{
					pSymbols->ofs = ofs;
				}
				else
				{
					pSymbols->ofs = RtlPointerToOffset(pv, name);
					name = xcscpy(name, RtlOffsetToPointer(pied, ofs));
				}

				pSymbols++->rva = RtlPointerToOffset(pv, name);
				name = xcscpy(name, RtlOffsetToPointer(pied, NANE_OFS(pd++->rva)));

			} while (--nForward);
		}
	}

	if (NotInImage)
	{
		pd = pRO + (NumberOfFunctions - NotInImage);
		do 
		{
			if (pd->ofs & FLAG_NOEXPORT)
			{
				pd->ofs &= ~FLAG_NOEXPORT;
				pd->rva &= ~FLAG_NOEXPORT;
			}

			pd->rva |= FLAG_FORWARD;
		} while (pd++, --NotInImage);
	}

	if (NumberOfFunctions)
	{
		if (pv = new CHAR[len + (NumberOfFunctions + 2) * sizeof(RVAOFS)])
		{
			pSymbols = (RVAOFS*)pv;

			pSymbols++->rva = 0;// special invalid rva before 

			pv = pSymbols;
			_pSymbols = pSymbols;
			_nSymbols = NumberOfFunctions;

			name = (PSTR)(pSymbols + NumberOfFunctions + 1);
			do 
			{
				if ((ofs = pRO->ofs) & FLAG_ORDINAL)
				{
					pSymbols->ofs = ofs;
				}
				else
				{
					pSymbols->ofs = RtlPointerToOffset(pv, name);
					name = xcscpy(name, RtlOffsetToPointer(pied, ofs));
				}
				pSymbols++->rva = pRO++->rva;
			} while (--NumberOfFunctions);

			pSymbols->rva = 0;// special invalid rva after

			*pLen = len;
		}
	}
}

void ZDll::ProcessExport(ZDbgDoc* pDoc, PVOID BaseOfDll, PULONG pLen)
{
	DWORD ExpSize = _tmpExpSize, ExpRVA = _tmpExpRVA;

	_nSymbols = 0, _nForwards = 0;

	if (sizeof(IMAGE_EXPORT_DIRECTORY) < ExpSize)
	{
		if (PVOID pied = new CHAR[ExpSize])
		{
			if (0 <= pDoc->Read(RtlOffsetToPointer(BaseOfDll, ExpRVA), pied, ExpSize, 0))
			{
				ProcessExport((PIMAGE_EXPORT_DIRECTORY)pied, ExpSize, ExpRVA, pLen);
			}
			delete pied;
		}
	}
}


DbiModuleInfo** getModules(_In_ PdbReader* pdb, _Out_ PULONG pn);

struct MI 
{
	USHORT imod = MAXUSHORT;
	PBYTE _pb = 0;
	ULONG _cb;

	~MI()
	{
		if (PVOID pv = _pb)
		{
			delete [] pv;
		}
	}

	ULONG rva(DbiModuleInfo* pm, PdbReader* pdb, ULONG ibSym, PCSTR name)
	{
		USHORT s = pm->stream;
		if (s != MAXUSHORT)
		{
			union {
				PVOID pv;
				PBYTE pb;
				SYM_HEADER* ph;
				PROCSYM32* ps;
			};
			ULONG cb;
			if (s == imod)
			{
				cb = _cb, pb = _pb;
			}
			else
			{
				if (0 > pdb->getStream(s, &pv, &cb))
				{
					return 0;
				}
				if (_pb)
				{
					delete [] _pb;
					_pb = 0;
				}
				_pb = pb;
				_cb = cb;
				imod = s;
			}

			if (ibSym < cb) // Offset of actual symbol in $$Symbols
			{
				pb += ibSym, cb -= ibSym;

				if (IsValidSymbol(ps, cb))
				{
					switch (ph->type)
					{
					case S_GPROC32:
					case S_LPROC32:
					case S_GPROC32_ID:
					case S_LPROC32_ID:
					case S_LPROC32_DPC:
					case S_LPROC32_DPC_ID:
						if (!strcmp(ps->name, name))
						{
							return pdb->rva(ps->seg, ps->off);
						}
					}
				}
			}
		}

		return 0;
	}
};

struct MD 
{
	ULONG n;
	DbiModuleInfo** pmm;

	MD(PdbReader* pdb) : pmm(getModules(pdb, &n))
	{
	}

	~MD()
	{
		if (PVOID pv = pmm)
		{
			delete [] pv;
		}
	}

	DbiModuleInfo* operator[](ULONG i)
	{
		return i < n ? pmm[i] : 0;
	}
};

BOOL IsRvaExist(ULONG rva, RVAOFS *pSymbols, ULONG b)
{
	if (!b || r_rva(pSymbols->rva) > rva)
	{
		return 0;
	}

	ULONG a = 0, o, r;

	do 
	{
		if ((r = r_rva(pSymbols[o = (a + b) >> 1].rva)) == rva)
		{
			return TRUE;
		}

		r < rva ? a = o + 1 : b = o;

	} while (a < b);

	return FALSE;
}

ULONG ZDll::LoadSymbols(PULONG pstr_len,
						PdbReader* pdb,
						PVOID stream, 
						ULONG size, 
						MD& md, 
						RVAOFS* pSymbolsBase,
						ULONG nSymbols,
						BOOL bSecondLoop)
{
	union {
		PVOID pv;
		PBYTE pb;
		SYM_HEADER* psh;
		PUBSYM32* pbs;
		PROCSYM32* pps;
		REFSYM2* pls;
	};

	pv = stream;

	ULONG n = 0, len, szlens = 0;

	PSTR name = 0;
	MI mi;

	RVAOFS* pSymbols = pSymbolsBase + nSymbols;

	do 
	{
		len = psh->len + sizeof(WORD);

		if (size < len) 
		{
			return 0;
		}

		ULONG rva = 0;

		switch (psh->type)
		{
		case S_DATAREF:
		case S_PROCREF:
		case S_LPROCREF:
			if (bSecondLoop && IsValidSymbol(pls, size))
			{
				if (DbiModuleInfo* pm = md[pls->imod - 1])
				{
					name = pls->name;
					if (!*name)
					{
						continue;
					}
					if (rva = mi.rva(pm, pdb, pls->ibSym, pls->name))
					{
						if (!IsRvaExist(rva, pSymbolsBase, nSymbols))
						{
							break;
						}
					}
				}
			}
			continue;

		case S_PUB32:
			if (!bSecondLoop && IsValidSymbol(pbs, size))
			{
				name = pbs->name;
				if (!*name)
				{
					continue;
				}
				if (rva = pdb->rva(pbs->seg, pbs->off))
				{
					break;
				}
			}
			continue;
		default:
			continue;
		}

		ULONG nlen = (ULONG)strlen(name);

		if (!_Is64Bit)
		{
			switch (*name)
			{
			case '_':
			case '@':
				nlen--;
				if (PSTR c = strnchr(nlen, ++name, '@'))
				{
					*--c = 0;
					nlen = RtlPointerToOffset(name, c);
				}
				break;
			}
		}

		switch(IsRvaExported(rva, name))
		{
		case -1:
			rva |= FLAG_RVAEXPORT;
			break;
		case 0:
			rva |= FLAG_NOEXPORT;
			break;
		default:// +1
			continue;
		}

		pSymbols->rva = rva;
		pSymbols++->ofs = RtlPointerToOffset(stream, name);
		n++, szlens += 1 + nlen;

	} while (pb += len, size -= len);

	*pstr_len += szlens;
	return n;
}

BOOL IncludeSymbol(_In_ PCSTR name)
{
	switch (*name)
	{
	case '_':
		// not include __imp_ 
		if (name[1] == '_' && name[2] == 'i' && name[3] == 'm' && name[4] == 'p' && name[5] == '_')
		{
			// but include __imp_load_
			if (name[6] == 'l' && name[7] == 'o' && name[8] == 'a' && name[9] == 'd' && name[10] == '_')
			{
				break;
			}
			return FALSE;
		}
		break;
	case '?':
		// ??_C@_ `string` - not include
		if (name[1] == '?' && name[2] == '_' && name[3] == 'C' && name[4] == '@' && name[5] == '_')
		{
			return FALSE;
		}
		break;
	}

	return TRUE;
}

ULONG GetMaxSymCount(PVOID stream, ULONG size)
{
	union {
		PVOID pv;
		PBYTE pb;
		SYM_HEADER* psh;
		PUBSYM32* pbs;
		REFSYM2* pls;
	};

	pv = stream;

	ULONG n = 0, len;

	PSTR name = 0;

	do 
	{
		len = psh->len + sizeof(WORD);

		if (size < len) 
		{
			return 0;
		}

		switch (psh->type)
		{
		case S_DATAREF:
		case S_PROCREF:
		case S_LPROCREF:
			if (IsValidSymbol(pls, size))
			{
				name = pls->name;
				break;
			}
			continue;

		case S_PUB32:
			if (IsValidSymbol(pbs, size))
			{
				name = pbs->name;
				break;
			}
			continue;
		default:
			continue;
		}

		if (IncludeSymbol(name))
		{
			n++;
		}
		else
		{
			*name = 0;
		}

	} while (pb += len, size -= len);

	return n;
}

NTSTATUS ZDll::LoadPublicSymbols(PdbReader* pdb, PVOID stream, ULONG size, ULONG expLen)
{
	if (ULONG n = GetMaxSymCount(stream, size))
	{
		if (RVAOFS* pSymbols = new RVAOFS[n])
		{
			MD md(pdb);

			DWORD szlens = 0, ofs;

			if (n = LoadSymbols(&szlens, pdb, stream, size, md, pSymbols, 0, FALSE))
			{
				qsort(pSymbols, n, sizeof(RVAOFS), (QSORTFN)RVAOFS::compare_nf);
			}

			if (n += LoadSymbols(&szlens, pdb, stream, size, md, pSymbols, n, TRUE))
			{
				ULONG nSymbols = _nSymbols;

				if (PVOID pv = new UCHAR[(n + nSymbols + 2) * sizeof(RVAOFS) + szlens + expLen])
				{
					RVAOFS* pRO = (RVAOFS*)pv, *qRO;
					pRO++->rva = 0;
					PSTR c = (PSTR)(pRO + nSymbols + n + 1);
					RVAOFS* qv = _pSymbols;

					pv = pRO;

					_nSymbols = n + nSymbols;

					if (nSymbols)
					{
						qRO = _pSymbols;
						do 
						{
							if ((ofs = qRO->ofs) & FLAG_ORDINAL)
							{
								pRO->ofs = ofs;
							}
							else
							{
								pRO->ofs = RtlPointerToOffset(pv, c);
								c = xcscpy(c, RtlOffsetToPointer(qv, ofs));
							}
							pRO++->rva = qRO++->rva;

						} while (--nSymbols);

						delete [](qv - 1);
					}

					_pSymbols = (RVAOFS*)pv, qRO = pSymbols;

					do 
					{
						ofs = qRO->rva;
						pRO->ofs = RtlPointerToOffset(pv, c) | (ofs & 0xf0000000);
						pRO++->rva = ofs & 0x0fffffff;
						c = xcscpy(c, RtlOffsetToPointer(stream, qRO++->ofs));
					} while (--n);

					pRO->rva = 0;

					qsort(pv, _nSymbols, sizeof(RVAOFS), (QSORTFN)RVAOFS::compare);
				}
			}

			delete [] pSymbols;

			return STATUS_SUCCESS;
		}

		return STATUS_INSUFFICIENT_RESOURCES;
	}

	return STATUS_NOT_FOUND;
}

NTSTATUS ZDll::LoadPublicSymbols(PdbReader* pdb, SHORT symrecStream, ULONG expLen)
{
	ULONG size;
	PVOID pv;
	NTSTATUS status = STATUS_NOT_FOUND;

	if (0 < symrecStream)
	{
		if (0 <= (status = pdb->getStream(symrecStream, &pv, &size)))
		{
			status = LoadPublicSymbols(pdb, pv, size, expLen);
			pdb->FreeStream(pv);
		}
	}

	return STATUS_SUCCESS;
}

#define OFS_TO_NAME(ofs) ((ofs) & FLAG_ORDINAL ? (PCSTR)NANE_OFS(ofs) : _szSymbols + NANE_OFS(ofs))

PCSTR ZDll::getNameByIndex(ULONG Index, PINT_PTR pVa, PULONG pFlags)
{
	if (Index < _nSymbols)
	{
		DWORD ofs = _pSymbols[Index].ofs;
		*pVa = (INT_PTR)_BaseOfDll + _pSymbols[Index].rva;
		if (pFlags) *pFlags = NAME_FLAGS(ofs);
		return OFS_TO_NAME(ofs);
	}

	*pVa = MAXINT_PTR;
	return 0;
}

PCSTR ZDll::getNameByVa2(PVOID Va, PINT_PTR pVa)
{
	DWORD Rva = RtlPointerToOffset(_BaseOfDll, Va), b;
	RVAOFS* pRO, *p;

	*pVa = MAXINT_PTR;

	if (Rva < _SizeOfImage && (b = _nSymbols) && (Rva >= (pRO = _pSymbols)->rva))
	{
		DWORD a = 0, o, rva;

		do 
		{		
			p = &pRO[o = (a + b) >> 1];

			rva = p->rva;

			if (rva == Rva)
			{
				a = o;
				break;
			}

			if (Rva < rva)
			{
				b = o;
			}
			else
			{
				a = o + 1;
			}

		} while (a < b);

		if (Rva < rva)
		{
			p = &pRO[a - 1];
		}
		*pVa = (INT_PTR)_BaseOfDll + p->rva;
		return OFS_TO_NAME(p->ofs);
	}

	return 0;
}

PCSTR ZDll::getNameByVaEx(PVOID Va, PINT_PTR pVa, PULONG Index)
{
	DWORD Rva = RtlPointerToOffset(_BaseOfDll, Va), b, ofs;
	RVAOFS* pRO, *p;

	*Index = MAXDWORD;
	*pVa = MAXINT_PTR;

	if (Rva < _SizeOfImage && (b = _nSymbols) && (Rva <= (pRO = _pSymbols)[b-1].rva))
	{
		DWORD a = 0, o, rva;

		if (Rva <= pRO->rva)
		{
			*Index = 0;
			*pVa = (INT_PTR)_BaseOfDll + pRO->rva;
			ofs = pRO->ofs;
			return OFS_TO_NAME(ofs);
		}

		do 
		{		
			p = &pRO[o = (a + b) >> 1];

			rva = p->rva;

			if (rva == Rva)
			{
				a = o;
				break;
			}

			if (Rva < rva)
			{
				b = o;
			}
			else
			{
				a = o + 1;
			}

		} while (a < b);

		p = &pRO[a];
		rva = p->rva;
		while ((--p)->rva == rva)
		{
			a--;
		}
		*Index = a;
		*pVa = (INT_PTR)_BaseOfDll + rva;
		ofs = p->ofs;
		return OFS_TO_NAME(ofs);
	}

	return 0;
}

PCSTR ZDll::getNameByVa(PVOID Va, PULONG pFlags)
{
	DWORD Rva = RtlPointerToOffset(_BaseOfDll, Va), b;

	if (Rva < _SizeOfImage && (b = _nSymbols))
	{
		DWORD a = 0, o, rva;
		RVAOFS* pRO = _pSymbols, *p;

		if (pRO[0].rva <= Rva && Rva <= pRO[b-1].rva)
		{
			do 
			{		
				p = &pRO[o = (a + b) >> 1];

				rva = p->rva;

				if (rva == Rva)
				{
					DWORD ofs = p->ofs;
					if (pFlags) *pFlags = NAME_FLAGS(ofs);
					return OFS_TO_NAME(ofs);
				}

				if (Rva < rva)
				{
					b = o;
				}
				else
				{
					a = o + 1;
				}

			} while (a < b);
		}
	}

	return 0;
}

PVOID ZDll::getVaByName(PCSTR name, ZDbgDoc* pDoc)
{
	DWORD nSymbols, ofs;
	RVAOFS* pRO;

	if (*name == '#')
	{
		char* c;
		DWORD o = strtoul(name + 1, &c, 10) | FLAG_ORDINAL;
		if (!*c)
		{
			if (nSymbols = _nSymbols)
			{
				pRO = _pSymbols;

				do 
				{
					if (pRO->ofs == o)
					{
						return RtlOffsetToPointer(_BaseOfDll, (LONG)pRO->rva);
					}

				} while (pRO++, --nSymbols);
			}

			if (nSymbols = _nForwards)
			{
				pRO = _pForwards;

				do 
				{
					if (pRO->ofs == o)
					{
						return pDoc->getVaByName(_szForwards + pRO->rva);
					}

				} while (pRO++, --nSymbols);
			}
		}
	}
	else
	{
		if (nSymbols = _nSymbols)
		{
			pRO = _pSymbols;

			do 
			{
				ofs = pRO->ofs;

				if (!(ofs & FLAG_ORDINAL) && !strcmp(_szSymbols + NANE_OFS(ofs), name))
				{
					return RtlOffsetToPointer(_BaseOfDll, (LONG)pRO->rva);
				}

			} while (pRO++, --nSymbols);
		}

		if (nSymbols = _nForwards)
		{
			pRO = _pForwards;

			do 
			{
				ofs = pRO->ofs;

				if (!(ofs & FLAG_ORDINAL) && !strcmp(_szForwards + NANE_OFS(ofs), name))
				{
					return pDoc->getVaByName(_szForwards + pRO->rva);
				}

			} while (pRO++, --nSymbols);
		}
	}

	return 0;
}
#ifdef _WIN64

PRUNTIME_FUNCTION findRTF(DWORD Va, PRUNTIME_FUNCTION firstRT, DWORD N)
{
	DWORD a = 0, o;
	do 
	{
		PRUNTIME_FUNCTION pRT = &firstRT[o = (a + N) >> 1];

		if (Va < pRT->BeginAddress)
		{
			N = o;
		}
		else if (pRT->EndAddress <= Va)
		{
			a = o + 1;
		}
		else
		{
			return pRT;
		}

	} while (a < N);

	return 0;
}

BOOL ZDll::DoUnwind(INT_PTR Va, CONTEXT& ctx, ZDbgDoc* pDoc)
{
	PVOID ImageBase = _BaseOfDll;

	UINT_PTR Rip = ctx.Rip;

	PUINT_PTR Rsp = (PUINT_PTR)ctx.Rsp, grV = &ctx.Rax, Frame;

	if (PRUNTIME_FUNCTION pRT = findRTF(RtlPointerToOffset(ImageBase, Va), _prtf, _nrtf))
	{
		DWORD RipRva = RtlPointerToOffset(ImageBase, Rip);

		DWORD UnwindData;

		for (;;) 
		{
			while ((UnwindData = pRT->UnwindData) & 1)
			{
				pRT = (PRUNTIME_FUNCTION)RtlOffsetToPointer(_prtf, UnwindData - 1);
			}

			PUNWIND_INFO pui = (PUNWIND_INFO)RtlOffsetToPointer(_prtf, UnwindData);

			BYTE CountOfCodes = pui->CountOfCodes, FrameRegister = pui->FrameRegister;
			PUNWIND_CODE UnwindCode = pui->UnwindCode;

			if (FrameRegister)
			{
				Frame = (PUINT_PTR)grV[FrameRegister];
			}
			else
			{
				Frame = Rsp;
				FrameRegister = 4;
			}

			DWORD CodeOffset = RipRva - pRT->BeginAddress;

			while (CountOfCodes--)
			{
				BYTE i;
				DWORD cb;
				BOOL doUnwind = UnwindCode->CodeOffset <= CodeOffset;

				switch(UnwindCode->UnwindOp)
				{
				case UWOP_PUSH_NONVOL:

					if (doUnwind) 
					{
						if (0 > pDoc->Read(Rsp++, &grV[UnwindCode->OpInfo], sizeof(UINT_PTR))) return FALSE;
					}

					break;

				case UWOP_ALLOC_LARGE:

					if (!CountOfCodes--) return FALSE;

					if (UnwindCode++->OpInfo)
					{
						if (!CountOfCodes--) return FALSE;
						cb = *(DWORD*)UnwindCode;
						UnwindCode++;
					}
					else
					{
						cb = *(WORD*)UnwindCode << 3;
					}

					if (doUnwind) Rsp = (PUINT_PTR)RtlOffsetToPointer(Rsp, cb);
					break;

				case UWOP_ALLOC_SMALL:
					if (doUnwind) Rsp += UnwindCode->OpInfo + 1;
					break;

				case UWOP_SET_FPREG:
					if (doUnwind) Rsp = Frame - (pui->FrameOffset << 1);
					break;

				case UWOP_SAVE_NONVOL:
					if (!CountOfCodes--) return FALSE;
					i = UnwindCode->OpInfo;
					cb = (++UnwindCode)->FrameOffset;
					if (doUnwind) 
					{
						if (0 > pDoc->Read(Rsp + cb, &grV[i], sizeof(UINT_PTR))) return FALSE;
					}
					break;

				case UWOP_SAVE_NONVOL_FAR:
					if (!CountOfCodes-- || !CountOfCodes--) return FALSE;
					i = UnwindCode->OpInfo;
					cb = *(DWORD*)(UnwindCode + 1);
					if (doUnwind) 
					{
						if (0 > pDoc->Read(RtlOffsetToPointer(Rsp, cb), &grV[i], sizeof(UINT_PTR))) return FALSE;
					}
					UnwindCode += 2;
					break;

				case UWOP_SAVE_XMM128:
				case UWOP_SAVE_XMM:
					if (!CountOfCodes--) return FALSE;
					++UnwindCode;
					break;

				case UWOP_SAVE_XMM_FAR:
				case UWOP_SAVE_XMM128_FAR:
					if (!CountOfCodes-- || !CountOfCodes--) return FALSE;
					UnwindCode += 2;
					break;

				case UWOP_PUSH_MACHFRAME:
					UINT_PTR mf[4];
					if (0 > pDoc->Read(Rsp + UnwindCode->OpInfo, mf, sizeof(mf))) return FALSE;
					ctx.Rip = mf[0];
					ctx.Rsp = mf[3];
					return TRUE;
				}
				UnwindCode++;
			}

			if (!(pui->Flags & UNW_FLAG_CHAININFO))
			{
				break;
			}

			pRT = GetChainedFunctionEntry(pui);
		}
	}

	if (0 > pDoc->Read(Rsp, &ctx.Rip, sizeof(UINT_PTR))) return FALSE;
	ctx.Rsp = (UINT_PTR)(Rsp + 1);

	return TRUE;
}

void ZDll::CreateRTF(ZDbgDoc* pDoc, PRUNTIME_FUNCTION firstRT, DWORD Size, DWORD Va)
{
	DWORD N = Size / sizeof(RUNTIME_FUNCTION), v;

	DWORD Address = 0, n = N, minUnwindData = MAXDWORD, maxUnwindData = 0, cbUnwindData, UnwindData, cb;

	PRUNTIME_FUNCTION pRT = firstRT, pRT1, pRT2;

	do 
	{
		if (pRT->EndAddress <= pRT->BeginAddress || pRT->BeginAddress < Address) return ;
		
		UnwindData = pRT->UnwindData;

		if (UnwindData & 1)
		{
			v = UnwindData - 1 - Va;
			if (Size <= v || (v % sizeof(RUNTIME_FUNCTION))) return;
		}
		else
		{
			if (UnwindData < minUnwindData) minUnwindData = UnwindData;
			if (maxUnwindData < UnwindData) maxUnwindData = UnwindData;
		}

		Address = pRT++->EndAddress;

	} while(--n);

	cbUnwindData = maxUnwindData - minUnwindData;

	n = N, v = minUnwindData - Size, pRT = firstRT;

	cb = cbUnwindData + FIELD_OFFSET(UNWIND_INFO, UnwindCode[MAXBYTE]);
	
	pRT = (PRUNTIME_FUNCTION)new CHAR[Size + cb];

	if (!pRT) return ;

	memcpy(pRT, firstRT, Size);

	firstRT = pRT;

	if (0 <= SymReadMemory(pDoc, RtlOffsetToPointer(_BaseOfDll, minUnwindData), RtlOffsetToPointer(pRT, Size), cb)) 
	{
		PUNWIND_INFO pui;

		do 
		{
			if ((UnwindData = pRT->UnwindData) & 1)
			{
				pRT->UnwindData = UnwindData - Va;
			}
			else
			{
				pRT->UnwindData = UnwindData -= v;

				pui = (PUNWIND_INFO)RtlOffsetToPointer(firstRT, UnwindData);

				if ((pui->Flags & ~(UNW_FLAG_EHANDLER|UNW_FLAG_UHANDLER|UNW_FLAG_CHAININFO))) goto mm;

				if (pui->Flags & UNW_FLAG_CHAININFO)
				{
					pRT1 = GetChainedFunctionEntry(pui);

					pRT2 = findRTF(pRT1->BeginAddress, firstRT, N);

					if (!pRT2 || pRT1->BeginAddress != pRT2->BeginAddress || pRT1->EndAddress != pRT2->EndAddress)
					{
						goto mm;
					}

					ULONG u = pRT1->UnwindData - minUnwindData;

					if (pRT < pRT2)
					{
						// pRT2->UnwindData not-fixed yet

						if (pRT1->UnwindData == pRT2->UnwindData)
						{
							// fix pRT1->UnwindData
							if (cbUnwindData <= u) 
							{
								goto mm;
							}
							pRT1->UnwindData = u + Size;
						}
						else
						{
							//pRT1->UnwindData already fixed !?!

							if (pRT1->UnwindData - Size + minUnwindData != pRT2->UnwindData)
							{
								goto mm;
							}
						}
					}				
					else
					{
						// pRT2->UnwindData fixed already

						if (pRT1->UnwindData != pRT2->UnwindData)
						{
							// fix pRT1->UnwindData
							if (cbUnwindData <= u) 
							{
								goto mm;
							}

							pRT1->UnwindData = u + Size;

							if (pRT1->UnwindData != pRT2->UnwindData)
							{
								goto mm;
							}
						}
					}
				}
			}
		} while (++pRT, --n);
	}

mm:
	if (n)
	{
		delete firstRT;
		return ;
	}

	_prtf = firstRT, _nrtf = N, _RtfPresent = TRUE;
}

void ZDll::CreateRTF(ZDbgDoc* pDoc)
{
	if (_RtfParced)
	{
		return ;
	}

	_RtfParced = TRUE;

	if (!_Is64Bit)
	{
		return;
	}

	Parse(pDoc);

	ULONG Size = _tmpRtfSize, VirtualAddress = _tmpRtfRVA;

	if (!VirtualAddress || !Size || (Size % sizeof(RUNTIME_FUNCTION)))
	{
		return ;
	}

	if (PRUNTIME_FUNCTION firstRT = (PRUNTIME_FUNCTION)new CHAR[Size])
	{
		if (0 <= SymReadMemory(pDoc, RtlOffsetToPointer(_BaseOfDll, VirtualAddress), firstRT, Size))
		{
			CreateRTF(pDoc, firstRT, Size, VirtualAddress);
		}
		delete [] firstRT;
	}
}

#endif
_NT_END