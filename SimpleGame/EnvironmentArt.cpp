#include "stdafx.h"
#include "EnvironmentArt.h"
#include "VisualStyle.h"

#include <cmath>

namespace
{
	const Color COL_STONE = { 0.61f, 0.65f, 0.54f, 1.0f };
	const Color COL_CAP = { 0.76f, 0.78f, 0.64f, 1.0f };
	const Color COL_ROOF = { 0.24f, 0.34f, 0.29f, 1.0f };
	const Color COL_RECESS = { 0.095f, 0.14f, 0.12f, 1.0f };
	const Color COL_WOOD = { 0.32f, 0.28f, 0.21f, 1.0f };
	const Color COL_MOSS = { 0.38f, 0.49f, 0.25f, 1.0f };
	const float FOUNDATION_HEIGHT = 0.45f;
	const float HOUSE_WALL_HEIGHT = 2.65f;
	const float HOUSE_ROOF_HEIGHT = 4.65f;
	const float SHRINE_WALL_HEIGHT = 3.4f;
	const float SHRINE_ROOF_HEIGHT = 5.9f;
	const float KEEP_WALL_HEIGHT = 7.4f;
	const float KEEP_ROOF_HEIGHT = 10.7f;
	const float TOWER_HEIGHT = 8.7f;
	const float WALL_HEIGHT = 3.5f;
	const float GATE_HEIGHT = 4.8f;
	const float RUIN_HEIGHT = 3.8f;
	const float TORII_HEIGHT = 3.4f;
	const float TORII_SPAN = StructureDimensions::TORII_HALF_SPAN;
	const float BATTLEMENT_HEIGHT = 0.65f;
	const float BATTLEMENT_SPACING = 1.1f;
	const float STONE_COURSE_HEIGHT = 0.75f;
	const float WINDOW_WIDTH = 0.45f;
	const float WINDOW_HEIGHT = 1.15f;
	const float ROOF_OVERHANG = 0.45f;
	const float CUTAWAY_ALPHA = 0.36f;
	const int ARCH_SEGMENTS = 6;
	const float PI = 3.14159265f;

	struct Point
	{
		float x, y, z;
	};

	struct Painter
	{
		Renderer* renderer;
		const Lighting& lighting;
		float groundZ;
		float opacity;

		void Face(const Point* vertices, int count, const Color& material, float depth, bool emissive = false) const
		{
			float points[8];
			Color colors[4];
			const Color lit = emissive ? Mix(material, lighting.GetFogColor(),
				lighting.FogFactor(vertices[0].x, vertices[0].y) * 0.7f)
				: lighting.Apply(material, vertices[0].x, vertices[0].y);

			for (int i = 0; i < count; ++i)
			{
				renderer->WorldToScreen(vertices[i].x, vertices[i].y, vertices[i].z + groundZ,
					&points[i * 2], &points[i * 2 + 1]);
				colors[i] = lit;
				colors[i].a = material.a * opacity;
			}

			renderer->PushPolygonShaded(points, colors, count, depth);
		}

		void South(float x0, float x1, float y, float bottom, float top, const Color& color, float depth) const
		{
			const Point face[4] = { { x0, y, bottom }, { x1, y, bottom }, { x1, y, top }, { x0, y, top } };
			Face(face, 4, color, depth);
		}

		void East(float x, float y0, float y1, float bottom, float top, const Color& color, float depth) const
		{
			const Point face[4] = { { x, y0, bottom }, { x, y1, bottom }, { x, y1, top }, { x, y0, top } };
			Face(face, 4, color, depth);
		}

		void Box(float x0, float y0, float x1, float y1, float bottom, float top,
			const Color& material, float depth) const
		{
			South(x0, x1, y1, bottom, top, material, depth);
			East(x1, y0, y1, bottom, top, Scale(material, 0.62f), depth + 0.01f);
			const Point roof[4] = { { x0, y0, top }, { x1, y0, top }, { x1, y1, top }, { x0, y1, top } };
			Face(roof, 4, Scale(material, 1.20f), depth + 0.02f);
		}

		void Line(const Point& a, const Point& b, const Color& color, float width, float depth) const
		{
			float ax, ay, bx, by;
			renderer->WorldToScreen(a.x, a.y, a.z + groundZ, &ax, &ay);
			renderer->WorldToScreen(b.x, b.y, b.z + groundZ, &bx, &by);
			Color lit = lighting.Apply(color, a.x, a.y);
			lit.a *= opacity;
			renderer->PushLine(ax, ay, bx, by, width, lit, depth);
		}

