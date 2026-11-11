#pragma once

// Cached text coverage is shared by the GDI and bundled-font backends.

#include <string>
#include <map>
#include <vector>

#include <GL/glew.h>

#include "FontRasterizer.h"

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
	const char* FontBackend() const { return m_Fonts.Backend(); }

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
	const TextTexture* Rasterize(const std::wstring& cacheKey, const std::wstring& text,
		int pixelSize, FontFace face, bool bold);

	std::map<std::wstring, TextTexture> m_Cache;
	TextCacheStats m_Stats;
	std::vector<Atlas> m_Atlases;
	FontRasterizer m_Fonts;
};
