#pragma once

struct LIC
{
	union {
		HBITMAP _hbmp;
		HICON _hi;
	};
	UINT _cx, _cy;

#ifdef __wincodec_h__
	HRESULT FillBitsFromBitmapSource(IWICBitmapSource* pIBitmap, INT cx, INT cy);
	HRESULT FillBitsFromBitmapSource(IWICImagingFactory* piFactory, IWICBitmapSource* pIBitmap);
#endif

	HRESULT CreateBMPFromPNG(_In_ PVOID pvPNG, _In_ ULONG cbPNG);
	HRESULT CreateBMPFromPNG(_In_ PCWSTR pszName, _In_ PVOID hmod = &__ImageBase, _In_ PCWSTR pszType = RT_RCDATA);
	HRESULT CreateIconFromPNG (_In_ PCWSTR pszName, _In_ PVOID hmod = &__ImageBase, _In_ PCWSTR pszType = RT_RCDATA);
	HRESULT LoadIconWithPNG (_In_ PCWSTR pszName, _In_ PVOID hmod = &__ImageBase);
};

NTSTATUS AccessResource(_Out_ PDATA_BLOB blob, _In_ PCWSTR pszType, _In_ PCWSTR pszName, _In_ PVOID hmod = &__ImageBase);