		void Arch(float center, float y, float width, float bottom, float spring,
			const Color& material, float depth, bool emissive = false) const
		{
			const float radius = width * 0.5f;
			const Point rectangle[4] = { { center - radius, y, bottom }, { center + radius, y, bottom },
				{ center + radius, y, spring }, { center - radius, y, spring } };
			Face(rectangle, 4, material, depth, emissive);

			for (int i = 0; i < ARCH_SEGMENTS; ++i)
			{
				const float angle0 = PI * (float)i / ARCH_SEGMENTS;
				const float angle1 = PI * (float)(i + 1) / ARCH_SEGMENTS;
				const Point fan[3] = { { center, y, spring },
					{ center + cosf(angle0) * radius, y, spring + sinf(angle0) * radius },
					{ center + cosf(angle1) * radius, y, spring + sinf(angle1) * radius } };
				Face(fan, 3, material, depth, emissive);
			}
		}
	};

	void Masonry(const Painter& painter, const Prop& prop, float top, float depth)
	{
		const float x1 = prop.x + prop.sizeX;
		const float y1 = prop.y + prop.sizeY;

		for (int row = 1; row * STONE_COURSE_HEIGHT < top; ++row)
		{
			const float z = row * STONE_COURSE_HEIGHT;
			painter.Line({ prop.x, y1, z }, { x1, y1, z }, Scale(COL_STONE, 0.74f), 0.7f, depth);
			painter.Line({ x1, prop.y, z }, { x1, y1, z }, Scale(COL_STONE, 0.47f), 0.7f, depth);

			for (float x = prop.x + 0.6f + (row % 2) * 0.6f; x < x1; x += 1.2f)
			{
				painter.Line({ x, y1, z - STONE_COURSE_HEIGHT }, { x, y1, z },
					Scale(COL_STONE, 0.78f), 0.7f, depth);
			}
		}
	}

	void Roof(const Painter& painter, const Prop& prop, float eaveHeight, float peak, float depth)
	{
		const float x0 = prop.x - ROOF_OVERHANG;
		const float y0 = prop.y - ROOF_OVERHANG;
		const float x1 = prop.x + prop.sizeX + ROOF_OVERHANG;
		const float y1 = prop.y + prop.sizeY + ROOF_OVERHANG;
		const float yc = (y0 + y1) * 0.5f;
		const Point back[4] = { { x0, y0, eaveHeight }, { x1, y0, eaveHeight }, { x1, yc, peak }, { x0, yc, peak } };
		const Point slope[4] = { { x0, y1, eaveHeight }, { x1, y1, eaveHeight }, { x1, yc, peak }, { x0, yc, peak } };
		const Point gable[3] = { { x1, y0, eaveHeight }, { x1, y1, eaveHeight }, { x1, yc, peak } };
		painter.Face(back, 4, Scale(COL_ROOF, 1.12f), depth);
		painter.Face(slope, 4, COL_ROOF, depth + 0.01f);
		painter.Face(gable, 3, Scale(COL_ROOF, 0.58f), depth + 0.02f);
		painter.South(x0, x1, y1, eaveHeight - 0.18f, eaveHeight, Scale(COL_ROOF, 0.54f), depth + 0.03f);
		painter.East(x1, y0, y1, eaveHeight - 0.18f, eaveHeight, Scale(COL_ROOF, 0.38f), depth + 0.03f);
		painter.Line({ x0, yc, peak }, { x1, yc, peak }, Scale(COL_ROOF, 1.45f), 3.0f, depth + 0.04f);

		for (float x = x0 + 0.65f; x < x1; x += 0.65f)
		{
			painter.Line({ x, y1, eaveHeight }, { x, yc, peak }, Scale(COL_ROOF, 1.21f), 0.8f, depth + 0.04f);
		}
	}

