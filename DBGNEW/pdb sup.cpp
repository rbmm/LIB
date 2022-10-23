#include "StdAfx.h"

_NT_BEGIN

#include "pdb.h"
#include "dll.h"

extern volatile const UCHAR guz;

C_ASSERT(__alignof(CV_DebugSFile)==4);
C_ASSERT(__alignof(CV_DebugSLinesHeader_t)==4);
C_ASSERT(__alignof(CV_DebugSSubsectionHeader_t)==4);

CV_SourceChksum_t GetFileChecksum(_In_ CV_DebugSFile* pFile, _Out_ PBYTE* Checksum, _Out_ ULONG* cbChecksum)
{
	*Checksum = pFile->Checksum;
	*cbChecksum = pFile->cbChecksum;
	return pFile->ChecksumType;
}

static int __cdecl compareDWORD(DWORD& a, DWORD& b)
{
	if (a < b) return -1;
	if (a > b) return +1;
	return 0;
}

int bsearch(PULONG pu, ULONG b, ULONG u)
{
	ULONG a = 0, o, v;

	do 
	{
		if ((v = pu[o = (a + b) >> 1]) == u)
		{
			return o;
		}

		if (v < u) a = o + 1; else b = o;

	} while (a < b);

	return -1;
}

/*
FileInfo(size)
---------------------------------------------------
USHORT Modules;
USHORT Files;
USHORT FirstFileId[Modules];
USHORT FilesInModule[Modules];
ULONG  FileNamesOffsets[FileNames];
CHAR   FileNames[size - (1 + Modules + Names) * 4];
---------------------------------------------------
*/

