#include "stdafx.h"
#include "Model.h"

#include <cmath>
#include <fstream>
#include <iostream>

namespace
{
	const float TWO_PI = 6.2831853f;

	const int MAX_POLY_VERTICES = 32;
	const int MAX_CACHED_MODELS = 4096;
	const int MAX_MODEL_NAME_LENGTH = 256;
	const int MAX_MODEL_VERTEX_FLOATS = 1000000;
	const int MAX_MODEL_POLYGONS = 100000;

	// Bump this whenever a builder below changes shape, so a stale cache from an
	// earlier run is discarded instead of being drawn.
	const int MODEL_CACHE_VERSION = 3;
	const char MODEL_CACHE_MAGIC[4] = { 'M', 'D', 'L', '1' };

	// --- shared palette for the baked shapes -----------------------------
	const Color COL_FOLIAGE = { 0.19f, 0.26f, 0.21f, 1.0f };
	const Color COL_TRUNK = { 0.23f, 0.19f, 0.16f, 1.0f };
	const Color COL_SKIN = { 0.60f, 0.50f, 0.42f, 1.0f };
	const Color COL_HAIR = { 0.11f, 0.10f, 0.11f, 1.0f };
	const Color COL_SASH = { 0.62f, 0.58f, 0.50f, 1.0f };
	const Color COL_STRAW = { 0.50f, 0.44f, 0.32f, 1.0f };
	const Color COL_STEEL = { 0.10f, 0.11f, 0.13f, 1.0f };
	const Color COL_SHADOW = { 0.0f, 0.0f, 0.0f, 0.30f };
	const Color COL_EMBER = { 1.0f, 0.72f, 0.36f, 1.0f };

	// Adult height in pixels. One world unit of height is 26 px, and a person is
	// a shade under one and a half of those.
	const float PERSON_HEIGHT = 36.0f;

	// --- builders --------------------------------------------------------

	void BuildCedar(ModelBuilder* builder, float height, int tier)
	{
		builder->AddEllipse(0.0f, 0.0f, 15.0f, 6.5f, 12, COL_SHADOW, -0.02f, MODEL_FLAG_UNLIT);
		builder->AddRect(-3.5f, -height * 0.34f, 7.0f, height * 0.34f, COL_TRUNK, -0.01f);

		for (int k = 0; k < 3; ++k)
		{
			const float baseFraction = 0.28f + 0.20f * k;
			const float apexFraction = 0.62f + 0.20f * k;
			const float halfWidth = 27.0f - k * 6.0f - tier * 1.5f;

			const float yBase = -height * baseFraction;
			const float yApex = -height * apexFraction;

			Color leaf = COL_FOLIAGE;
			leaf.r += 0.022f * k;
			leaf.g += 0.030f * k;
			leaf.b += 0.018f * k;

			// Higher skirts sway further, which is what sells wind in a still frame.
			builder->SetAnim(ANIM_SWAY, (float)tier * 1.7f + (float)k * 0.6f, 0.9f + 0.8f * k);
			builder->AddTriangle(-halfWidth, yBase, halfWidth, yBase, 0.0f, yApex, leaf, (float)k * 0.001f);
		}

		builder->ClearAnim();
	}

	void BuildBroadleaf(ModelBuilder* builder, float height)
	{
		builder->AddEllipse(0.0f, 0.0f, 15.0f, 6.5f, 12, COL_SHADOW, -0.02f, MODEL_FLAG_UNLIT);
		builder->AddRect(-3.5f, -height * 0.34f, 7.0f, height * 0.34f, COL_TRUNK, -0.01f);

		for (int i = 0; i < 3; ++i)
		{
			const float offsetX = (i - 1) * 11.0f;
			const float offsetY = -height * (0.58f + (i == 1 ? 0.16f : 0.0f));

			Color leaf = COL_FOLIAGE;
			leaf.r += 0.03f * i;
			leaf.g += 0.04f * i;

			builder->SetAnim(ANIM_SWAY, (float)i * 2.1f, 1.6f);
			builder->AddEllipse(offsetX, offsetY, 17.0f - i * 1.5f, 13.0f, 14, leaf, 0.0f);
		}

		builder->ClearAnim();
	}

