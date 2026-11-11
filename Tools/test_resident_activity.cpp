#include "../SimpleGame/stdafx.h"
#include "../SimpleGame/VillageSimulation.h"

#include "TestAssertions.h"
#include <cstdio>
#include <limits>
#include <map>
#include <set>

int main()
{
	ResidentActivityMotion clock;
	clock.BeginShot();
	assert(!clock.Advance(ResidentActivity::SHOT_RELEASE - 0.01f));
	assert(clock.BowPull() > 0.95f);
	assert(clock.Advance(0.02f));
	assert(!clock.Advance(2.0f));
	clock.BeginShot();
	assert(clock.Advance(3.0f));
	assert(!clock.Advance(3.0f));
	const float unchanged = clock.time;
	assert(!clock.Advance(std::numeric_limits<float>::quiet_NaN()));
	assert(clock.time == unchanged);

	World world;
	ModelLibrary models;
	DialogueDB dialogue;
	Lighting lighting;
	SceneGraph graph;
	VillageSimulation village;
	assert(world.Load("./Data/village.map"));
	assert(models.LoadOrBuild("./Data/Logs/visual-qa/resident-test-models.cache"));
	assert(models.Find("resident_active") != NULL && models.Find("resident_kasa") != NULL);
	assert(dialogue.Load("./Data/dialogue.txt"));
	Actor* player = graph.CreateActor("test_player");
	player->SetPosition(world.GetPlayerStartX(), world.GetPlayerStartY(),
		world.GetElevation(world.GetPlayerStartX(), world.GetPlayerStartY()));
	std::vector<Actor*> residents;
	std::map<std::string, Actor*> byId;

	for (const SpawnPoint& spawn : world.GetNpcSpawns())
	{
		Actor* actor = graph.CreateActor(spawn.id);
		actor->SetPosition(spawn.x, spawn.y, world.GetElevation(spawn.x, spawn.y));
		residents.push_back(actor);
		byId[spawn.id] = actor;
	}

	assert(village.Initialize(world, graph, player, residents, models, lighting, dialogue));
	assert(residents.size() == 40);
	const ActorPosition fisherHome = byId["fisher"]->GetWorldPosition();
	const ActorPosition smithHome = byId["smith"]->GetWorldPosition();
	std::set<std::string> seenStates;
	bool farmerWalked = false;
	bool farmerWorked = false;
	bool foreignRumor = false;

	for (int frame = 0; frame < 600; ++frame)
	{
		graph.Update(0.1f);
		MonitoredWorld snapshot;
		snapshot.active = true;
		snapshot.state = "play";
		village.CaptureMonitoring(snapshot);
		assert(snapshot.npcs.size() == residents.size());
		assert(snapshot.villages.size() == 5);

		for (const MonitoredVillage& town : snapshot.villages)
		{
			assert(town.homeNpcCount == (town.id == "sanctuary" ? 12u : 7u));
			foreignRumor = foreignRumor || town.knownRumors > 1;
		}

		for (const MonitoredEntity& resident : snapshot.npcs)
		{
			seenStates.insert(resident.state);
			assert(!world.IsBlocked(resident.position.x, resident.position.y, 0.275f));

			if (resident.id == "npc:farmer")
			{
				farmerWalked = farmerWalked || resident.state == "moving" || resident.state == "returning";
				farmerWorked = farmerWorked || resident.state == "garden";
			}
		}
	}

	assert(farmerWorked && farmerWalked);
	assert(foreignRumor);
	assert(seenStates.count("fish") && seenStates.count("forge") && seenStates.count("read"));
	assert(seenStates.count("sentry") && seenStates.count("torch") && seenStates.count("pray"));
	assert(byId["fisher"]->GetWorldPosition().x == fisherHome.x && byId["fisher"]->GetWorldPosition().y == fisherHome.y);
	assert(byId["smith"]->GetWorldPosition().x == smithHome.x && byId["smith"]->GetWorldPosition().y == smithHome.y);
	lighting.Clear();
	village.AddResidentLights(lighting);
	assert(lighting.GetLightCount() == 2);

	const ActorPosition beforePause = byId["guard_west"]->GetWorldPosition();
	village.SetState(false, true);
	graph.Update(5.0f);
	const ActorPosition afterPause = byId["guard_west"]->GetWorldPosition();
	assert(beforePause.x == afterPause.x && beforePause.y == afterPause.y);
	village.SetState(true, true);

	Actor* population = NULL;

	for (size_t i = 0; i < graph.GetRoot().GetChildCount(); ++i)
	{
		Actor* child = graph.GetRoot().GetChild(i);

		if (child->GetName() == "VillagePopulation")
		{
			population = child;
		}
	}

	assert(population != NULL);
	MonitoredWorld initialHunt;
	village.CaptureMonitoring(initialHunt);
	Actor* target = NULL;

	for (size_t i = 0; i < initialHunt.monsters.size(); ++i)
	{
		if (initialHunt.monsters[i].alive)
		{
			target = population->GetChild(i);
			break;
		}
	}

	assert(target != NULL);
	byId["guard_west"]->SetPosition(player->GetWorldPosition().x, player->GetWorldPosition().y);
	byId["guard_east"]->SetPosition(player->GetWorldPosition().x, player->GetWorldPosition().y);
	const ActorPosition hunter = byId["hunter"]->GetWorldPosition();
	bool placed = false;

	for (int dx = -2; dx <= 2 && !placed; ++dx)
	{
		for (int dy = -2; dy <= 2 && !placed; ++dy)
		{
			const float x = hunter.x + dx;
			const float y = hunter.y + dy;

			if (dx * dx + dy * dy >= 2 && world.CanMove(hunter.x, hunter.y, x, y, 0.28f))
			{
				target->SetPosition(x, y, world.GetElevation(x, y));
				placed = true;
			}
		}
	}

	assert(placed);
	MonitoredWorld beforeHunt;
	beforeHunt.active = true;
	beforeHunt.state = "play";
	village.CaptureMonitoring(beforeHunt);
	bool hunterAlert = false;
	bool arrowHit = false;

	for (int frame = 0; frame < 100; ++frame)
	{
		graph.Update(0.05f);
		MonitoredWorld snapshot;
		snapshot.active = true;
		snapshot.state = "play";
		village.CaptureMonitoring(snapshot);

		for (const MonitoredEntity& resident : snapshot.npcs)
		{
			hunterAlert = hunterAlert || (resident.id == "npc:hunter" && resident.state == "alert");
		}

		for (size_t i = 0; i < snapshot.monsters.size(); ++i)
		{
			arrowHit = arrowHit || snapshot.monsters[i].health < beforeHunt.monsters[i].health;
		}
	}

	assert(hunterAlert && arrowHit);
	std::puts("PASS: 40 residents; shot edge; 60s jobs/navigation; fixed quest NPCs; two torch lights; pause; hunter arrow damage");

	return 0;
}
