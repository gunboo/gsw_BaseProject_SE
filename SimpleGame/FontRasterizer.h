#pragma once

#include <memory>
#include <string>
#include <vector>

enum FontFace
{
	FONT_UI = 0,
	FONT_SERIF = 1
};

struct RasterizedText
{
	int width = 0;
	int height = 0;
	std::vector<unsigned char> coverage;
};

class FontRasterizer
{
public:
	explicit FontRasterizer(bool forcePortable = false);
	~FontRasterizer();
	bool Rasterize(const std::wstring& text, int size, FontFace face, bool bold, RasterizedText& result);
	const char* Backend() const;

private:
	struct Impl;
	std::unique_ptr<Impl> m_Impl;
};

std::wstring Utf8ToWide(const std::string& utf8);
