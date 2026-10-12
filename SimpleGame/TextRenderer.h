#pragma once

// Renders Korean/Japanese text by rasterizing it with GDI and uploading the
// result into shared R8 texture arrays. Keeping rasterization in GDI means the
// project needs no font library; the trade-off is that it is Windows-only,
// which the project already is.

#include <string>
#include <map>
#include <vector>

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
	int layer;
	float u0;
	float v0;
	float u1;
	float v1;
};

struct TextCacheStats
{
	unsigned long long hits = 0;
	unsigned long long misses = 0;
	unsigned long long resets = 0;
	size_t entries = 0;
	size_t atlases = 0;
	size_t storageBytes = 0;
};

class TextRenderer
{
public:
	TextRenderer();
	~TextRenderer();

	// Rasterizes on first use and caches afterwards. Returns NULL on failure.
	const TextTexture* Get(const std::wstring& text, int pixelSize, FontFace face, bool bold);

	// Called only after the renderer has discarded the previous text queue.
	void BeginFrame();
	void Shutdown();
	TextCacheStats GetStats() const;

private:
	struct Atlas
	{
		GLuint texture;
		int size;
		int layer;
		int x;
		int y;
		int rowHeight;
	};

	bool StoreCoverage(const std::vector<unsigned char>& pixels, int width, int height, TextTexture& entry);
	void ClearTextures();
	void* AcquireFont(int pixelSize, FontFace face, bool bold);	// returns HFONT
	const TextTexture* Rasterize(const std::wstring& cacheKey, const std::wstring& text,
		int pixelSize, FontFace face, bool bold);

	std::map<std::wstring, TextTexture> m_Cache;
	TextCacheStats m_Stats;
	std::vector<Atlas> m_Atlases;
	std::map<std::wstring, void*> m_Fonts;		// HFONT by size/face/bold
};

// Data files are authored in UTF-8 so that no Korean text has to live in the
// source (which MSVC would otherwise read in the system codepage).
std::wstring Utf8ToWide(const std::string& utf8);
