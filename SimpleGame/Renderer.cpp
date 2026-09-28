#include "stdafx.h"
#include "Renderer.h"
#include "AnalysisLog.h"

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>

namespace
{
	const int INSTANCE_TEXELS = 8;
	const GLuint TEXT_ATTRIBUTES = 4;
	const float STATS_LOG_INTERVAL = 5.0f;
	const char* const MESH_CACHE_PATH = "./Data/render_meshes.cache";

	// Ellipse meshes are prepared once, for each supported segment count.
	const int MIN_ELLIPSE_SEGMENTS = 3;
	const int MAX_ELLIPSE_SEGMENTS = 48;
}

Color RGBA(float r, float g, float b, float a)
{
	Color c;
	c.r = r;
	c.g = g;
	c.b = b;
	c.a = a;

	return c;
}

Color Mix(const Color& from, const Color& to, float t)
{
	if (t < 0.0f)
	{
		t = 0.0f;
	}

	if (t > 1.0f)
	{
		t = 1.0f;
	}

	Color c;
	c.r = from.r + (to.r - from.r) * t;
	c.g = from.g + (to.g - from.g) * t;
	c.b = from.b + (to.b - from.b) * t;
	c.a = from.a + (to.a - from.a) * t;

	return c;
}

Color Scale(const Color& c, float factor)
{
	Color out;
	out.r = c.r * factor;
	out.g = c.g * factor;
	out.b = c.b * factor;
	out.a = c.a;

	return out;
}

Color Modulate(const Color& c, const Color& multiplier)
{
	Color out;
	out.r = c.r * multiplier.r;
	out.g = c.g * multiplier.g;
	out.b = c.b * multiplier.b;
	out.a = c.a * multiplier.a;

	return out;
}

ShadeParams DefaultShade()
{
	ShadeParams shade;
	shade.light = RGBA(1.0f, 1.0f, 1.0f);
	shade.fogColor = RGBA(0.0f, 0.0f, 0.0f);
	shade.fog = 0.0f;
	shade.tint = RGBA(1.0f, 1.0f, 1.0f);

	return shade;
}

Renderer::Renderer(int windowSizeX, int windowSizeY)
	: m_Initialized(false)
	, m_WindowSizeX(windowSizeX)
	, m_WindowSizeY(windowSizeY)
	, m_CameraX(0.0f)
	, m_CameraY(0.0f)
	, m_ElapsedSeconds(0.0f)
	, m_AnimKind(0.0f)
	, m_AnimPhase(0.0f)
	, m_AnimStrength(0.0f)
	, m_BatchShader(0)
	, m_TextShader(0)
	, m_BatchUniformHalfViewport(-1)
	, m_BatchUniformTime(-1)
	, m_BatchUniformMeshes(-1)
	, m_BatchUniformInstances(-1)
	, m_TextUniformHalfViewport(-1)
	, m_TextUniformTexture(-1)
	, m_TextUniformMeshes(-1)
	, m_TextUniformQuad(-1)
	, m_BatchVAO(0)
	, m_BatchVBO(0)
	, m_TextVAO(0)
	, m_TextVBO(0)
	, m_MeshBuffer(0)
	, m_MeshTexture(0)
	, m_InstanceBuffer(0)
	, m_InstanceTexture(0)
	, m_MaxInstanceCount(0)
	, m_MeshesReady(false)
	, m_TriangleMesh(-1)
	, m_QuadMesh(-1)
	, m_LastLogTime(0.0f)
	, m_Stats{}
{
	Initialize();
}

