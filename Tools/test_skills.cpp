#include "../SimpleGame/stdafx.h"
#include "../SimpleGame/SkillPanel.h"
#include "../SimpleGame/WindowInput.h"
#include "TestAssertions.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <limits>

namespace
{
	void Near(float actual, float expected)
	{
		assert(std::fabs(actual - expected) < 0.001f);
	}

	void Master(PlayerStats& stats)
	{
		stats.Reset();
		stats.AddExperience(1000000);

		for (int i = 0; i < 6; ++i)
		{
			for (int rank = 0; rank < SkillBook::MAX_RANK; ++rank)
			{
				assert(stats.LearnSkill(i));
			}
		}

		assert(stats.skills.Advance());

		for (int i = 6; i < SKILL_COUNT; ++i)
		{
			for (int rank = 0; rank < SkillBook::MAX_RANK; ++rank)
			{
				assert(stats.LearnSkill(i));
			}
		}
	}
}

int main()
{
	PlayerStats stats;
	stats.Reset();
	assert(stats.skills.Points() == 3 && stats.skills.Tier() == 1);
	assert(!stats.skills.Learn(-1) && !stats.skills.Learn(SKILL_COUNT));
	assert(!stats.skills.Learn(SKILL_STORM));
	assert(!stats.skills.Advance());
	const float health = stats.MaxHealth();
	stats.health -= 10.0f;
	assert(stats.LearnSkill(SKILL_VITALITY));
	Near(stats.MaxHealth(), health + 12.0f);
	Near(stats.health, health + 2.0f);
	stats.skills.GrantLevel(10);
	assert(stats.skills.Points() == 29);
	stats.skills.GrantLevel(10);
	stats.skills.GrantLevel(1);
	assert(stats.skills.Points() == 29);
	assert(!stats.skills.CanAdvance());

	for (int rank = 0; rank < 5; ++rank)
	{
		assert(stats.skills.Learn(SKILL_MASTERY));
	}

	for (int rank = 0; rank < 3; ++rank)
	{
		assert(stats.skills.Learn(SKILL_COMBO));
	}

	assert(stats.skills.Advance());
	assert(!stats.skills.Advance());
	assert(stats.skills.Learn(SKILL_BARRAGE));
	assert(!stats.skills.Learn(SKILL_STORM));
	assert(!stats.skills.Learn(SKILL_MASTERY));

	Master(stats);
	assert(stats.level == 20 && stats.skills.Points() == 0);
	Near(stats.skills.Cooldown(SKILL_COMBO), 3.0f);
	stats.health = stats.MaxHealth() * 0.5f;
	stats.HealOnKill();
	Near(stats.health, stats.MaxHealth() * 0.6f);
	PlayerStats transferred = stats;
	assert(transferred.skills.Rank(SKILL_STORM) == 5 && transferred.skills.Tier() == 2);
	assert(transferred.totalExperience == stats.totalExperience);
	Near(transferred.AttackPower(), stats.AttackPower());
	stats.skills.GrantLevel(std::numeric_limits<int>::max());
	assert(stats.skills.Points() == 0);
	int activeCount[2] = {}, passiveCount[2] = {};

	for (int i = 0; i < SKILL_COUNT; ++i)
	{
		const SkillDefinition& definition = GetSkillDefinition(i);
		++(definition.Active() ? activeCount[definition.tier - 1] : passiveCount[definition.tier - 1]);

		if (definition.Active())
		{
			assert(SkillForKey(definition.key) == i);
		}
	}

	assert(activeCount[0] == 3 && activeCount[1] == 3 && passiveCount[0] == 3 && passiveCount[1] == 3);

	// A known arena makes obstruction tests deterministic, including a solid wall.
	const char* fixture = "skill_test_arena.map";
	{
		std::ofstream file(fixture);
		file << "SIZE 24 24\nTILES\n";

		for (int y = 0; y < 24; ++y)
		{
			std::string row(24, '.');
			row[0] = row[23] = 'T';
			row[15] = 'T';
			file << (y == 0 || y == 23 ? std::string(24, 'T') : row) << '\n';
		}

		file << "PLAYER 8 12\n";
	}
	World world;
	assert(world.Load(fixture));
	std::remove(fixture);
	SceneGraph graph;
	Actor* player = graph.CreateActor("player");
	player->SetPosition(8, 12);
	Actor* enemies[] = { graph.CreateActor("near"), graph.CreateActor("far"), graph.CreateActor("behind_wall") };
	enemies[0]->SetPosition(10, 12);
	enemies[1]->SetPosition(13, 12);
	enemies[2]->SetPosition(17, 12);
	int hits[3] = {};
	SkillCombat combat;
	combat.Initialize(graph, NULL, player, world, stats, 3,
		[&](size_t i) { return enemies[i]; }, [&](size_t i, float damage, bool)
		{
			assert(damage > 0);
			++hits[i];
		});
	assert(combat.Cast(SKILL_BREAKER, false, 1, 0));
	assert(hits[0] == 1 && hits[1] == 0 && hits[2] == 0);
	assert(!combat.Cast(SKILL_BREAKER, false, 1, 0));
	const float remaining = combat.Remaining(SKILL_BREAKER);
	combat.Update(0);
	combat.Update(std::numeric_limits<float>::quiet_NaN());
	Near(combat.Remaining(SKILL_BREAKER), remaining);
	combat.Update(20);
	assert(combat.Cast(SKILL_BREAKER, true, 1, 0));
	assert(hits[0] == 2 && hits[1] == 1 && hits[2] == 0);
	combat.Update(20);
	assert(combat.Cast(SKILL_WARD, true, 1, 0));
	Near(combat.IncomingMultiplier(), 0.6f);
	combat.Update(5);
	Near(combat.IncomingMultiplier(), 1);
	assert(combat.Cast(SKILL_SHADOW, false, 1, 0));
	assert(player->GetWorldPosition().x > 12 && combat.Invulnerable());
	combat.ClearEffects();
	assert(!combat.Invulnerable() && combat.Remaining(SKILL_SHADOW) > 0);
	combat.Update(20);
	player->SetPosition(14, 12);
	assert(combat.Cast(SKILL_SHADOW, true, 1, 0));
	assert(player->GetWorldPosition().x < 15);
	assert(!world.IsBlocked(player->GetWorldPosition().x, player->GetWorldPosition().y, 0.28f));
	combat.Update(20);
	player->SetPosition(8, 12);
	const int before = hits[0];
	assert(combat.Cast(SKILL_STORM, false, 1, 0));
	assert(hits[0] == before + 1);
	combat.Update(2.1f);
	assert(hits[0] == before + 3 && hits[2] == 0);

	// Every active skill can be cast with either weapon; passives cannot be cast.
	const int activeSkills[] = { SKILL_COMBO, SKILL_BREAKER, SKILL_WARD, SKILL_BARRAGE, SKILL_SHADOW, SKILL_STORM };

	for (bool bow : { false, true })
	{
		for (int skill : activeSkills)
		{
			combat.ClearEffects();
			combat.Update(30);
			player->SetPosition(8, 12);
			assert(combat.Cast(skill, bow, 1, 0));
		}
	}

	combat.Update(30);
	assert(!combat.Cast(SKILL_MASTERY, false, 1, 0));
	assert(!combat.Cast(-1, false, 1, 0));
	assert(!combat.Cast(SKILL_COUNT, false, 1, 0));

	DialogueDB dialogue;
	assert(dialogue.Load("./Data/dialogue.txt"));
	SkillPanel panel;
	panel.Initialize(stats, combat, dialogue);
	WindowInput input;
	const auto dispatch = [&](unsigned char key, bool down, bool) { panel.OnKey(key, down, true); };
	input.Key(GLFW_KEY_K, GLFW_PRESS, false, dispatch);
	input.Key(GLFW_KEY_K, GLFW_REPEAT, false, dispatch);
	assert(panel.IsOpen());
	input.ReleaseAll(dispatch);
	assert(panel.IsOpen());
	assert(panel.OnKey('m', true, true));
	assert(panel.OnKey('j', true, true));
	assert(panel.OnKey('q', true, true));
	assert(panel.OnKey(27, true, true));
	assert(!panel.IsOpen());
	assert(!panel.OnKey('k', true, false));
	std::puts("PASS: 12 skills, SP accounting, promotion, passives, scene transfer, both weapons, walls, buffs, cooldowns and modal input");
	return 0;
}
