#include "stdafx.h"
#include "Model.h"

#include <cmath>
#include <fstream>
#include <iostream>

namespace
{
	const float TWO_PI = 6.2831853f;

	const int MAX_POLY_VERTICES = 32;
	const int SOFT_ELLIPSE_SEGMENTS = 20;
	const int MAX_CACHED_MODELS = 4096;
	const int MAX_MODEL_NAME_LENGTH = 256;
	const int MAX_MODEL_VERTEX_FLOATS = 1000000;
	const int MAX_MODEL_POLYGONS = 100000;

	// Bump this whenever a builder below changes shape, so a stale cache from an
	// earlier run is discarded instead of being drawn.
	const int MODEL_CACHE_VERSION = 5;
	const char MODEL_CACHE_MAGIC[4] = { 'M', 'D', 'L', '1' };

	// --- shared palette for the baked shapes -----------------------------
	const Color COL_FOLIAGE = { 0.43f, 0.55f, 0.29f, 1.0f };
	const Color COL_TRUNK = { 0.40f, 0.32f, 0.24f, 1.0f };
	const Color COL_SKIN = { 0.77f, 0.66f, 0.50f, 1.0f };
	const Color COL_HAIR = { 0.11f, 0.10f, 0.11f, 1.0f };
	const Color COL_SASH = { 0.73f, 0.69f, 0.54f, 1.0f };
	const Color COL_STRAW = { 0.66f, 0.58f, 0.38f, 1.0f };
	const Color COL_STEEL = { 0.36f, 0.42f, 0.43f, 1.0f };
	const Color COL_SHADOW = { 0.0f, 0.0f, 0.0f, 0.30f };
	const Color COL_EMBER = { 1.0f, 0.72f, 0.36f, 1.0f };

	// Adult height in pixels. One world unit of height is 26 px, and a person is
	// a shade under one and a half of those.
	const float PERSON_HEIGHT = 36.0f;
	const float SURFACE_HIGHLIGHT = 1.18f;
	const float SURFACE_SHADE_RANGE = 0.52f;
	const float GROUND_SHADOW_DEPTH = -90000.0f;

	// --- builders --------------------------------------------------------

	void BuildCrownVolume(ModelBuilder* builder, float cx, float cy, float rx, float ry,
		float thickness, const Color& color, float depth)
	{
		const float ring[12] =
		{
			cx - rx, cy, cx - rx * 0.52f, cy - ry,
			cx + rx * 0.45f, cy - ry * 0.90f, cx + rx, cy,
			cx + rx * 0.42f, cy + ry * 0.65f, cx - rx * 0.48f, cy + ry * 0.75f
		};
		const float faceShade[6] = { 1.04f, 1.18f, 1.03f, 0.77f, 0.86f, 0.98f };
		const float centerX = cx - rx * 0.16f;
		const float centerY = cy - ry * 0.28f;

		// The visible lower rim supplies thickness; triangular caps supply flat facets.
		for (int edge = 3; edge < 6; ++edge)
		{
			const int next = (edge + 1) % 6;
			const float side[8] =
			{
				ring[edge * 2], ring[edge * 2 + 1], ring[next * 2], ring[next * 2 + 1],
				ring[next * 2] * 0.96f + cx * 0.04f, ring[next * 2 + 1] + thickness,
				ring[edge * 2] * 0.96f + cx * 0.04f, ring[edge * 2 + 1] + thickness
			};
			builder->AddPolygon(side, 4, Scale(color, 0.52f + (edge - 3) * 0.09f), depth);
		}

		for (int face = 0; face < 6; ++face)
		{
			const int next = (face + 1) % 6;
			builder->AddTriangle(centerX, centerY, ring[face * 2], ring[face * 2 + 1],
				ring[next * 2], ring[next * 2 + 1], Scale(color, faceShade[face]), depth + 0.001f);
		}
	}