Renderer::~Renderer()
{
	for (const GpuQuery& query : m_GpuQueries)
	{
		if (query.id != 0)
		{
			glDeleteQueries(1, &query.id);
		}
	}

	m_Text.Shutdown();
	glDeleteBuffers(1, &m_BatchVBO);
	glDeleteBuffers(1, &m_TextVBO);
	glDeleteBuffers(1, &m_MeshBuffer);
	glDeleteBuffers(1, &m_InstanceBuffer);
	glDeleteTextures(1, &m_MeshTexture);
	glDeleteTextures(1, &m_InstanceTexture);
	glDeleteVertexArrays(1, &m_BatchVAO);
	glDeleteVertexArrays(1, &m_TextVAO);
	glDeleteProgram(m_BatchShader);
	glDeleteProgram(m_TextShader);
}

void Renderer::Initialize()
{
	m_BatchShader = CompileShaders("./Shaders/Batch.vs", "./Shaders/Batch.fs");
	m_TextShader = CompileShaders("./Shaders/Text.vs", "./Shaders/Text.fs");

	if (m_BatchShader == 0 || m_TextShader == 0)
	{
		return;
	}

	m_BatchUniformHalfViewport = glGetUniformLocation(m_BatchShader, "u_HalfViewport");
	m_BatchUniformTime = glGetUniformLocation(m_BatchShader, "u_Time");
	m_BatchUniformMeshes = glGetUniformLocation(m_BatchShader, "u_Meshes");
	m_BatchUniformInstances = glGetUniformLocation(m_BatchShader, "u_Instances");
	m_TextUniformHalfViewport = glGetUniformLocation(m_TextShader, "u_HalfViewport");
	m_TextUniformTexture = glGetUniformLocation(m_TextShader, "u_Texture");
	m_TextUniformMeshes = glGetUniformLocation(m_TextShader, "u_Meshes");
	m_TextUniformQuad = glGetUniformLocation(m_TextShader, "u_Quad");

	GLint maxTexels = 0;
	glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE, &maxTexels);
	m_MaxInstanceCount = maxTexels / INSTANCE_TEXELS;

	if (m_MaxInstanceCount < 1)
	{
		return;
	}

	const bool loaded = m_MeshCache.Load(MESH_CACHE_PATH);
	AnalysisLog::Get().Event("mesh_cache.load", loaded ? "loaded" : "missing_or_invalid;rebuild");

	if (!m_MeshCache.PreparePrimitives())
	{
		std::cerr << "Mesh cache cannot prepare primitive meshes.\n";
		return;
	}

	m_TriangleMesh = m_MeshCache.Find("primitive/triangle");
	m_QuadMesh = m_MeshCache.Find("primitive/quad");

	for (int segments = MIN_ELLIPSE_SEGMENTS; segments <= MAX_ELLIPSE_SEGMENTS; ++segments)
	{
		m_EllipseMeshes[segments] = m_MeshCache.Find("primitive/ellipse/" + std::to_string(segments));
	}

	std::cout << MESH_CACHE_PATH << (loaded ? " loaded\n" : " will be built\n");
	CreateBuffers();

	if (AnalysisLog::Get().IsGpuTimingEnabled())
	{
		for (GpuQuery& query : m_GpuQueries)
		{
			glGenQueries(1, &query.id);
		}
	}

	m_Items.reserve(16384);
	m_Instances.reserve(16384);
	m_Commands.reserve(65536);
	m_SortOrder.reserve(16384);
	m_TextInstances.reserve(256);
	m_Initialized = true;
}

