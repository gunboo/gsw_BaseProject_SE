#pragma once

#include <cmath>

#include "Lighting.h"
#include "World.h"

// Shared materials keep the village and the generated forest in one art style.
namespace VisualStyle
{
	const Color COL_FOG = { 0.10f, 0.15f, 0.14f, 1.0f };
	const Color COL_SKY = { 0.075f, 0.105f, 0.095f, 1.0f };
	const Color COL_IVORY = { 0.91f, 0.91f, 0.79f, 1.0f };
	const Color COL_MOSS = { 0.63f, 0.75f, 0.39f, 1.0f };
	const Color COL_SPIRIT = { 0.38f, 0.81f, 0.72f, 1.0f };
	const Color COL_PLAYER = { 0.48f, 0.23f, 0.20f, 1.0f };
	const float GROUND_DEPTH = -100000.0f;
	const float SHADOW_DEPTH = -90000.0f;
	const float GROUND_DETAIL_DEPTH = 1.0f;
	const float UI_CORNER = 5.0f;
	const float UI_BORDER = 1.0f;
	const int HEALTH_SEGMENTS = 5;
	const float HEALTH_GAP = 4.0f;
	const float SHORE_WIDTH = 1.2f;

	inline float Clamp(float value, float low, float high)
	{
		return fminf(fmaxf(value, low), high);
	}

	inline unsigned int Hash(int x, int y)
	{
		unsigned int hash = (unsigned int)x * 73856093u ^ (unsigned int)y * 19349663u;
		hash ^= hash >> 13;
		hash *= 1274126177u;

		return hash ^ (hash >> 16);
	}

	inline Color GroundColor(TileType tile)
	{
		switch (tile)
		{
		case TILE_DIRT:      return RGBA(0.49f, 0.44f, 0.31f);
		case TILE_STONE:     return RGBA(0.57f, 0.58f, 0.46f);
		case TILE_SAND:      return RGBA(0.60f, 0.56f, 0.39f);
		case TILE_WATER:     return RGBA(0.18f, 0.36f, 0.35f);
		case TILE_TALLGRASS: return RGBA(0.35f, 0.43f, 0.22f);
		default:            return RGBA(0.42f, 0.49f, 0.27f);
		}
	}

	inline Color GroundCorner(const World& world, const Lighting& lighting, int x, int y)
	{
		Color base = RGBA(0.0f, 0.0f, 0.0f);
		int samples = 0;

		// Land shares corner colors; water keeps a clear collision boundary.
		for (int dy = -1; dy <= 0; ++dy)
		{
			for (int dx = -1; dx <= 0; ++dx)
			{
				const int tileX = (int)Clamp((float)(x + dx), 0.0f, (float)(world.GetWidth() - 1));
				const int tileY = (int)Clamp((float)(y + dy), 0.0f, (float)(world.GetHeight() - 1));
				const TileType tile = world.GetTile(tileX, tileY);

				if (tile == TILE_WATER)
				{
					continue;
				}

				const Color material = GroundColor(tile);
				base.r += material.r;
				base.g += material.g;
				base.b += material.b;
				++samples;
			}
		}

		base = samples > 0 ? Scale(base, 1.0f / samples) : GroundColor(TILE_WATER);
		const float broadShade = 0.94f + 0.10f * sinf(x * 0.34f + y * 0.18f)
			+ 0.06f * cosf(x * 0.19f - y * 0.43f);

		return lighting.Apply(Scale(base, broadShade), (float)x, (float)y);
	}