	void Crenellations(const Painter& painter, const Prop& prop, float z, float depth)
	{
		const float x1 = prop.x + prop.sizeX;
		const float y1 = prop.y + prop.sizeY;
		painter.Box(prop.x - 0.12f, prop.y - 0.12f, x1 + 0.12f, y1 + 0.12f,
			z - 0.22f, z, COL_CAP, depth);

		for (float x = prop.x; x < x1; x += BATTLEMENT_SPACING)
		{
			const float end = fminf(x + 0.62f, x1);
			painter.Box(x, prop.y, end, prop.y + 0.42f, z, z + BATTLEMENT_HEIGHT, COL_STONE, depth + 0.04f);
			painter.Box(x, y1 - 0.42f, end, y1, z, z + BATTLEMENT_HEIGHT, COL_STONE, depth + 0.06f);
		}

		for (float y = prop.y + BATTLEMENT_SPACING; y < y1 - 0.5f; y += BATTLEMENT_SPACING)
		{
			painter.Box(x1 - 0.42f, y, x1, y + 0.62f, z, z + BATTLEMENT_HEIGHT,
				COL_STONE, depth + 0.07f);
		}
	}

	void Vines(const Painter& painter, const Prop& prop, float top, float depth)
	{
		const float y = prop.y + prop.sizeY;
		const float x = prop.x + prop.sizeX * 0.16f;
		painter.Line({ x, y, top }, { x + 0.18f, y, top - 1.7f }, COL_MOSS, 2.2f, depth);

		for (int i = 0; i < 5; ++i)
		{
			const float z = top - 0.25f - i * 0.32f;
			const float dx = (i % 2 == 0 ? -1.0f : 1.0f) * 0.24f;
			const Point leaf[3] = { { x + 0.10f, y, z }, { x + dx, y, z + 0.12f }, { x + dx, y, z - 0.17f } };
			painter.Face(leaf, 3, Scale(COL_MOSS, 1.05f + i * 0.025f), depth + 0.01f);
		}
	}
}

float EnvironmentArt::StructureHeight(const Prop& prop)
{
	switch (prop.type)
	{
	case PROP_HOUSE:  return HOUSE_ROOF_HEIGHT + (prop.variant % 3) * 0.35f;
	case PROP_SHRINE: return SHRINE_ROOF_HEIGHT;
	case PROP_KEEP:   return KEEP_ROOF_HEIGHT;
	case PROP_TOWER:  return TOWER_HEIGHT + (prop.variant % 2) * 1.4f + BATTLEMENT_HEIGHT;
	case PROP_WALL:   return WALL_HEIGHT + BATTLEMENT_HEIGHT;
	case PROP_GATE:   return GATE_HEIGHT + BATTLEMENT_HEIGHT;
	case PROP_RUIN:   return RUIN_HEIGHT;
	case PROP_TORII:  return TORII_HEIGHT;
	default:         return 3.0f;
	}
}

float EnvironmentArt::OcclusionAlpha(const Renderer& renderer, const Prop& prop, float groundZ,
	float viewerX, float viewerY, float viewerZ)
{
	if (viewerX + viewerY >= prop.x + prop.y + prop.sizeX + prop.sizeY)
	{
		return 1.0f;
	}

	float px, py;
	renderer.WorldToScreen(viewerX, viewerY, viewerZ + 0.75f, &px, &py);
	float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;

	for (int i = 0; i < 8; ++i)
	{
		float x, y;
		renderer.WorldToScreen(prop.x + ((i & 1) ? prop.sizeX : 0.0f),
			prop.y + ((i & 2) ? prop.sizeY : 0.0f), groundZ + ((i & 4) ? StructureHeight(prop) : 0.0f), &x, &y);
		minX = fminf(minX, x);
		minY = fminf(minY, y);
		maxX = fmaxf(maxX, x);
		maxY = fmaxf(maxY, y);
	}

	return px > minX - 12.0f && px < maxX + 12.0f && py > minY - 12.0f && py < maxY + 12.0f
		? CUTAWAY_ALPHA : 1.0f;
}