void Renderer::CreateBuffers()
{
	static_assert(sizeof(PolygonInstance) == INSTANCE_TEXELS * 4 * sizeof(float),
		"PolygonInstance must match Batch.vs texel layout");

	glGenVertexArrays(1, &m_BatchVAO);
	glGenBuffers(1, &m_BatchVBO);
	glGenBuffers(1, &m_MeshBuffer);
	glGenTextures(1, &m_MeshTexture);
	glGenBuffers(1, &m_InstanceBuffer);
	glGenTextures(1, &m_InstanceTexture);
	glBindVertexArray(m_BatchVAO);
	glBindBuffer(GL_ARRAY_BUFFER, m_BatchVBO);
	glEnableVertexAttribArray(0);
	glVertexAttribIPointer(0, 2, GL_INT, sizeof(TriangleCommand), NULL);
	glVertexAttribDivisor(0, 1);

	glGenVertexArrays(1, &m_TextVAO);
	glGenBuffers(1, &m_TextVBO);
	glBindVertexArray(m_TextVAO);
	glBindBuffer(GL_ARRAY_BUFFER, m_TextVBO);

	for (GLuint attribute = 0; attribute < TEXT_ATTRIBUTES; ++attribute)
	{
		glEnableVertexAttribArray(attribute);
		glVertexAttribDivisor(attribute, 1);
	}

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

int Renderer::RegisterMesh(const std::string& name, const float* xy, int vertexCount)
{
	if (m_MeshesReady)
	{
		std::cerr << "RegisterMesh must run before PrepareMeshes: " << name << "\n";
		return -1;
	}

	return m_MeshCache.Register(name, xy, vertexCount);
}

void Renderer::PrepareMeshes()
{
	if (m_MeshesReady || !m_Initialized)
	{
		return;
	}

	const std::vector<float>& vertices = m_MeshCache.GetVertices();
	GLint maxTexels = 0;
	glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE, &maxTexels);

	if (vertices.size() / 2 > static_cast<size_t>(maxTexels))
	{
		std::cerr << "Mesh cache exceeds the GPU texture buffer limit.\n";
		m_Initialized = false;
		return;
	}

	glBindBuffer(GL_TEXTURE_BUFFER, m_MeshBuffer);
	glBufferData(GL_TEXTURE_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
	GLint residentBytes = 0;
	glGetBufferParameteriv(GL_TEXTURE_BUFFER, GL_BUFFER_SIZE, &residentBytes);

	if (static_cast<size_t>(residentBytes) != vertices.size() * sizeof(float))
	{
		std::cerr << "GPU mesh allocation failed.\n";
		glBindBuffer(GL_TEXTURE_BUFFER, 0);
		m_Initialized = false;
		return;
	}

	glBindTexture(GL_TEXTURE_BUFFER, m_MeshTexture);
	glTexBuffer(GL_TEXTURE_BUFFER, GL_RG32F, m_MeshBuffer);
	glBindTexture(GL_TEXTURE_BUFFER, 0);
	glBindBuffer(GL_TEXTURE_BUFFER, 0);

	if (m_MeshCache.IsDirty())
	{
		if (m_MeshCache.Save(MESH_CACHE_PATH))
		{
			m_MeshCache.MarkSaved();
			AnalysisLog::Get().Event("mesh_cache.save", "success");
			std::cout << MESH_CACHE_PATH << " written\n";
		}
		else
		{
			std::cerr << "Mesh cache write failed; using in-memory geometry.\n";
			AnalysisLog::Get().Event("mesh_cache.save", "failed;using_memory");
		}
	}

	m_MeshesReady = true;
	AnalysisLog::Get().Event("renderer.mesh_residency", "meshes=" + std::to_string(m_MeshCache.GetCount())
		+ ";bytes=" + std::to_string(vertices.size() * sizeof(float))
		+ ";max_polygon_instances_per_batch=" + std::to_string(m_MaxInstanceCount));
	std::cout << "[Renderer] resident meshes=" << m_MeshCache.GetCount()
		<< ", static mesh bytes=" << vertices.size() * sizeof(float) << "\n";
}

const RenderStats& Renderer::GetStats() const
{
	return m_Stats;
}

TextCacheStats Renderer::GetTextCacheStats() const
{
	return m_Text.GetStats();
}

void Renderer::BeginGpuTiming()
{
	if (!AnalysisLog::Get().IsGpuTimingEnabled())
	{
		return;
	}

	int oldest = -1;

	for (int i = 0; i < GPU_QUERY_SLOTS; ++i)
	{
		if (m_GpuQueries[i].pending && (oldest < 0 || m_GpuQueries[i].frame < m_GpuQueries[oldest].frame))
		{
			oldest = i;
		}
	}

	if (oldest >= 0)
	{
		GLint ready = GL_FALSE;
		glGetQueryObjectiv(m_GpuQueries[oldest].id, GL_QUERY_RESULT_AVAILABLE, &ready);

		if (ready == GL_TRUE)
		{
			GLuint64 nanoseconds = 0;
			glGetQueryObjectui64v(m_GpuQueries[oldest].id, GL_QUERY_RESULT, &nanoseconds);
			m_Stats.gpuSourceFrame = m_GpuQueries[oldest].frame;
			m_Stats.gpuElapsedMs = static_cast<double>(nanoseconds) / 1000000.0;
			m_GpuQueries[oldest].pending = false;
		}
	}

	for (int i = 0; i < GPU_QUERY_SLOTS; ++i)
	{
		GpuQuery& query = m_GpuQueries[i];

		if (!query.pending && query.id != 0)
		{
			query.frame = m_Stats.frameId;
			m_ActiveGpuQuery = i;
			glBeginQuery(GL_TIME_ELAPSED, query.id);
			return;
		}
	}

	// A full ring drops this measurement, never waits for the GPU.
	m_Stats.gpuQuerySkipped = true;
}

bool Renderer::IsInitialized() const
{
	return m_Initialized;
}

void Renderer::Resize(int windowSizeX, int windowSizeY)
{
	if (windowSizeX < 1)
	{
		windowSizeX = 1;
	}

	if (windowSizeY < 1)
	{
		windowSizeY = 1;
	}

	m_WindowSizeX = windowSizeX;
	m_WindowSizeY = windowSizeY;
	glViewport(0, 0, windowSizeX, windowSizeY);
}

void Renderer::SetCamera(float worldX, float worldY)
{
	m_CameraX = worldX;
	m_CameraY = worldY;
}

void Renderer::WorldToScreen(float worldX, float worldY, float worldZ, float* screenX, float* screenY) const
{
	const float dx = worldX - m_CameraX;
	const float dy = worldY - m_CameraY;

	*screenX = (dx - dy) * TileHalfWidth();
	*screenY = (dx + dy) * TileHalfHeight() - worldZ * HeightScale();
}

void Renderer::BeginFrame(const Color& clearColor, float elapsedSeconds)
{
	m_Stats = {};
	m_Stats.frameId = ++m_RenderFrame;
	m_Stats.gpuElapsedMs = -1.0;
	BeginGpuTiming();
	m_ElapsedSeconds = elapsedSeconds;
	ClearAnim();

	glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	m_Instances.clear();
	m_Commands.clear();
	m_Items.clear();
	m_SortOrder.clear();
	m_TextItems.clear();
	m_Text.BeginFrame();
}

void Renderer::EndFrame()
{
	const double worldStart = AnalysisLog::NowMs();
	FlushPolygons();
	m_Stats.worldSubmitCpuMs = AnalysisLog::NowMs() - worldStart;
	const double textStart = AnalysisLog::NowMs();
	FlushText();
	m_Stats.textSubmitCpuMs = AnalysisLog::NowMs() - textStart;

	if (m_ActiveGpuQuery >= 0)
	{
		glEndQuery(GL_TIME_ELAPSED);
		m_GpuQueries[m_ActiveGpuQuery].pending = true;
		m_ActiveGpuQuery = -1;
	}

	if (m_ElapsedSeconds < m_LastLogTime)
	{
		m_LastLogTime = m_ElapsedSeconds;
	}

	if (m_ElapsedSeconds - m_LastLogTime >= STATS_LOG_INTERVAL)
	{
		std::cout << "[Renderer] draws=" << m_Stats.worldDrawCalls + m_Stats.textDrawCalls
			<< " (world=" << m_Stats.worldDrawCalls << ", text=" << m_Stats.textDrawCalls
			<< "), polygons=" << m_Stats.polygonInstances << ", triangleInstances="
			<< m_Stats.triangleInstances << ", texts=" << m_Stats.textInstances
			<< ", instanceBytes=" << m_Stats.instanceUploadBytes << "\n";
		m_LastLogTime = m_ElapsedSeconds;
	}
}

void Renderer::SetAnim(AnimKind kind, float phase, float strength)
{
	m_AnimKind = (float)kind;
	m_AnimPhase = phase;
	m_AnimStrength = strength;
}

void Renderer::ClearAnim()
{
	m_AnimKind = (float)ANIM_NONE;
	m_AnimPhase = 0.0f;
	m_AnimStrength = 0.0f;
}

Renderer::PolygonInstance Renderer::MakeInstance(const Color& color) const
{
	PolygonInstance instance = {};

	for (Color& corner : instance.colors)
	{
		corner = color;
	}

	instance.animation[0] = m_AnimKind;
	instance.animation[1] = m_AnimPhase;
	instance.animation[2] = m_AnimStrength;
	instance.transform[2] = 1.0f;
	instance.transform[3] = 1.0f;

	return instance;
}

void Renderer::PushMesh(int mesh, float x, float y, float scaleX, float scaleY,
	const Color& color, float depth)
{
	if (!m_Initialized || mesh < 0 || mesh >= m_MeshCache.GetCount())
	{
		return;
	}

	BatchItem item;
	item.depth = depth;
	item.mesh = mesh;
	item.instance = MakeInstance(color);
	item.instance.transform[0] = x;
	item.instance.transform[1] = y;
	item.instance.transform[2] = scaleX;
	item.instance.transform[3] = scaleY;
	m_SortOrder.push_back(static_cast<int>(m_Items.size()));
	m_Items.push_back(item);
}

void Renderer::PushPolygonShaded(const float* xy, const Color* colors, int vertexCount, float depth)
{
	if (!m_Initialized || xy == NULL || colors == NULL || vertexCount < 3 || vertexCount > 4)
	{
		return;
	}

	BatchItem item;
	item.depth = depth;
	item.mesh = vertexCount == 3 ? m_TriangleMesh : m_QuadMesh;
	item.instance = MakeInstance(colors[0]);
	item.instance.animation[3] = 1.0f;

	for (int i = 0; i < 4; ++i)
	{
		// A unit triangle uses (0,1) for its third corner.
		const int source = vertexCount == 3 && i == 3 ? 2 : i;
		item.instance.corners[i * 2] = xy[source * 2];
		item.instance.corners[i * 2 + 1] = xy[source * 2 + 1];
		item.instance.colors[i] = colors[source];
	}

	m_SortOrder.push_back(static_cast<int>(m_Items.size()));
	m_Items.push_back(item);
}

void Renderer::PushPolygon(const float* xy, int vertexCount, const Color& color, float depth)
{
	const Color colors[4] = { color, color, color, color };
	PushPolygonShaded(xy, colors, vertexCount, depth);
}

void Renderer::PushRect(float x, float y, float width, float height, const Color& color, float depth)
{
	PushMesh(m_QuadMesh, x, y, width, height, color, depth);
}

void Renderer::PushDiamond(float centerX, float centerY, float halfWidth, float halfHeight, const Color& color, float depth)
{
	const float corners[8] =
	{
		centerX, centerY - halfHeight,
		centerX + halfWidth, centerY,
		centerX, centerY + halfHeight,
		centerX - halfWidth, centerY
	};

	PushPolygon(corners, 4, color, depth);
}

void Renderer::PushEllipse(float centerX, float centerY, float radiusX, float radiusY, const Color& color, float depth, int segments)
{
	segments = (std::max)(MIN_ELLIPSE_SEGMENTS, (std::min)(MAX_ELLIPSE_SEGMENTS, segments));
	PushMesh(m_EllipseMeshes[segments], centerX, centerY, radiusX, radiusY, color, depth);
}

void Renderer::PushLine(float x0, float y0, float x1, float y1, float thickness, const Color& color, float depth)
{
	const float dx = x1 - x0;
	const float dy = y1 - y0;
	const float length = sqrtf(dx * dx + dy * dy);

	if (length < 0.0001f)
	{
		return;
	}

	const float nx = -dy / length * thickness * 0.5f;
	const float ny = dx / length * thickness * 0.5f;
	const float corners[8] =
	{
		x0 + nx, y0 + ny,
		x1 + nx, y1 + ny,
		x1 - nx, y1 - ny,
		x0 - nx, y0 - ny
	};

	PushPolygon(corners, 4, color, depth);
}

void Renderer::PushText(const std::wstring& text, float x, float y, int pixelSize, FontFace face,
	bool bold, const Color& color, TextAlign align)
{
	if (!m_Initialized)
	{
		return;
	}

	const TextTexture* texture = m_Text.Get(text, pixelSize, face, bold);

	if (texture == NULL)
	{
		return;
	}

	TextItem item;
	item.texture = texture;
	item.y = y;
	item.color = color;

	if (align == ALIGN_CENTER)
	{
		item.x = x - texture->width * 0.5f;
	}
	else if (align == ALIGN_RIGHT)
	{
		item.x = x - (float)texture->width;
	}
	else
	{
		item.x = x;
	}

	m_TextItems.push_back(item);
}

float Renderer::MeasureTextWidth(const std::wstring& text, int pixelSize, FontFace face, bool bold)
{
	const TextTexture* texture = m_Text.Get(text, pixelSize, face, bold);
	return texture != NULL ? (float)texture->width : 0.0f;
}

float Renderer::MeasureTextHeight(const std::wstring& text, int pixelSize, FontFace face, bool bold)
{
	const TextTexture* texture = m_Text.Get(text, pixelSize, face, bold);
	return texture != NULL ? (float)texture->height : 0.0f;
}

void Renderer::FlushPolygons()
{
	if (m_Items.empty() || !m_MeshesReady)
	{
		return;
	}

	// Keep painter order, including ties. Commands reference resident triangles
	// in that order; grouping by model would break translucent overlaps.
	const double sortStart = AnalysisLog::NowMs();
	std::stable_sort(m_SortOrder.begin(), m_SortOrder.end(),
		[this](int a, int b)
		{
			return m_Items[a].depth < m_Items[b].depth;
		});
	m_Stats.sortCpuMs = AnalysisLog::NowMs() - sortStart;

	glUseProgram(m_BatchShader);
	glUniform2f(m_BatchUniformHalfViewport, m_WindowSizeX * 0.5f, m_WindowSizeY * 0.5f);
	glUniform1f(m_BatchUniformTime, m_ElapsedSeconds);
	glUniform1i(m_BatchUniformMeshes, 0);
	glUniform1i(m_BatchUniformInstances, 1);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_BUFFER, m_MeshTexture);
	glBindVertexArray(m_BatchVAO);

	for (size_t first = 0; first < m_SortOrder.size(); first += m_MaxInstanceCount)
	{
		m_Instances.clear();
		m_Commands.clear();
		const size_t end = (std::min)(m_SortOrder.size(), first + static_cast<size_t>(m_MaxInstanceCount));

		for (size_t i = first; i < end; ++i)
		{
			const BatchItem& item = m_Items[m_SortOrder[i]];
			const MeshRange& mesh = m_MeshCache.Get(item.mesh);
			const int instance = static_cast<int>(m_Instances.size());
			m_Instances.push_back(item.instance);

			for (int vertex = 0; vertex < mesh.count; vertex += 3)
			{
				const TriangleCommand command = { mesh.first + vertex, instance };
				m_Commands.push_back(command);
			}
		}

		glActiveTexture(GL_TEXTURE1);
		glBindBuffer(GL_TEXTURE_BUFFER, m_InstanceBuffer);
		glBufferData(GL_TEXTURE_BUFFER, m_Instances.size() * sizeof(PolygonInstance),
			m_Instances.data(), GL_STREAM_DRAW);
		glBindTexture(GL_TEXTURE_BUFFER, m_InstanceTexture);
		glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, m_InstanceBuffer);
		glBindBuffer(GL_ARRAY_BUFFER, m_BatchVBO);
		glBufferData(GL_ARRAY_BUFFER, m_Commands.size() * sizeof(TriangleCommand),
			m_Commands.data(), GL_STREAM_DRAW);
		glDrawArraysInstanced(GL_TRIANGLES, 0, 3, static_cast<GLsizei>(m_Commands.size()));
		++m_Stats.worldDrawCalls;
		m_Stats.polygonInstances += m_Instances.size();
		m_Stats.triangleInstances += m_Commands.size();
		m_Stats.instanceUploadBytes += m_Instances.size() * sizeof(PolygonInstance)
			+ m_Commands.size() * sizeof(TriangleCommand);
	}

	glBindVertexArray(0);
	glActiveTexture(GL_TEXTURE0);
}

