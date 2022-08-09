#include "stdafx.h"

#include <initguid.h>
#include <wincodec.h>
#include "..\NtVer\nt_ver.h"
_NT_BEGIN

#include "wic.h"

NTSTATUS AccessResource(_Out_ PDATA_BLOB blob, _In_ PCWSTR pszType, _In_ PCWSTR pszName, _In_ PVOID hmod)
{
	NTSTATUS status;
	PIMAGE_RESOURCE_DATA_ENTRY pirde;
	PCWSTR pri[] = { pszType, pszName, 0 };

	0 <= (status = LdrFindResource_U(hmod, pri, _countof(pri), &pirde)) &&
		0 <= (status = LdrAccessResource(hmod, pirde, (void**)&blob->pbData, &blob->cbData));

	return status;
}

HRESULT CreateWicFactory(IWICImagingFactory** ppiFactory)
{
	return CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, __uuidof(IWICImagingFactory), (void**)ppiFactory);
}

HRESULT LIC::FillBitsFromBitmapSource(IWICBitmapSource* pIBitmap, UINT cbStride, UINT cbBufferSize, PBYTE Bits)
{
	HRESULT hr = pIBitmap->CopyPixels(0, cbStride, cbBufferSize, Bits);

	if (0 <= hr)
	{
		union BGRA {
			ULONG rgba;
			struct
			{
				UCHAR B, G, R, A;
			};
		} *p = (BGRA*)Bits;

		cbStride = cbBufferSize >>= 2;

		do
		{
			ULONG A = p->A;
			if (A < p->B || A < p->G || A < p->R)
			{
				p = (BGRA*)Bits;

				do
				{
					if (A = p->A)
					{
						A = (A << 24) | ((A * p->B) / 0xff) | (((A * p->G) / 0xff) << 8) | (((A * p->R) / 0xff) << 16);
					}
					p++->rgba = A;
				} while (--cbBufferSize);

				break;
			}
		} while (p++, --cbStride);
	}

	return hr;
}

HRESULT LIC::FillBitsFromBitmapSource(IWICBitmapSource* pIBitmap, INT cx, INT cy)
{
	PVOID Bits;
	UINT cbStride = cx << 2, cbBufferSize = cbStride * cy;

	_cx = cx, _cy = cy;

	if (Bits = _pvBits)
	{
		return FillBitsFromBitmapSource(pIBitmap, cbStride, cbBufferSize, (PBYTE)Bits);
	}

	BITMAPINFO bmi = { { sizeof(BITMAPINFOHEADER), cx, -cy, 1, 32, BI_RGB }};

	if (HBITMAP hbmp = CreateDIBSection(0, &bmi, DIB_RGB_COLORS, &Bits, 0, 0))
	{
		HRESULT hr = FillBitsFromBitmapSource(pIBitmap, cbStride, cbBufferSize, (PBYTE)Bits);

		if (0 > hr) DeleteObject(hbmp); else _hbmp = hbmp;

		return hr;
	}

	return HRESULT_FROM_WIN32(GetLastError());
}

HRESULT LIC::FillBitsFromBitmapSource(IWICImagingFactory* piFactory, IWICBitmapSource* pIBitmap)
{
	UINT cx_image, cy_image, cx = _cx, cy = _cy;

	HRESULT hr = pIBitmap->GetSize(&cx_image, &cy_image);

	if (0 <= hr)
	{
		if (cx >= cx_image && cy >= cy_image)
		{
			return FillBitsFromBitmapSource(pIBitmap, cx_image, cy_image);
		}

		if (cx * cy_image != cy * cx_image)
		{
			struct __declspec(align(16)) __m128d_ab { double a, b; };

			union {
				__m128d f;
				__m128d_ab g;
			};

			g.a = (double)cx / (double)cx_image, g.b = (double)cy / (double)cy_image;

			g.b < g.a ? g.a = g.b : g.b = g.a;

			g.a *= cx_image, cx = _mm_cvttsd_si32(f);

			g.a = g.b * cy_image, cy = _mm_cvttsd_si32(f);
		}

		IWICBitmapScaler* piScaler;

		if (0 <= (hr = piFactory->CreateBitmapScaler(&piScaler)))
		{
			if (0 <= (hr = piScaler->Initialize(pIBitmap, cx, cy, 
				g_nt_ver.Version < _WIN32_WINNT_WIN10 ? WICBitmapInterpolationModeFant : WICBitmapInterpolationModeHighQualityCubic)))
			{
				hr = FillBitsFromBitmapSource(piScaler, cx, cy);
			}

			piScaler->Release();
		}
	}

	return hr;
}