NTSTATUS ValidateFileInfo(PUSHORT pu, ULONG size, PULONG pcbNames, PULONG pMaxFilesInModule)
{
	if (size <= sizeof(DWORD) || *RtlOffsetToPointer(pu, size - 1))
	{
		return STATUS_INVALID_IMAGE_FORMAT;
	}

	ULONG Modules = *pu++, Files = *pu++;;

	ULONG cbNames = size - (1 + Modules + Files) * 4;

	if (!Modules || !Files || 0 >= (LONG)cbNames)
	{
		return STATUS_INVALID_IMAGE_FORMAT;
	}

	PUSHORT pFilesInModule = pu + Modules;
	ULONG MaxFilesInModule = 0, FilesInModule;
	do 
	{
		ULONG FileId = *pu++;

		FileId += FilesInModule = *pFilesInModule++;

		if (FileId > Files)
		{
			return STATUS_INVALID_IMAGE_FORMAT;
		}

		if (FilesInModule > MaxFilesInModule)
		{
			MaxFilesInModule = FilesInModule;
		}

	} while (--Modules);

	PULONG FileNamesOffsets = (PULONG)pFilesInModule;
	do 
	{
		if (*FileNamesOffsets++ >= cbNames) return STATUS_INVALID_IMAGE_FORMAT;
	} while (--Files);

	*pcbNames = cbNames;
	*pMaxFilesInModule = MaxFilesInModule;

	return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////
//

struct ModuleParseContext 
{
	PULONG _FileNamesOffsets;	// for all files
	PCSTR _FileNames;			// for all files
	PLONG _bits;				// bitmask of all files with line numbers
	PULONG _DebugSFileOffsets;	// offsets of CV_DebugSFile from first
	SIZE_T _cbFileNames;		// size of all file names with line numbers
	ULONG _DebugSFileOfs;		// offset CV_DebugSFile array from stream

	ModuleParseContext(PULONG FileNamesOffsets, PCSTR FileNames, PLONG bits, PULONG DebugSFileOffsets)
	{
		_FileNamesOffsets = FileNamesOffsets;
		_FileNames = FileNames;
		_bits = bits;
		_DebugSFileOffsets = DebugSFileOffsets;

		_cbFileNames = 0;
	}

	NTSTATUS ValidateModuleFiles(
		IN CV_DebugSSubsectionHeader_t* pHeader, 
		IN ULONG size, 
		IN ULONG FilesInModule, 
		IN ULONG FileId 
		);

	NTSTATUS LoadModLines(
		PdbReader* pdb, 
		CV_DebugSSubsectionHeader_t* pHeader, 
		ULONG size,
		ULONG FilesInModule
		);

	PSTR CopyModuleFileNames(PSTR buf, CV_DebugSSubsectionHeader_t* pHeader);
};

PSTR ModuleParseContext::CopyModuleFileNames(PSTR buf, CV_DebugSSubsectionHeader_t* pHeader )
{
	ULONG cb, len;

	union
	{
		PBYTE pb;
		CV_DebugSFile* pFiles;
		CV_DebugSLinesHeader_t* Lines;
		CV_DebugSSubsectionHeader_t* Header;
	};

	Header = pHeader;

	for(;;) 
	{
		cb = (Header->cbLen + (__alignof(CV_DebugSSubsectionHeader_t) - 1)) & ~(__alignof(CV_DebugSSubsectionHeader_t) - 1);

		if (Header++->type == DEBUG_S_FILECHKSMS)
		{
			do 
			{
				len = (FIELD_OFFSET(CV_DebugSFile, Checksum[pFiles->cbChecksum]) + 
					(__alignof(CV_DebugSFile) - 1)) & ~(__alignof(CV_DebugSFile) - 1);

				LONG id = pFiles->OfsFileName;

				if (0 <= id)
				{
					if (_bittest(_bits, id))
					{
						// file have line numbers

						ULONG o = _FileNamesOffsets[id];

						if (!(o & 0x80000000))
						{
							PCSTR sz = _FileNames + o;
							SIZE_T cbStr = strlen(sz) + 1;
							buf = (PSTR)memcpy(buf, sz, cbStr) + cbStr;
							_FileNamesOffsets[id] = o = (ULONG)_cbFileNames;
							_cbFileNames += cbStr;
						}

						pFiles->OfsFileName = o & ~0x80000000;
					}
					else
					{
						// file have not line numbers
						pFiles->OfsFileName = MAXDWORD;
					}
				}

			} while (pb += len, cb -= len);

			return buf;
		}
		pb += cb;
	}
}

NTSTATUS ModuleParseContext::LoadModLines(
	PdbReader* pdb, 
	CV_DebugSSubsectionHeader_t* pHeader, 
	ULONG size,
	ULONG FilesInModule
	)
{
	ULONG cb, cbLen;
	union 
	{
		PBYTE pb;
		CV_DebugSLinesHeader_t* Lines;
		CV_DebugSLinesFileBlockHeader_t* Block;
		CV_DebugSSubsectionHeader_t* Header;
	};

	PVOID pvStream = pHeader;

	Header = pHeader;

	CV_DebugSLinesHeader_t* pLines = 0;

	do 
	{
		size -= sizeof(CV_DebugSSubsectionHeader_t);

		cbLen = Header->cbLen;

		cb = (cbLen + (__alignof(CV_DebugSSubsectionHeader_t) - 1)) & ~(__alignof(CV_DebugSSubsectionHeader_t) - 1);

		// already validated in ModuleParseContext::ValidateModuleFiles
		// if (size < cb) return STATUS_INVALID_IMAGE_FORMAT;

		size -= cb;

		pHeader = Header;

		if (Header++->type == DEBUG_S_LINES)
		{
			if (cb != cbLen || cb < sizeof(CV_DebugSLinesHeader_t))
			{
				return STATUS_INVALID_IMAGE_FORMAT;
			}

			cb -= sizeof(CV_DebugSLinesHeader_t);

			if (!(Lines->offCon = pdb->rva(Lines->segCon, Lines->offCon)))
			{
				return STATUS_INVALID_IMAGE_FORMAT;
			}

			pLines = Lines++;

			ULONG TotalLinesCount = 0, cbBlock, linesCount, offFile;

			ULONG DebugSFileOfs = _DebugSFileOfs;
			PULONG DebugSFileOffsets = _DebugSFileOffsets;

			// walk by CV_DebugSLinesFileBlockHeader_t

			do 
			{
				if (
					cb < sizeof(CV_DebugSLinesFileBlockHeader_t) 
					||
					cb < (cbBlock = Block->cbBlock) 
					||
					(cbBlock - FIELD_OFFSET(CV_DebugSLinesFileBlockHeader_t, lines[linesCount = Block->nLines]))
					)
				{
					return STATUS_INVALID_IMAGE_FORMAT;
				}

				TotalLinesCount += linesCount;

				// Block->offFile is offset into array of CV_DebugSFile[] in DEBUG_S_FILECHKSMS section 
				if (0 > bsearch(DebugSFileOffsets, FilesInModule, offFile = Block->offFile))
				{
					return STATUS_INVALID_IMAGE_FORMAT;
				}

				CV_DebugSFile* pFile = (CV_DebugSFile*)RtlOffsetToPointer(pvStream, offFile + DebugSFileOfs);

				// make Block->offFile relative offset from CV_DebugSLinesFileBlockHeader_t
				Block->offFile = RtlPointerToOffset(Block, pFile);

				if (!_bittestandset(_bits, pFile->OfsFileName))
				{
					// we first time view fileId (pFile->OfsFileName now is id)
					_cbFileNames += 1 + strlen(_FileNames + _FileNamesOffsets[pFile->OfsFileName]);
				}

			} while (pb += cbBlock, cb -= cbBlock);

			if (!TotalLinesCount)
			{
				pHeader->type = DEBUG_S_IGNORE;
				continue;
			}

			// not last CV_DebugSLinesHeader_t
			pLines->flags = 0;
		}

	} while (pb += cb, size);

	if (pLines)
	{
		// mark the last CV_DebugSLinesHeader_t
		pLines->flags = 1;
		return 0;
	}

	// no CV_DebugSLinesHeader_t at all, ignore this module
	return STATUS_INVALID_IMAGE_FORMAT;
}

NTSTATUS ModuleParseContext::ValidateModuleFiles(
	IN CV_DebugSSubsectionHeader_t* pHeader, 
	IN ULONG size, 
	IN ULONG FilesInModule, 
	IN ULONG FileId
	)
{
	ULONG cb;
	union
	{
		PBYTE pb;
		CV_DebugSFile* pFiles;
		CV_DebugSSubsectionHeader_t* Header;
	};

	Header = pHeader;

	do 
	{
		if (size < sizeof(CV_DebugSSubsectionHeader_t))
		{
			return STATUS_INVALID_IMAGE_FORMAT;
		}

		size -= sizeof(CV_DebugSSubsectionHeader_t);

		cb = (Header->cbLen + (__alignof(CV_DebugSSubsectionHeader_t) - 1)) & ~(__alignof(CV_DebugSSubsectionHeader_t) - 1);

		if (size < cb)
		{
			return STATUS_INVALID_IMAGE_FORMAT;
		}

		size -= cb;

		if (Header++->type == DEBUG_S_FILECHKSMS)
		{
			if (!FilesInModule)
			{
				return STATUS_INVALID_IMAGE_FORMAT;
			}

			// offset CV_DebugSFile array from stream
			_DebugSFileOfs = RtlPointerToOffset(pHeader, pFiles);

			PULONG DebugSFileOffsets = _DebugSFileOffsets;

			ULONG len, DebugSFileOffset = 0;

			do 
			{
				if (cb < sizeof(CV_DebugSFile))
				{
					return STATUS_INVALID_IMAGE_FORMAT;
				}

				len = (FIELD_OFFSET(CV_DebugSFile, Checksum[pFiles->cbChecksum]) + 
					(__alignof(CV_DebugSFile) - 1)) & ~(__alignof(CV_DebugSFile) - 1);

				if (cb < len)
				{
					return STATUS_INVALID_IMAGE_FORMAT;
				}

				ULONG cbChecksum = pFiles->cbChecksum;

				switch (pFiles->ChecksumType)
				{
				case CHKSUM_TYPE_NONE:
					if (cbChecksum != 0)
					{
						return STATUS_INVALID_IMAGE_FORMAT;
					}
					break;
				case CHKSUM_TYPE_MD5:
					if (cbChecksum != 16)
					{
						return STATUS_INVALID_IMAGE_FORMAT;
					}
					break;
				case CHKSUM_TYPE_SHA1:
					if (cbChecksum != 20)
					{
						return STATUS_INVALID_IMAGE_FORMAT;
					}
				case CHKSUM_TYPE_SHA_256:
					if (cbChecksum != 32)
					{
						return STATUS_INVALID_IMAGE_FORMAT;
					}
					break;
				}

				pFiles->OfsFileName = FileId++;// temporary store file ID

				*DebugSFileOffsets++ = DebugSFileOffset;// offset of current CV_DebugSFile
				DebugSFileOffset += len;
				pb += len, cb -= len;
				--FilesInModule;
			} while (cb && FilesInModule);

			if (FilesInModule || cb)
			{
				return STATUS_INVALID_IMAGE_FORMAT;
			}
		}
		else
		{
			pb += cb;
		}

	} while (size);

	return FilesInModule ? STATUS_NOT_FOUND : 0;
}

//////////////////////////////////////////////////////////////////////////
// ZDll

PCSTR ZDll::GetFileName(CV_DebugSFile* pFile)
{
	return _pFileNames + pFile->OfsFileName;
}

void ZDll::DeleteLineInfo()
{
	if (_pSC)
	{
		delete [] _pSC;
		_pSC = 0;
	}

	if (_pFileNames) 
	{
		delete [] _pFileNames;
		_pFileNames = 0;
	}

	if (CV_DebugSSubsectionHeader_t ** ppv = _pMI)
	{
		ULONG n = _nMI;
		do 
		{
			if (CV_DebugSSubsectionHeader_t * pv = *ppv++)
			{
				delete [] pv;
			}
		} while (--n);

		delete [] _pMI;
		_pMI = 0;
	}
}

ULONG ZDll::GetLineByRVA(ULONG rva, CV_DebugSSubsectionHeader_t* pHeader, CV_DebugSFile** ppFile)
{
	union {
		PBYTE pb;
		CV_DebugSLinesFileBlockHeader_t* Block;
		CV_DebugSLinesHeader_t* pLines;
		CV_DebugSSubsectionHeader_t* Header;
	};

	Header = pHeader;

	ULONG linesCount;
	CV_Line_t* plo;

	for(;;) 
	{
		ULONG cb = (Header->cbLen + (__alignof(CV_DebugSSubsectionHeader_t)-1)) & ~(__alignof(CV_DebugSSubsectionHeader_t)-1);

		if (Header++->type == DEBUG_S_LINES)
		{
			BOOL IsLastLineBlock = pLines->flags;

			ULONG ofs = pLines->offCon;

			if (rva - ofs < pLines->cbCon)
			{
				++pLines, cb -= sizeof(CV_DebugSLinesHeader_t), rva -= ofs;

				ULONG cbBlock;

				CV_Line_t* _plo = 0;
				CV_DebugSLinesFileBlockHeader_t* _Block = 0;
				do 
				{
					cbBlock = Block->cbBlock;

					if (linesCount = Block->nLines)
					{
						plo = Block->lines;

						do 
						{
							if (rva < plo->offset)
							{
								if (_plo)
								{
									if (_plo + 1 != plo)
									{
__last_line:
										Block = _Block;
									}
									*ppFile = (CV_DebugSFile*)RtlOffsetToPointer(Block, (LONG)Block->offFile);

									return _plo->linenumStart;
								}

								return 0;
							}

						} while (_plo = plo++, --linesCount);

						_Block = Block;
					}

				} while (pb += cbBlock, cb -= cbBlock);

				goto __last_line;
			}

			if (IsLastLineBlock)
			{
				// will be - look ModuleParseContext::LoadModLines
				return 0;
			}
		}

		pb += cb;
	}
}

ULONG ZDll::GetLineByVA(PVOID Va, CV_DebugSFile** ppFile)
{
	union {
		ULONG_PTR up;
		ULONG rva;
	};

	up = (ULONG_PTR)Va - (ULONG_PTR)_BaseOfDll;

	SC* pSC = _pSC, *qSC = pSC;

	if (!pSC)
	{
		return 0;
	}

	ULONG a = 0, b = _nSC, o, ofs;

	do 
	{
		o = (a + b) >> 1;

		pSC = qSC + o;
		ofs = pSC->offset;

		if (rva < ofs)
		{
			b = o;
		}
		else if (rva < ofs + pSC->size)
		{
			if (pSC->module < _nMI)
			{
				if (CV_DebugSSubsectionHeader_t* pv = _pMI[pSC->module])
				{
					return GetLineByRVA(rva, pv, ppFile);
				}
			}
			return 0;
		}
		else
		{
			a = o + 1;
		}

	} while (a < b);

	return 0;
}

LINE_INFO* ZDll::GetSrcLines(CV_DebugSFile* pFile, PULONG pN)
{
	if (CV_DebugSSubsectionHeader_t** pm = _pMI)
	{
		PVOID stack = alloca(guz);
		LINE_INFO* pLI = (LINE_INFO*)stack, *_pLI, LI;
		ULONG nMI = _nMI, k = 0;
		ULONG Rva, EndRva;

		do 
		{
			union {
				PBYTE pb;
				CV_DebugSLinesFileBlockHeader_t* Block;
				CV_DebugSLinesHeader_t* pLines;
				CV_DebugSSubsectionHeader_t* Header;
			};

			if (Header = *pm++)
			{
				for(;;) 
				{
					ULONG cb = (Header->cbLen + (__alignof(CV_DebugSSubsectionHeader_t)-1)) & ~(__alignof(CV_DebugSSubsectionHeader_t)-1);

					if (Header++->type == DEBUG_S_LINES)
					{
						Rva = pLines->offCon, EndRva = Rva + pLines->cbCon;

						cb -= sizeof(CV_DebugSLinesHeader_t);

						BOOL IsLastLineBlock = pLines++->flags;

						_pLI = &LI;

						ULONG cbBlock;
						do 
						{
							if (pFile == (CV_DebugSFile*)RtlOffsetToPointer(Block, (LONG)Block->offFile))
							{
								if (ULONG linesCount = Block->nLines)
								{
									CV_Line_t* plo = Block->lines;

									do 
									{
										if (--pLI < stack)
										{
											stack = alloca(sizeof(LINE_INFO)*64);
										}

										pLI->line = plo->linenumStart;
										pLI->Rva = Rva + plo->offset;
										_pLI->len = pLI->Rva - _pLI->Rva;
										k++;

										_pLI = pLI;

									} while (++plo, --linesCount);
								}
							}
							else if (_pLI != &LI)
							{
								if (Block->lines)
								{

									pLI->len = Rva + Block->lines->offset - pLI->Rva;
									_pLI = &LI;
								}
							}

							cbBlock = Block->cbBlock;

						} while (pb += cbBlock, cb -= cbBlock);

						if (_pLI != &LI)
						{
							pLI->len = EndRva - pLI->Rva;
						}

						if (IsLastLineBlock)
						{
							// we always will be here - look ModuleParseContext::LoadModLines
							break;
						}
					}

					pb += cb;
				}
			}
		} while (--nMI);

		if (k)
		{
			if (LINE_INFO* p = new LINE_INFO[k])
			{
				qsort(pLI, k, sizeof(LINE_INFO), QSORTFN(LINE_INFO::compare));
				memcpy(p, pLI, k * sizeof(LINE_INFO));
				*pN = k;
				return p;
			}
		}
	}

	return 0;
}

NTSTATUS ZDll::LoadModuleInfo(PdbReader* pdb)
{
	LONG size;
	PUSHORT pFileInfo = pdb->getFileInfo((ULONG&)size);

	if (!pFileInfo)
	{
		return STATUS_NOT_FOUND;
	}

	ULONG cbNames, MaxFilesInModule;

	NTSTATUS status = ValidateFileInfo(pFileInfo, size, &cbNames, &MaxFilesInModule);

	if (0 > status)
	{
		return status;
	}

	union {
		DbiModuleInfo* module;
		PCSTR sz;
		ULONG_PTR up;
	};

	module = pdb->getModuleInfo((ULONG&)size);

	if (!module || size < 2)
	{
		return STATUS_NOT_FOUND;
	}

	PCSTR end = RtlOffsetToPointer(module, size - 2);

	if (end[0] || end[1])
	{
		return STATUS_INVALID_IMAGE_FORMAT;
	}

	ULONG Modules = *pFileInfo++, Files = *pFileInfo++;

	PUSHORT FilesInModule = pFileInfo + Modules;
	PULONG FileNamesOffsets = (PULONG)(FilesInModule + Modules);
	PCSTR FileNames = (PCSTR)(FileNamesOffsets + Files);
	PLONG bits = (PLONG)alloca( (Files + 7) >> 3 );
	RtlZeroMemory(bits, (Files + 7) >> 3);

	ModuleParseContext ctx(
		FileNamesOffsets,FileNames,bits,(PULONG)alloca(MaxFilesInModule * sizeof(ULONG))
		);

	if (CV_DebugSSubsectionHeader_t** pm = new CV_DebugSSubsectionHeader_t* [Modules])
	{
		_pMI = pm, _nMI = Modules;

		CV_DebugSSubsectionHeader_t* Stream;

		do 
		{
			if (0 > (size -= sizeof(DbiModuleInfo)))
			{
				break;
			}

			USHORT stream = module->stream, files = module->files, FirstFileId = *pFileInfo++;
			ULONG cbLines = module->cbLines, cbSym = module->cbSyms;

			if (*FilesInModule++ != files)
			{
				break;
			}

			sz = module->buf;

			ULONG len = 1 + (ULONG)strlen(sz);

			if (0 >= (size -= len))
			{
				break;
			}

			sz += len;

			len = 1 + (ULONG)strlen(sz);

			if (0 > (size -= len))
			{
				break;
			}

			sz += len;

			len = (__alignof(DbiModuleInfo) - up) & (__alignof(DbiModuleInfo) - 1);

			if (0 > (size -= len))
			{
				break;
			}

			sz += len;

			Stream = 0;

			if (cbLines && files)
			{
				if (ULONG s = pdb->getStreamSize(stream))
				{
					if (s > cbSym && (s -= cbSym) >= cbLines)
					{
						if (Stream = (CV_DebugSSubsectionHeader_t*)new BYTE[cbLines])
						{
							if (pdb->Read(stream, cbSym, Stream, cbLines))
							{
								if (
									ctx.ValidateModuleFiles(Stream, cbLines, files, FirstFileId) 
									|| 
									ctx.LoadModLines(pdb, Stream, cbLines, files)
									)
								{
									delete [] Stream;
									Stream = 0;
								}
							}
							else
							{
								delete [] Stream;
								Stream = 0;
							}
						}
					}
				}
			}

			*pm++ = Stream;

		} while (--Modules && size);

		if (Modules || size)
		{
			return STATUS_INVALID_IMAGE_FORMAT;
		}

		if (ctx._cbFileNames)
		{
			PSTR NewFileNames = new char[ctx._cbFileNames];

			if (!NewFileNames)
			{
				return STATUS_INSUFFICIENT_RESOURCES;
			}

			_pFileNames = NewFileNames;

			pm = _pMI, Modules = _nMI, ctx._cbFileNames = 0x80000000;

			do 
			{
				if (Stream = *pm++)
				{
					NewFileNames = ctx.CopyModuleFileNames(NewFileNames, Stream);
				}
			} while (--Modules);
		}
		return STATUS_SUCCESS;
	}

	return STATUS_INSUFFICIENT_RESOURCES;
}

NTSTATUS ZDll::LoadSC(PdbReader* pdb)
{
	ULONG nSecCon;
	DbiSecCon* pSecCon = pdb->getSecCon(nSecCon);

	if (!pSecCon || !nSecCon)
	{
		return STATUS_NOT_FOUND;
	}

	if (SC* pSC = new SC[nSecCon])
	{
		_pSC = pSC;
		_nSC = nSecCon;

		ULONG _offset = 0, offset;
		BOOL bNeedSort = FALSE;

		do 
		{
			if (!(offset = pdb->rva(pSecCon->section, pSecCon->offset)))
			{
				delete []_pSC;
				_pSC = 0;
				return STATUS_INVALID_IMAGE_FORMAT;
			}

			if (offset <= _offset)
			{
				bNeedSort = TRUE;
			}
			else
			{
				_offset = offset;
			}

			pSC->offset = offset;
			pSC->size = pSecCon->size;
			pSC++->module = pSecCon++->module;

		} while (--nSecCon);

		if (bNeedSort)
		{
			qsort(_pSC, _nSC, sizeof(SC), (QSORTFN)compareDWORD);
		}

		return STATUS_SUCCESS;
	}

	return STATUS_INSUFFICIENT_RESOURCES;
}

_NT_END