#include "stdafx.h"
#include "Stats.h"

#include <climits>

namespace
{
	const int BASE_EXPERIENCE = 24;
	const int EXPERIENCE_STEP = 16;
	const int POINTS_PER_LEVEL = 3;
	const int MAX_LEVEL = 20;

	const float BASE_HEALTH = 60.0f;
	const float HEALTH_PER_LEVEL = 8.0f;
	const float HEALTH_PER_POINT = 12.0f;

	const float BASE_ATTACK = 9.0f;
	const float ATTACK_PER_LEVEL = 1.2f;
	const float ATTACK_PER_POINT = 3.0f;

	const float GUARD_PER_POINT = 0.045f;
	const float MAX_REDUCTION = 0.70f;

	const float BASE_SPEED = 3.4f;
	const float SPEED_PER_POINT = 0.14f;
	const float MAX_SPEED = 6.5f;

	const float BASE_SWING = 0.62f;
	const float SWING_PER_POINT = 0.028f;
	const float MIN_SWING = 0.22f;
}

int ExperienceForLevel(int level)
{
	if (level < 1)
	{
		level = 1;
	}

	// Gently superlinear: early levels arrive fast so the loop teaches itself,
	// later ones take long enough that spending points feels like a decision.
	return BASE_EXPERIENCE + EXPERIENCE_STEP * (level - 1) * level / 2;
}

void PlayerStats::Reset()
{
	level = 1;
	experience = 0;
	totalExperience = 0;
	unspentPoints = 0;

	for (int i = 0; i < STAT_COUNT; ++i)
	{
		invested[i] = 0;
	}

	health = MaxHealth();
}

int PlayerStats::ExperienceToNext() const
{
	return ExperienceForLevel(level);
}

bool PlayerStats::AddExperience(int amount)
{
	if (amount <= 0)
	{
		return false;
	}

	// Farming still contributes to the lifetime total at the level cap.
	const int remainingTotal = INT_MAX - totalExperience;
	totalExperience += amount > remainingTotal ? remainingTotal : amount;

	if (IsMaxLevel())
	{
		return false;
	}

	const int remainingExperience = INT_MAX - experience;
	experience += amount > remainingExperience ? remainingExperience : amount;

	bool levelled = false;

	while (level < MAX_LEVEL && experience >= ExperienceToNext())
	{
		experience -= ExperienceToNext();
		++level;
		unspentPoints += POINTS_PER_LEVEL;
		levelled = true;
	}

	if (levelled)
	{
		// A level up is also a breather: the new maximum is restored in full.
		health = MaxHealth();
	}

	if (level >= MAX_LEVEL)
	{
		experience = 0;
	}

	return levelled;
}

bool PlayerStats::IsMaxLevel() const
{
	return level >= MAX_LEVEL;
}

bool PlayerStats::Spend(StatKind stat)
{
	if (unspentPoints <= 0 || stat < 0 || stat >= STAT_COUNT)
	{
		return false;
	}

	--unspentPoints;
	++invested[stat];

	if (stat == STAT_VITALITY)
	{
		// Vitality adds the new capacity as healing, not as an empty bar.
		health += HEALTH_PER_POINT;
	}

	return true;
}

int PlayerStats::GetPoints(StatKind stat) const
{
	if (stat < 0 || stat >= STAT_COUNT)
	{
		return 0;
	}

	return invested[stat];
}

float PlayerStats::MaxHealth() const
{
	return BASE_HEALTH + HEALTH_PER_LEVEL * (level - 1) + HEALTH_PER_POINT * invested[STAT_VITALITY];
}

float PlayerStats::AttackPower() const
{
	return BASE_ATTACK + ATTACK_PER_LEVEL * (level - 1) + ATTACK_PER_POINT * invested[STAT_STRENGTH];
}

float PlayerStats::DamageReduction() const
{
	const float reduction = GUARD_PER_POINT * invested[STAT_GUARD];

	return reduction > MAX_REDUCTION ? MAX_REDUCTION : reduction;
}

float PlayerStats::MoveSpeed() const
{
	const float speed = BASE_SPEED + SPEED_PER_POINT * invested[STAT_AGILITY];

	return speed > MAX_SPEED ? MAX_SPEED : speed;
}

float PlayerStats::AttackInterval() const
{
	const float interval = BASE_SWING - SWING_PER_POINT * invested[STAT_AGILITY];

	return interval < MIN_SWING ? MIN_SWING : interval;
}
