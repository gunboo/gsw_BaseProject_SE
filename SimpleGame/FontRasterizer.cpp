#include "stdafx.h"
#include "FontRasterizer.h"
#include "Platform.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <map>
#include <cstring>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#ifdef _WIN32
#include <Windows.h>
#endif

namespace
{
	const int TEXT_PAD = 2;
	const int MAX_TEXT_PIXELS = 16384;
	const char* const FONT_FILES[] =
	{
		"./Data/Fonts/NanumGothic-Regular.ttf", "./Data/Fonts/NanumGothic-Bold.ttf",
		"./Data/Fonts/NanumMyeongjo-Regular.ttf", "./Data/Fonts/NanumMyeongjo-Bold.ttf"
	};

	std::vector<int> Codepoints(const std::wstring& text)
	{
		std::vector<int> result;

		for (size_t i = 0; i < text.size(); ++i)
		{
			unsigned int code = static_cast<unsigned int>(text[i]);

			if (code >= 0xd800 && code <= 0xdbff && i + 1 < text.size())
			{
				const unsigned int low = static_cast<unsigned int>(text[i + 1]);

				if (low >= 0xdc00 && low <= 0xdfff)
				{
					code = 0x10000 + ((code - 0xd800) << 10) + low - 0xdc00;
					++i;
				}
			}

			result.push_back(static_cast<int>(code));
		}

		return result;
	}

#ifdef _WIN32
	int CALLBACK FoundFont(const LOGFONTW*, const TEXTMETRICW*, DWORD, LPARAM value)
	{
		*reinterpret_cast<bool*>(value) = true;
		return 0;
	}

	bool HasSystemFont(const wchar_t* name)
	{
		HDC dc = CreateCompatibleDC(NULL);
		bool found = false;

		if (dc != NULL)
		{
			LOGFONTW font = {};
			font.lfCharSet = DEFAULT_CHARSET;
			std::wcsncpy(font.lfFaceName, name, LF_FACESIZE - 1);
			EnumFontFamiliesExW(dc, &font, FoundFont, reinterpret_cast<LPARAM>(&found), 0);
			DeleteDC(dc);
		}

		return found;
	}
#endif
}

struct FontRasterizer::Impl
{
	struct Font
	{
		std::vector<unsigned char> bytes;
		stbtt_fontinfo info = {};
		bool loaded = false;
	};
	Font fonts[4];
	bool portable = true;
#ifdef _WIN32
	std::map<int, HFONT> handles;
	std::vector<std::wstring> registered;
	bool nativeFaces[2] = {};
#endif

	bool Load(int index)
	{
		Font& font = fonts[index];

		if (!font.loaded)
		{
			std::ifstream stream(FONT_FILES[index], std::ios::binary);
			font.bytes.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());

			if (font.bytes.empty() || !stbtt_InitFont(&font.info, font.bytes.data(), 0))
			{
				return false;
			}

			font.loaded = true;
		}

		return true;
	}
};

FontRasterizer::FontRasterizer(bool forcePortable)
	: m_Impl(new Impl())
{
#ifdef _WIN32
	m_Impl->portable = forcePortable || Platform::Environment("GAME_PORTABLE_FONTS") == "1";
	m_Impl->nativeFaces[FONT_UI] = HasSystemFont(L"Malgun Gothic");
	m_Impl->nativeFaces[FONT_SERIF] = HasSystemFont(L"Batang");

	if (!m_Impl->portable)
	{
		for (int i = 0; i < 4; ++i)
		{
			if (!m_Impl->nativeFaces[i / 2])
			{
				const std::wstring path = Utf8ToWide(FONT_FILES[i]);

				if (AddFontResourceExW(path.c_str(), FR_PRIVATE, NULL) > 0)
				{
					m_Impl->registered.push_back(path);
				}
			}
		}
	}
#else
	(void)forcePortable;
#endif
}

FontRasterizer::~FontRasterizer()
{
#ifdef _WIN32
	for (const auto& font : m_Impl->handles)
	{
		DeleteObject(font.second);
	}

	for (const std::wstring& path : m_Impl->registered)
	{
		RemoveFontResourceExW(path.c_str(), FR_PRIVATE, NULL);
	}
#endif
}

