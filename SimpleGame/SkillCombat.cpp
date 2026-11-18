#include "stdafx.h"
#include "SkillCombat.h"
#include "Renderer.h"
#include "AnalysisLog.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
	const float SAMPLE_STEP = 0.12f;
	const float PLAYER_RADIUS = 0.28f;
}

void SkillCombat::Initialize(SceneGraph& graph, Actor* parent, Actor* player, World& world, PlayerStats& stats,
	size_t targetCount, std::function<Actor*(size_t)> target, std::function<void(size_t, float, bool)> hit, float damageScale, const ModelLibrary* models)
{
	m_World = &world;
	m_Player = player;
	m_Stats = &stats;
	m_TargetCount = targetCount;
	m_Target = target;
	m_Hit = hit;
	m_DamageScale = damageScale;
	m_Cooldowns.fill(0.0f);
	m_Vfx.Initialize(models);
	ClearEffects();
	Actor* effect = graph.CreateActor("skill_effects", parent);
	effect->SetRenderCallback([this](const Actor&, Renderer& renderer) { Draw(renderer); });
}

void SkillCombat::ClearEffects()
{
	// Cooldowns survive death; respawning cannot reset powerful skills.
	m_Ward = m_Invulnerable = m_Recovery = m_PulseTimer = 0.0f;
	m_Pulses = 0;
	m_Vfx.Clear();
}

float SkillCombat::Remaining(int skill) const
{
	return skill >= 0 && skill < SKILL_COUNT ? m_Cooldowns[skill] : 0.0f;
}

float SkillCombat::IncomingMultiplier() const
{
	return m_Ward > 0.0f ? 1.0f - m_Stats->skills.Value(SKILL_WARD) : 1.0f;
}

bool SkillCombat::RollCritical()
{
	return m_Rng.NextFloat() < 0.12f + m_Stats->skills.Value(SKILL_PRECISION);
}

void SkillCombat::Hit(size_t index, float damage, bool bow)
{
	Actor* target = m_Target(index);

	if (target == NULL)
	{
		return;
	}

	const bool critical = RollCritical();
	// Capture the hit before the callback can kill, hide or move the target.
	Impact(target->GetWorldPosition(), critical, bow);
	m_Hit(index, damage * (critical ? 1.8f : 1.0f), critical);
}

void SkillCombat::Impact(const ActorPosition& position, bool critical, bool bow)
{
	m_Vfx.Impact(position, critical, bow);
}

bool SkillCombat::ClearLine(const ActorPosition& from, const ActorPosition& to) const
{
	const float dx = to.x - from.x, dy = to.y - from.y;
	const int steps = std::max(1, static_cast<int>(std::ceil(std::sqrt(dx * dx + dy * dy) / SAMPLE_STEP)));

	for (int i = 1; i <= steps; ++i)
	{
		const float t = static_cast<float>(i) / steps;

		if (m_World->IsBlocked(from.x + dx * t, from.y + dy * t, 0.03f))
		{
			return false;
		}
	}

	return std::fabs(from.z - to.z) < 2.5f;
}

ActorPosition SkillCombat::Aim(float x, float y, float range) const
{
	const ActorPosition origin = m_Player->GetWorldPosition();
	ActorPosition result = origin;
	float nearest = range * range;
	bool found = false;

	for (size_t i = 0; i < m_TargetCount; ++i)
	{
		const Actor* target = m_Target(i);

		if (target == NULL)
		{
			continue;
		}

		const ActorPosition position = target->GetWorldPosition();
		const float dx = position.x - origin.x, dy = position.y - origin.y;
		const float distance = dx * dx + dy * dy;

		if (distance < nearest && ClearLine(origin, position))
		{
			nearest = distance;
			result = position;
			found = true;
		}
	}

	if (!found)
	{
		for (float step = SAMPLE_STEP; step <= range; step += SAMPLE_STEP)
		{
			const float px = origin.x + x * step, py = origin.y + y * step;

			if (m_World->IsBlocked(px, py, 0.03f))
			{
				break;
			}

			result = { px, py, m_World->GetElevation(px, py) };
		}
	}

	return result;
}

void SkillCombat::Dash(float x, float y, float range, bool shadow, bool bow)
{
	const ActorPosition from = m_Player->GetWorldPosition();
	ActorPosition to = from;

	for (float remaining = range; remaining > 0.0f; remaining -= SAMPLE_STEP)
	{
		const float step = std::min(remaining, SAMPLE_STEP);
		const float px = to.x + x * step, py = to.y + y * step;

		if (!m_World->CanMove(to.x, to.y, px, py, PLAYER_RADIUS))
		{
			break;
		}

		to = { px, py, m_World->GetElevation(px, py) };
	}

	const ActorPosition local = m_Player->GetPosition();
	m_Player->SetPosition(local.x + to.x - from.x, local.y + to.y - from.y, local.z + to.z - from.z);
	m_Vfx.Dash(from, to, shadow, bow);
}

void SkillCombat::AreaHit(const ActorPosition& center, float radius, float damage, bool bow)
{
	for (size_t i = 0; i < m_TargetCount; ++i)
	{
		const Actor* target = m_Target(i);

		if (target == NULL)
		{
			continue;
		}

		const ActorPosition position = target->GetWorldPosition();
		const float dx = position.x - center.x, dy = position.y - center.y;

		if (dx * dx + dy * dy <= radius * radius && ClearLine(center, position))
		{
			Hit(i, damage, bow);
		}
	}
}

