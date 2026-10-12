#pragma once

#include "SceneGraph.h"
#include "World.h"
#include <array>
#include <functional>

enum WeaponKind
{
	WEAPON_SWORD,
	WEAPON_BOW
};

// Fixed actor pool: arrows reuse cached line primitives and never create meshes.
class Bow
{
public:
	void Initialize(SceneGraph& graph, Actor* parent, World& world, size_t targetCount,
		std::function<Actor*(size_t)> target, std::function<void(size_t, float)> hit);
	bool Shoot(const ActorPosition& origin, float damage);
	void Clear();
	static void DrawEquipped(Renderer& renderer, const ActorPosition& position);

private:
	static const size_t ARROW_COUNT = 16;
	struct Arrow
	{
		Actor* actor = NULL;
		float dx = 0.0f;
		float dy = 0.0f;
		float remaining = 0.0f;
		float damage = 0.0f;
	};

	bool ClearShot(const ActorPosition& origin, const ActorPosition& target) const;
	void Update(size_t index, float deltaSeconds);
	void Draw(size_t index, Renderer& renderer);
	void Stop(Arrow& arrow);
	std::array<Arrow, ARROW_COUNT> m_Arrows;
	World* m_World = NULL;
	size_t m_TargetCount = 0;
	std::function<Actor*(size_t)> m_Target;
	std::function<void(size_t, float)> m_Hit;
};
