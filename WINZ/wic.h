#pragma once

struct LIC
{
	union {
		PVOID _pvBits;
		HBITMAP _hbmp;
		HICON _hi;
	};
	UINT _cx, _cy;

#ifdef __wincodec_h__
	static HRESULT FillBitsFromBitmapSource(IWICBitmapSource* pIBitmap, UINT cbStride, UINT cbBufferSize, PBYTE Bits);
	HRESULT FillBitsFromBitmapSource(IWICBitmapSource* pIBitmap, INT cx, INT cy);
	HRESULT FillBitsFromBitmapSource(IWICImagingFactory* piFactory, IWICBitmapSource* pIBitmap);
	HRESULT CreateBMPFromDecoder(_In_ IWICImagingFactory* piFactory, _In_ IWICBitmapDecoder* pIDecoder);
#endif

	HRESULT CreateBMPFromFile(_In_ PCWSTR pszFileName);
	HRESULT CreateBMPFromPNG(_In_ PVOID pvPNG, _In_ ULONG cbPNG);
	HRESULT CreateBMPFromPNG(_In_ PCWSTR pszName, _In_ PVOID hmod = &__ImageBase, _In_ PCWSTR pszType = RT_RCDATA);
	HRESULT CreateIconFromPNG (_In_ PCWSTR pszName, _In_ PVOID hmod = &__ImageBase, _In_ PCWSTR pszType = RT_RCDATA);
	HRESULT LoadIconWithPNG (_In_ PCWSTR pszName, _In_ PVOID hmod = &__ImageBase, _In_ UINT uType = IMAGE_ICON);
};

NTSTATUS AccessResource(_Out_ PDATA_BLOB blob, _In_ PCWSTR pszType, _In_ PCWSTR pszName, _In_ PVOID hmod = &__ImageBase);
HRESULT CreateIconFromBMP (_Out_ HICON *phico, _In_ HBITMAP hbmp, _In_ UINT cx, _In_ UINT cy);
HRESULT CreateIconFromBMP (_Out_ HICON *phico, _In_ HBITMAP hbmp);

#ifdef __wincodec_h__
HRESULT CreateWicFactory(_Out_ IWICImagingFactory** ppiFactory);
#endif