	void BuildBush(ModelBuilder* builder)
	{
		builder->AddEllipse(0.0f, 0.0f, 11.0f, 4.5f, 10, COL_SHADOW, -0.02f, MODEL_FLAG_UNLIT);

		Color leaf = COL_FOLIAGE;
		leaf.r += 0.05f;
		leaf.g += 0.06f;

		builder->SetAnim(ANIM_SWAY, 0.4f, 1.1f);
		builder->AddEllipse(-5.0f, -7.0f, 10.0f, 8.0f, 10, leaf, 0.0f);
		builder->AddEllipse(5.0f, -6.0f, 9.0f, 7.0f, 10, leaf, 0.001f);
		builder->ClearAnim();
	}

	void BuildRock(ModelBuilder* builder)
	{
		builder->AddEllipse(0.0f, 0.0f, 12.0f, 5.0f, 10, COL_SHADOW, -0.02f, MODEL_FLAG_UNLIT);

		const float shape[10] =
		{
			-13.0f, 2.0f,
			-8.0f, -11.0f,
			3.0f, -14.0f,
			13.0f, -4.0f,
			9.0f, 4.0f
		};

		builder->AddPolygon(shape, 5, RGBA(0.35f, 0.36f, 0.35f), 0.0f);
		builder->AddEllipse(-2.0f, -9.0f, 5.0f, 3.0f, 8, RGBA(0.45f, 0.46f, 0.44f), 0.001f);
	}

	// The robe is tintable so that one baked person serves every villager.
	void BuildPerson(ModelBuilder* builder, float height)
	{
		const float shoulderY = -height * 0.62f;
		const float hipY = -height * 0.34f;
		const float headY = -height * 0.80f;
		const float halfHip = height * 0.20f;
		const float halfShoulder = height * 0.15f;

		builder->AddEllipse(0.0f, 0.0f, 10.0f, 4.5f, 12, COL_SHADOW, -0.02f, MODEL_FLAG_UNLIT);

		const float body[8] =
		{
			-halfHip, 0.0f,
			halfHip, 0.0f,
			halfShoulder, shoulderY,
			-halfShoulder, shoulderY
		};

		builder->AddPolygon(body, 4, RGBA(1.0f, 1.0f, 1.0f), 0.0f, MODEL_FLAG_TINT);
		builder->AddRect(-halfHip * 0.92f, hipY, halfHip * 1.84f, height * 0.07f, COL_SASH, 0.001f);
		builder->AddEllipse(0.0f, headY, height * 0.115f, height * 0.125f, 12, COL_SKIN, 0.002f);
		builder->AddEllipse(0.0f, headY - height * 0.045f, height * 0.125f, height * 0.085f, 12, COL_HAIR, 0.003f);
	}

	void BuildKasa(ModelBuilder* builder, float height)
	{
		const float headY = -height * 0.80f;
		const float brimY = headY - height * 0.02f;
		const float apexY = headY - height * 0.30f;
		const float halfBrim = height * 0.30f;

		builder->AddTriangle(-halfBrim, brimY, halfBrim, brimY, 0.0f, apexY, COL_STRAW, 0.004f);
	}

	void BuildSword(ModelBuilder* builder, float height)
	{
		const float hipY = -height * 0.34f;
		const float halfHip = height * 0.20f;

		builder->AddLine(halfHip * 0.3f, hipY + 2.0f, halfHip * 2.1f, hipY + 9.0f, 4.5f, COL_STEEL, 0.004f);

		// The one spot of vermilion the player carries: the sword cord.
		builder->AddLine(halfHip * 0.1f, hipY + 1.0f, -halfHip * 0.5f, hipY - 2.0f, 3.0f,
			RGBA(0.66f, 0.24f, 0.16f), 0.005f);
	}