const char* FontRasterizer::Backend() const
{
#ifdef _WIN32
	return m_Impl->portable ? "bundled_stb_truetype"
		: m_Impl->nativeFaces[0] && m_Impl->nativeFaces[1] ? "windows_gdi" : "windows_gdi_bundled_fallback";
#else
	return "bundled_stb_truetype";
#endif
}

bool FontRasterizer::Rasterize(const std::wstring& text, int size, FontFace face, bool bold, RasterizedText& result)
{
	result = {};

	if (text.empty() || size < 1 || size > 512 || face < FONT_UI || face > FONT_SERIF)
	{
		return false;
	}

#ifdef _WIN32
	if (!m_Impl->portable)
	{
		const int key = size * 4 + static_cast<int>(face) * 2 + (bold ? 1 : 0);
		HFONT& font = m_Impl->handles[key];

		if (font == NULL)
		{
			const wchar_t* name = face == FONT_SERIF ? L"Batang" : L"Malgun Gothic";

			if (!m_Impl->nativeFaces[face])
			{
				name = face == FONT_SERIF ? L"NanumMyeongjo" : L"NanumGothic";
			}

			font = CreateFontW(-size, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE,
				HANGEUL_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, name);
		}

		HDC dc = CreateCompatibleDC(NULL);

		if (font == NULL || dc == NULL)
		{
			if (dc != NULL)
			{
				DeleteDC(dc);
			}

			return false;
		}

		HGDIOBJ oldFont = SelectObject(dc, font);
		SIZE extent = {};
		const bool measured = GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &extent) != 0;
		result.width = extent.cx + TEXT_PAD * 2;
		result.height = extent.cy + TEXT_PAD * 2;

		if (!measured || result.width > MAX_TEXT_PIXELS || result.height > MAX_TEXT_PIXELS)
		{
			SelectObject(dc, oldFont);
			DeleteDC(dc);
			return false;
		}

		BITMAPINFO info = {};
		info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		info.bmiHeader.biWidth = result.width;
		info.bmiHeader.biHeight = -result.height;
		info.bmiHeader.biPlanes = 1;
		info.bmiHeader.biBitCount = 32;
		info.bmiHeader.biCompression = BI_RGB;
		void* bits = NULL;
		HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, NULL, 0);

		if (bitmap == NULL || bits == NULL)
		{
			SelectObject(dc, oldFont);
			DeleteDC(dc);
			return false;
		}

		HGDIOBJ oldBitmap = SelectObject(dc, bitmap);
		std::memset(bits, 0, static_cast<size_t>(result.width) * result.height * 4);
		SetBkMode(dc, TRANSPARENT);
		SetTextColor(dc, RGB(255, 255, 255));
		TextOutW(dc, TEXT_PAD, TEXT_PAD, text.c_str(), static_cast<int>(text.size()));
		GdiFlush();
		const unsigned char* source = static_cast<const unsigned char*>(bits);
		result.coverage.resize(static_cast<size_t>(result.width) * result.height);

		for (size_t i = 0; i < result.coverage.size(); ++i)
		{
			result.coverage[i] = source[i * 4 + 2];
		}

		SelectObject(dc, oldBitmap);
		DeleteObject(bitmap);
		SelectObject(dc, oldFont);
		DeleteDC(dc);
		return true;
	}
