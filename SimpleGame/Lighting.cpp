#include "stdafx.h"
#include "Lighting.h"

#include <cmath>

namespace
{
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
	: m_FogNear(8.5f)
	, m_FogFar(19.0f)
	, m_FogMax(0.90f)
	, m_ViewerX(0.0f)
	, m_ViewerY(0.0f)
	, m_Time(0.0f)
{
	m_Ambient = RGBA(0.38f, 0.45f, 0.62f);
	m_FogColor = RGBA(0.055f, 0.075f, 0.105f);
}

void Lighting::Clear()
{
	m_Lights.clear();
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
}

void Lighting::SetViewer(float worldX, float worldY)
{
	m_ViewerX = worldX;
	m_ViewerY = worldY;
}

void Lighting::SetTime(float seconds)
{
	m_Time = seconds;
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
	const float t = (distance - m_FogNear) / (m_FogFar - m_FogNear);

	return Clamp(t, 0.0f, 1.0f) * m_FogMax;
}

Color Lighting::SampleLight(float worldX, float worldY) const
{
	Color multiplier = m_Ambient;

	for (size_t i = 0; i < m_Lights.size(); ++i)
	{
		const LightSource& light = m_Lights[i];
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

		if (light.flicker)
		{
			// The flame itself flickers on the GPU, but the light it throws on
			// everything else has to be computed here.
			amount *= 1.0f + 0.09f * sinf(m_Time * 6.3f + (float)i * 2.1f)
				+ 0.05f * sinf(m_Time * 11.7f + (float)i);
		}

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