void Renderer::FlushText()
{
	if (m_TextItems.empty() || !m_MeshesReady)
	{
		return;
	}

	m_TextInstances.clear();

	for (const TextItem& item : m_TextItems)
	{
		const TextTexture& texture = *item.texture;
		TextInstance instance =
		{
			{ item.x, item.y, static_cast<float>(texture.width), static_cast<float>(texture.height) },
			{ texture.u0, texture.v0, texture.u1, texture.v1 },
			item.color, static_cast<float>(texture.layer)
		};
		m_TextInstances.push_back(instance);
	}

	glUseProgram(m_TextShader);
	glUniform2f(m_TextUniformHalfViewport, m_WindowSizeX * 0.5f, m_WindowSizeY * 0.5f);
	glUniform1i(m_TextUniformTexture, 0);
	glUniform1i(m_TextUniformMeshes, 1);
	glUniform1i(m_TextUniformQuad, m_MeshCache.Get(m_QuadMesh).first);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_BUFFER, m_MeshTexture);
	glActiveTexture(GL_TEXTURE0);
	glBindVertexArray(m_TextVAO);
	glBindBuffer(GL_ARRAY_BUFFER, m_TextVBO);
	glBufferData(GL_ARRAY_BUFFER, m_TextInstances.size() * sizeof(TextInstance),
		m_TextInstances.data(), GL_STREAM_DRAW);

	// Normally every string shares one array. If it overflows, consecutive
	// runs preserve text overlay order instead of sorting transparent labels.
	size_t first = 0;

	while (first < m_TextItems.size())
	{
		const GLuint texture = m_TextItems[first].texture->texture;
		size_t end = first + 1;

		while (end < m_TextItems.size() && m_TextItems[end].texture->texture == texture)
		{
			++end;
		}

		const size_t base = first * sizeof(TextInstance);
		const size_t offsets[TEXT_ATTRIBUTES] =
		{
			offsetof(TextInstance, rectangle), offsetof(TextInstance, uv),
			offsetof(TextInstance, color), offsetof(TextInstance, layer)
		};

		for (GLuint attribute = 0; attribute < TEXT_ATTRIBUTES; ++attribute)
		{
			glVertexAttribPointer(attribute, attribute == 3 ? 1 : 4, GL_FLOAT, GL_FALSE,
				sizeof(TextInstance), reinterpret_cast<const void*>(base + offsets[attribute]));
		}

		glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
		glDrawArraysInstanced(GL_TRIANGLES, 0, 6, static_cast<GLsizei>(end - first));
		++m_Stats.textDrawCalls;
		first = end;
	}

	m_Stats.textInstances = m_TextInstances.size();
	m_Stats.instanceUploadBytes += m_TextInstances.size() * sizeof(TextInstance);
	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	glBindVertexArray(0);
}