	void BuildTreeTrunk(ModelBuilder* builder, float height, float width)
	{
		const float left[8] = { -width, 0.0f, 0.0f, 4.0f, 0.0f, -height * 0.70f, -width * 0.52f, -height * 0.74f };
		const float right[8] = { 0.0f, 4.0f, width, 0.0f, width * 0.48f, -height * 0.71f, 0.0f, -height * 0.70f };
		builder->AddPolygon(left, 4, Scale(COL_TRUNK, 1.20f), -0.01f);
		builder->AddPolygon(right, 4, Scale(COL_TRUNK, 0.61f), -0.009f);
		builder->AddLine(-1.0f, -height * 0.42f, -22.0f, -height * 0.70f,
			width * 0.60f, Scale(COL_TRUNK, 0.95f), -0.007f);
		builder->AddLine(2.0f, -height * 0.52f, 23.0f, -height * 0.80f,
			width * 0.55f, Scale(COL_TRUNK, 0.68f), -0.006f);
		builder->AddTriangle(-width, -8.0f, -width * 1.9f, 5.0f, 0.0f, 2.0f,
			Scale(COL_TRUNK, 1.05f), -0.005f);
		builder->AddTriangle(0.0f, 2.0f, width * 1.6f, 5.0f, width, -8.0f,
			Scale(COL_TRUNK, 0.65f), -0.004f);
	}

	void BuildCedar(ModelBuilder* builder, float height, int tier)
	{
		builder->AddSoftEllipse(18.0f, 8.0f, 48.0f, 19.0f, COL_SHADOW, -0.02f,
			MODEL_FLAG_UNLIT | MODEL_FLAG_GROUND);
		BuildTreeTrunk(builder, height, 6.5f + tier * 0.6f);
		builder->SetAnim(ANIM_SWAY, tier * 1.7f, 0.45f);
		BuildCrownVolume(builder, 17.0f, -height * 0.80f, 28.0f, 20.0f, 10.0f,
			Scale(COL_FOLIAGE, 0.98f), 0.002f);
		builder->SetAnim(ANIM_SWAY, tier * 1.7f, 0.60f);
		BuildCrownVolume(builder, -17.0f, -height * 0.66f, 31.0f, 22.0f, 12.0f,
			Scale(COL_FOLIAGE, 1.12f), 0.004f);
		builder->ClearAnim();
	}

	void BuildBroadleaf(ModelBuilder* builder, float height)
	{
		builder->AddSoftEllipse(20.0f, 8.0f, 62.0f, 22.0f, COL_SHADOW, -0.02f,
			MODEL_FLAG_UNLIT | MODEL_FLAG_GROUND);
		BuildTreeTrunk(builder, height, 10.0f);
		builder->SetAnim(ANIM_SWAY, 0.7f, 0.45f);
		BuildCrownVolume(builder, 0.0f, -height * 0.91f, 31.0f, 22.0f, 12.0f,
			Scale(COL_FOLIAGE, 1.03f), 0.002f);
		BuildCrownVolume(builder, 25.0f, -height * 0.70f, 34.0f, 24.0f, 13.0f,
			Scale(COL_FOLIAGE, 0.91f), 0.004f);
		builder->SetAnim(ANIM_SWAY, 0.7f, 0.65f);
		BuildCrownVolume(builder, -25.0f, -height * 0.63f, 35.0f, 24.0f, 13.0f,
			Scale(COL_FOLIAGE, 1.13f), 0.006f);
		builder->ClearAnim();
	}

	void BuildBush(ModelBuilder* builder)
	{
		builder->AddSoftEllipse(3.0f, 2.0f, 21.0f, 8.0f, COL_SHADOW, -0.02f,
			MODEL_FLAG_UNLIT | MODEL_FLAG_GROUND);

		Color leaf = COL_FOLIAGE;
		leaf.r += 0.05f;
		leaf.g += 0.06f;

		builder->SetAnim(ANIM_SWAY, 0.4f, 0.65f);
		BuildCrownVolume(builder, 6.0f, -9.0f, 12.0f, 8.0f, 5.0f, Scale(leaf, 0.88f), 0.0f);
		BuildCrownVolume(builder, -6.0f, -12.0f, 13.0f, 9.0f, 5.0f, leaf, 0.002f);
		builder->ClearAnim();
	}

