#include "../SimpleGame/stdafx.h"
#include "../SimpleGame/FontRasterizer.h"

#include <algorithm>
#include "TestAssertions.h"
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#include <Windows.h>
#endif

int main()
{
	const std::string text = u8"\uBB38\uC591\uC758 \uD589\uBC29 100 / 100 Space";
	const std::wstring wide = Utf8ToWide(text);
	assert(!wide.empty());
	assert(Utf8ToWide("abc") == L"abc");
	assert(Utf8ToWide("\xf0\x9f\x98\x80").size() == (sizeof(wchar_t) == 2 ? 2u : 1u));
	assert(Utf8ToWide("\xc0").size() == 1);
	FontRasterizer portable(true);
	FontRasterizer native;

	for (int face = 0; face < 2; ++face)
	{
		for (int bold = 0; bold < 2; ++bold)
		{
			RasterizedText raster;
			RasterizedText again;
			const int size = 18 + face * 8;
			assert(portable.Rasterize(wide, size, static_cast<FontFace>(face), bold != 0, raster));
			assert(raster.width > 40 && raster.height >= size);
			assert(raster.coverage.size() == static_cast<size_t>(raster.width) * raster.height);
			assert(*std::max_element(raster.coverage.begin(), raster.coverage.end()) > 100);
			assert(portable.Rasterize(wide, size, static_cast<FontFace>(face), bold != 0, again));
			assert(again.coverage == raster.coverage);
			assert(native.Rasterize(wide, size, static_cast<FontFace>(face), bold != 0, raster));
#ifdef _WIN32
			if (std::strcmp(native.Backend(), "windows_gdi") == 0)
			{
				// Original GDI settings and two-pixel padding must keep identical metrics/coverage.
				HDC dc = CreateCompatibleDC(NULL);
				HFONT font = CreateFontW(-size, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE,
					HANGEUL_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
					face == FONT_SERIF ? L"Batang" : L"Malgun Gothic");
				HGDIOBJ oldFont = SelectObject(dc, font);
				SIZE extent = {};
				assert(GetTextExtentPoint32W(dc, wide.c_str(), static_cast<int>(wide.size()), &extent));
				assert(raster.width == extent.cx + 4 && raster.height == extent.cy + 4);
				BITMAPINFO info = {};
				info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
				info.bmiHeader.biWidth = raster.width;
				info.bmiHeader.biHeight = -raster.height;
				info.bmiHeader.biPlanes = 1;
				info.bmiHeader.biBitCount = 32;
				void* bits = NULL;
				HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, NULL, 0);
				HGDIOBJ oldBitmap = SelectObject(dc, bitmap);
				std::memset(bits, 0, raster.coverage.size() * 4);
				SetBkMode(dc, TRANSPARENT);
				SetTextColor(dc, RGB(255, 255, 255));
				TextOutW(dc, 2, 2, wide.c_str(), static_cast<int>(wide.size()));
				GdiFlush();

				for (size_t i = 0; i < raster.coverage.size(); ++i)
				{
					assert(raster.coverage[i] == static_cast<unsigned char*>(bits)[i * 4 + 2]);
				}

				SelectObject(dc, oldBitmap);
				SelectObject(dc, oldFont);
				DeleteObject(bitmap);
				DeleteObject(font);
				DeleteDC(dc);
			}
#endif
		}
	}

	RasterizedText empty;
	assert(!portable.Rasterize(L"", 18, FONT_UI, false, empty));
	std::puts("PASS: UTF-8; bundled Korean regular/bold UI/serif; deterministic coverage; native font metrics/coverage");

	return 0;
}
