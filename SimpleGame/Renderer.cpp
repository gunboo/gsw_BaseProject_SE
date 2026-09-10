#include "stdafx.h"
#include "Renderer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>

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
	if (t < 0.0f) t = 0.0f;
	if (t > 1.0f) t = 1.0f;

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

Renderer::Renderer(int windowSizeX, int windowSizeY)
	: m_Initialized(false)
	, m_WindowSizeX(windowSizeX)
	, m_WindowSizeY(windowSizeY)
	, m_CameraX(0.0f)
	, m_CameraY(0.0f)
	, m_BatchShader(0)
	, m_TextShader(0)
	, m_BatchAttribPosition(-1)
	, m_BatchAttribColor(-1)
	, m_BatchUniformHalfViewport(-1)
	, m_TextAttribPosition(-1)
	, m_TextAttribTexCoord(-1)
	, m_TextUniformHalfViewport(-1)
	, m_TextUniformTexture(-1)
	, m_TextUniformColor(-1)
	, m_BatchVAO(0)
	, m_BatchVBO(0)
	, m_TextVAO(0)
	, m_TextVBO(0)
{
	Initialize();
}

Renderer::~Renderer()
{
	m_Text.Shutdown();

	if (m_BatchVBO != 0) glDeleteBuffers(1, &m_BatchVBO);
	if (m_TextVBO != 0) glDeleteBuffers(1, &m_TextVBO);
	if (m_BatchVAO != 0) glDeleteVertexArrays(1, &m_BatchVAO);
	if (m_TextVAO != 0) glDeleteVertexArrays(1, &m_TextVAO);
	if (m_BatchShader != 0) glDeleteProgram(m_BatchShader);
	if (m_TextShader != 0) glDeleteProgram(m_TextShader);
}

void Renderer::Initialize()
{
	m_BatchShader = CompileShaders("./Shaders/Batch.vs", "./Shaders/Batch.fs");
	m_TextShader = CompileShaders("./Shaders/Text.vs", "./Shaders/Text.fs");

	if (m_BatchShader == 0 || m_TextShader == 0)
	{
		return;
	}

	// Locations are resolved once here. Looking them up per draw call, as the
	// base project did, means a string query into the driver every sprite.
	m_BatchAttribPosition = glGetAttribLocation(m_BatchShader, "a_Position");
	m_BatchAttribColor = glGetAttribLocation(m_BatchShader, "a_Color");
	m_BatchUniformHalfViewport = glGetUniformLocation(m_BatchShader, "u_HalfViewport");

	m_TextAttribPosition = glGetAttribLocation(m_TextShader, "a_Position");
	m_TextAttribTexCoord = glGetAttribLocation(m_TextShader, "a_TexCoord");
	m_TextUniformHalfViewport = glGetUniformLocation(m_TextShader, "u_HalfViewport");
	m_TextUniformTexture = glGetUniformLocation(m_TextShader, "u_Texture");
	m_TextUniformColor = glGetUniformLocation(m_TextShader, "u_Color");

	CreateBuffers();

	m_PolygonVertices.reserve(65536);
	m_TriangleVertices.reserve(98304);
	m_Items.reserve(16384);
	m_SortOrder.reserve(16384);

	m_Initialized = true;
}