	void BuildCarriedLantern(ModelBuilder* builder, float height)
	{
		const float hipY = -height * 0.34f;
		const float shoulderY = -height * 0.62f;
		const float halfHip = height * 0.20f;

		const float lanternX = -halfHip * 1.9f;
		const float lanternY = hipY - 3.0f;

		builder->SetAnim(ANIM_FLAME, 0.0f, 1.2f);

		for (int i = 3; i >= 1; --i)
		{
			const Color halo = RGBA(1.0f, 0.66f, 0.30f, 0.12f / i);
			builder->AddEllipse(lanternX, lanternY, 13.0f * i, 11.0f * i, 14, halo, -0.01f, MODEL_FLAG_UNLIT);
		}

		builder->ClearAnim();
		builder->AddLine(-halfHip * 0.8f, shoulderY + 3.0f, lanternX, lanternY - 7.0f, 1.5f,
			RGBA(0.25f, 0.22f, 0.18f), 0.005f);

		builder->SetAnim(ANIM_FLAME, 1.1f, 1.6f);
		builder->AddEllipse(lanternX, lanternY, 5.0f, 6.5f, 12, RGBA(1.0f, 0.80f, 0.46f), 0.006f, MODEL_FLAG_UNLIT);
		builder->ClearAnim();
	}

	// --- enemies ---------------------------------------------------------

	void BuildShade(ModelBuilder* builder)
	{
		// A vengeful spirit: no shadow, because it does not touch the ground.
		const float height = 34.0f;
		const Color pale = RGBA(0.60f, 0.71f, 0.78f, 0.60f);
		const Color paleDim = RGBA(0.44f, 0.55f, 0.64f, 0.46f);

		builder->SetAnim(ANIM_SWAY, 0.0f, 1.8f);

		const float tail[8] =
		{
			-5.0f, 0.0f,
			5.0f, 0.0f,
			10.0f, -height * 0.45f,
			-10.0f, -height * 0.45f
		};

		builder->AddPolygon(tail, 4, paleDim, 0.0f);
		builder->ClearAnim();

		const float body[8] =
		{
			-10.0f, -height * 0.42f,
			10.0f, -height * 0.42f,
			8.0f, -height * 0.72f,
			-8.0f, -height * 0.72f
		};

		builder->AddPolygon(body, 4, pale, 0.001f);
		builder->AddEllipse(0.0f, -height * 0.82f, 6.5f, 7.0f, 12, pale, 0.002f);
		builder->AddEllipse(-2.6f, -height * 0.84f, 1.5f, 2.0f, 6, RGBA(0.05f, 0.07f, 0.10f, 0.85f), 0.003f);
		builder->AddEllipse(2.6f, -height * 0.84f, 1.5f, 2.0f, 6, RGBA(0.05f, 0.07f, 0.10f, 0.85f), 0.003f);
	}

	void BuildOni(ModelBuilder* builder)
	{
		const float height = 44.0f;
		const Color skin = RGBA(0.46f, 0.24f, 0.20f);
		const Color cloth = RGBA(0.28f, 0.26f, 0.20f);

		builder->AddEllipse(0.0f, 0.0f, 13.0f, 5.5f, 12, COL_SHADOW, -0.02f, MODEL_FLAG_UNLIT);

		const float body[8] =
		{
			-11.0f, 0.0f,
			11.0f, 0.0f,
			10.0f, -height * 0.60f,
			-10.0f, -height * 0.60f
		};

		builder->AddPolygon(body, 4, skin, 0.0f);
		builder->AddRect(-11.0f, -height * 0.30f, 22.0f, height * 0.10f, cloth, 0.001f);
		builder->AddEllipse(0.0f, -height * 0.74f, 9.0f, 9.5f, 12, skin, 0.002f);

		// Horns.
		builder->AddTriangle(-7.0f, -height * 0.82f, -3.0f, -height * 0.82f, -7.5f, -height * 1.02f,
			RGBA(0.80f, 0.76f, 0.66f), 0.003f);
		builder->AddTriangle(3.0f, -height * 0.82f, 7.0f, -height * 0.82f, 7.5f, -height * 1.02f,
			RGBA(0.80f, 0.76f, 0.66f), 0.003f);

		builder->AddEllipse(-3.2f, -height * 0.76f, 1.8f, 2.2f, 6, RGBA(0.95f, 0.82f, 0.30f), 0.004f);
		builder->AddEllipse(3.2f, -height * 0.76f, 1.8f, 2.2f, 6, RGBA(0.95f, 0.82f, 0.30f), 0.004f);
	}