void SkillCombat::RayHit(float x, float y, float range, float damage, bool pierce, bool bow)
{
	const ActorPosition from = m_Player->GetWorldPosition();
	ActorPosition to = from;
	float length = 0.0f;

	for (float step = SAMPLE_STEP; step <= range; step += SAMPLE_STEP)
	{
		const float px = from.x + x * step, py = from.y + y * step;

		if (m_World->IsBlocked(px, py, 0.03f))
		{
			break;
		}

		length = step;
		to = { px, py, m_World->GetElevation(px, py) };
	}

	std::vector<std::pair<float, size_t>> hits;

	for (size_t i = 0; i < m_TargetCount; ++i)
	{
		const Actor* target = m_Target(i);

		if (target == NULL)
		{
			continue;
		}

		const ActorPosition position = target->GetWorldPosition();
		const float dx = position.x - from.x, dy = position.y - from.y;
		const float along = dx * x + dy * y;
		const float across = std::fabs(dx * y - dy * x);

		if (along >= 0.0f && along <= length && across < 0.65f && ClearLine(from, position))
		{
			hits.push_back({ along, i });
		}
	}

	std::sort(hits.begin(), hits.end());

	for (const auto& hit : hits)
	{
		Hit(hit.second, damage, bow);

		if (!pierce)
		{
			to = { from.x + x * hit.first, from.y + y * hit.first, from.z };
			break;
		}
	}

	m_Vfx.Ray(from, to, bow, pierce);
}

bool SkillCombat::Cast(int skill, bool bow, float facingX, float facingY)
{
	if (m_Stats == NULL || skill < 0 || skill >= SKILL_COUNT || !GetSkillDefinition(skill).Active()
		|| !std::isfinite(facingX) || !std::isfinite(facingY)
		|| m_Stats->skills.Rank(skill) == 0 || !m_Stats->skills.Unlocked(skill)
		|| Remaining(skill) > 0.0f || Busy() || m_Stats->health <= 0.0f)
	{
		return false;
	}

	float length = std::sqrt(facingX * facingX + facingY * facingY);

	if (length < 0.001f)
	{
		facingX = facingY = 0.707106f;
	}
	else
	{
		facingX /= length;
		facingY /= length;
	}

	const SkillDefinition& definition = GetSkillDefinition(skill);
	const float value = m_Stats->skills.Value(skill);
	const float damage = m_Stats->AttackPower() * m_DamageScale * value;
	const ActorPosition origin = m_Player->GetWorldPosition();
	const ActorPosition aim = bow ? Aim(facingX, facingY, 8.0f) : origin;

	if (bow && skill != SKILL_SHADOW)
	{
		const float dx = aim.x - origin.x, dy = aim.y - origin.y;
		length = std::sqrt(dx * dx + dy * dy);

		if (length > 0.001f)
		{
			facingX = dx / length;
			facingY = dy / length;
		}
	}

	m_Cooldowns[skill] = m_Stats->skills.Cooldown(skill);
	m_Recovery = 0.35f;

	switch (skill)
	{
	case SKILL_COMBO:
		if (bow)
		{
			for (int i = -1; i <= 1; ++i)
			{
				const float angle = i * 0.13f;
				RayHit(facingX * std::cos(angle) - facingY * std::sin(angle),
					facingX * std::sin(angle) + facingY * std::cos(angle), 8.0f, damage * 0.65f, false, true);
			}
		}
		else
		{
			Dash(facingX, facingY, definition.range, false, false);
			AreaHit(m_Player->GetWorldPosition(), 2.0f, damage);
			m_Vfx.Slash(m_Player->GetWorldPosition(), facingX, facingY, 2.0f);
		}
		break;
	case SKILL_BREAKER:
		RayHit(facingX, facingY, bow ? definition.range : 3.2f, damage, true, bow);
		break;
	case SKILL_WARD:
		m_Ward = definition.duration;
		m_Vfx.Ward(origin, bow);
		break;
	case SKILL_BARRAGE:
		AreaHit(aim, definition.range, damage, bow);
		m_Vfx.Area(aim, definition.range, bow, false);
		break;
	case SKILL_SHADOW:
		Dash(facingX, facingY, definition.range, true, bow);
		m_Invulnerable = value;
		break;
	case SKILL_STORM:
		m_StormCenter = aim;
		m_StormDamage = damage;
		m_StormRain = bow;
		m_Pulses = 2;
		m_PulseTimer = definition.duration * 0.5f;
		AreaHit(m_StormCenter, definition.range, damage, bow);
		m_Vfx.Area(m_StormCenter, definition.range, bow, true);
		break;
	default: break;
	}

	AnalysisLog::Get().Event("skill.cast", std::string(definition.textKey) + (bow ? ";bow" : ";sword"));
	return true;
}

void SkillCombat::Update(float seconds)
{
	if (!std::isfinite(seconds) || seconds <= 0.0f)
	{
		return;
	}

	for (float& cooldown : m_Cooldowns)
	{
		cooldown = std::max(0.0f, cooldown - seconds);
	}

	m_Ward = std::max(0.0f, m_Ward - seconds);
	m_Invulnerable = std::max(0.0f, m_Invulnerable - seconds);
	m_Recovery = std::max(0.0f, m_Recovery - seconds);

	m_Vfx.Update(seconds);

	if (m_Pulses > 0)
	{
		m_PulseTimer -= seconds;

		while (m_Pulses > 0 && m_PulseTimer <= 0.0f)
		{
			--m_Pulses;
			m_PulseTimer += GetSkillDefinition(SKILL_STORM).duration * 0.5f;
			AreaHit(m_StormCenter, GetSkillDefinition(SKILL_STORM).range, m_StormDamage, m_StormRain);
			m_Vfx.Area(m_StormCenter, GetSkillDefinition(SKILL_STORM).range, m_StormRain, true);
		}
	}
}

void SkillCombat::Draw(Renderer& renderer)
{
	m_Vfx.Draw(renderer, m_Player->GetWorldPosition(), m_Ward, Invulnerable());
}