	void BuildRock(ModelBuilder* builder)
	{
		builder->AddSoftEllipse(5.0f, 2.0f, 21.0f, 8.0f, COL_SHADOW, -0.02f,
			MODEL_FLAG_UNLIT | MODEL_FLAG_GROUND);

		const float shape[10] =
		{
			-13.0f, 2.0f,
			-8.0f, -11.0f,
			3.0f, -14.0f,
			13.0f, -4.0f,
			9.0f, 4.0f
		};

		builder->AddPolygon(shape, 5, RGBA(0.47f, 0.50f, 0.41f), 0.0f, MODEL_FLAG_SHADED);
		const float top[8] = { -8.0f, -11.0f, 3.0f, -14.0f, 10.0f, -7.0f, -4.0f, -4.0f };
		builder->AddPolygon(top, 4, RGBA(0.64f, 0.65f, 0.52f), 0.001f, MODEL_FLAG_SHADED);
		builder->AddTriangle(-4.0f, -4.0f, 10.0f, -7.0f, 9.0f, 4.0f,
			RGBA(0.32f, 0.38f, 0.32f), 0.002f, MODEL_FLAG_SHADED);
		builder->AddEllipse(-6.0f, -7.0f, 5.0f, 2.0f, 7,
			RGBA(0.38f, 0.46f, 0.25f), 0.003f, MODEL_FLAG_SHADED);
	}

	// The robe is tintable so that one baked person serves every villager.
	void BuildPerson(ModelBuilder* builder, float height)
	{
		const float shoulderY = -height * 0.62f;
		const float hipY = -height * 0.34f;
		const float headY = -height * 0.80f;
		const float halfHip = height * 0.20f;
		const float halfShoulder = height * 0.15f;

		builder->AddSoftEllipse(3.0f, 1.0f, 14.0f, 6.0f, COL_SHADOW, -0.02f,
			MODEL_FLAG_UNLIT | MODEL_FLAG_GROUND);
		builder->AddRect(-halfHip * 0.80f, -height * 0.10f, height * 0.10f, height * 0.12f,
			COL_HAIR, -0.005f, MODEL_FLAG_SHADED);
		builder->AddRect(halfHip * 0.28f, -height * 0.10f, height * 0.11f, height * 0.12f,
			COL_HAIR, -0.005f, MODEL_FLAG_SHADED);

		const float body[8] =
		{
			-halfHip, -height * 0.08f,
			halfHip, -height * 0.08f,
			halfShoulder, shoulderY,
			-halfShoulder, shoulderY
		};

		builder->AddPolygon(body, 4, RGBA(1.0f, 1.0f, 1.0f), 0.0f, MODEL_FLAG_TINT | MODEL_FLAG_SHADED);
		builder->AddEllipse(-halfShoulder, shoulderY + 5.0f, height * 0.10f, height * 0.15f,
			8, RGBA(0.86f, 0.86f, 0.86f), 0.0005f, MODEL_FLAG_TINT | MODEL_FLAG_SHADED);
		const float lapel[6] = { -halfShoulder, shoulderY, 2.0f, hipY, -3.0f, hipY };
		builder->AddPolygon(lapel, 3, RGBA(0.65f, 0.65f, 0.65f), 0.0007f, MODEL_FLAG_TINT);
		builder->AddRect(-halfHip * 0.92f, hipY, halfHip * 1.84f, height * 0.07f, COL_SASH, 0.001f);
		builder->AddEllipse(0.0f, headY, height * 0.15f, height * 0.16f, 10, COL_SKIN, 0.002f, MODEL_FLAG_SHADED);
		builder->AddEllipse(-0.7f, headY - height * 0.06f, height * 0.16f, height * 0.11f,
			9, COL_HAIR, 0.003f, MODEL_FLAG_SHADED);
		builder->AddEllipse(-1.0f, headY - height * 0.18f, height * 0.065f, height * 0.075f,
			8, COL_HAIR, 0.0035f, MODEL_FLAG_SHADED);
	}

	void BuildPlayerCloak(ModelBuilder* builder, float height)
	{
		const float cloak[10] =
		{
			-height * 0.32f, -height * 0.11f, -height * 0.07f, -height * 0.08f,
			height * 0.16f, -height * 0.46f, height * 0.11f, -height * 0.66f,
			-height * 0.14f, -height * 0.64f
		};
		builder->SetAnim(ANIM_SWAY, 0.3f, 0.7f);
		builder->AddPolygon(cloak, 5, RGBA(1.0f, 1.0f, 1.0f), -0.002f,
			MODEL_FLAG_TINT | MODEL_FLAG_SHADED);
		builder->ClearAnim();
	}