bool Renderer::ReadFile(const char* filename, std::string* target)
{
	std::ifstream file(filename);

	if (file.fail())
	{
		std::cout << filename << " file loading failed..\n";
		return false;
	}

	std::string line;

	while (getline(file, line))
	{
		target->append(line);
		target->append("\n");
	}

	return true;
}

GLuint Renderer::AddShader(GLuint shaderProgram, const char* shaderText, GLenum shaderType)
{
	GLuint shaderObject = glCreateShader(shaderType);

	if (shaderObject == 0)
	{
		fprintf(stderr, "Error creating shader type %d\n", shaderType);
		return 0;
	}

	const GLchar* sources[1] = { shaderText };
	const GLint lengths[1] = { (GLint)strlen(shaderText) };

	glShaderSource(shaderObject, 1, sources, lengths);
	glCompileShader(shaderObject);

	GLint success = 0;
	glGetShaderiv(shaderObject, GL_COMPILE_STATUS, &success);

	if (!success)
	{
		GLchar infoLog[1024] = { 0 };
		glGetShaderInfoLog(shaderObject, sizeof(infoLog), NULL, infoLog);
		fprintf(stderr, "Error compiling shader type %d: %s\n", shaderType, infoLog);
		AnalysisLog::Get().Event("shader.compile_failed", infoLog);
		glDeleteShader(shaderObject);

		return 0;
	}

	glAttachShader(shaderProgram, shaderObject);
	return shaderObject;
}

