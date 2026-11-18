#pragma once

#include "Bow.h"
#include "Stats.h"
#include "SkillVfx.h"

class SkillCombat
{
public:
	void Initialize(SceneGraph& graph, Actor* parent, Actor* player, World& world, PlayerStats& stats,
		size_t targetCount, std::function<Actor*(size_t)> target, std::function<void(size_t, float, bool)> hit,
		float damageScale = 1.0f, const ModelLibrary* models = NULL);
	bool Cast(int skill, bool bow, float facingX, float facingY);
	void Update(float seconds);
	void ClearEffects();
	float Remaining(int skill) const;
	float WardRemaining() const { return m_Ward; }
	bool Invulnerable() const { return m_Invulnerable > 0.0f; }
	float IncomingMultiplier() const;
	bool Busy() const { return m_Recovery > 0.0f; }
	bool RollCritical();
	// Also used by normal projectiles so the precision passive affects both weapons.
	void Hit(size_t index, float damage, bool bow = false);
	void Impact(const ActorPosition& position, bool critical, bool bow = false);

private:
	void Draw(Renderer& renderer);
	bool ClearLine(const ActorPosition& from, const ActorPosition& to) const;
	ActorPosition Aim(float x, float y, float range) const;
	void Dash(float x, float y, float range, bool shadow, bool bow);
	void AreaHit(const ActorPosition& center, float radius, float damage, bool bow = false);
	void RayHit(float x, float y, float range, float damage, bool pierce, bool bow);
	World* m_World = NULL;
	Actor* m_Player = NULL;
	PlayerStats* m_Stats = NULL;
	size_t m_TargetCount = 0;
	std::function<Actor*(size_t)> m_Target;
	std::function<void(size_t, float, bool)> m_Hit;
	std::array<float, SKILL_COUNT> m_Cooldowns = {};
	SkillVfx m_Vfx;
	Rng m_Rng = { 0xA421u };
	float m_Ward = 0.0f;
	float m_Invulnerable = 0.0f;
	float m_Recovery = 0.0f;
	float m_PulseTimer = 0.0f;
	float m_StormDamage = 0.0f;
	float m_DamageScale = 1.0f;
	bool m_StormRain = false;
	int m_Pulses = 0;
	ActorPosition m_StormCenter = {};
};
