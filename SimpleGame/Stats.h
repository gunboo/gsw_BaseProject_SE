#pragma once

// The four things a point can be spent on. Kept as an enum so the allocation
// screen and the derived values cannot drift apart.
enum StatKind
{
	STAT_VITALITY = 0,		// maximum health
	STAT_STRENGTH,			// attack power
	STAT_GUARD,				// damage reduction
	STAT_AGILITY,			// move and swing speed
	STAT_COUNT
};

// Player progression. Base values are what level 1 starts with; the invested
// points are what the player assigns after each level up.
struct PlayerStats
{
	int level;
	int experience;			// toward the next level
	int totalExperience;
	int unspentPoints;
	int invested[STAT_COUNT];

	float health;

	void Reset();

	int ExperienceToNext() const;
	bool IsMaxLevel() const;

	// Returns true if a level was gained, so the caller can play the effect.
	bool AddExperience(int amount);

	bool Spend(StatKind stat);

	int GetPoints(StatKind stat) const;

	float MaxHealth() const;
	float AttackPower() const;
	float DamageReduction() const;		// 0..0.7
	float MoveSpeed() const;
	float AttackInterval() const;		// seconds between swings
};

// Experience needed to go from `level` to `level + 1`.
int ExperienceForLevel(int level);