HRESULT LIC::CreateBMPFromDecoder(_In_ IWICImagingFactory* piFactory, _In_ IWICBitmapDecoder* pIDecoder)
{
	HRESULT hr;
	IWICBitmapFrameDecode* pIBitmapFrame;
	IWICFormatConverter* pIFormatConverter;

	if (0 <= (hr = pIDecoder->GetFrame(0, &pIBitmapFrame)))
	{
		WICPixelFormatGUID pf;
		if (0 <= (hr = pIBitmapFrame->GetPixelFormat(&pf)))
		{
			if (pf == GUID_WICPixelFormat32bppBGRA ||
				pf == GUID_WICPixelFormat32bppBGR)
			{
				hr = FillBitsFromBitmapSource(piFactory, pIBitmapFrame);
			}
			else
			{
				if (0 <= (hr = piFactory->CreateFormatConverter(&pIFormatConverter)))
				{
					if (0 <= (hr = pIFormatConverter->Initialize(pIBitmapFrame, 
						GUID_WICPixelFormat32bppBGRA,
						WICBitmapDitherTypeNone, 0, 0.0, WICBitmapPaletteTypeCustom)))
					{
						hr = FillBitsFromBitmapSource(piFactory, pIFormatConverter);
					}

					pIFormatConverter->Release();
				}
			}
		}

		pIBitmapFrame->Release();
	}

	return hr;
}
HRESULT LIC::CreateBMPFromPNG(PVOID pvPNG, ULONG cbPNG)
{
	IWICBitmapDecoder *pIDecoder;
	IWICImagingFactory *piFactory;

	HRESULT hr = CreateWicFactory(&piFactory);

	if (0 <= hr)
	{
		IWICStream *pIWICStream;
		if (0 <= (hr = piFactory->CreateStream(&pIWICStream)))
		{
			if (0 <= (hr = pIWICStream->InitializeFromMemory((WICInProcPointer)pvPNG, cbPNG)))
			{
				if (0 <= (hr = piFactory->CreateDecoderFromStream(pIWICStream, 0, 
					WICDecodeMetadataCacheOnDemand, &pIDecoder)))
				{
					hr = CreateBMPFromDecoder(piFactory, pIDecoder);

					pIDecoder->Release();
				}
			}

			pIWICStream->Release();
		}

		piFactory->Release();
	}

	return hr;
}

HRESULT LIC::CreateBMPFromFile(_In_ PCWSTR pszFileName)
{
	IWICBitmapDecoder* pIDecoder;
	IWICImagingFactory* piFactory;

	HRESULT hr = CreateWicFactory(&piFactory);

	if (0 <= hr)
	{
		if (0 <= (hr = piFactory->CreateDecoderFromFilename(pszFileName,
			0, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &pIDecoder)))
		{
			hr = CreateBMPFromDecoder(piFactory, pIDecoder);
			pIDecoder->Release();
		}

		piFactory->Release();
	}

	return hr;
}

HRESULT LIC::CreateBMPFromPNG(_In_ PCWSTR pszName, _In_ PVOID hmod, _In_ PCWSTR pszType)
{
	DATA_BLOB png;

	NTSTATUS status = AccessResource(&png, pszType, pszName, hmod);

	return 0 > status ? HRESULT_FROM_NT(status) : CreateBMPFromPNG(png.pbData, png.cbData);
}

HRESULT CreateIconFromBMP (_Out_ HICON *phico, _In_ HBITMAP hbmp, _In_ UINT cx, _In_ UINT cy)
{
	ICONINFO ii = { 
		TRUE, 0, 0, CreateBitmap(cx, cy, 1, 1, 0), hbmp
	};

	if (ii.hbmMask)
	{
		HICON hi = CreateIconIndirect(&ii);

		DeleteObject(ii.hbmMask);

		if (hi)
		{
			*phico = hi;
			return S_OK;
		}
	}

	return HRESULT_FROM_WIN32(GetLastError());
}

HRESULT CreateIconFromBMP (_Out_ HICON *phico, _In_ HBITMAP hbmp)
{
	BITMAP bm;

	return GetObject(hbmp, sizeof(bm), &bm) ?
		CreateIconFromBMP (phico, hbmp, bm.bmWidth, bm.bmHeight) : HRESULT_FROM_WIN32(GetLastError());
}


HRESULT LIC::CreateIconFromPNG (_In_ PCWSTR pszName, _In_ PVOID hmod, _In_ PCWSTR pszType)
{
	HRESULT hr = CreateBMPFromPNG(pszName, hmod, pszType);

	if (0 <= hr)
	{
		HBITMAP hbmp = _hbmp;
		hr = CreateIconFromBMP(&_hi, hbmp, _cx, _cy);
		DeleteObject(hbmp);
	}

	return hr;
}

HRESULT LIC::LoadIconWithPNG (_In_ PCWSTR pszName, _In_ PVOID hmod, _In_ UINT uType)
{
	switch (uType)
	{
	case IMAGE_ICON:
	case IMAGE_BITMAP:
		break;
	default: return E_INVALIDARG;
	}

	DATA_BLOB ico;
	NTSTATUS status = AccessResource(&ico, RT_GROUP_ICON, pszName, hmod);

	if (0 > status)
	{
		return HRESULT_FROM_NT(status);
	}

	if (UINT id = LookupIconIdFromDirectoryEx(ico.pbData, TRUE, _cx, _cy, 0))
	{
		return uType == IMAGE_ICON ? 
			CreateIconFromPNG (MAKEINTRESOURCEW(id), hmod, RT_ICON) : CreateBMPFromPNG (MAKEINTRESOURCEW(id), hmod, RT_ICON);
	}

	return HRESULT_FROM_NT(STATUS_RESOURCE_NAME_NOT_FOUND);
}

_NT_END