	inline void DrawGroundTile(Renderer* renderer, const World& world, const Lighting& lighting,
		int x, int y, float worldZ = 0.0f)
	{
		const int cornerX[4] = { x, x + 1, x + 1, x };
		const int cornerY[4] = { y, y, y + 1, y + 1 };
		float points[8];
		Color colors[4];
		const float depth = GROUND_DEPTH + (float)(x + y) * 0.01f;
		const TileType tile = world.GetTile(x, y);
		const unsigned int hash = Hash(x, y);

		for (int i = 0; i < 4; ++i)
		{
			renderer->WorldToScreen((float)cornerX[i], (float)cornerY[i], worldZ,
				&points[i * 2], &points[i * 2 + 1]);
			colors[i] = tile == TILE_WATER
				? lighting.Apply(GroundColor(tile), (float)cornerX[i], (float)cornerY[i])
				: GroundCorner(world, lighting, cornerX[i], cornerY[i]);
		}

		renderer->PushPolygonShaded(points, colors, 4, depth);

		float screenX = 0.0f;
		float screenY = 0.0f;
		renderer->WorldToScreen((float)x + 0.5f, (float)y + 0.5f, worldZ, &screenX, &screenY);

		if (tile == TILE_WATER)
		{
			const int neighborX[4] = { x, x + 1, x, x - 1 };
			const int neighborY[4] = { y - 1, y, y + 1, y };
			const Color shore = lighting.Apply(RGBA(0.61f, 0.72f, 0.54f, 0.38f), (float)x, (float)y);

			for (int edge = 0; edge < 4; ++edge)
			{
				if (world.GetTile(neighborX[edge], neighborY[edge]) == TILE_WATER)
				{
					continue;
				}

				const int next = (edge + 1) % 4;
				renderer->PushLine(points[edge * 2], points[edge * 2 + 1],
					points[next * 2], points[next * 2 + 1], SHORE_WIDTH,
					shore, depth + GROUND_DETAIL_DEPTH);
			}
		}

		if (tile == TILE_STONE)
		{
			const float halfWidth = Renderer::TileHalfWidth() * 0.80f;
			const float halfHeight = Renderer::TileHalfHeight() * 0.80f;
			const float slab[8] =
			{
				screenX, screenY - halfHeight,
				screenX + halfWidth, screenY,
				screenX, screenY + halfHeight,
				screenX - halfWidth, screenY
			};
			const Color stone = lighting.Apply(GroundColor(tile), (float)x + 0.5f, (float)y + 0.5f);
			const Color slabColors[4] = { Scale(stone, 1.15f), stone, Scale(stone, 0.88f), stone };
			renderer->PushPolygonShaded(slab, slabColors, 4, depth + GROUND_DETAIL_DEPTH);
			renderer->PushLine(slab[0], slab[1], slab[6], slab[7], UI_BORDER,
				Scale(stone, 1.23f), depth + GROUND_DETAIL_DEPTH + 0.1f);
		}
		else if (tile == TILE_WATER && hash % 4 == 0)
		{
			const float phase = x * 0.62f + y * 0.44f;
			Color ripple = lighting.Apply(RGBA(0.49f, 0.68f, 0.58f, 0.24f), (float)x, (float)y);
			renderer->SetAnim(ANIM_WATER, phase, 0.7f);
			renderer->PushLine(screenX - 11.0f, screenY, screenX + 8.0f, screenY - 2.0f,
				0.8f, ripple, depth + GROUND_DETAIL_DEPTH);
			renderer->ClearAnim();
		}
		else if (tile != TILE_WATER && tile != TILE_STONE && hash % 7 == 0)
		{
			const float offsetX = (float)((hash >> 8) % 21) - 10.0f;
			const float offsetY = (float)((hash >> 16) % 9) - 4.0f;
			const Color leaf = lighting.Apply(RGBA(0.62f, 0.63f, 0.35f, 0.48f), (float)x, (float)y);
			renderer->PushEllipse(screenX + offsetX, screenY + offsetY, 3.0f, 1.2f,
				leaf, depth + GROUND_DETAIL_DEPTH, 6);
		}
	}

	inline void DrawPanel(Renderer* renderer, float x, float y, float width, float height,
		float alpha, float depth)
	{
		const float corner = fminf(UI_CORNER, fminf(width, height) * 0.25f);
		const float points[16] =
		{
			x + corner, y, x + width - corner, y,
			x + width, y + corner, x + width, y + height - corner,
			x + width - corner, y + height, x + corner, y + height,
			x, y + height - corner, x, y + corner
		};
		const Color top = RGBA(0.075f, 0.105f, 0.095f, alpha);
		const Color bottom = RGBA(0.025f, 0.042f, 0.038f, alpha);
		const Color colors[8] = { top, top, top, bottom, bottom, bottom, bottom, top };
		for (int i = 1; i < 7; ++i)
		{
			const float triangle[] = { points[0], points[1], points[i * 2], points[i * 2 + 1],
				points[(i + 1) * 2], points[(i + 1) * 2 + 1] };
			const Color tint[] = { colors[0], colors[i], colors[i + 1] };
			renderer->PushPolygonShaded(triangle, tint, 3, depth);
		}
		renderer->PushLine(x + corner, y, x + width - corner, y, UI_BORDER,
			RGBA(0.67f, 0.72f, 0.57f, alpha * 0.35f), depth + 0.1f);
		renderer->PushLine(x + corner, y + height, x + width - corner, y + height, UI_BORDER,
			RGBA(0.01f, 0.025f, 0.02f, alpha * 0.65f), depth + 0.1f);
	}

	inline void DrawMeter(Renderer* renderer, float x, float y, float width, float height,
		float fill, const Color& color, const Color& back, float depth)
	{
		renderer->PushRect(x, y, width, height, back, depth);
		const float filledWidth = width * Clamp(fill, 0.0f, 1.0f);

		if (filledWidth <= 0.0f)
		{
			return;
		}

		const float points[8] = { x, y, x + filledWidth, y, x + filledWidth, y + height, x, y + height };
		const Color bright = Scale(color, 1.12f);
		const Color dim = Scale(color, 0.78f);
		const Color colors[4] = { bright, bright, dim, dim };
		renderer->PushPolygonShaded(points, colors, 4, depth + 0.1f);
	}

	inline void DrawHealth(Renderer* renderer, float x, float y, float width, float height,
		float fill, float depth)
	{
		const float segmentWidth = (width - HEALTH_GAP * (HEALTH_SEGMENTS - 1)) / HEALTH_SEGMENTS;
		const Color health = fill < 0.25f ? RGBA(0.87f, 0.39f, 0.28f) : COL_MOSS;

		for (int i = 0; i < HEALTH_SEGMENTS; ++i)
		{
			const float segmentX = x + (segmentWidth + HEALTH_GAP) * i;
			renderer->PushRect(segmentX - UI_BORDER, y - UI_BORDER,
				segmentWidth + UI_BORDER * 2.0f, height + UI_BORDER * 2.0f,
				RGBA(COL_IVORY.r, COL_IVORY.g, COL_IVORY.b, 0.65f), depth);
			DrawMeter(renderer, segmentX, y, segmentWidth, height, fill * HEALTH_SEGMENTS - i,
				health, RGBA(0.10f, 0.15f, 0.12f, 0.95f), depth + 0.1f);
		}
	}
}
