#include "stdafx.h"
#include "TextRenderer.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <vector>
#include <algorithm>
#include <cstdio>

namespace
{
	const size_t MAX_CACHED_TEXTS = 512;
	const int ATLAS_SIZE = 2048;
	const int ATLAS_LAYERS = 4;
	const int ATLAS_GUTTER = 1;

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

void TextRenderer::BeginFrame()
{
	// Changing counters would otherwise keep every past HUD texture forever.
	// Frame boundaries are safe because no queued TextItem still owns a pointer.
	if (m_Cache.size() > MAX_CACHED_TEXTS)
	{
		++m_Stats.resets;
		ClearTextures();
	}
}

TextCacheStats TextRenderer::GetStats() const
{
	TextCacheStats result = m_Stats;
	result.entries = m_Cache.size();
	result.atlases = m_Atlases.size();

	for (const Atlas& atlas : m_Atlases)
	{
		result.storageBytes += static_cast<size_t>(atlas.size) * atlas.size * ATLAS_LAYERS;
	}

	return result;
}

void TextRenderer::ClearTextures()
{
	for (const Atlas& atlas : m_Atlases)
	{
		glDeleteTextures(1, &atlas.texture);
	}

	m_Atlases.clear();
	m_Cache.clear();
}

bool TextRenderer::StoreCoverage(const std::vector<unsigned char>& pixels, int width, int height, TextTexture& entry)
{
	GLint maxSize = 0;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);
	const int packedWidth = width + ATLAS_GUTTER * 2;
	const int packedHeight = height + ATLAS_GUTTER * 2;

	if (packedWidth > maxSize || packedHeight > maxSize)
	{
		return false;
	}

	Atlas* selected = NULL;

	for (Atlas& atlas : m_Atlases)
	{
		if (packedWidth > atlas.size || packedHeight > atlas.size)
		{
			continue;
		}

		int x = atlas.x;
		int y = atlas.y;
		int layer = atlas.layer;
		int rowHeight = atlas.rowHeight;

		if (x + packedWidth > atlas.size)
		{
			x = 0;
			y += rowHeight;
			rowHeight = 0;
		}

		if (y + packedHeight > atlas.size)
		{
			++layer;
			x = 0;
			y = 0;
			rowHeight = 0;
		}

		if (layer >= ATLAS_LAYERS)
		{
			continue;
		}

		atlas.x = x;
		atlas.y = y;
		atlas.layer = layer;
		atlas.rowHeight = rowHeight;
		selected = &atlas;
		break;
	}

	glActiveTexture(GL_TEXTURE0);

	if (selected == NULL)
	{
		Atlas atlas = {};
		atlas.size = (std::max)((std::min)(ATLAS_SIZE, static_cast<int>(maxSize)),
			(std::max)(packedWidth, packedHeight));
		glGenTextures(1, &atlas.texture);
		glBindTexture(GL_TEXTURE_2D_ARRAY, atlas.texture);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_R8, atlas.size, atlas.size, ATLAS_LAYERS,
			0, GL_RED, GL_UNSIGNED_BYTE, NULL);
		m_Atlases.push_back(atlas);
		selected = &m_Atlases.back();
	}

	Atlas& atlas = *selected;
	GLint unpackAlignment = 0;
	glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpackAlignment);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glBindTexture(GL_TEXTURE_2D_ARRAY, atlas.texture);
	glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, atlas.x, atlas.y, atlas.layer,
		packedWidth, packedHeight, 1, GL_RED, GL_UNSIGNED_BYTE, pixels.data());
	glPixelStorei(GL_UNPACK_ALIGNMENT, unpackAlignment);
	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

	entry.texture = atlas.texture;
	entry.width = width;
	entry.height = height;
	entry.layer = atlas.layer;
	entry.u0 = static_cast<float>(atlas.x + ATLAS_GUTTER) / atlas.size;
	entry.v0 = static_cast<float>(atlas.y + ATLAS_GUTTER) / atlas.size;
	entry.u1 = static_cast<float>(atlas.x + ATLAS_GUTTER + width) / atlas.size;
	entry.v1 = static_cast<float>(atlas.y + ATLAS_GUTTER + height) / atlas.size;
	atlas.x += packedWidth;
	atlas.rowHeight = (std::max)(atlas.rowHeight, packedHeight);

	return true;
}

void TextRenderer::Shutdown()
{
	ClearTextures();

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
		++m_Stats.hits;
		return &it->second;
	}

	++m_Stats.misses;
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

	// Coverage-only atlas with an explicit zero gutter prevents neighbouring
	// labels (and uninitialized atlas space) bleeding through linear filtering.
	const int packedWidth = width + ATLAS_GUTTER * 2;
	const int packedHeight = height + ATLAS_GUTTER * 2;
	std::vector<unsigned char> pixels(static_cast<size_t>(packedWidth) * packedHeight, 0);
	const unsigned char* source = static_cast<const unsigned char*>(bits);

	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			pixels[(y + ATLAS_GUTTER) * packedWidth + x + ATLAS_GUTTER]
				= source[(y * width + x) * 4 + 2];
		}
	}

	SelectObject(dc, oldBitmap);
	DeleteObject(dib);
	SelectObject(dc, oldFont);
	DeleteDC(dc);

	TextTexture entry = {};

	if (!StoreCoverage(pixels, width, height, entry))
	{
		return NULL;
	}

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