GLuint Renderer::CompileShaders(const char* filenameVS, const char* filenameFS)
{
	GLuint shaderProgram = glCreateProgram();

	if (shaderProgram == 0)
	{
		fprintf(stderr, "Error creating shader program\n");
		return 0;
	}

	std::string vs;
	std::string fs;

	if (!ReadFile(filenameVS, &vs) || !ReadFile(filenameFS, &fs))
	{
		AnalysisLog::Get().Event("shader.read_failed", std::string(filenameVS) + ";" + filenameFS);
		glDeleteProgram(shaderProgram);
		return 0;
	}

	// 0 means failure. The base project returned -1 from a GLuint function,
	// which wrapped to 0xFFFFFFFF and slipped past its own "> 0" success check.
	GLuint vertexShader = AddShader(shaderProgram, vs.c_str(), GL_VERTEX_SHADER);
	GLuint fragmentShader = AddShader(shaderProgram, fs.c_str(), GL_FRAGMENT_SHADER);

	if (vertexShader == 0 || fragmentShader == 0)
	{
		AnalysisLog::Get().Event("shader.program_failed", std::string(filenameVS) + ";" + filenameFS);
		glDeleteProgram(shaderProgram);
		return 0;
	}

	GLint success = 0;
	GLchar errorLog[1024] = { 0 };

	glLinkProgram(shaderProgram);
	glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);

	if (success == 0)
	{
		glGetProgramInfoLog(shaderProgram, sizeof(errorLog), NULL, errorLog);
		std::cout << filenameVS << ", " << filenameFS << " Error linking shader program\n" << errorLog << "\n";
		AnalysisLog::Get().Event("shader.link_failed", std::string(filenameVS) + ";" + errorLog);
		glDeleteProgram(shaderProgram);

		return 0;
	}

	// Validation depends on bound textures; link status is sufficient during initialization.
	glDetachShader(shaderProgram, vertexShader);
	glDetachShader(shaderProgram, fragmentShader);
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	std::cout << filenameVS << ", " << filenameFS << " compiled.\n";
	AnalysisLog::Get().Event("shader.ready", std::string(filenameVS) + ";" + filenameFS);

	return shaderProgram;
}
