#include "stdafx.h"
#include "TextRenderer.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <vector>
#include <cstdio>

namespace
{
	std::wstring MakeCacheKey(const std::wstring& text, int pixelSize, FontFace face, bool bold)
	{
		wchar_t prefix[48];
		swprintf_s(prefix, 48, L"%d|%d|%d|", pixelSize, (int)face, bold ? 1 : 0);
		return std::wstring(prefix) + text;
	}
}

TextRenderer::TextRenderer()
{
}

TextRenderer::~TextRenderer()
{
	Shutdown();
}

void TextRenderer::Shutdown()
{
	for (std::map<std::wstring, TextTexture>::iterator it = m_Cache.begin(); it != m_Cache.end(); ++it)
	{
		if (it->second.texture != 0)
		{
			glDeleteTextures(1, &it->second.texture);
		}
	}
	m_Cache.clear();

	for (std::map<std::wstring, void*>::iterator it = m_Fonts.begin(); it != m_Fonts.end(); ++it)
	{
		if (it->second != NULL)
		{
			DeleteObject((HFONT)it->second);
		}
	}
	m_Fonts.clear();
}

void* TextRenderer::AcquireFont(int pixelSize, FontFace face, bool bold)
{
	wchar_t key[48];
	swprintf_s(key, 48, L"%d|%d|%d", pixelSize, (int)face, bold ? 1 : 0);

	std::map<std::wstring, void*>::iterator it = m_Fonts.find(key);
	if (it != m_Fonts.end())
	{
		return it->second;
	}

	// HANGEUL_CHARSET lets GDI substitute a Hangul-capable face if the requested
	// one is missing, so a machine without Batang still renders readable text.
	const wchar_t* faceName = (face == FONT_SERIF) ? L"Batang" : L"Malgun Gothic";

	HFONT font = CreateFontW(
		-pixelSize, 0, 0, 0,
		bold ? FW_BOLD : FW_NORMAL,
		FALSE, FALSE, FALSE,
		HANGEUL_CHARSET,
		OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
		ANTIALIASED_QUALITY,			// not CLEARTYPE: subpixel AA would tint the glyphs
		DEFAULT_PITCH | FF_DONTCARE,
		faceName);

	if (font == NULL)
	{
		return NULL;
	}

	m_Fonts[key] = font;
	return font;
}

const TextTexture* TextRenderer::Get(const std::wstring& text, int pixelSize, FontFace face, bool bold)
{
	if (text.empty())
	{
		return NULL;
	}

	std::wstring key = MakeCacheKey(text, pixelSize, face, bold);

	std::map<std::wstring, TextTexture>::iterator it = m_Cache.find(key);
	if (it != m_Cache.end())
	{
		return &it->second;
	}

	return Rasterize(key, text, pixelSize, face, bold);
}

const TextTexture* TextRenderer::Rasterize(const std::wstring& cacheKey, const std::wstring& text,
	int pixelSize, FontFace face, bool bold)
{
	HFONT font = (HFONT)AcquireFont(pixelSize, face, bold);
	if (font == NULL)
	{
		return NULL;
	}

	HDC dc = CreateCompatibleDC(NULL);
	if (dc == NULL)
	{
		return NULL;
	}

	HGDIOBJ oldFont = SelectObject(dc, font);

	SIZE extent;
	if (!GetTextExtentPoint32W(dc, text.c_str(), (int)text.length(), &extent))
	{
		SelectObject(dc, oldFont);
		DeleteDC(dc);
		return NULL;
	}

	const int pad = 2;					// keeps LINEAR filtering from clipping the edges
	const int width = extent.cx + pad * 2;
	const int height = extent.cy + pad * 2;

	if (width <= 0 || height <= 0)
	{
		SelectObject(dc, oldFont);
		DeleteDC(dc);
		return NULL;
	}

	BITMAPINFO info;
	ZeroMemory(&info, sizeof(info));
	info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	info.bmiHeader.biWidth = width;
	info.bmiHeader.biHeight = -height;	// negative: top-down rows, matching GL upload order
	info.bmiHeader.biPlanes = 1;
	info.bmiHeader.biBitCount = 32;
	info.bmiHeader.biCompression = BI_RGB;

	void* bits = NULL;
	HBITMAP dib = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, NULL, 0);
	if (dib == NULL || bits == NULL)
	{
		SelectObject(dc, oldFont);
		DeleteDC(dc);
		return NULL;
	}

	HGDIOBJ oldBitmap = SelectObject(dc, dib);

	// White text on a cleared black field: the resulting grey level per pixel is
	// exactly the coverage value we want in the alpha channel.
	memset(bits, 0, (size_t)width * height * 4);
	SetBkMode(dc, TRANSPARENT);
	SetTextColor(dc, RGB(255, 255, 255));
	TextOutW(dc, pad, pad, text.c_str(), (int)text.length());
	GdiFlush();

	std::vector<unsigned char> pixels((size_t)width * height * 4);
	const unsigned char* source = (const unsigned char*)bits;
	for (int i = 0; i < width * height; ++i)
	{
		unsigned char coverage = source[i * 4 + 2];		// BGRA layout; channels are equal here
		pixels[i * 4 + 0] = 255;
		pixels[i * 4 + 1] = 255;
		pixels[i * 4 + 2] = 255;
		pixels[i * 4 + 3] = coverage;
	}

	SelectObject(dc, oldBitmap);
	DeleteObject(dib);
	SelectObject(dc, oldFont);
	DeleteDC(dc);

	GLuint texture = 0;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, &pixels[0]);
	glBindTexture(GL_TEXTURE_2D, 0);

	TextTexture entry;
	entry.texture = texture;
	entry.width = width;
	entry.height = height;

	m_Cache[cacheKey] = entry;
	return &m_Cache[cacheKey];
}

std::wstring Utf8ToWide(const std::string& utf8)
{
	if (utf8.empty())
	{
		return std::wstring();
	}

	int needed = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), NULL, 0);
	if (needed <= 0)
	{
		return std::wstring();
	}

	std::wstring result((size_t)needed, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), &result[0], needed);
	return result;
}
