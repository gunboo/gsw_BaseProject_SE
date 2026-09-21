#pragma once

#include <vector>

#include "Renderer.h"

struct LightSource
{
	float x;
	float y;
	float radius;
	float intensity;
	Color color;
	bool flicker;
};

// Night ambient, lantern pools and distance fog, shared by every scene.
//
// Light is kept as a MULTIPLIER rather than a finished colour. That is what
// lets a cached model be shaded once per instance instead of once per polygon:
// evaluate the multiplier at the object anchor, then modulate each baked colour.
class Lighting
{
public:
	Lighting();

	void Clear();
	int AddLight(float x, float y, float radius, float intensity, const Color& color, bool flicker);
	void MoveLight(int index, float x, float y);
	int GetLightCount() const { return (int)m_Lights.size(); }
	const LightSource& GetLight(int index) const { return m_Lights[index]; }

	void SetViewer(float worldX, float worldY);
	void SetTime(float seconds);
	void SetAmbient(const Color& ambient);
	void SetFog(const Color& color, float nearDistance, float farDistance, float maximum);

	const Color& GetFogColor() const { return m_FogColor; }

	Color SampleLight(float worldX, float worldY) const;
	float FogFactor(float worldX, float worldY) const;

	// Convenience for code that still shades one polygon at a time.
	Color Apply(const Color& base, float worldX, float worldY) const;

	// Everything a model instance needs, in one evaluation.
	ShadeParams Shade(float worldX, float worldY, const Color& tint) const;

private:
	std::vector<LightSource> m_Lights;

	Color m_Ambient;
	Color m_FogColor;
	float m_FogNear;
	float m_FogFar;
	float m_FogMax;

	float m_ViewerX;
	float m_ViewerY;
	float m_Time;
};