	void BuildWisp(ModelBuilder* builder)
	{
		// Will-o-the-wisp: entirely a light source, so every polygon is unlit and
		// every polygon burns on the GPU.
		const float centerY = -22.0f;

		builder->SetAnim(ANIM_FLAME, 0.0f, 2.4f);

		for (int i = 3; i >= 1; --i)
		{
			const Color halo = RGBA(0.55f, 0.95f, 0.80f, 0.14f / i);
			builder->AddEllipse(0.0f, centerY, 9.0f * i, 10.0f * i, 14, halo, -0.01f, MODEL_FLAG_UNLIT);
		}

		builder->AddEllipse(0.0f, centerY, 6.0f, 7.5f, 12, RGBA(0.78f, 1.0f, 0.90f), 0.002f, MODEL_FLAG_UNLIT);
		builder->ClearAnim();
	}

	// --- pickups ---------------------------------------------------------

	void BuildExperienceOrb(ModelBuilder* builder)
	{
		const float centerY = -8.0f;

		builder->SetAnim(ANIM_FLAME, 0.0f, 1.1f);

		for (int i = 2; i >= 1; --i)
		{
			const Color halo = RGBA(0.62f, 0.82f, 1.0f, 0.18f / i);
			builder->AddEllipse(0.0f, centerY, 7.0f * i, 7.0f * i, 12, halo, -0.01f, MODEL_FLAG_UNLIT);
		}

		builder->AddEllipse(0.0f, centerY, 4.0f, 4.0f, 10, RGBA(0.80f, 0.92f, 1.0f), 0.002f, MODEL_FLAG_UNLIT);
		builder->ClearAnim();
	}

	void BuildPotion(ModelBuilder* builder)
	{
		builder->AddEllipse(0.0f, 0.0f, 7.0f, 3.0f, 10, COL_SHADOW, -0.02f, MODEL_FLAG_UNLIT);
		builder->AddEllipse(0.0f, -7.0f, 6.0f, 7.0f, 12, RGBA(0.55f, 0.18f, 0.16f), 0.0f);
		builder->AddRect(-2.0f, -16.0f, 4.0f, 6.0f, RGBA(0.42f, 0.38f, 0.32f), 0.001f);
		builder->AddRect(-3.0f, -19.0f, 6.0f, 3.0f, RGBA(0.30f, 0.26f, 0.21f), 0.002f);
	}

	void BuildWhetstone(ModelBuilder* builder)
	{
		builder->AddEllipse(0.0f, 0.0f, 8.0f, 3.5f, 10, COL_SHADOW, -0.02f, MODEL_FLAG_UNLIT);

		const float block[8] =
		{
			-8.0f, -2.0f,
			8.0f, -2.0f,
			6.0f, -11.0f,
			-6.0f, -11.0f
		};

		builder->AddPolygon(block, 4, RGBA(0.44f, 0.45f, 0.43f), 0.0f);
		builder->AddRect(-6.0f, -12.5f, 12.0f, 2.0f, RGBA(0.58f, 0.58f, 0.55f), 0.001f);
	}

	void BuildEmber(ModelBuilder* builder)
	{
		builder->SetAnim(ANIM_FLAME, 0.0f, 1.0f);
		builder->AddEllipse(0.0f, 0.0f, 2.0f, 2.0f, 6, COL_EMBER, 0.0f, MODEL_FLAG_UNLIT);
		builder->ClearAnim();
	}

	// --- cache io --------------------------------------------------------

	void WriteInt(std::ofstream& stream, int value)
	{
		stream.write((const char*)&value, sizeof(int));
	}

	void WriteFloat(std::ofstream& stream, float value)
	{
		stream.write((const char*)&value, sizeof(float));
	}

	int ReadInt(std::ifstream& stream)
	{
		int value = 0;
		stream.read((char*)&value, sizeof(int));

		return value;
	}

	float ReadFloat(std::ifstream& stream)
	{
		float value = 0.0f;
		stream.read((char*)&value, sizeof(float));

		return value;
	}
}

// ---------------------------------------------------------------- drawing