void Renderer::CreateBuffers()
{
	glGenVertexArrays(1, &m_BatchVAO);
	glGenBuffers(1, &m_BatchVBO);

	glBindVertexArray(m_BatchVAO);
	glBindBuffer(GL_ARRAY_BUFFER, m_BatchVBO);
	if (m_BatchAttribPosition >= 0)
	{
		glEnableVertexAttribArray(m_BatchAttribPosition);
		glVertexAttribPointer(m_BatchAttribPosition, 2, GL_FLOAT, GL_FALSE, sizeof(BatchVertex), (const void*)0);
	}
	if (m_BatchAttribColor >= 0)
	{
		glEnableVertexAttribArray(m_BatchAttribColor);
		glVertexAttribPointer(m_BatchAttribColor, 4, GL_FLOAT, GL_FALSE, sizeof(BatchVertex), (const void*)(sizeof(float) * 2));
	}

	glGenVertexArrays(1, &m_TextVAO);
	glGenBuffers(1, &m_TextVBO);

	glBindVertexArray(m_TextVAO);
	glBindBuffer(GL_ARRAY_BUFFER, m_TextVBO);
	if (m_TextAttribPosition >= 0)
	{
		glEnableVertexAttribArray(m_TextAttribPosition);
		glVertexAttribPointer(m_TextAttribPosition, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (const void*)0);
	}
	if (m_TextAttribTexCoord >= 0)
	{
		glEnableVertexAttribArray(m_TextAttribTexCoord);
		glVertexAttribPointer(m_TextAttribTexCoord, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (const void*)(sizeof(float) * 2));
	}

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

bool Renderer::IsInitialized() const
{
	return m_Initialized;
}

void Renderer::Resize(int windowSizeX, int windowSizeY)
{
	if (windowSizeX < 1) windowSizeX = 1;
	if (windowSizeY < 1) windowSizeY = 1;

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

void Renderer::BeginFrame(const Color& clearColor)
{
	glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	m_PolygonVertices.clear();
	m_TriangleVertices.clear();
	m_Items.clear();
	m_SortOrder.clear();
	m_TextItems.clear();
}

void Renderer::EndFrame()
{
	FlushPolygons();
	FlushText();
}

void Renderer::PushPolygonShaded(const float* xy, const Color* colors, int vertexCount, float depth)
{
	if (!m_Initialized || vertexCount < 3)
	{
		return;
	}

	BatchItem item;
	item.depth = depth;
	item.first = (int)m_PolygonVertices.size();
	item.count = vertexCount;

	for (int i = 0; i < vertexCount; ++i)
	{
		BatchVertex v;
		v.x = xy[i * 2 + 0];
		v.y = xy[i * 2 + 1];
		v.r = colors[i].r;
		v.g = colors[i].g;
		v.b = colors[i].b;
		v.a = colors[i].a;
		m_PolygonVertices.push_back(v);
	}

	m_SortOrder.push_back((int)m_Items.size());
	m_Items.push_back(item);
}

void Renderer::PushPolygon(const float* xy, int vertexCount, const Color& color, float depth)
{
	if (!m_Initialized || vertexCount < 3)
	{
		return;
	}

	BatchItem item;
	item.depth = depth;
	item.first = (int)m_PolygonVertices.size();
	item.count = vertexCount;

	for (int i = 0; i < vertexCount; ++i)
	{
		BatchVertex v;
		v.x = xy[i * 2 + 0];
		v.y = xy[i * 2 + 1];
		v.r = color.r;
		v.g = color.g;
		v.b = color.b;
		v.a = color.a;
		m_PolygonVertices.push_back(v);
	}

	m_SortOrder.push_back((int)m_Items.size());
	m_Items.push_back(item);
}

void Renderer::PushRect(float x, float y, float width, float height, const Color& color, float depth)
{
	const float xy[8] =
	{
		x, y,
		x + width, y,
		x + width, y + height,
		x, y + height
	};
	PushPolygon(xy, 4, color, depth);
}

void Renderer::PushDiamond(float centerX, float centerY, float halfWidth, float halfHeight, const Color& color, float depth)
{
	const float xy[8] =
	{
		centerX, centerY - halfHeight,
		centerX + halfWidth, centerY,
		centerX, centerY + halfHeight,
		centerX - halfWidth, centerY
	};
	PushPolygon(xy, 4, color, depth);
}

void Renderer::PushEllipse(float centerX, float centerY, float radiusX, float radiusY, const Color& color, float depth, int segments)
{
	if (segments < 3) segments = 3;
	if (segments > 48) segments = 48;

	float xy[96];
	for (int i = 0; i < segments; ++i)
	{
		const float angle = 6.2831853f * (float)i / (float)segments;
		xy[i * 2 + 0] = centerX + cosf(angle) * radiusX;
		xy[i * 2 + 1] = centerY + sinf(angle) * radiusY;
	}
	PushPolygon(xy, segments, color, depth);
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

	const float xy[8] =
	{
		x0 + nx, y0 + ny,
		x1 + nx, y1 + ny,
		x1 - nx, y1 - ny,
		x0 - nx, y0 - ny
	};
	PushPolygon(xy, 4, color, depth);
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
	if (m_Items.empty())
	{
		return;
	}

	// Painter ordering: one buffer, sorted back to front. stable_sort keeps the
	// push order among items that share a depth, which is what lets a sprite
	// shadow stay underneath the body it belongs to.
	std::stable_sort(m_SortOrder.begin(), m_SortOrder.end(),
		[this](int a, int b) { return m_Items[a].depth < m_Items[b].depth; });

	for (size_t i = 0; i < m_SortOrder.size(); ++i)
	{
		const BatchItem& item = m_Items[m_SortOrder[i]];
		const BatchVertex* base = &m_PolygonVertices[item.first];

		for (int t = 1; t + 1 < item.count; ++t)
		{
			m_TriangleVertices.push_back(base[0]);
			m_TriangleVertices.push_back(base[t]);
			m_TriangleVertices.push_back(base[t + 1]);
		}
	}

	if (m_TriangleVertices.empty())
	{
		return;
	}

	glUseProgram(m_BatchShader);
	glUniform2f(m_BatchUniformHalfViewport, m_WindowSizeX * 0.5f, m_WindowSizeY * 0.5f);

	glBindVertexArray(m_BatchVAO);
	glBindBuffer(GL_ARRAY_BUFFER, m_BatchVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(BatchVertex) * m_TriangleVertices.size(),
		&m_TriangleVertices[0], GL_DYNAMIC_DRAW);

	glDrawArrays(GL_TRIANGLES, 0, (GLsizei)m_TriangleVertices.size());

	glBindVertexArray(0);
}

void Renderer::FlushText()
{
	if (m_TextItems.empty())
	{
		return;
	}

	glUseProgram(m_TextShader);
	glUniform2f(m_TextUniformHalfViewport, m_WindowSizeX * 0.5f, m_WindowSizeY * 0.5f);
	glUniform1i(m_TextUniformTexture, 0);
	glActiveTexture(GL_TEXTURE0);

	glBindVertexArray(m_TextVAO);
	glBindBuffer(GL_ARRAY_BUFFER, m_TextVBO);

	for (size_t i = 0; i < m_TextItems.size(); ++i)
	{
		const TextItem& item = m_TextItems[i];

		const float x0 = item.x;
		const float y0 = item.y;
		const float x1 = item.x + item.texture->width;
		const float y1 = item.y + item.texture->height;

		const float vertices[24] =
		{
			x0, y0, 0.0f, 0.0f,
			x1, y0, 1.0f, 0.0f,
			x1, y1, 1.0f, 1.0f,

			x0, y0, 0.0f, 0.0f,
			x1, y1, 1.0f, 1.0f,
			x0, y1, 0.0f, 1.0f
		};

		glUniform4f(m_TextUniformColor, item.color.r, item.color.g, item.color.b, item.color.a);
		glBindTexture(GL_TEXTURE_2D, item.texture->texture);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
		glDrawArrays(GL_TRIANGLES, 0, 6);
	}

	glBindTexture(GL_TEXTURE_2D, 0);
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
		glDeleteProgram(shaderProgram);
		return 0;
	}

	// 0 means failure. The base project returned -1 from a GLuint function,
	// which wrapped to 0xFFFFFFFF and slipped past its own "> 0" success check.
	GLuint vertexShader = AddShader(shaderProgram, vs.c_str(), GL_VERTEX_SHADER);
	GLuint fragmentShader = AddShader(shaderProgram, fs.c_str(), GL_FRAGMENT_SHADER);

	if (vertexShader == 0 || fragmentShader == 0)
	{
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
		glDeleteProgram(shaderProgram);
		return 0;
	}

	glValidateProgram(shaderProgram);
	glGetProgramiv(shaderProgram, GL_VALIDATE_STATUS, &success);
	if (!success)
	{
		glGetProgramInfoLog(shaderProgram, sizeof(errorLog), NULL, errorLog);
		std::cout << filenameVS << ", " << filenameFS << " Error validating shader program\n" << errorLog << "\n";
		glDeleteProgram(shaderProgram);
		return 0;
	}

	glDetachShader(shaderProgram, vertexShader);
	glDetachShader(shaderProgram, fragmentShader);
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	std::cout << filenameVS << ", " << filenameFS << " compiled.\n";
	return shaderProgram;
}
