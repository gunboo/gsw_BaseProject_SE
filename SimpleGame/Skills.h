#pragma once

#include <array>

enum SkillId
{
	SKILL_COMBO = 0,
	SKILL_BREAKER,
	SKILL_WARD,
	SKILL_MASTERY,
	SKILL_FLEET,
	SKILL_VITALITY,
	SKILL_BARRAGE,
	SKILL_SHADOW,
	SKILL_STORM,
	SKILL_PRECISION,
	SKILL_LEECH,
	SKILL_FOCUS,
	SKILL_COUNT
};

struct SkillDefinition
{
	const char* textKey;
	int tier;
	unsigned char key;
	int prerequisite;
	int requiredRank;
	float cooldown;
	float baseValue;
	float perRank;
	float range;
	float duration;

	bool Active() const { return key != 0; }
	float Value(int rank) const { return rank > 0 ? baseValue + perRank * (rank - 1) : 0.0f; }
};

const SkillDefinition& GetSkillDefinition(int index);
int SkillForKey(unsigned char key);

// Session progression is copied with PlayerStats when the village hands off to the forest.
class SkillBook
{
public:
	static const int MAX_RANK = 5;
	static const int ADVANCE_LEVEL = 10;
	static const int ADVANCE_SP = 9;
	void Reset();
	void GrantLevel(int level);
	int Rank(int index) const;
	int Points() const { return m_Points; }
	int Tier() const { return m_Tier; }
	int Level() const { return m_Level; }
	int Spent(int tier) const;
	bool Unlocked(int index) const;
	bool CanLearn(int index) const;
	bool Learn(int index);
	bool CanAdvance() const;
	bool Advance();
	float Value(int index) const;
	float Cooldown(int index) const;

private:
	std::array<int, SKILL_COUNT> m_Ranks = {};
	int m_Points = 3;
	int m_Tier = 1;
	int m_Level = 1;
};
