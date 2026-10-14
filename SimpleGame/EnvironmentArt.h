#pragma once

#include "Lighting.h"
#include "World.h"

namespace EnvironmentArt
{
	float StructureHeight(const Prop& prop);
	float OcclusionAlpha(const Renderer& renderer, const Prop& prop, float groundZ,
		float viewerX, float viewerY, float viewerZ);
	void DrawStructure(Renderer* renderer, const Lighting& lighting, const Prop& prop,
		float groundZ, float opacity);
}