void DrawModel(Renderer* renderer, const Model& model, float screenX, float screenY,
	float scale, float depth, const ShadeParams& shade)
{
	float points[MAX_POLY_VERTICES * 2];

	for (size_t i = 0; i < model.polys.size(); ++i)
	{
		const ModelPoly& poly = model.polys[i];

		if (poly.vertexCount < 3 || poly.vertexCount > MAX_POLY_VERTICES)
		{
			continue;
		}

		for (int v = 0; v < poly.vertexCount; ++v)
		{
			const int index = (poly.firstVertex + v) * 2;
			points[v * 2 + 0] = screenX + model.vertices[index + 0] * scale;
			points[v * 2 + 1] = screenY + model.vertices[index + 1] * scale;
		}

		Color color = poly.color;
		float alpha = poly.color.a;

		if ((poly.flags & MODEL_FLAG_TINT) != 0)
		{
			color = Modulate(color, shade.tint);
			alpha *= shade.tint.a;
		}

		if ((poly.flags & MODEL_FLAG_UNLIT) == 0)
		{
			color = Modulate(color, shade.light);
		}

		color = Mix(color, shade.fogColor, shade.fog);
		color.a = alpha;

		renderer->SetAnim((AnimKind)poly.anim, poly.animPhase, poly.animStrength * scale);
		renderer->PushPolygon(points, poly.vertexCount, color, depth + poly.depthBias);
	}

	renderer->ClearAnim();
}

// ---------------------------------------------------------------- builder

ModelBuilder::ModelBuilder()
	: m_Anim(ANIM_NONE)
	, m_AnimPhase(0.0f)
	, m_AnimStrength(0.0f)
{
}

void ModelBuilder::SetAnim(AnimKind kind, float phase, float strength)
{
	m_Anim = (int)kind;
	m_AnimPhase = phase;
	m_AnimStrength = strength;
}

void ModelBuilder::ClearAnim()
{
	m_Anim = (int)ANIM_NONE;
	m_AnimPhase = 0.0f;
	m_AnimStrength = 0.0f;
}

void ModelBuilder::AddPolygon(const float* xy, int vertexCount, const Color& color, float depthBias,
	unsigned int flags)
{
	if (vertexCount < 3 || vertexCount > MAX_POLY_VERTICES)
	{
		return;
	}

	ModelPoly poly;
	poly.firstVertex = (int)(m_Model.vertices.size() / 2);
	poly.vertexCount = vertexCount;
	poly.color = color;
	poly.depthBias = depthBias;
	poly.flags = flags;
	poly.anim = m_Anim;
	poly.animPhase = m_AnimPhase;
	poly.animStrength = m_AnimStrength;

	for (int i = 0; i < vertexCount * 2; ++i)
	{
		m_Model.vertices.push_back(xy[i]);
	}

	m_Model.polys.push_back(poly);
}

void ModelBuilder::AddTriangle(float x0, float y0, float x1, float y1, float x2, float y2,
	const Color& color, float depthBias, unsigned int flags)
{
	const float xy[6] = { x0, y0, x1, y1, x2, y2 };

	AddPolygon(xy, 3, color, depthBias, flags);
}

void ModelBuilder::AddRect(float x, float y, float width, float height, const Color& color,
	float depthBias, unsigned int flags)
{
	const float xy[8] =
	{
		x, y,
		x + width, y,
		x + width, y + height,
		x, y + height
	};

	AddPolygon(xy, 4, color, depthBias, flags);
}

void ModelBuilder::AddEllipse(float centerX, float centerY, float radiusX, float radiusY,
	int segments, const Color& color, float depthBias, unsigned int flags)
{
	if (segments < 3)
	{
		segments = 3;
	}

	if (segments > MAX_POLY_VERTICES)
	{
		segments = MAX_POLY_VERTICES;
	}

	float xy[MAX_POLY_VERTICES * 2];

	for (int i = 0; i < segments; ++i)
	{
		const float angle = TWO_PI * (float)i / (float)segments;
		xy[i * 2 + 0] = centerX + cosf(angle) * radiusX;
		xy[i * 2 + 1] = centerY + sinf(angle) * radiusY;
	}

	AddPolygon(xy, segments, color, depthBias, flags);
}

void ModelBuilder::AddLine(float x0, float y0, float x1, float y1, float thickness,
	const Color& color, float depthBias, unsigned int flags)
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

	AddPolygon(xy, 4, color, depthBias, flags);
}

// ---------------------------------------------------------------- library

ModelLibrary::ModelLibrary()
	: m_LoadedFromCache(false)
{
}

