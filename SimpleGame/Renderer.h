#pragma once

#include <string>
#include <vector>

#include "Dependencies\glew.h"
#include "TextRenderer.h"
#include "MeshCache.h"

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
Color Modulate(const Color& c, const Color& multiplier);

enum TextAlign
{
	ALIGN_LEFT = 0,
	ALIGN_CENTER = 1,
	ALIGN_RIGHT = 2
};

// Vertex animation kinds handled by Shaders/Batch.vs. Anything listed here
// moves on the GPU; the CPU only supplies a phase and an amplitude.
enum AnimKind
{
	ANIM_NONE = 0,
	ANIM_WATER = 1,
	ANIM_FLAME = 2,
	ANIM_SWAY = 3
};

// Everything needed to shade one instance. Lighting is evaluated once per
// object rather than once per polygon, which is what makes cached models
// cheaper than rebuilding their shapes every frame.
struct ShadeParams
{
	Color light;		// multiplier applied to lit polygons
	Color fogColor;
	float fog;			// 0 = clear, 1 = fully fogged
	Color tint;			// multiplier for polygons flagged as tintable
};

ShadeParams DefaultShade();

struct RenderStats
{
	unsigned long long frameId;
	double sortCpuMs;
	double worldSubmitCpuMs;
	double textSubmitCpuMs;
	unsigned long long gpuSourceFrame;
	double gpuElapsedMs;
	bool gpuQuerySkipped;
	unsigned int worldDrawCalls;
	unsigned int textDrawCalls;
	size_t polygonInstances;
	size_t triangleInstances;
	size_t textInstances;
	size_t instanceUploadBytes;
};

// Screen space used by every Push* call: pixels, origin at the window centre,
// +x right and +y DOWN (matching the quarter-view projection below).
//
//   screenX = (wx - wy) * TileHalfWidth()
//   screenY = (wx + wy) * TileHalfHeight() - wz * HeightScale()
//
// Geometry stays resident. Depth-sorted instances share one world draw unless
// the GPU buffer limit requires a split; text uses ordered atlas batches.
class Renderer
{
public:
	Renderer(int windowSizeX, int windowSizeY);
	~Renderer();

	bool IsInitialized() const;
	void Resize(int windowSizeX, int windowSizeY);

	int GetWidth() const { return (int)m_WindowSizeX; }
	int GetHeight() const { return (int)m_WindowSizeY; }

	void BeginFrame(const Color& clearColor, float elapsedSeconds);
	void EndFrame();
	const RenderStats& GetStats() const;
	TextCacheStats GetTextCacheStats() const;

	// Register immutable geometry during initialization, then upload/save once.
	int RegisterMesh(const std::string& name, const float* xy, int vertexCount);
	void PrepareMeshes();
	void PushMesh(int mesh, float x, float y, float scaleX, float scaleY,
		const Color& color, float depth);

	void SetCamera(float worldX, float worldY);
	void WorldToScreen(float worldX, float worldY, float worldZ, float* screenX, float* screenY) const;

	// Applies to every polygon pushed until the next call. Reset with ClearAnim.
	void SetAnim(AnimKind kind, float phase, float strength);
	void ClearAnim();

	// Three/four control points deform cached templates; larger meshes must be
	// registered once and submitted through PushMesh.
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
	static const int GPU_QUERY_SLOTS = 4;
	struct GpuQuery
	{
		GLuint id = 0;
		unsigned long long frame = 0;
		bool pending = false;
	};

	void BeginGpuTiming();
	// Eight RGBA32F texels, consumed by Batch.vs. No generated mesh vertices.
	struct PolygonInstance
	{
		float corners[8];
		Color colors[4];
		float animation[4];
		float transform[4];
	};

	struct BatchItem
	{
		float depth;
		int mesh;
		PolygonInstance instance;
	};

	struct TriangleCommand
	{
		int firstVertex;
		int instance;
	};

	struct TextInstance
	{
		float rectangle[4];
		float uv[4];
		Color color;
		float layer;
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
	PolygonInstance MakeInstance(const Color& color) const;

	bool m_Initialized;

	unsigned int m_WindowSizeX;
	unsigned int m_WindowSizeY;

	float m_CameraX;
	float m_CameraY;
	float m_ElapsedSeconds;

	float m_AnimKind;
	float m_AnimPhase;
	float m_AnimStrength;

	GLuint m_BatchShader;
	GLuint m_TextShader;

	GLint m_BatchUniformHalfViewport;
	GLint m_BatchUniformTime;
	GLint m_BatchUniformMeshes;
	GLint m_BatchUniformInstances;

	GLint m_TextUniformHalfViewport;
	GLint m_TextUniformTexture;
	GLint m_TextUniformMeshes;
	GLint m_TextUniformQuad;

	GLuint m_BatchVAO;
	GLuint m_BatchVBO;
	GLuint m_TextVAO;
	GLuint m_TextVBO;
	GLuint m_MeshBuffer;
	GLuint m_MeshTexture;
	GLuint m_InstanceBuffer;
	GLuint m_InstanceTexture;
	GLint m_MaxInstanceCount;
	bool m_MeshesReady;
	int m_TriangleMesh;
	int m_QuadMesh;
	int m_EllipseMeshes[49];
	float m_LastLogTime;
	RenderStats m_Stats;
	GpuQuery m_GpuQueries[GPU_QUERY_SLOTS];
	int m_ActiveGpuQuery = -1;
	unsigned long long m_RenderFrame = 0;
	MeshCache m_MeshCache;

	std::vector<BatchItem> m_Items;
	std::vector<PolygonInstance> m_Instances;
	std::vector<TriangleCommand> m_Commands;
	std::vector<int> m_SortOrder;
	std::vector<TextItem> m_TextItems;
	std::vector<TextInstance> m_TextInstances;

	TextRenderer m_Text;
};