	void BuildKasa(ModelBuilder* builder, float height)
	{
		const float headY = -height * 0.80f;
		const float brimY = headY - height * 0.02f;
		const float apexY = headY - height * 0.30f;
		const float halfBrim = height * 0.30f;

		builder->AddTriangle(-halfBrim, brimY, halfBrim, brimY, 0.0f, apexY,
			COL_STRAW, 0.004f, MODEL_FLAG_SHADED);
		builder->AddLine(-halfBrim, brimY, halfBrim, brimY, 1.5f, Scale(COL_STRAW, 0.75f), 0.0045f);
	}

	void BuildSword(ModelBuilder* builder, float height)
	{
		const float hipY = -height * 0.34f;
		const float halfHip = height * 0.20f;

		builder->AddLine(halfHip * 0.3f, hipY + 2.0f, halfHip * 2.1f, hipY + 9.0f, 4.5f, COL_STEEL, 0.004f);
		builder->AddLine(halfHip * 0.3f, hipY + 0.8f, halfHip * 2.1f, hipY + 7.8f, 1.0f,
			RGBA(0.80f, 0.85f, 0.77f), 0.0045f);

		// The muted red cord ties the blade to the player's cloak palette.
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

		builder->AddSoftEllipse(lanternX, lanternY, 36.0f, 29.0f,
			RGBA(1.0f, 0.70f, 0.35f, 0.22f), -0.01f);

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
		const Color pale = RGBA(0.65f, 0.86f, 0.77f, 0.75f);
		const Color paleDim = RGBA(0.35f, 0.59f, 0.54f, 0.46f);
		builder->AddSoftEllipse(0.0f, -height * 0.60f, 18.0f, 24.0f,
			RGBA(0.32f, 0.72f, 0.63f, 0.16f), -0.01f);

		builder->SetAnim(ANIM_SWAY, 0.0f, 1.8f);

		const float tail[8] =
		{
			-5.0f, 0.0f,
			5.0f, 0.0f,
			10.0f, -height * 0.45f,
			-10.0f, -height * 0.45f
		};

		builder->AddPolygon(tail, 4, paleDim, 0.0f, MODEL_FLAG_SHADED);
		builder->ClearAnim();

		const float body[8] =
		{
			-10.0f, -height * 0.42f,
			10.0f, -height * 0.42f,
			8.0f, -height * 0.72f,
			-8.0f, -height * 0.72f
		};

		builder->AddPolygon(body, 4, pale, 0.001f, MODEL_FLAG_SHADED);
		builder->AddEllipse(0.0f, -height * 0.82f, 7.5f, 8.0f, 10, pale, 0.002f, MODEL_FLAG_SHADED);
		builder->AddEllipse(-2.6f, -height * 0.84f, 1.5f, 2.0f, 6, RGBA(0.05f, 0.07f, 0.10f, 0.85f), 0.003f);
		builder->AddEllipse(2.6f, -height * 0.84f, 1.5f, 2.0f, 6, RGBA(0.05f, 0.07f, 0.10f, 0.85f), 0.003f);
	}