bool ModelLibrary::LoadOrBuild(const char* cachePath)
{
	m_LoadedFromCache = false;

	if (LoadCache(cachePath))
	{
		m_LoadedFromCache = true;
		std::cout << cachePath << " loaded: " << m_Models.size() << " models (cached)\n";

		return true;
	}

	BuildAll();

	if (m_Models.empty())
	{
		std::cout << "model library is empty after building\n";

		return false;
	}

	if (SaveCache(cachePath))
	{
		std::cout << cachePath << " written: " << m_Models.size() << " models\n";
	}
	else
	{
		std::cout << cachePath << " could not be written; models will rebuild next run\n";
	}

	return true;
}

const Model* ModelLibrary::Find(const std::string& name) const
{
	std::map<std::string, Model>::const_iterator it = m_Models.find(name);

	if (it == m_Models.end())
	{
		return NULL;
	}

	return &it->second;
}

void ModelLibrary::BuildAll()
{
	m_Models.clear();

	ModelBuilder cedarSmall;
	BuildCedar(&cedarSmall, 78.0f, 0);
	m_Models["tree_cedar_small"] = cedarSmall.Get();

	ModelBuilder cedarMid;
	BuildCedar(&cedarMid, 92.0f, 1);
	m_Models["tree_cedar_mid"] = cedarMid.Get();

	ModelBuilder cedarTall;
	BuildCedar(&cedarTall, 106.0f, 2);
	m_Models["tree_cedar_tall"] = cedarTall.Get();

	ModelBuilder broadleaf;
	BuildBroadleaf(&broadleaf, 62.0f);
	m_Models["tree_broadleaf"] = broadleaf.Get();

	ModelBuilder bush;
	BuildBush(&bush);
	m_Models["bush"] = bush.Get();

	ModelBuilder rock;
	BuildRock(&rock);
	m_Models["rock"] = rock.Get();

	ModelBuilder person;
	BuildPerson(&person, PERSON_HEIGHT);
	m_Models["person"] = person.Get();

	ModelBuilder kasa;
	BuildKasa(&kasa, PERSON_HEIGHT);
	m_Models["person_kasa"] = kasa.Get();

	ModelBuilder sword;
	BuildSword(&sword, PERSON_HEIGHT);
	m_Models["person_sword"] = sword.Get();

	ModelBuilder lantern;
	BuildCarriedLantern(&lantern, PERSON_HEIGHT);
	m_Models["person_lantern"] = lantern.Get();

	ModelBuilder shade;
	BuildShade(&shade);
	m_Models["enemy_shade"] = shade.Get();

	ModelBuilder oni;
	BuildOni(&oni);
	m_Models["enemy_oni"] = oni.Get();

	ModelBuilder wisp;
	BuildWisp(&wisp);
	m_Models["enemy_wisp"] = wisp.Get();

	ModelBuilder orb;
	BuildExperienceOrb(&orb);
	m_Models["item_orb"] = orb.Get();

	ModelBuilder potion;
	BuildPotion(&potion);
	m_Models["item_potion"] = potion.Get();

	ModelBuilder whetstone;
	BuildWhetstone(&whetstone);
	m_Models["item_whetstone"] = whetstone.Get();

	ModelBuilder ember;
	BuildEmber(&ember);
	m_Models["ember"] = ember.Get();
}

bool ModelLibrary::SaveCache(const char* path) const
{
	std::ofstream stream(path, std::ios::binary | std::ios::trunc);

	if (stream.fail())
	{
		return false;
	}

	stream.write(MODEL_CACHE_MAGIC, sizeof(MODEL_CACHE_MAGIC));
	WriteInt(stream, MODEL_CACHE_VERSION);
	WriteInt(stream, (int)m_Models.size());

	std::map<std::string, Model>::const_iterator it;

	for (it = m_Models.begin(); it != m_Models.end(); ++it)
	{
		const std::string& name = it->first;
		const Model& model = it->second;

		WriteInt(stream, (int)name.size());
		stream.write(name.c_str(), (std::streamsize)name.size());

		WriteInt(stream, (int)model.vertices.size());

		for (size_t i = 0; i < model.vertices.size(); ++i)
		{
			WriteFloat(stream, model.vertices[i]);
		}

		WriteInt(stream, (int)model.polys.size());

		for (size_t i = 0; i < model.polys.size(); ++i)
		{
			const ModelPoly& poly = model.polys[i];

			WriteInt(stream, poly.firstVertex);
			WriteInt(stream, poly.vertexCount);
			WriteFloat(stream, poly.color.r);
			WriteFloat(stream, poly.color.g);
			WriteFloat(stream, poly.color.b);
			WriteFloat(stream, poly.color.a);
			WriteFloat(stream, poly.depthBias);
			WriteInt(stream, (int)poly.flags);
			WriteInt(stream, poly.anim);
			WriteFloat(stream, poly.animPhase);
			WriteFloat(stream, poly.animStrength);
		}
	}

	stream.close();

	return !stream.fail();
}

