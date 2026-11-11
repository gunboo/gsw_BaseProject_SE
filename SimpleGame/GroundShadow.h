#pragma once

#include "Lighting.h"
#include "Model.h"
#include "SceneGraph.h"
#include "World.h"

namespace GroundShadow
{
	// Static receiver patches are prepared once and culled independently of the caster.
	void CreateStructure(SceneGraph& graph, Actor* caster, const World& world,
		const Lighting& lighting, const Prop& prop);
	void CreateTree(SceneGraph& graph, Actor* caster, const World& world,
		const Lighting& lighting, const Model& model);
}