#endif

	const int index = static_cast<int>(face) * 2 + (bold ? 1 : 0);

	if (!m_Impl->Load(index))
	{
		return false;
	}

	const stbtt_fontinfo& font = m_Impl->fonts[index].info;
	const float scale = stbtt_ScaleForPixelHeight(&font, static_cast<float>(size));
	int ascent, descent, gap;
	stbtt_GetFontVMetrics(&font, &ascent, &descent, &gap);
	const std::vector<int> codes = Codepoints(text);
	float advance = 0.0f;

	for (size_t i = 0; i < codes.size(); ++i)
	{
		int width, bearing;
		stbtt_GetCodepointHMetrics(&font, codes[i], &width, &bearing);
		advance += width * scale;

		if (i + 1 < codes.size())
		{
			advance += stbtt_GetCodepointKernAdvance(&font, codes[i], codes[i + 1]) * scale;
		}
	}

	result.width = static_cast<int>(ceilf(advance)) + TEXT_PAD * 2;
	result.height = static_cast<int>(ceilf((ascent - descent) * scale)) + TEXT_PAD * 2;

	if (result.width <= 0 || result.width > MAX_TEXT_PIXELS || result.height > MAX_TEXT_PIXELS)
	{
		return false;
	}

	result.coverage.assign(static_cast<size_t>(result.width) * result.height, 0);
	const int baseline = TEXT_PAD + static_cast<int>(ceilf(ascent * scale));
	advance = static_cast<float>(TEXT_PAD);

	for (size_t i = 0; i < codes.size(); ++i)
	{
		int width, bearing, x0, y0, x1, y1;
		stbtt_GetCodepointHMetrics(&font, codes[i], &width, &bearing);
		const float fractional = advance - floorf(advance);
		stbtt_GetCodepointBitmapBoxSubpixel(&font, codes[i], scale, scale, fractional, 0.0f, &x0, &y0, &x1, &y1);
		const int glyphWidth = x1 - x0;
		const int glyphHeight = y1 - y0;

		if (glyphWidth > 0 && glyphHeight > 0)
		{
			std::vector<unsigned char> bitmap(static_cast<size_t>(glyphWidth) * glyphHeight);
			stbtt_MakeCodepointBitmapSubpixel(&font, bitmap.data(), glyphWidth, glyphHeight, glyphWidth, scale, scale, fractional, 0.0f, codes[i]);

			for (int y = 0; y < glyphHeight; ++y)
			{
				for (int x = 0; x < glyphWidth; ++x)
				{
					const int px = static_cast<int>(floorf(advance)) + x0 + x;
					const int py = baseline + y0 + y;

					if (px >= 0 && px < result.width && py >= 0 && py < result.height)
					{
						unsigned char& pixel = result.coverage[py * result.width + px];
						pixel = (std::max)(pixel, bitmap[y * glyphWidth + x]);
					}
				}
			}
		}

		advance += width * scale;

		if (i + 1 < codes.size())
		{
			advance += stbtt_GetCodepointKernAdvance(&font, codes[i], codes[i + 1]) * scale;
		}
	}

	return true;
}

std::wstring Utf8ToWide(const std::string& utf8)
{
	std::wstring result;

	for (size_t i = 0; i < utf8.size();)
	{
		const unsigned char first = static_cast<unsigned char>(utf8[i]);
		unsigned int code = first;
		size_t length = first < 0x80 ? 1 : first >= 0xc2 && first <= 0xdf ? 2
			: first >= 0xe0 && first <= 0xef ? 3 : first >= 0xf0 && first <= 0xf4 ? 4 : 0;
		bool valid = length > 0 && i + length <= utf8.size();
		code &= length == 1 ? 0x7f : length == 2 ? 0x1f : length == 3 ? 0x0f : 0x07;

		for (size_t n = 1; valid && n < length; ++n)
		{
			const unsigned char byte = static_cast<unsigned char>(utf8[i + n]);
			valid = (byte & 0xc0) == 0x80;
			code = (code << 6) | (byte & 0x3f);
		}

		valid = valid && (length != 2 || code >= 0x80) && (length != 3 || code >= 0x800)
			&& (length != 4 || code >= 0x10000) && code <= 0x10ffff && !(code >= 0xd800 && code <= 0xdfff);

		if (!valid)
		{
			code = 0xfffd;
			length = 1;
		}

		if (sizeof(wchar_t) == 2 && code > 0xffff)
		{
			result.push_back(static_cast<wchar_t>(0xd800 + ((code - 0x10000) >> 10)));
			result.push_back(static_cast<wchar_t>(0xdc00 + ((code - 0x10000) & 0x3ff)));
		}
		else
		{
			result.push_back(static_cast<wchar_t>(code));
		}

		i += length;
	}

	return result;
}
