#pragma once

// Renders Korean/Japanese text by rasterizing it with GDI and uploading the
// result as an OpenGL texture. Keeping this out of the shader path means the
// project needs no font library; the trade-off is that it is Windows-only,
// which the project already is.

#include <string>
#include <map>

#include "Dependencies\glew.h"

enum FontFace
{
	FONT_UI = 0,		// Malgun Gothic - readable at small sizes, used for HUD
	FONT_SERIF = 1		// Batang - period feel, used for dialogue and titles
};

struct TextTexture
{
	GLuint texture;
	int width;
	int height;
};

class TextRenderer
{
public:
	TextRenderer();
	~TextRenderer();

	// Rasterizes on first use and caches afterwards. Returns NULL on failure.
	const TextTexture* Get(const std::wstring& text, int pixelSize, FontFace face, bool bold);

	void Shutdown();

private:
	void* AcquireFont(int pixelSize, FontFace face, bool bold);	// returns HFONT
	const TextTexture* Rasterize(const std::wstring& cacheKey, const std::wstring& text,
		int pixelSize, FontFace face, bool bold);

	std::map<std::wstring, TextTexture> m_Cache;
	std::map<std::wstring, void*> m_Fonts;		// HFONT by size/face/bold
};

// Data files are authored in UTF-8 so that no Korean text has to live in the
// source (which MSVC would otherwise read in the system codepage).
std::wstring Utf8ToWide(const std::string& utf8);