bool ModelLibrary::LoadCache(const char* path)
{
	std::ifstream stream(path, std::ios::binary);

	if (stream.fail())
	{
		return false;
	}

	char magic[sizeof(MODEL_CACHE_MAGIC)] = { 0 };
	stream.read(magic, sizeof(magic));

	for (size_t i = 0; i < sizeof(magic); ++i)
	{
		if (magic[i] != MODEL_CACHE_MAGIC[i])
		{
			return false;
		}
	}

	if (ReadInt(stream) != MODEL_CACHE_VERSION)
	{
		return false;
	}

	const int modelCount = ReadInt(stream);

	if (modelCount <= 0 || modelCount > MAX_CACHED_MODELS || stream.fail())
	{
		return false;
	}

	std::map<std::string, Model> loaded;

	for (int m = 0; m < modelCount; ++m)
	{
		const int nameLength = ReadInt(stream);

		if (nameLength <= 0 || nameLength > MAX_MODEL_NAME_LENGTH || stream.fail())
		{
			return false;
		}

		std::string name((size_t)nameLength, '\0');
		stream.read(&name[0], nameLength);

		if (loaded.find(name) != loaded.end())
		{
			return false;
		}

		Model model;

		const int vertexFloats = ReadInt(stream);

		if (vertexFloats <= 0 || vertexFloats > MAX_MODEL_VERTEX_FLOATS
			|| vertexFloats % 2 != 0 || stream.fail())
		{
			return false;
		}

		model.vertices.resize((size_t)vertexFloats);

		for (int i = 0; i < vertexFloats; ++i)
		{
			model.vertices[i] = ReadFloat(stream);

			if (!std::isfinite(model.vertices[i]))
			{
				return false;
			}
		}

		const int polyCount = ReadInt(stream);

		if (polyCount <= 0 || polyCount > MAX_MODEL_POLYGONS || stream.fail())
		{
			return false;
		}

		for (int i = 0; i < polyCount; ++i)
		{
			ModelPoly poly;
			poly.firstVertex = ReadInt(stream);
			poly.vertexCount = ReadInt(stream);
			poly.color.r = ReadFloat(stream);
			poly.color.g = ReadFloat(stream);
			poly.color.b = ReadFloat(stream);
			poly.color.a = ReadFloat(stream);
			poly.depthBias = ReadFloat(stream);
			poly.flags = (unsigned int)ReadInt(stream);
			poly.anim = ReadInt(stream);
			poly.animPhase = ReadFloat(stream);
			poly.animStrength = ReadFloat(stream);

			// Reject invalid ranges before DrawModel indexes the cached vertices.
			if (stream.fail() || poly.firstVertex < 0 || poly.vertexCount < 3
				|| poly.vertexCount > MAX_POLY_VERTICES
				|| poly.firstVertex > vertexFloats / 2 - poly.vertexCount
				|| poly.anim < ANIM_NONE || poly.anim > ANIM_SWAY
				|| !std::isfinite(poly.depthBias) || !std::isfinite(poly.animPhase)
				|| !std::isfinite(poly.animStrength) || !std::isfinite(poly.color.r)
				|| !std::isfinite(poly.color.g) || !std::isfinite(poly.color.b)
				|| !std::isfinite(poly.color.a))
			{
				return false;
			}

			model.polys.push_back(poly);
		}

		if (stream.fail())
		{
			return false;
		}

		loaded[name] = model;
	}

	m_Models.swap(loaded);

	return true;
}