void EnvironmentArt::DrawStructure(Renderer* renderer, const Lighting& lighting, const Prop& prop,
	float groundZ, float opacity)
{
	const Painter painter = { renderer, lighting, groundZ, opacity };
	const float x0 = prop.x;
	const float y0 = prop.y;
	const float x1 = x0 + prop.sizeX;
	const float y1 = y0 + prop.sizeY;
	const float depth = x1 + y1;
	const float cx = (x0 + x1) * 0.5f;
	const float cy = (y0 + y1) * 0.5f;
	float sx, sy;
	renderer->WorldToScreen(cx, cy, groundZ, &sx, &sy);
	renderer->PushSoftEllipse(sx + 24.0f, sy + 10.0f, (prop.sizeX + prop.sizeY) * 20.0f,
		(prop.sizeX + prop.sizeY) * 7.5f, RGBA(0.01f, 0.025f, 0.015f, 0.40f),
		VisualStyle::SHADOW_DEPTH + depth * 0.01f);

	if (prop.type == PROP_TORII)
	{
		const Color red = RGBA(0.60f, 0.32f, 0.23f);
		renderer->PushSoftEllipse(sx + 10.0f, sy + 5.0f, 45.0f, 15.0f,
			RGBA(0.0f, 0.02f, 0.01f, 0.3f), VisualStyle::SHADOW_DEPTH + depth * 0.01f);

		for (int i = 0; i < 2; ++i)
		{
			const float postX = x0 + (i == 0 ? -TORII_SPAN : TORII_SPAN);
			painter.Box(postX - StructureDimensions::TORII_BASE_HALF_WIDTH, y0 - StructureDimensions::TORII_BASE_HALF_DEPTH,
				postX + StructureDimensions::TORII_BASE_HALF_WIDTH, y0 + StructureDimensions::TORII_BASE_HALF_DEPTH,
				0.0f, 0.4f, COL_STONE, depth - 0.1f);
			painter.Box(postX - 0.11f, y0 - 0.15f, postX + 0.11f, y0 + 0.15f,
				0.4f, TORII_HEIGHT - 0.1f, red, depth);
		}

		painter.Box(x0 - TORII_SPAN - 0.25f, y0 - 0.15f, x0 + TORII_SPAN + 0.25f, y0 + 0.15f,
			2.35f, 2.56f, red, depth + 0.04f);
		painter.Box(x0 - TORII_SPAN - 0.4f, y0 - 0.23f, x0 + TORII_SPAN + 0.4f, y0 + 0.23f,
			3.0f, TORII_HEIGHT, red, depth + 0.06f);
		painter.Box(x0 - TORII_SPAN - 0.45f, y0 - 0.25f, x0 + TORII_SPAN + 0.45f, y0 + 0.25f,
			TORII_HEIGHT - 0.06f, TORII_HEIGHT, COL_ROOF, depth + 0.1f);
		return;
	}

	if (prop.type == PROP_RUIN)
	{
		painter.Box(x0, y0, x1, y1, 0.0f, 0.22f, Scale(COL_STONE, 0.68f), depth - 0.3f);
		painter.Box(x0, y1 - 0.8f, x0 + 0.65f, y1, 0.22f, RUIN_HEIGHT - 0.5f, COL_STONE, depth - 0.2f);
		painter.Box(x1 - 0.65f, y0, x1, y1, 0.22f, 2.6f, COL_STONE, depth - 0.19f);
		const float radius = fminf(prop.sizeX * 0.5f - 0.35f, 1.5f);
		const float inner = radius - 0.42f;
		const float spring = 2.2f;

		for (int i = 0; i < ARCH_SEGMENTS - 1; ++i)
		{
			const float a = PI * (float)i / ARCH_SEGMENTS;
			const float b = PI * (float)(i + 1) / ARCH_SEGMENTS;
			const Point voussoir[4] =
			{
				{ cx + cosf(a) * radius, y1, spring + sinf(a) * radius },
				{ cx + cosf(b) * radius, y1, spring + sinf(b) * radius },
				{ cx + cosf(b) * inner, y1, spring + sinf(b) * inner },
				{ cx + cosf(a) * inner, y1, spring + sinf(a) * inner }
			};
			painter.Face(voussoir, 4, Scale(COL_CAP, 0.90f + (i % 2) * 0.12f), depth - 0.15f);
		}

		painter.Box(x0 + 0.8f, y0 + 0.7f, x0 + 1.65f, y0 + 1.7f, 0.22f, 1.0f, COL_STONE, depth - 0.1f);
		painter.Box(x0 + 1.45f, y1 - 0.8f, x0 + 2.25f, y1 - 0.15f, 0.22f, 0.8f, COL_CAP, depth - 0.09f);
		Vines(painter, prop, 3.1f, depth - 0.04f);
		return;
	}

	if (prop.type == PROP_GATE)
	{
		painter.Box(x0, y0, x0 + StructureDimensions::GATE_PIER_WIDTH, y1,
			0.0f, GATE_HEIGHT, COL_STONE, depth - 0.10f);
		painter.Box(x1 - StructureDimensions::GATE_PIER_WIDTH, y0, x1, y1,
			0.0f, GATE_HEIGHT, COL_STONE, depth - 0.09f);
		painter.Box(x0, y0, x1, y1, GATE_HEIGHT - 1.2f, GATE_HEIGHT, COL_STONE, depth);
		Crenellations(painter, prop, GATE_HEIGHT, depth + 0.05f);
		Vines(painter, prop, GATE_HEIGHT - 0.5f, depth + 0.15f);
		return;
	}

	float wallTop = HOUSE_WALL_HEIGHT + (prop.variant % 3) * 0.35f;
	float roofTop = StructureHeight(prop);
	bool roof = true;

	switch (prop.type)
	{
	case PROP_SHRINE: wallTop = SHRINE_WALL_HEIGHT; break;
	case PROP_KEEP:   wallTop = KEEP_WALL_HEIGHT; break;
	case PROP_TOWER:  wallTop = roofTop - BATTLEMENT_HEIGHT; roof = false; break;
	case PROP_WALL:   wallTop = WALL_HEIGHT; roof = false; break;
	default: break;
	}

	painter.Box(x0 - 0.18f, y0 - 0.18f, x1 + 0.18f, y1 + 0.18f,
		0.0f, FOUNDATION_HEIGHT, Scale(COL_STONE, 0.72f), depth - 0.3f);
	painter.Box(x0, y0, x1, y1, FOUNDATION_HEIGHT, wallTop, COL_STONE, depth - 0.2f);
	Masonry(painter, prop, wallTop, depth - 0.15f);

	// Quoins, a projecting cornice and a dark reveal separate the visible planes.
	painter.Box(x1 - 0.22f, y1 - 0.22f, x1 + 0.04f, y1 + 0.04f,
		FOUNDATION_HEIGHT, wallTop, COL_CAP, depth - 0.13f);
	painter.Box(x0 - 0.12f, y0 - 0.12f, x1 + 0.12f, y1 + 0.12f,
		wallTop - 0.18f, wallTop, COL_CAP, depth - 0.12f);
	Vines(painter, prop, wallTop - 0.3f, depth - 0.09f);

	if (prop.type != PROP_WALL)
	{
		const int floors = prop.type == PROP_KEEP || prop.type == PROP_TOWER ? 3 : 1;

		for (int floor = 0; floor < floors; ++floor)
		{
			const float bottom = 1.10f + floor * 2.0f;

			for (float windowX = x0 + 0.7f; windowX < x1 - 0.35f; windowX += 1.4f)
			{
				painter.Arch(windowX, y1 + 0.015f, WINDOW_WIDTH + 0.20f, bottom - 0.10f,
					bottom + WINDOW_HEIGHT, COL_CAP, depth - 0.08f);
				painter.Arch(windowX, y1 + 0.025f, WINDOW_WIDTH, bottom,
					bottom + WINDOW_HEIGHT - 0.10f, COL_RECESS, depth - 0.07f);
				painter.Box(windowX - 0.35f, y1, windowX + 0.35f, y1 + 0.18f,
					bottom - 0.12f, bottom, COL_CAP, depth - 0.06f);
			}
		}

		painter.Arch(cx, y1 + 0.03f, 1.15f, FOUNDATION_HEIGHT,
			1.8f, COL_CAP, depth - 0.055f);
		painter.Arch(cx, y1 + 0.04f, 0.85f, FOUNDATION_HEIGHT,
			1.68f, COL_RECESS, depth - 0.05f);
		painter.South(cx - 0.36f, cx + 0.36f, y1 + 0.045f,
			FOUNDATION_HEIGHT, 1.65f, COL_WOOD, depth - 0.04f);

		if (prop.type == PROP_HOUSE || prop.type == PROP_SHRINE)
		{
			const Point window[4] = { { x1 + 0.01f, y0 + 0.65f, 1.15f }, { x1 + 0.01f, y0 + 1.5f, 1.15f },
				{ x1 + 0.01f, y0 + 1.5f, 2.1f }, { x1 + 0.01f, y0 + 0.65f, 2.1f } };
			painter.Face(window, 4, RGBA(0.93f, 0.72f, 0.39f), depth - 0.03f, true);
			painter.East(x1 + 0.02f, y0 + 1.03f, y0 + 1.1f, 1.15f, 2.1f, COL_WOOD, depth - 0.02f);
		}
	}

	if (roof)
	{
		Roof(painter, prop, wallTop + 0.15f, roofTop, depth + 0.10f);
	}
	else
	{
		Crenellations(painter, prop, wallTop, depth + 0.10f);
	}
}
