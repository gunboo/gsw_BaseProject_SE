#include "stdafx.h"
#include "Model.h"
#include "Bow.h"

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
	const int MODEL_CACHE_VERSION = 13;
	const char MODEL_CACHE_MAGIC[4] = { 'M', 'D', 'L', '1' };

	// --- shared palette for the baked shapes -----------------------------
	const Color COL_FOLIAGE = { 0.43f, 0.55f, 0.29f, 1.0f };
	const Color COL_TREE_FOLIAGE = { 0.27f, 0.46f, 0.31f, 1.0f };
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
	const int CANOPY_SEGMENTS = 16;
	const int CANOPY_EDGE_LOBES = 7;
	const int TRUNK_SEGMENTS = 4;
	const Color COL_FOX_ROBE = { 0.19f, 0.23f, 0.23f, 1.0f };
	const Color COL_FOX_CAPE = { 0.63f, 0.25f, 0.19f, 1.0f };
	const Color COL_FOX_MASK = { 0.94f, 0.91f, 0.79f, 1.0f };
	const Color COL_FOX_MARK = { 0.65f, 0.17f, 0.13f, 1.0f };
	const Color COL_FOX_LEATHER = { 0.31f, 0.26f, 0.22f, 1.0f };
	const Color COL_FOX_GOLD = { 0.72f, 0.58f, 0.34f, 1.0f };
	const unsigned int FOX_SURFACE = MODEL_FLAG_TINT | MODEL_FLAG_SHADED;

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

	void BuildCanopyMass(ModelBuilder* builder, float cx, float cy, float rx, float ry,
		const Color& material, float phase, float depth)
	{
		float outline[CANOPY_SEGMENTS * 2];

		for (int i = 0; i < CANOPY_SEGMENTS; ++i)
		{
			const float angle = TWO_PI * i / CANOPY_SEGMENTS;
			const float radius = 0.96f + 0.03f * cosf(angle * 3.0f + phase)
				+ 0.015f * sinf(angle * 5.0f - phase);
			outline[i * 2] = cx + cosf(angle) * rx * radius;
			outline[i * 2 + 1] = cy + sinf(angle) * ry * radius;
		}

		// A broad shaded mass carries the volume; a few edge lobes carry the leaf silhouette.
		builder->AddEllipse(cx + 3.0f, cy + ry * 0.22f, rx * 0.95f, ry * 0.94f,
			CANOPY_SEGMENTS, Scale(material, 0.50f), depth, MODEL_FLAG_SHADED);

		for (int i = 0; i < CANOPY_EDGE_LOBES; ++i)
		{
			const float angle = TWO_PI * i / CANOPY_EDGE_LOBES + phase * 0.15f;
			const float leafX = cx + cosf(angle) * rx * 0.91f;
			const float leafY = cy + sinf(angle) * ry * 0.89f;
			const float shade = 0.95f - sinf(angle) * 0.10f - cosf(angle) * 0.06f;
			builder->AddEllipse(leafX, leafY, rx * 0.16f, ry * 0.20f, 8,
				Scale(material, shade), depth + 0.001f, MODEL_FLAG_SHADED);
		}

		// Cover the interior of each lobe so small leaves do not read as bumps on the canopy.
		builder->AddPolygon(outline, CANOPY_SEGMENTS, material, depth + 0.002f, MODEL_FLAG_SHADED);

		Color highlight = Scale(material, 1.8f);
		highlight.a = 0.19f;
		builder->AddSoftEllipse(cx - rx * 0.28f, cy - ry * 0.26f, rx * 0.64f, ry * 0.62f,
			highlight, depth + 0.003f, MODEL_FLAG_NONE);
	}

	void BuildTreeTrunk(ModelBuilder* builder, float height, float width, float bend)
	{
		const float centerX[5] = { 0.0f, bend * 0.16f, bend * 0.72f, bend, bend * 0.68f };
		const float centerY[5] = { 2.0f, -height * 0.20f, -height * 0.40f, -height * 0.59f, -height * 0.78f };
		const float radius[5] = { width, width * 0.77f, width * 0.65f, width * 0.49f, width * 0.25f };

		for (int segment = 0; segment < TRUNK_SEGMENTS; ++segment)
		{
			const int next = segment + 1;
			const float left[8] = { centerX[segment] - radius[segment], centerY[segment],
				centerX[segment], centerY[segment] + 1.0f, centerX[next], centerY[next],
				centerX[next] - radius[next], centerY[next] };
			const float right[8] = { centerX[segment], centerY[segment] + 1.0f,
				centerX[segment] + radius[segment], centerY[segment],
				centerX[next] + radius[next], centerY[next], centerX[next], centerY[next] };
			// Continuous face tones avoid a repeated gradient seam at every bend.
			builder->AddPolygon(left, 4, Scale(COL_TRUNK, 1.06f), -0.01f);
			builder->AddPolygon(right, 4, Scale(COL_TRUNK, 0.64f), -0.009f);
		}

		builder->AddLine(centerX[2], centerY[2], bend - 18.0f, -height * 0.63f,
			width * 0.69f, Scale(COL_TRUNK, 0.91f), -0.007f);
		builder->AddLine(bend - 18.0f, -height * 0.63f, bend - 31.0f, -height * 0.72f,
			width * 0.38f, Scale(COL_TRUNK, 0.87f), -0.006f);
		builder->AddLine(centerX[2], centerY[2], bend + 21.0f, -height * 0.65f,
			width * 0.60f, Scale(COL_TRUNK, 0.67f), -0.005f);
		builder->AddTriangle(-width, -8.0f, -width * 1.6f, 5.0f, width * 0.30f, 1.0f,
			Scale(COL_TRUNK, 1.02f), -0.004f);
		builder->AddTriangle(0.0f, 2.0f, width * 1.4f, 5.0f, width, -8.0f,
			Scale(COL_TRUNK, 0.63f), -0.003f);
	}

	void BuildCedar(ModelBuilder* builder, float height, int tier)
	{
		builder->AddSoftEllipse(2.0f, 1.0f, 21.0f, 8.0f, RGBA(0.02f, 0.04f, 0.03f, 0.38f), -0.02f,
			MODEL_FLAG_UNLIT | MODEL_FLAG_GROUND);
		builder->AddSoftEllipse(0.0f, 1.0f, 10.0f, 4.0f, RGBA(0.01f, 0.02f, 0.015f, 0.68f), -0.019f,
			MODEL_FLAG_UNLIT | MODEL_FLAG_GROUND);
		const float bend = tier == 1 ? -10.0f : 9.0f + tier * 2.0f;
		BuildTreeTrunk(builder, height, 7.5f + tier * 0.6f, bend);
		builder->SetAnim(ANIM_SWAY, tier * 1.7f, 0.45f);
		BuildCanopyMass(builder, bend + 12.0f, -height * 0.84f, 33.0f + tier * 2.0f, 24.0f,
			Scale(COL_TREE_FOLIAGE, 0.88f), tier * 1.7f, 0.002f);
		builder->SetAnim(ANIM_SWAY, tier * 1.7f, 0.60f);
		BuildCanopyMass(builder, bend - 12.0f, -height * 0.64f, 39.0f + tier, 29.0f,
			Scale(COL_TREE_FOLIAGE, 1.10f), tier * 1.7f + 0.5f, 0.008f);
		builder->ClearAnim();
	}

	void BuildBroadleaf(ModelBuilder* builder, float height)
	{
		builder->AddSoftEllipse(2.0f, 1.0f, 25.0f, 10.0f, RGBA(0.02f, 0.04f, 0.03f, 0.38f), -0.02f,
			MODEL_FLAG_UNLIT | MODEL_FLAG_GROUND);
		builder->AddSoftEllipse(0.0f, 1.0f, 13.0f, 5.0f, RGBA(0.01f, 0.02f, 0.015f, 0.68f), -0.019f,
			MODEL_FLAG_UNLIT | MODEL_FLAG_GROUND);
		BuildTreeTrunk(builder, height, 11.5f, -14.0f);
		builder->SetAnim(ANIM_SWAY, 0.7f, 0.45f);
		BuildCanopyMass(builder, -12.0f, -height * 0.86f, 39.0f, 28.0f,
			Scale(COL_TREE_FOLIAGE, 0.93f), 0.7f, 0.002f);
		BuildCanopyMass(builder, 24.0f, -height * 0.66f, 34.0f, 29.0f,
			Scale(COL_TREE_FOLIAGE, 0.82f), 1.4f, 0.008f);
		builder->SetAnim(ANIM_SWAY, 0.7f, 0.65f);
		BuildCanopyMass(builder, -30.0f, -height * 0.56f, 37.0f, 31.0f,
			Scale(COL_TREE_FOLIAGE, 1.11f), 2.2f, 0.014f);
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
	void BuildPerson(ModelBuilder* builder, float height, bool articulated = false)
	{
		const float shoulderY = -height * 0.62f;
		const float hipY = -height * 0.34f;
		const float headY = -height * 0.80f;
		const float halfHip = height * 0.20f;
		const float halfShoulder = height * 0.15f;

		builder->AddSoftEllipse(3.0f, 1.0f, 14.0f, 6.0f, COL_SHADOW, -0.02f,
			MODEL_FLAG_UNLIT | MODEL_FLAG_GROUND);
		builder->SetFlags(articulated ? MODEL_FLAG_PLAYER_LEFT_LEG : MODEL_FLAG_NONE);
		builder->AddRect(-halfHip * 0.80f, -height * 0.10f, height * 0.10f, height * 0.12f,
			COL_HAIR, -0.005f, MODEL_FLAG_SHADED);
		builder->SetFlags(articulated ? MODEL_FLAG_PLAYER_RIGHT_LEG : MODEL_FLAG_NONE);
		builder->AddRect(halfHip * 0.28f, -height * 0.10f, height * 0.11f, height * 0.12f,
			COL_HAIR, -0.005f, MODEL_FLAG_SHADED);
		builder->SetFlags(articulated ? MODEL_FLAG_PLAYER_UPPER : MODEL_FLAG_NONE);

		const float body[8] =
		{
			-halfHip, -height * 0.08f,
			halfHip, -height * 0.08f,
			halfShoulder, shoulderY,
			-halfShoulder, shoulderY
		};

		builder->AddPolygon(body, 4, RGBA(1.0f, 1.0f, 1.0f), 0.0f, MODEL_FLAG_TINT | MODEL_FLAG_SHADED);
		builder->AddEllipse(-halfShoulder, shoulderY + 5.0f, height * 0.10f, height * 0.15f,
			8, RGBA(0.86f, 0.86f, 0.86f), 0.0005f, MODEL_FLAG_TINT | MODEL_FLAG_SHADED
			| (articulated ? MODEL_FLAG_PLAYER_LEFT_ARM : MODEL_FLAG_NONE));
		const float lapel[6] = { -halfShoulder, shoulderY, 2.0f, hipY, -3.0f, hipY };
		builder->AddPolygon(lapel, 3, RGBA(0.65f, 0.65f, 0.65f), 0.0007f, MODEL_FLAG_TINT);
		builder->AddRect(-halfHip * 0.92f, hipY, halfHip * 1.84f, height * 0.07f, COL_SASH, 0.001f);
		builder->AddEllipse(0.0f, headY, height * 0.15f, height * 0.16f, 10, COL_SKIN, 0.002f, MODEL_FLAG_SHADED);
		builder->AddEllipse(-0.7f, headY - height * 0.06f, height * 0.16f, height * 0.11f,
			9, COL_HAIR, 0.003f, MODEL_FLAG_SHADED);
		builder->AddEllipse(-1.0f, headY - height * 0.18f, height * 0.065f, height * 0.075f,
			8, COL_HAIR, 0.0035f, MODEL_FLAG_SHADED);
		builder->SetFlags(MODEL_FLAG_NONE);
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

	void BuildFoxPlayer(ModelBuilder* builder)
	{
		// This silhouette is player-only; the tintable village person stays unchanged.
		const float cape[] = { -18.0f, -10.0f, -7.0f, -7.0f, 6.0f, -29.0f, -7.0f, -35.0f };
		const float capeFold[] = { -18.0f, -10.0f, -12.0f, -11.0f, -3.0f, -33.0f, -7.0f, -35.0f };
		const float farLeg[] = { -8.0f, -3.0f, -2.0f, -2.0f, -1.0f, -14.0f, -9.0f, -15.0f };
		const float nearLeg[] = { 1.0f, -2.0f, 7.0f, -2.0f, 9.0f, -14.0f, 0.0f, -15.0f };
		const float robe[] = { -9.0f, -12.0f, 10.0f, -12.0f, 8.0f, -29.0f, -5.0f, -32.0f };
		const float robeSide[] = { 5.0f, -12.0f, 10.0f, -12.0f, 8.0f, -29.0f, 4.0f, -29.0f };
		const float lapel[] = { -5.0f, -31.0f, 5.0f, -25.0f, 0.0f, -18.0f, -2.0f, -22.0f };
		const float sleeve[] = { -12.0f, -23.0f, -6.0f, -21.0f, -4.0f, -30.0f, -9.0f, -29.0f };
		const float shoulder[] = { -10.0f, -29.0f, 5.0f, -25.0f, 10.0f, -30.0f, -3.0f, -35.0f };
		const float collar[] = { -6.0f, -33.0f, 4.0f, -30.0f, 6.0f, -32.0f, -2.0f, -36.0f };
		const float mask[] =
		{
			-4.0f, -40.0f, 1.0f, -44.0f, 8.0f, -42.0f,
			11.0f, -36.0f, 6.0f, -31.0f, -2.0f, -34.0f
		};
		const float maskSide[] = { 7.0f, -42.0f, 8.0f, -42.0f, 11.0f, -36.0f, 6.0f, -31.0f };

		builder->AddSoftEllipse(1.0f, 1.0f, 14.0f, 6.0f, COL_SHADOW, -0.02f,
			MODEL_FLAG_UNLIT | MODEL_FLAG_GROUND);
		builder->SetFlags(MODEL_FLAG_PLAYER_UPPER);
		builder->SetAnim(ANIM_SWAY, 0.3f, 0.55f);
		builder->AddPolygon(cape, 4, COL_FOX_CAPE, -0.012f, FOX_SURFACE);
		builder->AddPolygon(capeFold, 4, Scale(COL_FOX_CAPE, 0.63f), -0.011f, MODEL_FLAG_TINT);
		builder->AddTriangle(-18.0f, -10.0f, -12.0f, -12.0f, -17.0f, -6.0f,
			Scale(COL_FOX_CAPE, 0.85f), -0.01f, FOX_SURFACE);
		builder->AddTriangle(-12.0f, -9.0f, -7.0f, -7.0f, -10.0f, -4.0f,
			COL_FOX_CAPE, -0.01f, FOX_SURFACE);
		builder->ClearAnim();
		builder->SetFlags(MODEL_FLAG_NONE);

		builder->SetFlags(MODEL_FLAG_PLAYER_LEFT_LEG);
		builder->AddPolygon(farLeg, 4, Scale(COL_FOX_ROBE, 0.72f), -0.008f, FOX_SURFACE);
		builder->AddEllipse(-5.0f, -0.5f, 4.0f, 2.0f, 8, COL_FOX_LEATHER, -0.006f, FOX_SURFACE);
		builder->AddRect(-7.0f, -6.0f, 4.5f, 4.0f, COL_FOX_LEATHER, -0.005f, FOX_SURFACE);
		builder->AddLine(-6.5f, -4.5f, -2.7f, -3.8f, 0.8f, Scale(COL_FOX_MASK, 0.60f), -0.004f);
		builder->SetFlags(MODEL_FLAG_PLAYER_RIGHT_LEG);
		builder->AddPolygon(nearLeg, 4, COL_FOX_ROBE, -0.007f, FOX_SURFACE);
		builder->AddEllipse(5.0f, 0.0f, 4.5f, 2.2f, 8, COL_FOX_LEATHER, -0.006f, FOX_SURFACE);
		builder->AddRect(2.0f, -6.0f, 5.0f, 4.5f, COL_FOX_LEATHER, -0.005f, FOX_SURFACE);
		builder->AddLine(2.5f, -4.5f, 6.8f, -3.7f, 0.8f, Scale(COL_FOX_MASK, 0.60f), -0.004f);
		builder->SetFlags(MODEL_FLAG_PLAYER_UPPER);
		builder->AddPolygon(robe, 4, COL_FOX_ROBE, 0.0f, FOX_SURFACE);
		builder->AddPolygon(robeSide, 4, Scale(COL_FOX_ROBE, 0.60f), 0.001f, MODEL_FLAG_TINT);
		builder->AddPolygon(lapel, 4, Scale(COL_FOX_ROBE, 1.42f), 0.002f, FOX_SURFACE);
		builder->AddPolygon(sleeve, 4, Scale(COL_FOX_ROBE, 0.82f), 0.003f, FOX_SURFACE | MODEL_FLAG_PLAYER_LEFT_ARM);
		builder->AddLine(-11.5f, -24.0f, -9.0f, -23.0f, 2.0f, Scale(COL_FOX_MASK, 0.65f), 0.004f, MODEL_FLAG_PLAYER_LEFT_ARM);
		builder->AddEllipse(-11.0f, -21.0f, 2.0f, 2.5f, 8, COL_SKIN, 0.005f, FOX_SURFACE | MODEL_FLAG_PLAYER_LEFT_ARM);
		builder->AddEllipse(10.0f, -24.0f, 3.5f, 5.0f, 10, COL_FOX_ROBE, 0.004f, FOX_SURFACE | MODEL_FLAG_PLAYER_RIGHT_ARM);
		builder->AddEllipse(11.0f, -20.0f, 2.0f, 2.5f, 8, COL_SKIN, 0.005f, FOX_SURFACE | MODEL_FLAG_PLAYER_RIGHT_ARM);
		builder->AddLine(-8.0f, -18.0f, 9.0f, -17.0f, 3.0f, COL_FOX_CAPE, 0.006f, FOX_SURFACE);
		builder->AddLine(2.0f, -17.0f, 0.0f, -11.0f, 1.6f, Scale(COL_FOX_CAPE, 1.15f), 0.007f);
		builder->AddLine(2.0f, -17.0f, 4.0f, -12.0f, 1.4f, COL_FOX_CAPE, 0.007f);
		builder->AddPolygon(shoulder, 4, COL_FOX_CAPE, 0.008f, FOX_SURFACE);
		builder->AddPolygon(collar, 4, Scale(COL_FOX_CAPE, 1.22f), 0.009f, FOX_SURFACE);
		builder->AddEllipse(4.5f, -29.0f, 2.2f, 2.0f, 10, COL_FOX_GOLD, 0.01f, FOX_SURFACE);
		builder->AddEllipse(4.5f, -29.0f, 1.1f, 1.0f, 8, COL_FOX_LEATHER, 0.011f);

		builder->AddEllipse(-1.5f, -38.0f, 7.5f, 7.0f, 12, COL_HAIR, 0.012f, FOX_SURFACE);
		builder->AddEllipse(-5.0f, -46.0f, 3.5f, 3.0f, 10, Scale(COL_HAIR, 1.7f), 0.013f, FOX_SURFACE);
		builder->AddTriangle(-7.0f, -45.0f, -10.0f, -41.0f, -2.0f, -43.0f,
			COL_HAIR, 0.014f, FOX_SURFACE);
		builder->AddPolygon(mask, 6, COL_FOX_MASK, 0.015f, FOX_SURFACE);
		builder->AddPolygon(maskSide, 4, Scale(COL_FOX_MASK, 0.73f), 0.016f, MODEL_FLAG_TINT);
		builder->AddTriangle(-4.0f, -40.0f, -5.0f, -48.0f, 1.0f, -44.0f,
			COL_FOX_MASK, 0.017f, FOX_SURFACE);
		builder->AddTriangle(5.0f, -43.0f, 8.0f, -50.0f, 10.0f, -41.0f,
			COL_FOX_MASK, 0.017f, FOX_SURFACE);
		builder->AddTriangle(-2.8f, -42.0f, -3.7f, -45.8f, -0.3f, -43.4f,
			COL_FOX_MARK, 0.018f);
		builder->AddTriangle(6.4f, -43.4f, 8.0f, -47.0f, 8.8f, -42.5f,
			COL_FOX_MARK, 0.018f);
		builder->AddLine(-1.2f, -38.5f, 2.0f, -37.5f, 1.1f, COL_HAIR, 0.019f);
		builder->AddLine(5.0f, -37.5f, 8.5f, -38.5f, 1.1f, COL_HAIR, 0.019f);
		builder->AddTriangle(3.5f, -36.0f, 6.5f, -34.5f, 4.0f, -33.8f,
			Scale(COL_FOX_MASK, 0.63f), 0.019f, MODEL_FLAG_TINT);
		builder->AddTriangle(4.2f, -35.4f, 6.4f, -34.7f, 5.3f, -33.8f,
			COL_HAIR, 0.02f);
		builder->AddLine(-1.8f, -35.0f, 0.2f, -33.8f, 1.2f, COL_FOX_MARK, 0.02f);
		builder->AddLine(7.1f, -35.0f, 7.8f, -33.5f, 1.0f, COL_FOX_MARK, 0.02f);
		builder->SetFlags(MODEL_FLAG_NONE);
	}

	void BuildFoxSword(ModelBuilder* builder)
	{
		const float blade[] = { 7.0f, -18.0f, 10.0f, -18.0f, 26.0f, -3.0f, 22.0f, -4.0f };

		builder->AddPolygon(blade, 4, COL_STEEL, 0.025f, FOX_SURFACE);
		builder->AddLine(9.0f, -18.0f, 25.0f, -3.5f, 0.9f,
			RGBA(0.88f, 0.90f, 0.83f), 0.026f, MODEL_FLAG_TINT);
		builder->AddLine(4.0f, -22.0f, 8.0f, -18.0f, 2.8f,
			COL_FOX_LEATHER, 0.027f, FOX_SURFACE);
		builder->AddLine(6.0f, -17.0f, 10.0f, -21.0f, 1.5f, COL_FOX_GOLD, 0.028f, MODEL_FLAG_TINT);
	}

	void BuildFoxLantern(ModelBuilder* builder)
	{
		const float lanternX = -17.0f;
		const float lanternY = -15.0f;
		const float glowWidth = 25.0f;
		const float glowHeight = 21.0f;
		const float cageWidth = 7.0f;
		const float cageHeight = 9.0f;
		const Color wood = Scale(COL_FOX_LEATHER, 0.80f);

		builder->SetAnim(ANIM_FLAME, 0.0f, 0.25f);
		builder->AddSoftEllipse(lanternX, lanternY, glowWidth, glowHeight,
			RGBA(1.0f, 0.68f, 0.32f, 0.16f), -0.03f);
		builder->ClearAnim();
		builder->AddLine(-11.0f, -21.0f, lanternX, lanternY - cageHeight * 0.5f - 3.0f,
			1.0f, COL_FOX_GOLD, 0.021f, FOX_SURFACE);
		builder->AddLine(lanternX, lanternY - cageHeight * 0.5f - 3.0f,
			lanternX, lanternY - cageHeight * 0.5f, 1.0f, COL_FOX_GOLD, 0.021f, FOX_SURFACE);
		builder->AddRect(lanternX - cageWidth * 0.5f - 1.0f, lanternY - cageHeight * 0.5f - 1.0f,
			cageWidth + 2.0f, cageHeight + 2.0f, wood, 0.022f, FOX_SURFACE);
		builder->SetAnim(ANIM_FLAME, 1.1f, 0.15f);
		builder->AddRect(lanternX - cageWidth * 0.5f, lanternY - cageHeight * 0.5f,
			cageWidth, cageHeight, RGBA(1.0f, 0.72f, 0.32f), 0.023f, MODEL_FLAG_UNLIT);
		builder->ClearAnim();
		builder->AddLine(lanternX, lanternY - cageHeight * 0.5f,
			lanternX, lanternY + cageHeight * 0.5f, 0.9f, wood, 0.024f, FOX_SURFACE);
		builder->AddLine(lanternX - cageWidth * 0.5f, lanternY,
			lanternX + cageWidth * 0.5f, lanternY, 0.8f, wood, 0.024f, FOX_SURFACE);
		builder->AddLine(lanternX - cageWidth * 0.5f - 1.0f, lanternY - cageHeight * 0.5f - 1.0f,
			lanternX + cageWidth * 0.5f + 1.0f, lanternY - cageHeight * 0.5f - 1.0f,
			1.2f, COL_FOX_GOLD, 0.025f, FOX_SURFACE);
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
	float scale, float depth, const ShadeParams& shade, float bodyTilt, unsigned int hiddenFlags,
	const PlayerMovePose* movePose)
{
	const float cornerShade[4] = { 0.0f, 0.35f, 1.0f, 0.65f };

	for (const ModelPoly& poly : model.polys)
	{
		if ((poly.flags & hiddenFlags) != 0)
		{
			continue;
		}

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

		float polyDepth = (poly.flags & MODEL_FLAG_GROUND) != 0
			? GROUND_SHADOW_DEPTH + depth * 0.01f : depth + poly.depthBias;
		float originX = screenX;
		float originY = screenY;
		renderer->SetAnim((AnimKind)poly.anim, poly.animPhase, poly.animStrength * scale);

		if ((poly.flags & MODEL_FLAG_PLAYER_UPPER) != 0)
		{
			if (movePose != NULL)
			{
				originX += movePose->bodyX * scale;
				originY += movePose->bodyY * scale;
			}

			if (bodyTilt != 0.0f)
			{
				renderer->SetAnim(ANIM_PLAYER_POSE, bodyTilt, poly.anim == ANIM_SWAY ? poly.animStrength * scale : 0.0f);
			}

			if (movePose != NULL && movePose->cape != 0.0f && poly.anim == ANIM_SWAY)
			{
				renderer->SetAnim(ANIM_PLAYER_CAPE, bodyTilt, movePose->cape * scale);
			}
		}
		else if (movePose != NULL
			&& (poly.flags & (MODEL_FLAG_PLAYER_LEFT_LEG | MODEL_FLAG_PLAYER_RIGHT_LEG)) != 0)
		{
			const int side = (poly.flags & MODEL_FLAG_PLAYER_LEFT_LEG) != 0 ? 0 : 1;
			const PlayerLimbOffset& foot = movePose->feet[side];
			originX += movePose->bodyX * scale;
			originY += movePose->bodyY * scale;
			renderer->SetAnim(ANIM_PLAYER_LEG, (foot.x - movePose->bodyX) * scale,
				(foot.y - movePose->bodyY) * scale);
			polyDepth += foot.y * 0.0004f;
		}

		renderer->PushMeshShaded(poly.mesh, originX, originY, scale, scale,
			colors, poly.shadeBounds, (poly.flags & MODEL_FLAG_RADIAL) != 0, polyDepth);
	}

	renderer->ClearAnim();
}

namespace
{
	const float SWORD_HAND_RADIUS = 8.0f;
	const float SWORD_LENGTH = 32.0f;
	const float SWORD_FLATTEN = 0.55f;

	float SwordAngle(const PlayerAttackMotion& motion)
	{
		const float halfSweep = motion.fullCircle ? PlayerMotion::PI : acosf(PlayerMotion::FORWARD_DOT);

		return motion.Angle() + motion.side * halfSweep * (motion.SwordSweep() * 2.0f - 1.0f);
	}

	Color PlayerMaterial(const Color& color, const ShadeParams& shade)
	{
		return Mix(Modulate(Modulate(color, shade.tint), shade.light), shade.fogColor, shade.fog);
	}

	void PosePoint(float x, float y, float tilt, float& resultX, float& resultY)
	{
		resultX = x * cosf(tilt) - (y - PlayerMotion::HIP_Y) * sinf(tilt);
		resultY = PlayerMotion::HIP_Y + x * sinf(tilt) + (y - PlayerMotion::HIP_Y) * cosf(tilt);
	}

	void DrawPlayerArm(Renderer* renderer, float x, float y, float shoulderX, float shoulderY,
		float handX, float handY, const ShadeParams& shade, float depth)
	{
		const float elbowX = (shoulderX + handX) * 0.5f;
		const float elbowY = (shoulderY + handY) * 0.5f + 2.0f;
		const Color cloth = PlayerMaterial(COL_FOX_ROBE, shade);
		const Color wrap = PlayerMaterial(Scale(COL_FOX_MASK, 0.65f), shade);
		const Color skin = PlayerMaterial(COL_SKIN, shade);

		renderer->PushLine(x + shoulderX, y + shoulderY, x + elbowX, y + elbowY, 5.0f, cloth, depth);
		renderer->PushLine(x + elbowX, y + elbowY, x + handX, y + handY, 3.0f, wrap, depth + 0.001f);
		renderer->PushEllipse(x + handX, y + handY, 2.0f, 2.2f, skin, depth + 0.002f, 8);
	}
}

void DrawFoxPlayer(Renderer* renderer, const ModelLibrary& models, float screenX, float screenY,
	float depth, const ShadeParams& shade, bool swordEquipped, const PlayerAttackMotion* motion,
	const PlayerMoveMotion* movement)
{
	const char* const parts[] = { "player_fox", "player_fox_lantern", "player_fox_sword" };
	const int partCount = 3;
	const int swordPart = 2;
	const bool attacking = motion != NULL && motion->active;
	const float attackWeight = attacking ? PlayerMotion::Ease(motion->Progress() / 0.12f)
		* (1.0f - PlayerMotion::Ease((motion->Progress() - 0.80f) / 0.20f)) : 0.0f;
	const PlayerMovePose movePose = movement != NULL ? movement->Sample(1.0f - attackWeight * 0.85f) : PlayerMovePose();
	const float tilt = movePose.tilt + (attacking ? motion->Tilt() : 0.0f);
	const unsigned int hidden = MODEL_FLAG_PLAYER_RIGHT_ARM | MODEL_FLAG_PLAYER_LEFT_ARM;
	const float bodyX = screenX + movePose.bodyX;
	const float bodyY = screenY + movePose.bodyY;

	for (int part = 0; part < partCount; ++part)
	{
		if (part == swordPart && (!swordEquipped || attacking))
		{
			continue;
		}

		const Model* model = models.Find(parts[part]);

		if (model != NULL)
		{
			float offsetX = 0.0f;
			float offsetY = 0.0f;

			if (part == 1)
			{
				PosePoint(-11.0f, -21.0f, tilt, offsetX, offsetY);
				offsetX += 11.0f + movePose.bodyX + movePose.hands[0].x;
				offsetY += 21.0f + movePose.bodyY + movePose.hands[0].y;

				if (attacking && motion->bow)
				{
					offsetX += 8.0f * motion->BowPull();
					offsetY += 3.0f * motion->BowPull();
				}
			}
			else if (part == swordPart)
			{
				PosePoint(8.0f, -20.0f, tilt, offsetX, offsetY);
				offsetX += -8.0f + movePose.bodyX + movePose.hands[1].x;
				offsetY += 20.0f + movePose.bodyY + movePose.hands[1].y;
			}

			DrawModel(renderer, *model, screenX + offsetX, screenY + offsetY, 1.0f, depth, shade,
				part == 0 ? tilt : 0.0f, part == 0 ? hidden : 0, part == 0 ? &movePose : NULL);
		}
	}

	if (!(attacking && motion->bow))
	{
		float shoulderX, shoulderY, handX, handY;
		PosePoint(-8.0f, -27.0f, tilt, shoulderX, shoulderY);
		PosePoint(-11.0f, -21.0f, tilt, handX, handY);
		DrawPlayerArm(renderer, bodyX, bodyY, shoulderX, shoulderY,
			handX + movePose.hands[0].x, handY + movePose.hands[0].y, shade, depth + 0.006f);
	}

	if (!attacking)
	{
		float shoulderX, shoulderY, handX, handY;
		PosePoint(7.0f, -28.0f, tilt, shoulderX, shoulderY);
		PosePoint(swordEquipped ? 8.0f : 14.0f, swordEquipped ? -20.0f : -24.0f, tilt, handX, handY);
		DrawPlayerArm(renderer, bodyX, bodyY, shoulderX, shoulderY,
			handX + movePose.hands[1].x, handY + movePose.hands[1].y, shade, depth + 0.012f);
	}

	if (!swordEquipped)
	{
		const float pull = attacking ? motion->BowPull() : 0.0f;
		const float angle = attacking ? motion->Angle() : 0.0f;
		const float recoil = attacking && motion->Progress() > PlayerMotion::BOW_RELEASE
			? sinf((motion->Progress() - PlayerMotion::BOW_RELEASE) / (1.0f - PlayerMotion::BOW_RELEASE) * PlayerMotion::PI) * 2.0f : 0.0f;
		float gripX, gripY;
		PosePoint(14.0f, -24.0f, attacking ? 0.0f : tilt, gripX, gripY);
		const float bowX = bodyX + (attacking ? 0.0f : gripX - 14.0f + movePose.hands[1].x);
		const float bowY = bodyY + (attacking ? 0.0f : gripY + 24.0f + movePose.hands[1].y);
		Bow::DrawEquippedAt(*renderer, bowX, bowY, depth + 0.04f, pull, angle, recoil,
			attacking && motion->Progress() < PlayerMotion::BOW_RELEASE);

		if (attacking)
		{
			float shoulderX, shoulderY;
			PosePoint(7.0f, -28.0f, tilt, shoulderX, shoulderY);
			DrawPlayerArm(renderer, bodyX, bodyY, shoulderX, shoulderY,
				14.0f + recoil * cosf(angle), -24.0f + recoil * sinf(angle), shade, depth + 0.05f);
			PosePoint(-8.0f, -27.0f, tilt, shoulderX, shoulderY);
			DrawPlayerArm(renderer, bodyX, bodyY, shoulderX, shoulderY,
				14.0f - pull * 8.0f * cosf(angle), -24.0f - pull * 8.0f * sinf(angle), shade, depth + 0.052f);
		}
	}
	else if (attacking)
	{
		const float angle = SwordAngle(*motion);
		const float cosine = cosf(angle);
		const float sine = sinf(angle) * SWORD_FLATTEN;
		const float handX = cosine * SWORD_HAND_RADIUS;
		const float handY = PlayerMotion::HIP_Y + sine * SWORD_HAND_RADIUS;
		const float swordDepth = depth + (sinf(angle) < 0.0f ? -0.03f : 0.04f);
		float shoulderX, shoulderY;
		PosePoint(7.0f, -28.0f, tilt, shoulderX, shoulderY);
		DrawPlayerArm(renderer, bodyX, bodyY, shoulderX, shoulderY, handX, handY, shade, swordDepth);
		renderer->PushLine(bodyX + handX, bodyY + handY,
			bodyX + handX + cosine * SWORD_LENGTH, bodyY + handY + sine * SWORD_LENGTH,
			3.0f, PlayerMaterial(COL_STEEL, shade), swordDepth + 0.004f);
		renderer->PushLine(bodyX + handX, bodyY + handY - 0.7f,
			bodyX + handX + cosine * SWORD_LENGTH, bodyY + handY + sine * SWORD_LENGTH - 0.7f,
			0.9f, PlayerMaterial(RGBA(0.88f, 0.90f, 0.83f), shade), swordDepth + 0.005f);
		renderer->PushLine(bodyX + handX - sinf(angle) * 3.0f, bodyY + handY + cosine * 2.0f,
			bodyX + handX + sinf(angle) * 3.0f, bodyY + handY - cosine * 2.0f,
			1.5f, PlayerMaterial(COL_FOX_GOLD, shade), swordDepth + 0.006f);
	}
}

void DrawFoxSlash(Renderer* renderer, float screenX, float screenY, float depth,
	const PlayerAttackMotion& motion)
{
	const float progress = motion.Progress();

	if (!motion.active || motion.bow || progress < 0.24f || progress > 0.82f)
	{
		return;
	}

	const int segments = 8;
	const float radius = motion.fullCircle ? 80.0f : 48.0f;
	const float angle = SwordAngle(motion);
	const float tail = motion.side * (motion.fullCircle ? 1.9f : 1.2f) * fminf(motion.SwordSweep() * 4.0f, 1.0f);
	const float alpha = sinf((progress - 0.24f) / 0.58f * PlayerMotion::PI) * 0.65f;

	for (int segment = 0; segment < segments; ++segment)
	{
		const float a0 = angle - tail * (float)segment / segments;
		const float a1 = angle - tail * (float)(segment + 1) / segments;
		const float outer0 = radius * (1.0f - (float)segment / segments * 0.12f);
		const float outer1 = radius * (1.0f - (float)(segment + 1) / segments * 0.12f);
		const float inner = radius * 0.72f;
		const float points[] =
		{
			screenX + cosf(a0) * inner, screenY + PlayerMotion::HIP_Y + sinf(a0) * inner * SWORD_FLATTEN,
			screenX + cosf(a1) * inner, screenY + PlayerMotion::HIP_Y + sinf(a1) * inner * SWORD_FLATTEN,
			screenX + cosf(a1) * outer1, screenY + PlayerMotion::HIP_Y + sinf(a1) * outer1 * SWORD_FLATTEN,
			screenX + cosf(a0) * outer0, screenY + PlayerMotion::HIP_Y + sinf(a0) * outer0 * SWORD_FLATTEN
		};
		const float fade = 1.0f - (float)segment / segments;
		const Color clear = RGBA(0.91f, 0.93f, 0.81f, 0.0f);
		const Color edge = RGBA(0.91f, 0.93f, 0.81f, alpha * fade);
		const Color colors[] = { clear, clear, edge, edge };
		renderer->PushPolygonShaded(points, colors, 4,
			depth + (sinf((a0 + a1) * 0.5f) < 0.0f ? -0.04f : 0.06f));
	}
}

// ---------------------------------------------------------------- builder

ModelBuilder::ModelBuilder()
	: m_Anim(ANIM_NONE)
	, m_AnimPhase(0.0f)
	, m_AnimStrength(0.0f)
	, m_Flags(MODEL_FLAG_NONE)
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

void ModelBuilder::SetFlags(unsigned int flags)
{
	m_Flags = flags;
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
	poly.flags = flags | m_Flags;
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
		model.bounds[0] = model.bounds[1] = 1e9f;
		model.bounds[2] = model.bounds[3] = -1e9f;

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

			if ((poly.flags & MODEL_FLAG_GROUND) == 0)
			{
				model.bounds[0] = fminf(model.bounds[0], minX);
				model.bounds[1] = fminf(model.bounds[1], minY);
				model.bounds[2] = fmaxf(model.bounds[2], maxX);
				model.bounds[3] = fmaxf(model.bounds[3], maxY);
			}

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
	ModelBuilder resident;
	BuildPerson(&resident, PERSON_HEIGHT, true);
	m_Models["resident_active"] = resident.Get();
	ModelBuilder residentHat;
	residentHat.SetFlags(MODEL_FLAG_PLAYER_UPPER);
	BuildKasa(&residentHat, PERSON_HEIGHT);
	m_Models["resident_kasa"] = residentHat.Get();

	ModelBuilder kasa;
	BuildKasa(&kasa, PERSON_HEIGHT);
	m_Models["person_kasa"] = kasa.Get();

	ModelBuilder cloak;
	BuildPlayerCloak(&cloak, PERSON_HEIGHT);
	m_Models["player_cloak"] = cloak.Get();

	ModelBuilder fox;
	BuildFoxPlayer(&fox);
	m_Models["player_fox"] = fox.Get();

	ModelBuilder foxSword;
	BuildFoxSword(&foxSword);
	m_Models["player_fox_sword"] = foxSword.Get();

	ModelBuilder foxLantern;
	BuildFoxLantern(&foxLantern);
	m_Models["player_fox_lantern"] = foxLantern.Get();

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
					| MODEL_FLAG_RADIAL | MODEL_FLAG_GROUND | MODEL_FLAG_PLAYER_UPPER
					| MODEL_FLAG_PLAYER_LEFT_ARM | MODEL_FLAG_PLAYER_RIGHT_ARM
					| MODEL_FLAG_PLAYER_LEFT_LEG | MODEL_FLAG_PLAYER_RIGHT_LEG)) != 0
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