	void BuildOni(ModelBuilder* builder)
	{
		const float height = 44.0f;
		const Color skin = RGBA(0.58f, 0.34f, 0.25f);
		const Color cloth = RGBA(0.26f, 0.30f, 0.20f);

		builder->AddSoftEllipse(4.0f, 2.0f, 20.0f, 8.0f, COL_SHADOW, -0.02f,
			MODEL_FLAG_UNLIT | MODEL_FLAG_GROUND);

		const float body[8] =
		{
			-11.0f, 0.0f,
			11.0f, 0.0f,
			10.0f, -height * 0.60f,
			-10.0f, -height * 0.60f
		};

		builder->AddEllipse(-11.0f, -height * 0.43f, 6.0f, 10.0f, 9,
			Scale(skin, 0.88f), -0.001f, MODEL_FLAG_SHADED);
		builder->AddEllipse(11.0f, -height * 0.43f, 6.0f, 10.0f, 9,
			Scale(skin, 0.75f), -0.001f, MODEL_FLAG_SHADED);
		builder->AddPolygon(body, 4, skin, 0.0f, MODEL_FLAG_SHADED);
		builder->AddRect(-11.0f, -height * 0.30f, 22.0f, height * 0.22f, cloth, 0.001f, MODEL_FLAG_SHADED);
		builder->AddEllipse(0.0f, -height * 0.74f, 9.0f, 9.5f, 10, skin, 0.002f, MODEL_FLAG_SHADED);
		builder->AddLine(-4.0f, -height * 0.58f, 4.0f, -height * 0.58f, 2.0f,
			Scale(skin, 0.60f), 0.0025f);

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

		builder->AddSoftEllipse(0.0f, centerY, 28.0f, 31.0f,
			RGBA(0.39f, 0.95f, 0.80f, 0.26f), -0.01f);

		builder->AddEllipse(0.0f, centerY, 7.0f, 9.0f, 10,
			RGBA(0.44f, 0.91f, 0.77f), 0.002f, MODEL_FLAG_UNLIT | MODEL_FLAG_SHADED);
		builder->AddEllipse(-1.0f, centerY - 2.0f, 3.0f, 4.0f, 8,
			RGBA(0.85f, 1.0f, 0.87f), 0.003f, MODEL_FLAG_UNLIT);
		builder->ClearAnim();
	}

	// --- pickups ---------------------------------------------------------

	void BuildExperienceOrb(ModelBuilder* builder)
	{
		const float centerY = -8.0f;

		builder->SetAnim(ANIM_FLAME, 0.0f, 1.1f);

		builder->AddSoftEllipse(0.0f, centerY, 16.0f, 16.0f,
			RGBA(0.39f, 0.81f, 0.72f, 0.30f), -0.01f);

		const float crystal[8] = { 0.0f, centerY - 5.0f, 4.0f, centerY, 0.0f, centerY + 5.0f, -4.0f, centerY };
		builder->AddPolygon(crystal, 4, RGBA(0.70f, 0.95f, 0.79f), 0.002f,
			MODEL_FLAG_UNLIT | MODEL_FLAG_SHADED);
		builder->ClearAnim();
	}

	void BuildPotion(ModelBuilder* builder)
	{
		builder->AddSoftEllipse(2.0f, 1.0f, 10.0f, 4.0f, COL_SHADOW, -0.02f,
			MODEL_FLAG_UNLIT | MODEL_FLAG_GROUND);
		builder->AddEllipse(0.0f, -7.0f, 6.0f, 7.0f, 10, RGBA(0.68f, 0.29f, 0.20f), 0.0f, MODEL_FLAG_SHADED);
		builder->AddLine(-2.5f, -10.0f, -2.5f, -6.0f, 1.5f, RGBA(0.90f, 0.76f, 0.49f), 0.0005f);
		builder->AddRect(-2.0f, -16.0f, 4.0f, 6.0f, RGBA(0.42f, 0.38f, 0.32f), 0.001f);
		builder->AddRect(-3.0f, -19.0f, 6.0f, 3.0f, RGBA(0.30f, 0.26f, 0.21f), 0.002f);
	}

