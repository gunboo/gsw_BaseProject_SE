#pragma once

#include "Actor.h"
#include "World.h"
#include <vector>

class ModelLibrary;
class Renderer;

// Cosmetic state is bounded and uses its own RNG, independent of combat rolls.
class SkillVfx
{
public:
	void Initialize(const ModelLibrary* models);
	void Clear();
	void Update(float seconds);
	void Slash(const ActorPosition& center, float x, float y, float radius);
	void Dash(const ActorPosition& from, const ActorPosition& to, bool shadow, bool bow);
	void Ray(const ActorPosition& from, const ActorPosition& to, bool bow, bool pierce);
	void Area(const ActorPosition& center, float radius, bool bow, bool storm);
	void Ward(const ActorPosition& center, bool bow);
	void Impact(const ActorPosition& position, bool critical, bool bow);
	void Draw(Renderer& renderer, const ActorPosition& player, float ward, bool invulnerable) const;

private:
	enum Kind { SLASH, BLADE_WAVE, ARROW, RAIN, SPIN, STORM, GHOST, WARD, IMPACT };
	struct Effect
	{
		Kind kind;
		ActorPosition from, to;
		float radius, angle, remaining, duration;
		bool bow, strong;
	};
	struct Fragment
	{
		ActorPosition origin, velocity;
		float remaining, duration, angle, size;
		bool bow, violet;
	};
	void Add(Kind kind, const ActorPosition& from, const ActorPosition& to, float radius,
		float angle, float duration, bool bow, bool strong = false, float delay = 0.0f);
	void Fragments(const ActorPosition& origin, int count, float speed, bool bow, bool violet = false);
	void DrawEffect(Renderer& renderer, const Effect& effect) const;
	const ModelLibrary* m_Models = NULL;
	std::vector<Effect> m_Effects;
	std::vector<Fragment> m_Fragments;
	Rng m_VisualRng = { 0xB17F39u };
	float m_Time = 0.0f;
};
