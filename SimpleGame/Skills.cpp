#include "stdafx.h"
#include "Skills.h"

#include <algorithm>

namespace
{
	const SkillDefinition DEFINITIONS[SKILL_COUNT] = {
		{ "skill_combo",     1, 'q', -1, 0, 4.0f,  1.20f, 0.20f, 2.4f, 0.0f },
		{ "skill_breaker",   1, 'r', -1, 0, 7.0f,  1.80f, 0.30f, 7.0f, 0.0f },
		{ "skill_ward",      1, 'f', -1, 0, 12.0f, 0.24f, 0.04f, 0.0f, 5.0f },
		{ "skill_mastery",   1, 0,   -1, 0, 0.0f,  0.06f, 0.06f, 0.0f, 0.0f },
		{ "skill_fleet",     1, 0,   -1, 0, 0.0f,  0.04f, 0.04f, 0.0f, 0.0f },
		{ "skill_vitality",  1, 0,   -1, 0, 0.0f,  12.0f, 12.0f, 0.0f, 0.0f },
		{ "skill_barrage",   2, 'z', SKILL_COMBO,    3, 9.0f,  2.00f, 0.35f, 3.6f, 0.0f },
		{ "skill_shadow",    2, 'x', SKILL_WARD,     3, 6.0f,  0.35f, 0.08f, 4.5f, 0.0f },
		{ "skill_storm",     2, 'v', SKILL_BREAKER,  3, 18.0f, 1.00f, 0.20f, 3.8f, 2.0f },
		{ "skill_precision", 2, 0,   SKILL_MASTERY,  3, 0.0f,  0.03f, 0.03f, 0.0f, 0.0f },
		{ "skill_leech",     2, 0,   SKILL_VITALITY, 3, 0.0f,  0.02f, 0.02f, 0.0f, 0.0f },
		{ "skill_focus",     2, 0,   SKILL_FLEET,    3, 0.0f,  0.05f, 0.05f, 0.0f, 0.0f }
	};
}

const SkillDefinition& GetSkillDefinition(int index)
{
	return DEFINITIONS[std::max(0, std::min(SKILL_COUNT - 1, index))];
}

int SkillForKey(unsigned char key)
{
	for (int i = 0; i < SKILL_COUNT; ++i)
	{
		if (DEFINITIONS[i].Active() && DEFINITIONS[i].key == key)
		{
			return i;
		}
	}

	return -1;
}

void SkillBook::Reset()
{
	m_Ranks.fill(0);
	m_Points = 3;
	m_Tier = 1;
	m_Level = 1;
}

void SkillBook::GrantLevel(int level)
{
	level = std::max(1, std::min(20, level));

	if (level > m_Level)
	{
		m_Points += (level - m_Level) * 3;
		m_Level = level;
	}
}

int SkillBook::Rank(int index) const
{
	return index >= 0 && index < SKILL_COUNT ? m_Ranks[index] : 0;
}

int SkillBook::Spent(int tier) const
{
	int result = 0;

	for (int i = 0; i < SKILL_COUNT; ++i)
	{
		if (DEFINITIONS[i].tier == tier)
		{
			result += m_Ranks[i];
		}
	}

	return result;
}

bool SkillBook::Unlocked(int index) const
{
	if (index < 0 || index >= SKILL_COUNT)
	{
		return false;
	}

	const SkillDefinition& skill = DEFINITIONS[index];
	return skill.tier <= m_Tier && (skill.prerequisite < 0 || Rank(skill.prerequisite) >= skill.requiredRank);
}

bool SkillBook::CanLearn(int index) const
{
	return Unlocked(index) && m_Points > 0 && Rank(index) < MAX_RANK;
}

bool SkillBook::Learn(int index)
{
	if (!CanLearn(index))
	{
		return false;
	}

	++m_Ranks[index];
	--m_Points;
	return true;
}

bool SkillBook::CanAdvance() const
{
	return m_Tier == 1 && m_Level >= ADVANCE_LEVEL && Spent(1) >= ADVANCE_SP;
}

bool SkillBook::Advance()
{
	if (!CanAdvance())
	{
		return false;
	}

	m_Tier = 2;
	return true;
}

float SkillBook::Value(int index) const
{
	return GetSkillDefinition(index).Value(Rank(index));
}

float SkillBook::Cooldown(int index) const
{
	return index >= 0 && index < SKILL_COUNT
		? GetSkillDefinition(index).cooldown * (1.0f - Value(SKILL_FOCUS)) : 0.0f;
}