	void BuildWhetstone(ModelBuilder* builder)
	{
		builder->AddSoftEllipse(2.0f, 1.0f, 11.0f, 4.5f, COL_SHADOW, -0.02f,
			MODEL_FLAG_UNLIT | MODEL_FLAG_GROUND);

		const float block[8] =
		{
			-8.0f, -2.0f,
			8.0f, -2.0f,
			6.0f, -11.0f,
			-6.0f, -11.0f
		};

		builder->AddPolygon(block, 4, RGBA(0.52f, 0.56f, 0.48f), 0.0f, MODEL_FLAG_SHADED);
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
	const float cornerShade[4] = { 0.0f, 0.35f, 1.0f, 0.65f };

	for (const ModelPoly& poly : model.polys)
	{
		Color color = poly.color;
		const float alpha = poly.color.a * shade.tint.a;

		if ((poly.flags & MODEL_FLAG_TINT) != 0)
		{
			color = Modulate(color, shade.tint);
		}

		if ((poly.flags & MODEL_FLAG_UNLIT) == 0)
		{
			color = Modulate(color, shade.light);
		}

		Color colors[4];

		for (int corner = 0; corner < 4; ++corner)
		{
			const float factor = (poly.flags & MODEL_FLAG_SHADED) != 0
				? SURFACE_HIGHLIGHT - SURFACE_SHADE_RANGE * cornerShade[corner] : 1.0f;
			colors[corner] = Mix(Scale(color, factor), shade.fogColor, shade.fog);
			colors[corner].a = alpha;
		}

		const float polyDepth = (poly.flags & MODEL_FLAG_GROUND) != 0
			? GROUND_SHADOW_DEPTH + depth * 0.01f : depth + poly.depthBias;
		renderer->SetAnim((AnimKind)poly.anim, poly.animPhase, poly.animStrength * scale);
		renderer->PushMeshShaded(poly.mesh, screenX, screenY, scale, scale,
			colors, poly.shadeBounds, (poly.flags & MODEL_FLAG_RADIAL) != 0, polyDepth);
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

void ModelBuilder::AddSoftEllipse(float centerX, float centerY, float radiusX, float radiusY,
	const Color& color, float depthBias, unsigned int flags)
{
	if (radiusX <= 0.0f || radiusY <= 0.0f)
	{
		return;
	}

	float xy[(SOFT_ELLIPSE_SEGMENTS + 2) * 2];
	xy[0] = centerX;
	xy[1] = centerY;

	for (int i = 0; i <= SOFT_ELLIPSE_SEGMENTS; ++i)
	{
		const float angle = TWO_PI * (float)i / (float)SOFT_ELLIPSE_SEGMENTS;
		xy[(i + 1) * 2] = centerX + cosf(angle) * radiusX;
		xy[(i + 1) * 2 + 1] = centerY + sinf(angle) * radiusY;
	}

	AddPolygon(xy, SOFT_ELLIPSE_SEGMENTS + 2, color, depthBias, flags | MODEL_FLAG_RADIAL);
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

bool ModelLibrary::RegisterMeshes(Renderer* renderer)
{
	for (auto& entry : m_Models)
	{
		Model& model = entry.second;

		for (size_t i = 0; i < model.polys.size(); ++i)
		{
			ModelPoly& poly = model.polys[i];
			const float* xy = &model.vertices[poly.firstVertex * 2];
			float minX = xy[0], maxX = xy[0], minY = xy[1], maxY = xy[1];
			for (int vertex = 1; vertex < poly.vertexCount; ++vertex)
			{
				minX = fminf(minX, xy[vertex * 2]);
				maxX = fmaxf(maxX, xy[vertex * 2]);
				minY = fminf(minY, xy[vertex * 2 + 1]);
				maxY = fmaxf(maxY, xy[vertex * 2 + 1]);
			}
			poly.shadeBounds[0] = minX;
			poly.shadeBounds[1] = minY;
			poly.shadeBounds[2] = fmaxf(maxX - minX, 1.0f);
			poly.shadeBounds[3] = fmaxf(maxY - minY, 1.0f);
			poly.mesh = renderer->RegisterMesh("model/" + entry.first + "/" + std::to_string(i),
				&model.vertices[poly.firstVertex * 2], poly.vertexCount);

			if (poly.mesh < 0)
			{
				return false;
			}
		}
	}

	renderer->PrepareMeshes();

	return renderer->IsInitialized();
}

void ModelLibrary::BuildAll()
{
	m_Models.clear();

	ModelBuilder cedarSmall;
	BuildCedar(&cedarSmall, 98.0f, 0);
	m_Models["tree_cedar_small"] = cedarSmall.Get();

	ModelBuilder cedarMid;
	BuildCedar(&cedarMid, 116.0f, 1);
	m_Models["tree_cedar_mid"] = cedarMid.Get();

	ModelBuilder cedarTall;
	BuildCedar(&cedarTall, 134.0f, 2);
	m_Models["tree_cedar_tall"] = cedarTall.Get();

	ModelBuilder broadleaf;
	BuildBroadleaf(&broadleaf, 102.0f);
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

	ModelBuilder cloak;
	BuildPlayerCloak(&cloak, PERSON_HEIGHT);
	m_Models["player_cloak"] = cloak.Get();

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
				|| (poly.flags & ~(MODEL_FLAG_UNLIT | MODEL_FLAG_TINT | MODEL_FLAG_SHADED
					| MODEL_FLAG_RADIAL | MODEL_FLAG_GROUND)) != 0
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
