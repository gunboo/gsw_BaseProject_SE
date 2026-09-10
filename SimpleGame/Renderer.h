#pragma once

#include <string>
#include <vector>

#include "Dependencies\glew.h"
#include "TextRenderer.h"

struct Color
{
	float r;
	float g;
	float b;
	float a;
};

Color RGBA(float r, float g, float b, float a = 1.0f);
Color Mix(const Color& from, const Color& to, float t);
Color Scale(const Color& c, float factor);

enum TextAlign
{
	ALIGN_LEFT = 0,
	ALIGN_CENTER = 1,
	ALIGN_RIGHT = 2
};

// Screen space used by every Push* call: pixels, origin at the window centre,
// +x right and +y DOWN (matching the quarter-view projection below).
//
//   screenX = (wx - wy) * TileHalfWidth()
//   screenY = (wx + wy) * TileHalfHeight() - wz * HeightScale()
//
// Everything is collected into one vertex buffer, sorted back-to-front by the
// caller-supplied depth, and issued as a single draw call per frame.
class Renderer
{
public:
	Renderer(int windowSizeX, int windowSizeY);
	~Renderer();

	bool IsInitialized() const;
	void Resize(int windowSizeX, int windowSizeY);

	int GetWidth() const { return (int)m_WindowSizeX; }
	int GetHeight() const { return (int)m_WindowSizeY; }

	void BeginFrame(const Color& clearColor);
	void EndFrame();

	void SetCamera(float worldX, float worldY);
	void WorldToScreen(float worldX, float worldY, float worldZ, float* screenX, float* screenY) const;

	// Convex polygons only - they are triangulated as a fan.
	void PushPolygon(const float* xy, int vertexCount, const Color& color, float depth);
	void PushPolygonShaded(const float* xy, const Color* colors, int vertexCount, float depth);

	void PushRect(float x, float y, float width, float height, const Color& color, float depth);
	void PushDiamond(float centerX, float centerY, float halfWidth, float halfHeight, const Color& color, float depth);
	void PushEllipse(float centerX, float centerY, float radiusX, float radiusY, const Color& color, float depth, int segments = 14);
	void PushLine(float x0, float y0, float x1, float y1, float thickness, const Color& color, float depth);

	// Text is drawn in a separate pass after the polygon batch, in push order.
	void PushText(const std::wstring& text, float x, float y, int pixelSize, FontFace face,
		bool bold, const Color& color, TextAlign align = ALIGN_LEFT);
	float MeasureTextWidth(const std::wstring& text, int pixelSize, FontFace face, bool bold);
	float MeasureTextHeight(const std::wstring& text, int pixelSize, FontFace face, bool bold);

	static float TileHalfWidth() { return 32.0f; }
	static float TileHalfHeight() { return 16.0f; }
	static float HeightScale() { return 26.0f; }

private:
	struct BatchVertex
	{
		float x;
		float y;
		float r;
		float g;
		float b;
		float a;
	};

	struct BatchItem
	{
		float depth;
		int first;
		int count;
	};

	struct TextItem
	{
		const TextTexture* texture;
		float x;
		float y;
		Color color;
	};

	void Initialize();
	bool ReadFile(const char* filename, std::string* target);
	GLuint AddShader(GLuint shaderProgram, const char* shaderText, GLenum shaderType);
	GLuint CompileShaders(const char* filenameVS, const char* filenameFS);
	void CreateBuffers();
	void FlushPolygons();
	void FlushText();

	bool m_Initialized;

	unsigned int m_WindowSizeX;
	unsigned int m_WindowSizeY;

	float m_CameraX;
	float m_CameraY;

	GLuint m_BatchShader;
	GLuint m_TextShader;

	GLint m_BatchAttribPosition;
	GLint m_BatchAttribColor;
	GLint m_BatchUniformHalfViewport;

	GLint m_TextAttribPosition;
	GLint m_TextAttribTexCoord;
	GLint m_TextUniformHalfViewport;
	GLint m_TextUniformTexture;
	GLint m_TextUniformColor;

	GLuint m_BatchVAO;
	GLuint m_BatchVBO;
	GLuint m_TextVAO;
	GLuint m_TextVBO;

	std::vector<BatchVertex> m_PolygonVertices;
	std::vector<BatchItem> m_Items;
	std::vector<BatchVertex> m_TriangleVertices;
	std::vector<int> m_SortOrder;
	std::vector<TextItem> m_TextItems;

	TextRenderer m_Text;
};
