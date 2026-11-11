#include "stdafx.h"
#include "Lighting.h"

#include <cmath>

namespace
{
	const float LIGHT_CELL_SIZE = 8.0f;
	float Clamp(float value, float low, float high)
	{
		if (value < low)
		{
			return low;
		}

		if (value > high)
		{
			return high;
		}

		return value;
	}
}

Lighting::Lighting()
	: m_FogNear(12.0f)
	, m_FogFar(26.0f)
	, m_FogMax(0.72f)
	, m_ViewerX(0.0f)
	, m_ViewerY(0.0f)
	, m_Time(0.0f)
{
	m_Ambient = RGBA(0.72f, 0.77f, 0.66f);
	m_FogColor = RGBA(0.10f, 0.15f, 0.14f);
}

void Lighting::Clear()
{
	m_Lights.clear();
	m_CellsDirty = m_FlickerDirty = true;
}

int Lighting::AddLight(float x, float y, float radius, float intensity, const Color& color, bool flicker)
{
	LightSource light;
	light.x = x;
	light.y = y;
	light.radius = radius;
	light.intensity = intensity;
	light.color = color;
	light.flicker = flicker;

	m_Lights.push_back(light);
	m_CellsDirty = m_FlickerDirty = true;

	return (int)m_Lights.size() - 1;
}

void Lighting::MoveLight(int index, float x, float y)
{
	if (index < 0 || index >= (int)m_Lights.size())
	{
		return;
	}

	m_Lights[index].x = x;
	m_Lights[index].y = y;
	m_CellsDirty = true;
}

void Lighting::SetViewer(float worldX, float worldY)
{
	m_ViewerX = worldX;
	m_ViewerY = worldY;
}

void Lighting::SetTime(float seconds)
{
	m_FlickerDirty = m_FlickerDirty || m_Time != seconds;
	m_Time = seconds;
}

unsigned int Lighting::CellBucket(int x, int y)
{
	return (static_cast<unsigned int>(x) * 73856093u
		^ static_cast<unsigned int>(y) * 19349663u) & (LIGHT_BUCKET_COUNT - 1);
}

void Lighting::PrepareSamples() const
{
	const LightSource* lights = m_Lights.data();
	const size_t count = m_Lights.size();

	if (m_CellsDirty)
	{
		for (std::vector<LightCell>& cell : m_LightCells)
		{
			cell.clear();
		}

		// Keep source order within each cell so floating-point accumulation is unchanged.
		for (size_t i = 0; i < count; ++i)
		{
			const LightSource& light = lights[i];
			const float radius = fabsf(light.radius);
			const int minX = static_cast<int>(floorf((light.x - radius) / LIGHT_CELL_SIZE));
			const int minY = static_cast<int>(floorf((light.y - radius) / LIGHT_CELL_SIZE));
			const int maxX = static_cast<int>(floorf((light.x + radius) / LIGHT_CELL_SIZE));
			const int maxY = static_cast<int>(floorf((light.y + radius) / LIGHT_CELL_SIZE));

			for (int y = minY; y <= maxY; ++y)
			{
				for (int x = minX; x <= maxX; ++x)
				{
					m_LightCells[CellBucket(x, y)].push_back({ x, y, static_cast<int>(i) });
				}
			}
		}

		m_CellsDirty = false;
	}

	if (m_FlickerDirty)
	{
		m_Flicker.resize(count);
		float* flicker = m_Flicker.data();

		for (size_t i = 0; i < count; ++i)
		{
			flicker[i] = lights[i].flicker
				? 1.0f + 0.09f * sinf(m_Time * 6.3f + (float)i * 2.1f)
					+ 0.05f * sinf(m_Time * 11.7f + (float)i) : 1.0f;
		}

		m_FlickerDirty = false;
	}
}

void Lighting::SetAmbient(const Color& ambient)
{
	m_Ambient = ambient;
}

void Lighting::SetFog(const Color& color, float nearDistance, float farDistance, float maximum)
{
	m_FogColor = color;
	m_FogNear = nearDistance;
	m_FogFar = farDistance;
	m_FogMax = maximum;
}

float Lighting::FogFactor(float worldX, float worldY) const
{
	const float dx = worldX - m_ViewerX;
	const float dy = worldY - m_ViewerY;
	const float distance = sqrtf(dx * dx + dy * dy);
	const float t = Clamp((distance - m_FogNear) / (m_FogFar - m_FogNear), 0.0f, 1.0f);

	return t * t * (3.0f - 2.0f * t) * m_FogMax;
}

Color Lighting::SampleLight(float worldX, float worldY) const
{
	PrepareSamples();
	Color multiplier = m_Ambient;
	const int cellX = static_cast<int>(floorf(worldX / LIGHT_CELL_SIZE));
	const int cellY = static_cast<int>(floorf(worldY / LIGHT_CELL_SIZE));
	const std::vector<LightCell>& candidates = m_LightCells[CellBucket(cellX, cellY)];
	const LightCell* cells = candidates.data();
	const LightSource* lights = m_Lights.data();
	const float* flicker = m_Flicker.data();

	for (size_t i = 0; i < candidates.size(); ++i)
	{
		if (cells[i].x != cellX || cells[i].y != cellY)
		{
			continue;
		}

		const int source = cells[i].source;
		const LightSource& light = lights[source];
		const float dx = worldX - light.x;
		const float dy = worldY - light.y;
		const float distanceSq = dx * dx + dy * dy;

		if (distanceSq >= light.radius * light.radius)
		{
			continue;
		}

		float falloff = 1.0f - sqrtf(distanceSq) / light.radius;
		falloff *= falloff;

		float amount = falloff * light.intensity;

		amount *= flicker[source];

		multiplier.r += light.color.r * amount;
		multiplier.g += light.color.g * amount;
		multiplier.b += light.color.b * amount;
	}

	multiplier.r = Clamp(multiplier.r, 0.0f, 1.0f);
	multiplier.g = Clamp(multiplier.g, 0.0f, 1.0f);
	multiplier.b = Clamp(multiplier.b, 0.0f, 1.0f);
	multiplier.a = 1.0f;

	return multiplier;
}

Color Lighting::Apply(const Color& base, float worldX, float worldY) const
{
	Color lit = Modulate(base, SampleLight(worldX, worldY));
	lit.a = base.a;

	Color fogged = Mix(lit, m_FogColor, FogFactor(worldX, worldY));
	fogged.a = base.a;

	return fogged;
}

ShadeParams Lighting::Shade(float worldX, float worldY, const Color& tint) const
{
	ShadeParams shade;
	shade.light = SampleLight(worldX, worldY);
	shade.fogColor = m_FogColor;
	shade.fog = FogFactor(worldX, worldY);
	shade.tint = tint;

	return shade;
}
