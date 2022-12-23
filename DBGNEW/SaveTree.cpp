#include "stdafx.h"

_NT_BEGIN

struct VBuf 
{
	PVOID _BaseAddress = 0;
	SIZE_T _CommitSize = 0, _RegionSize = 0, _Ptr = 0;

	SIZE_T RoundTo64Kb(SIZE_T RegionSize)
	{
		return (RegionSize + 0xffff) & ~0xffff;
	}

	NTSTATUS Create(SIZE_T RegionSize)
	{
		NTSTATUS status = ZwAllocateVirtualMemory(NtCurrentProcess(), &_BaseAddress, 0, 
			&(RegionSize = RoundTo64Kb(RegionSize)), MEM_RESERVE, PAGE_READWRITE);

		if (0 <= status)
		{
			_RegionSize = RegionSize;
		}
		return status;
	}

	~VBuf()
	{
		if (_BaseAddress)
		{
			SIZE_T RegionSize = 0;
			ZwFreeVirtualMemory(NtCurrentProcess(), &_BaseAddress, &RegionSize, MEM_RELEASE);
		}
	}

	NTSTATUS EnsureBuf(SIZE_T RegionSize)
	{
		SIZE_T Size = _CommitSize - _Ptr;

		if (Size >= RegionSize)
		{
			return STATUS_SUCCESS;
		}

		if ((RegionSize -= Size) > _RegionSize)
		{
			return STATUS_BUFFER_TOO_SMALL;
		}

		PVOID BaseAddress = (PUCHAR)_BaseAddress + _CommitSize;

		NTSTATUS status = ZwAllocateVirtualMemory(NtCurrentProcess(), &BaseAddress, 0, 
			&(RegionSize = RoundTo64Kb(RegionSize)), MEM_COMMIT, PAGE_READWRITE);

		if (0 <= status)
		{
			_CommitSize += RegionSize, _RegionSize -= RegionSize;
		}

		return status;
	}

	NTSTATUS Add(PCWSTR pszText)
	{
		NTSTATUS status = EnsureBuf(MAXUCHAR + 1);

		if (0 > status)
		{
			return status;
		}

		ULONG cchWideChar = (ULONG)wcslen(pszText);

		ULONG cch = WideCharToMultiByte(CP_UTF8, 0, pszText, min(cchWideChar, MAXUCHAR), (PSTR)_BaseAddress + _Ptr + 1, MAXUCHAR, 0, 0);
		
		if (!cch)
		{
			return STATUS_UNSUCCESSFUL;
		}

		*((PUCHAR)_BaseAddress + _Ptr) = (UCHAR)cch;

		_Ptr += cch + 1;

		return STATUS_SUCCESS;
	}

	NTSTATUS Add_0()
	{
		NTSTATUS status = EnsureBuf(1);

		if (0 > status)
		{
			return status;
		}

		*((PUCHAR)_BaseAddress + _Ptr) = 0;

		_Ptr++;

		return STATUS_SUCCESS;
	}

	NTSTATUS Save(HANDLE hFile)
	{
		IO_STATUS_BLOCK iosb;
		return NtWriteFile(hFile, 0, 0, 0, &iosb, _BaseAddress, (ULONG)_Ptr, 0, 0);
	}
};

NTSTATUS Save(VBuf& vb, HWND hwndTV, TVITEM* item)
{
	if (!TreeView_GetItem(hwndTV, item) || !item->pszText || !*item->pszText)
	{
		return STATUS_UNSUCCESSFUL;
	}

	NTSTATUS status = vb.Add(item->pszText);

	if (0 <= status)
	{
		if (HTREEITEM hItem = TreeView_GetChild(hwndTV, item->hItem))
		{
			do 
			{
				item->hItem = hItem;
				if (0 > (status = Save(vb, hwndTV, item)))
				{
					return status;
				}
			} while (hItem = TreeView_GetNextSibling(hwndTV, hItem));
		}
		status = vb.Add_0();
	}
	
	return status;
}

HRESULT OnBrowse(_In_ HWND hwndDlg, 
				 _In_ UINT cFileTypes, 
				 _In_ const COMDLG_FILTERSPEC *rgFilterSpec, 
				 _Out_ PWSTR* ppszFilePath, 
				 _In_ UINT iFileType = 0,
				 _In_ const CLSID* pclsid = &__uuidof(FileOpenDialog),
				 _In_ PCWSTR pszDefaultExtension = 0);

NTSTATUS Save(HWND hwndTV, TVITEM* item, ULONG dwSize)
{
	if (item->hItem = TreeView_GetChild(hwndTV, TVI_ROOT))
	{
		VBuf vb;
		NTSTATUS status = vb.Create(dwSize);

		if (0 <= status)
		{
			if (0 <= (status = Save(vb, hwndTV, item)))
			{
				static const COMDLG_FILTERSPEC rgSpec[] =
				{ 
					{ L"TVI files", L"*.tvi" },
				};

				PWSTR lpstrFile;
				if (0 <= (status = OnBrowse(hwndTV, _countof(rgSpec), rgSpec, &lpstrFile, 0, &__uuidof(FileSaveDialog), L"tvi")))
				{
					if (HANDLE hFile = fixH(CreateFile(lpstrFile, FILE_APPEND_DATA, 0, 0, CREATE_ALWAYS, 0, 0)))
					{
						status = vb.Save(hFile);
						NtClose(hFile);
					}
					else
					{
						status = RtlGetLastNtStatus();
					}

					CoTaskMemFree(lpstrFile);
				}
			}
		}

		return status;
	}

	return STATUS_UNSUCCESSFUL;
}

_NT_END