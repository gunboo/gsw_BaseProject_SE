#include "stdafx.h"
#include "VillageSimulation.h"
#include "AnalysisLog.h"
#include "VisualStyle.h"
#include "ResidentArt.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <set>

namespace
{
	const int MONSTER_COUNT = 100;
	const int SETTLEMENT_COUNT = 5;
	const int MIN_RESIDENTS_PER_VILLAGE = 5;
	const int COURIERS_PER_VILLAGE = 2;
	const int MONSTERS_PER_ROAD = 25;
	const float RESPAWN_RELOCATION = 8.0f;
	const float ROAD_HALF_WIDTH = 12.0f;
	const float CONTACT_INTERVAL = 2.0f;
	const float CONTACT_RANGE = 3.5f;
	const float COURIER_VISIT_TIME = 12.0f;
	const float PLAYER_WALK_SPEED = 3.1f;
	const float RESPAWN_RETRY = 0.5f;
	const float SCORE_MAX = 100.0f;
	const float FEAR_TENSE = 40.0f;
	const float FEAR_THREATENED = 65.0f;
	const float TRUST_HOPEFUL = 70.0f;
	const float BODY_RADIUS = 0.28f;
	const float MAX_HEALTH = 100.0f;
	const float ATTACK_RANGE = 2.4f;
	const float ATTACK_INTERVAL = 0.45f;
	const float BOW_INTERVAL = 0.65f;
	const float PLAYER_DAMAGE = 24.0f;
	const float RESPAWN_DISTANCE = 11.0f;
	const float REPATH_INTERVAL = 1.2f;
	const float ARRIVAL_DISTANCE = 0.08f;
	const float LOG_INTERVAL = 5.0f;
	const float DRAW_DEPTH = 925000.0f;
	const int DIRECTION_COUNT = 4;
	const int MONSTER_KIND_COUNT = 3;
	const int NEARBY_ATTEMPTS = 40;
	const int FLEE_CANDIDATES = 8;
	const float COLLISION_SAMPLE_STEP = 0.2f;
	const float MAX_RESIDENT_RADIUS = 12.0f;
	const float MAX_RESIDENT_SPEED = 4.0f;
	const float HEAL_RANGE = 2.2f;
	const float HEAL_AMOUNT = 25.0f;
	const float HEAL_INTERVAL = 8.0f;
	const float GUARD_NOTICE_RANGE = 5.0f;
	const float GUARD_ATTACK_RANGE = 1.8f;
	const float GUARD_DAMAGE = 20.0f;
	const float GUARD_ATTACK_INTERVAL = 1.0f;
	const float BANDIT_ATTACK_RANGE = 1.5f;
	const float BANDIT_DAMAGE = 6.0f;
	const float BANDIT_ATTACK_INTERVAL = 2.0f;
	const float BANDIT_RETREAT_TIME = 6.0f;
	const float RESIDENT_NOTICE_RANGE = 7.0f;
	const float RESIDENT_STOP_RANGE = 2.0f;
	const float RESIDENT_TALK_RANGE = 1.7f;
	const float RESIDENT_DECISION_MIN = 2.0f;
	const float RESIDENT_DECISION_MAX = 4.0f;
	const float MONSTER_NOTICE_RANGE = 9.0f;
	const float MONSTER_LEASH_RANGE = 14.0f;
	const float MONSTER_ATTACK_RANGE = 1.15f;
	const float MONSTER_ATTACK_INTERVAL = 1.4f;
	const float MONSTER_SPAWN_SEPARATION = 1.5f;
	const float MONSTER_RESPAWN_MIN = 1.0f;
	const float MONSTER_RESPAWN_MAX = 2.0f;
	const float DAMAGE_GRACE_TIME = 0.7f;
	const float RECOVERY_GRACE_TIME = 3.0f;
	const float HIT_FLASH_TIME = 0.2f;
	const Color TORCH_LIGHT_COLOR = { 1.0f, 0.62f, 0.25f, 1.0f };
	const char* const BEHAVIORS[] =
	{
		"stay", "wander", "patrol", "merchant", "healer", "guard", "bandit", "flee", "follow"
	};

	struct MonsterProfile
	{
		const char* model;
		float health;
		float speed;
		float damage;
	};

	const MonsterProfile PROFILES[] =
	{
		{ "enemy_wisp", 24.0f, 2.6f, 5.0f },
		{ "enemy_shade", 48.0f, 2.0f, 8.0f },
		{ "enemy_oni", 96.0f, 1.4f, 14.0f }
	};

	float Distance(const ActorPosition& a, const ActorPosition& b)
	{
		const float dx = a.x - b.x;
		const float dy = a.y - b.y;

		return sqrtf(dx * dx + dy * dy);
	}

	float Score(float value)
	{
		return (std::max)(0.0f, (std::min)(SCORE_MAX, value));
	}

	unsigned int BitCount(unsigned int bits)
	{
		unsigned int count = 0;

		while (bits != 0)
		{
			bits &= bits - 1;
			++count;
		}

		return count;
	}
}

bool VillageSimulation::Initialize(World& world, SceneGraph& graph, Actor* player,
	const std::vector<Actor*>& npcs, ModelLibrary& models, Lighting& lighting, DialogueDB& dialogue)
{
	m_World = &world;
	m_Player = player;
	m_Models = &models;
	m_Lighting = &lighting;
	m_Dialogue = &dialogue;
	m_Rng.Seed(9302026u);
	m_Residents.clear();
	m_Monsters.clear();
	m_Health = MAX_HEALTH;
	m_Kills = 0;
	m_AttackCooldown = m_Invulnerable = m_LogTimer = 0.0f;
	m_AttackMotion.Clear();
	m_ContactTimer = 0.0f;

	if (!LoadSettlements())
	{
		AnalysisLog::Get().Event("village.data_error", "settlements_routes_or_rumors");
		return false;
	}

	BuildNavigation();

	for (const Settlement& village : m_Settlements)
	{
		const int cell = Cell(village.center);

		if (cell < 0 || !m_Walkable[cell])
		{
			AnalysisLog::Get().Event("village.data_error", village.id + ";unreachable_plaza");
			return false;
		}
	}

	for (const Road& road : m_Roads)
	{
		if (road.spawns.size() < MONSTERS_PER_ROAD)
		{
			AnalysisLog::Get().Event("village.data_error", "insufficient_road_spawns");
			return false;
		}

		const float distance = Distance(m_Settlements[road.from].center, m_Settlements[road.to].center);
		AnalysisLog::Get().Event("village.route", m_Settlements[road.from].id + ";to=" + m_Settlements[road.to].id
			+ ";plaza_distance=" + std::to_string(distance) + ";walk_seconds=" + std::to_string(distance / PLAYER_WALK_SPEED)
			+ ";spawn_candidates=" + std::to_string(road.spawns.size()));
	}

	if (m_Open.empty() || m_Spawns.size() < MONSTER_COUNT || !LoadResidents(npcs))
	{
		AnalysisLog::Get().Event("village.data_error", "navigation_or_resident_data");
		return false;
	}

	for (const MonsterProfile& profile : PROFILES)
	{
		if (m_Models->Find(profile.model) == NULL)
		{
			AnalysisLog::Get().Event("village.data_error", profile.model);
			return false;
		}
	}

	const char* const residentModels[] = { "resident_active", "resident_kasa" };

	for (const char* name : residentModels)
	{
		if (m_Models->Find(name) == NULL)
		{
			AnalysisLog::Get().Event("village.data_error", name);
			return false;
		}
	}

	m_Group = graph.CreateActor("VillagePopulation");
	m_Group->SetUpdateCallback([this](Actor&, float deltaSeconds)
	{
		Update(deltaSeconds);
	});
	m_Monsters.resize(MONSTER_COUNT);

	for (size_t i = 0; i < m_Monsters.size(); ++i)
	{
		Monster& monster = m_Monsters[i];
		monster.road = static_cast<int>(i / MONSTERS_PER_ROAD);
		monster.kind = static_cast<int>(i % MONSTER_KIND_COUNT);
		monster.walker.actor = graph.CreateActor("VillageMonster_" + std::to_string(i), m_Group);
		monster.walker.actor->SetRenderBounds(-80.0f, -140.0f, 80.0f, 48.0f);
		monster.walker.actor->SetRenderCallback([this, i](const Actor&, Renderer& renderer)
		{
			DrawMonster(i, renderer);
		});

		if (!SpawnMonster(i, true))
		{
			return false;
		}
	}

	UpdateAtmosphere();

	m_Bow.Initialize(graph, m_Group, world, m_Monsters.size() + m_Residents.size(),
		[this](size_t index) -> Actor*
		{
			if (index < m_Monsters.size())
			{
				return m_Monsters[index].health > 0.0f ? m_Monsters[index].walker.actor : NULL;
			}

			Resident& resident = m_Residents[index - m_Monsters.size()];

			return resident.behavior == BANDIT && resident.afraid <= 0.0f ? resident.walker.actor : NULL;
		},
		[this](size_t index, float damage)
		{
			if (index < m_Monsters.size())
			{
				HitMonster(index, damage);
			}
			else
			{
				Resident& resident = m_Residents[index - m_Monsters.size()];
				resident.afraid = BANDIT_RETREAT_TIME;
				resident.walker.repath = 0.0f;
				AnalysisLog::Get().Event("village.bandit_repelled", resident.id + ";bow");
			}
		});
	Actor* huntingArrows = graph.CreateActor("HuntingArrows", m_Group);
	m_HuntingBow.Initialize(graph, huntingArrows, world, m_Monsters.size(),
		[this](size_t index) -> Actor*
		{
			return m_Monsters[index].health > 0.0f ? m_Monsters[index].walker.actor : NULL;
		},
		[this](size_t index, float damage)
		{
			HitMonster(index, damage);
		});

	for (size_t i = 0; i < m_Residents.size(); ++i)
	{
		Resident& resident = m_Residents[i];
		FindWorkTarget(resident);
		const bool fishing = resident.activity.occupation == OCCUPATION_FISH;
		resident.walker.actor->SetRenderBounds(fishing ? -210.0f : -75.0f, -120.0f,
			fishing ? 210.0f : 75.0f, fishing ? 140.0f : 40.0f);

		if (resident.activity.occupation == OCCUPATION_GARDEN && resident.taskAvailable)
		{
			Actor* garden = graph.CreateActor("Garden_" + resident.id, m_Group);
			garden->SetPosition(resident.workTarget.x, resident.workTarget.y, resident.workTarget.z);
			garden->SetActive(false);
			garden->SetRenderBounds(-30.0f, -22.0f, 30.0f, 18.0f);
			garden->SetRenderCallback([this](const Actor& actor, Renderer& renderer)
			{
				const ActorPosition position = actor.GetWorldPosition();
				float x, y;
				renderer.WorldToScreen(position.x, position.y, position.z, &x, &y);
				DrawResidentGarden(renderer, x, y, position.x + position.y,
					m_Lighting->Shade(position.x, position.y, RGBA(1.0f, 1.0f, 1.0f)));
			});
		}
	}

	Actor* swing = graph.CreateActor("VillageAttack", player);
	swing->SetRenderBounds(-130.0f, -80.0f, 130.0f, 80.0f);
	swing->SetRenderCallback([this](const Actor&, Renderer& renderer)
	{
		DrawSwing(renderer);
	});
	AnalysisLog::Get().Event("village.ready", "width=" + std::to_string(world.GetWidth())
		+ ";height=" + std::to_string(world.GetHeight()) + ";npcs=" + std::to_string(m_Residents.size())
		+ ";monsters=" + std::to_string(MONSTER_COUNT) + ";reachable=" + std::to_string(m_Open.size())
		+ ";villages=" + std::to_string(m_Settlements.size()));

	return true;
}

bool VillageSimulation::LoadSettlements()
{
	m_Settlements.clear();
	m_Roads.clear();
	m_Rumors.clear();
	std::ifstream file("./Data/settlements.tsv");
	std::string line;
	std::set<std::string> ids;

	if (!file)
	{
		return false;
	}

	while (std::getline(file, line))
	{
		if (line.empty() || line[0] == '#')
		{
			continue;
		}

		Settlement village;
		std::string extra;
		std::istringstream row(line);

		if (!(row >> village.id >> village.name >> village.center.x >> village.center.y >> village.radius
			>> village.baseFear >> village.baseTrust >> village.baseProsperity) || (row >> extra)
			|| !ids.insert(village.id).second || !std::isfinite(village.center.x) || !std::isfinite(village.center.y)
			|| !std::isfinite(village.radius) || village.radius <= 0.0f || village.radius > 20.0f
			|| !std::isfinite(village.baseFear) || !std::isfinite(village.baseTrust) || !std::isfinite(village.baseProsperity)
			|| Score(village.baseFear) != village.baseFear || Score(village.baseTrust) != village.baseTrust
			|| Score(village.baseProsperity) != village.baseProsperity)
		{
			return false;
		}

		village.statistics.id = village.id;
		village.statistics.name = village.name;
		village.statistics.hasAtmosphere = true;
		m_Settlements.push_back(village);
	}

	if (m_Settlements.size() != SETTLEMENT_COUNT)
	{
		return false;
	}

	std::ifstream routes("./Data/village_routes.tsv");
	std::set<std::pair<int, int>> links;

	if (!routes)
	{
		return false;
	}

	while (std::getline(routes, line))
	{
		if (line.empty() || line[0] == '#')
		{
			continue;
		}

		std::string from, to, extra;
		std::istringstream row(line);
		Road road;

		if (!(row >> from >> to) || (row >> extra))
		{
			return false;
		}

		for (int i = 0; i < SETTLEMENT_COUNT; ++i)
		{
			if (m_Settlements[i].id == from)
			{
				road.from = i;
			}

			if (m_Settlements[i].id == to)
			{
				road.to = i;
			}
		}

		if (road.from < 0 || road.to < 0 || road.from == road.to
			|| !links.insert(std::minmax(road.from, road.to)).second)
		{
			return false;
		}

		m_Settlements[road.from].neighbors.push_back(road.to);
		m_Settlements[road.to].neighbors.push_back(road.from);
		m_Roads.push_back(road);
	}

	if (m_Roads.size() * MONSTERS_PER_ROAD != MONSTER_COUNT)
	{
		return false;
	}

	std::set<int> reached = { 0 };

	for (int pass = 0; pass < SETTLEMENT_COUNT; ++pass)
	{
		for (const Road& road : m_Roads)
		{
			if (reached.count(road.from) != 0 || reached.count(road.to) != 0)
			{
				reached.insert(road.from);
				reached.insert(road.to);
			}
		}
	}

	std::ifstream rumors("./Data/rumors.tsv");
	ids.clear();

	if (reached.size() != SETTLEMENT_COUNT || !rumors)
	{
		return false;
	}

	while (std::getline(rumors, line))
	{
		if (line.empty() || line[0] == '#')
		{
			continue;
		}

		Rumor rumor;
		std::string extra;
		std::istringstream row(line);

		if (!(row >> rumor.id >> rumor.dialogue >> rumor.fear >> rumor.trust >> rumor.prosperity)
			|| (row >> extra) || !ids.insert(rumor.id).second || m_Dialogue->Find(rumor.dialogue) == NULL
			|| !std::isfinite(rumor.fear) || !std::isfinite(rumor.trust) || !std::isfinite(rumor.prosperity))
		{
			return false;
		}

		m_Rumors.push_back(rumor);
	}

	return m_Rumors.size() == SETTLEMENT_COUNT;
}

int VillageSimulation::VillageAt(const ActorPosition& position) const
{
	for (size_t i = 0; i < m_Settlements.size(); ++i)
	{
		if (Distance(position, m_Settlements[i].center) <= m_Settlements[i].radius)
		{
			return static_cast<int>(i);
		}
	}

	return -1;
}

void VillageSimulation::UpdateCourier(Resident& resident, float deltaSeconds)
{
	Walker& walker = resident.walker;
	const int current = VillageAt(walker.actor->GetWorldPosition());

	if (!resident.traveling)
	{
		resident.visitTimer -= deltaSeconds;

		if (resident.visitTimer > 0.0f)
		{
			return;
		}

		const int from = current >= 0 ? current : resident.homeVillage;
		const std::vector<int>& neighbors = m_Settlements[from].neighbors;
		resident.destinationVillage = neighbors[m_Rng.RangeInt(0, static_cast<int>(neighbors.size()) - 1)];
		resident.traveling = true;
		walker.repath = 0.0f;
		walker.route.clear();
		AnalysisLog::Get().Event("rumor.depart", resident.id + ";from=" + m_Settlements[from].id
			+ ";to=" + m_Settlements[resident.destinationVillage].id + ";known=" + std::to_string(BitCount(resident.rumors)));
	}

	const ActorPosition destination = m_Settlements[resident.destinationVillage].center;
	Navigate(walker, Cell(destination), resident.speed, deltaSeconds);

	if (Distance(walker.actor->GetWorldPosition(), destination) < ResidentActivity::ARRIVAL_RANGE)
	{
		resident.traveling = false;
		resident.visitTimer = COURIER_VISIT_TIME;
		AnalysisLog::Get().Event("rumor.arrive", resident.id + ";village=" + m_Settlements[resident.destinationVillage].id);
	}
}

void VillageSimulation::ShareRumors()
{
	std::vector<unsigned int> pending;
	pending.reserve(m_Residents.size());

	for (const Resident& resident : m_Residents)
	{
		pending.push_back(resident.rumors);
	}

	// A contact tick advances one hop; vector order cannot broadcast a rumor.
	for (size_t i = 0; i < m_Residents.size(); ++i)
	{
		for (size_t j = i + 1; j < m_Residents.size(); ++j)
		{
			const ActorPosition a = m_Residents[i].walker.actor->GetWorldPosition();
			const ActorPosition b = m_Residents[j].walker.actor->GetWorldPosition();

			if (Distance(a, b) <= CONTACT_RANGE && m_Residents[i].rumors != m_Residents[j].rumors && ClearLine(a, b))
			{
				pending[i] |= m_Residents[j].rumors;
				pending[j] |= m_Residents[i].rumors;
			}
		}
	}

	for (size_t i = 0; i < m_Residents.size(); ++i)
	{
		if (pending[i] != m_Residents[i].rumors)
		{
			m_Residents[i].rumors = pending[i];
			AnalysisLog::Get().Event("rumor.learned", m_Residents[i].id + ";known=" + std::to_string(BitCount(pending[i])));
		}
	}
}

void VillageSimulation::UpdateAtmosphere()
{
	for (size_t index = 0; index < m_Settlements.size(); ++index)
	{
		Settlement& village = m_Settlements[index];
		MonitoredVillage& stats = village.statistics;
		stats.homeNpcCount = stats.presentNpcCount = stats.travelers = stats.nearbyMonsters = 0;
		unsigned int frightened = 0;
		unsigned int working = 0;

		for (const Resident& resident : m_Residents)
		{
			if (resident.homeVillage == static_cast<int>(index))
			{
				++stats.homeNpcCount;
			}

			if (VillageAt(resident.walker.actor->GetWorldPosition()) == static_cast<int>(index))
			{
				++stats.presentNpcCount;
				stats.travelers += resident.courier ? 1 : 0;
				frightened += resident.afraid > 0.0f || resident.behavior == FLEE ? 1 : 0;
				working += resident.activity.working ? 1 : 0;

				if (resident.homeVillage == static_cast<int>(index))
				{
					village.rumors |= resident.rumors;
				}
			}
		}

		for (const Monster& monster : m_Monsters)
		{
			if (monster.health > 0.0f && Distance(village.center, monster.walker.actor->GetWorldPosition()) <= village.radius + 10.0f)
			{
				++stats.nearbyMonsters;
			}
		}

		float fear = village.baseFear + stats.nearbyMonsters * 1.5f + frightened * 2.0f;
		float trust = village.baseTrust;
		float prosperity = village.baseProsperity + working * 0.5f;

		for (size_t rumor = 0; rumor < m_Rumors.size(); ++rumor)
		{
			if ((village.rumors & (1u << rumor)) != 0)
			{
				fear += m_Rumors[rumor].fear;
				trust += m_Rumors[rumor].trust;
				prosperity += m_Rumors[rumor].prosperity;
			}
		}

		stats.fear = Score(fear);
		stats.trust = Score(trust);
		stats.prosperity = Score(prosperity);
		stats.knownRumors = BitCount(village.rumors);
		stats.mood = stats.fear >= FEAR_THREATENED ? "threatened" : stats.fear >= FEAR_TENSE ? "tense"
			: stats.trust >= TRUST_HOPEFUL ? "hopeful" : "calm";
	}
}

std::string VillageSimulation::RumorDialogue(const std::string& id) const
{
	for (const Resident& resident : m_Residents)
	{
		if (resident.id == id)
		{
			// Foreign news takes priority so physical delivery is observable in play.
			for (size_t rumor = 0; rumor < m_Rumors.size(); ++rumor)
			{
				if (rumor != static_cast<size_t>(resident.homeVillage) && (resident.rumors & (1u << rumor)) != 0)
				{
					return m_Rumors[rumor].dialogue;
				}
			}
		}
	}

	return "";
}

bool VillageSimulation::LoadResidents(const std::vector<Actor*>& actors)
{
	std::ifstream file("./Data/village_npcs.tsv");
	std::string line;
	std::set<std::string> loaded;
	const std::vector<SpawnPoint>& spawns = m_World->GetNpcSpawns();

	if (!file || actors.size() != spawns.size())
	{
		return false;
	}

	while (std::getline(file, line))
	{
		if (line.empty() || line[0] == '#')
		{
			continue;
		}

		Resident resident;
		std::string behavior;
		std::string occupation;
		std::string extra;
		std::string villageId;
		int courier = 0;
		std::istringstream row(line);

		if (!(row >> resident.id >> behavior >> resident.radius >> resident.speed >> occupation
			>> resident.activity.facingX >> resident.activity.facingY >> villageId >> courier)
			|| (row >> extra) || !loaded.insert(resident.id).second
			|| !std::isfinite(resident.radius) || !std::isfinite(resident.speed)
			|| resident.radius < 0.0f || resident.radius > MAX_RESIDENT_RADIUS
			|| resident.speed < 0.0f || resident.speed > MAX_RESIDENT_SPEED)
		{
			return false;
		}

		int kind = -1;
		int role = -1;
		const float facingLength = sqrtf(resident.activity.facingX * resident.activity.facingX
			+ resident.activity.facingY * resident.activity.facingY);

		if (!std::isfinite(facingLength) || facingLength < 0.0001f)
		{
			return false;
		}

		resident.activity.facingX /= facingLength;
		resident.activity.facingY /= facingLength;

		for (int i = 0; i < OCCUPATION_COUNT; ++i)
		{
			if (occupation == ResidentActivity::Name(static_cast<ResidentOccupation>(i)))
			{
				role = i;
			}
		}

		for (int i = 0; i < static_cast<int>(sizeof(BEHAVIORS) / sizeof(BEHAVIORS[0])); ++i)
		{
			if (behavior == BEHAVIORS[i])
			{
				kind = i;
			}
		}

		for (size_t i = 0; i < spawns.size(); ++i)
		{
			if (spawns[i].id == resident.id)
			{
				resident.walker.actor = actors[i];
			}
		}

		if (kind < 0 || role < 0 || resident.walker.actor == NULL)
		{
			return false;
		}

		resident.behavior = static_cast<Behavior>(kind);

		for (size_t village = 0; village < m_Settlements.size(); ++village)
		{
			if (m_Settlements[village].id == villageId)
			{
				resident.homeVillage = static_cast<int>(village);
			}
		}

		if (resident.homeVillage < 0 || (courier != 0 && courier != 1))
		{
			return false;
		}

		resident.courier = courier != 0;

		if (resident.courier && resident.speed <= 0.0f)
		{
			return false;
		}

		resident.visitTimer = COURIER_VISIT_TIME + m_Rng.Range(0.0f, 6.0f);
		resident.rumors = resident.courier ? (1u << resident.homeVillage) : 0u;
		resident.activity.occupation = static_cast<ResidentOccupation>(role);
		resident.activity.time = m_Rng.Range(0.0f, ResidentActivity::FISH_CYCLE);
		resident.dutyTimer = m_Rng.Range(ResidentActivity::PATROL_REST_MIN, ResidentActivity::PATROL_REST_MAX);

		const bool questNpc = resident.id == "elder" || resident.id == "smith"
			|| resident.id == "fisher" || resident.id == "child";

		if (!questNpc && m_Dialogue->Find(resident.id) == NULL)
		{
			AnalysisLog::Get().Event("village.dialogue_missing", resident.id);
			return false;
		}

		resident.walker.home = resident.walker.actor->GetWorldPosition();
		const int cell = Cell(resident.walker.home);

		if (cell < 0 || !m_Walkable[cell]
			|| m_World->IsBlocked(resident.walker.home.x, resident.walker.home.y, BODY_RADIUS)
			|| VillageAt(resident.walker.home) != resident.homeVillage)
		{
			AnalysisLog::Get().Event("village.npc_blocked", resident.id);
			return false;
		}

		resident.destination = cell;
		resident.walker.decision = m_Rng.Range(0.0f, 3.0f);
		m_Residents.push_back(resident);
		AnalysisLog::Get().Event("village.npc_profile", resident.id + ";behavior=" + behavior
			+ ";radius=" + std::to_string(resident.radius) + ";speed=" + std::to_string(resident.speed)
			+ ";occupation=" + occupation);
	}

	for (size_t village = 0; village < m_Settlements.size(); ++village)
	{
		int population = 0;
		int couriers = 0;
		int guards = 0;
		int healers = 0;

		for (const Resident& resident : m_Residents)
		{
			population += resident.homeVillage == static_cast<int>(village) ? 1 : 0;
			couriers += resident.homeVillage == static_cast<int>(village) && resident.courier ? 1 : 0;
			guards += resident.homeVillage == static_cast<int>(village) && resident.behavior == GUARD ? 1 : 0;
			healers += resident.homeVillage == static_cast<int>(village) && resident.behavior == HEALER ? 1 : 0;
		}

		if (population < MIN_RESIDENTS_PER_VILLAGE || couriers != COURIERS_PER_VILLAGE || guards == 0 || healers == 0)
		{
			return false;
		}
	}

	const char* const questResidents[] = { "elder", "smith", "fisher", "child" };

	for (const char* id : questResidents)
	{
		if (loaded.count(id) == 0)
		{
			AnalysisLog::Get().Event("village.quest_npc_missing", id);
			return false;
		}
	}

	return m_Residents.size() == actors.size();
}

int VillageSimulation::Cell(const ActorPosition& position) const
{
	const int x = static_cast<int>(floorf(position.x));
	const int y = static_cast<int>(floorf(position.y));

	if (x < 0 || y < 0 || x >= m_World->GetWidth() || y >= m_World->GetHeight())
	{
		return -1;
	}

	return y * m_World->GetWidth() + x;
}

ActorPosition VillageSimulation::Center(int cell) const
{
	const float x = static_cast<float>(cell % m_World->GetWidth()) + 0.5f;
	const float y = static_cast<float>(cell / m_World->GetWidth()) + 0.5f;

	return { x, y, m_World->GetElevation(x, y) };
}

bool VillageSimulation::Safe(const ActorPosition& position) const
{
	return VillageAt(position) >= 0;
}

void VillageSimulation::BuildNavigation()
{
	const int width = m_World->GetWidth();
	const int count = width * m_World->GetHeight();
	m_Walkable.assign(count, 0);
	m_Edges.assign(count, 0);
	m_Open.clear();
	m_Spawns.clear();

	for (Road& road : m_Roads)
	{
		road.spawns.clear();
	}
	m_Parents.resize(count);
	m_Queue.reserve(count);

	for (int i = 0; i < count; ++i)
	{
		const ActorPosition point = Center(i);
		m_Walkable[i] = !m_World->IsBlocked(point.x, point.y, BODY_RADIUS);
	}

	const ActorPosition start = { m_World->GetPlayerStartX(), m_World->GetPlayerStartY(), 0.0f };
	const int root = Cell(start);

	if (root < 0 || !m_Walkable[root])
	{
		return;
	}

	std::vector<unsigned char> reachable(count, 0);
	m_Open.push_back(root);
	reachable[root] = 1;
	const int offsets[] = { -1, 1, -width, width };

	// Static collision edges are baked once; runtime BFS only reads these bits.
	for (int cell = 0; cell < count; ++cell)
	{
		if (!m_Walkable[cell])
		{
			continue;
		}

		for (int direction = 0; direction < DIRECTION_COUNT; ++direction)
		{
			const int next = cell + offsets[direction];

			if (next >= 0 && next < count && abs(next % width - cell % width) <= 1
				&& m_Walkable[next] && ClearLine(Center(cell), Center(next)))
			{
				m_Edges[cell] |= static_cast<unsigned char>(1 << direction);
			}
		}
	}

	for (size_t cursor = 0; cursor < m_Open.size(); ++cursor)
	{
		const int cell = m_Open[cursor];

		for (int direction = 0; direction < DIRECTION_COUNT; ++direction)
		{
			const int next = cell + offsets[direction];

			if ((m_Edges[cell] & (1 << direction)) != 0 && !reachable[next])
			{
				reachable[next] = 1;
				m_Open.push_back(next);
			}
		}
	}

	m_Walkable.swap(reachable);

	for (int cell : m_Open)
	{
		const ActorPosition point = Center(cell);

		if (!Safe(point))
		{
			for (Road& road : m_Roads)
			{
				const ActorPosition a = m_Settlements[road.from].center;
				const ActorPosition b = m_Settlements[road.to].center;
				const float dx = b.x - a.x;
				const float dy = b.y - a.y;
				const float t = ((point.x - a.x) * dx + (point.y - a.y) * dy) / (dx * dx + dy * dy);
				const ActorPosition projected = { a.x + dx * t, a.y + dy * t, 0.0f };

				if (t > 0.0f && t < 1.0f && Distance(point, projected) <= ROAD_HALF_WIDTH)
				{
					road.spawns.push_back(cell);
					m_Spawns.push_back(cell);
					break;
				}
			}
		}
	}
}

bool VillageSimulation::ClearLine(const ActorPosition& a, const ActorPosition& b) const
{
	const int steps = static_cast<int>(ceilf(Distance(a, b) / COLLISION_SAMPLE_STEP));
	float previousX = a.x;
	float previousY = a.y;

	for (int i = 0; i <= steps; ++i)
	{
		const float t = steps == 0 ? 0.0f : static_cast<float>(i) / steps;

		const float x = a.x + (b.x - a.x) * t;
		const float y = a.y + (b.y - a.y) * t;

		if (!m_World->CanMove(previousX, previousY, x, y, BODY_RADIUS))
		{
			return false;
		}

		previousX = x;
		previousY = y;
	}

	return true;
}

int VillageSimulation::Nearby(const ActorPosition& origin, float radius)
{
	for (int retry = 0; retry < NEARBY_ATTEMPTS; ++retry)
	{
		ActorPosition candidate = { origin.x + m_Rng.Range(-radius, radius),
			origin.y + m_Rng.Range(-radius, radius), 0.0f };
		const int cell = Cell(candidate);

		if (cell >= 0 && m_Walkable[cell] && Distance(Center(cell), origin) <= radius)
		{
			return cell;
		}
	}

	return Cell(origin);
}

void VillageSimulation::Navigate(Walker& walker, int target, float speed, float deltaSeconds)
{
	walker.repath -= deltaSeconds;
	const int start = Cell(walker.actor->GetWorldPosition());
	const int width = m_World->GetWidth();

	if (target < 0 || start < 0 || !m_Walkable[target] || speed <= 0.0f)
	{
		return;
	}

	if (start == target && Distance(walker.actor->GetWorldPosition(), Center(target)) < ARRIVAL_DISTANCE)
	{
		walker.route.clear();
		return;
	}

	if (walker.repath <= 0.0f && (walker.plannedTarget != target || walker.route.empty()))
	{
		walker.plannedTarget = target;
		++m_PathRequests;
		walker.repath = REPATH_INTERVAL + m_Rng.Range(0.0f, 0.4f);
		walker.route.clear();
		walker.next = 0;
		std::fill(m_Parents.begin(), m_Parents.end(), -1);
		m_Queue.clear();
		m_Queue.push_back(start);
		m_Parents[start] = start;
		const int offsets[] = { -1, 1, -width, width };

		for (size_t cursor = 0; cursor < m_Queue.size() && m_Parents[target] < 0; ++cursor)
		{
			const int cell = m_Queue[cursor];

			for (int direction = 0; direction < DIRECTION_COUNT; ++direction)
			{
				const int next = cell + offsets[direction];

				if ((m_Edges[cell] & (1 << direction)) != 0 && m_Parents[next] < 0)
				{
					m_Parents[next] = cell;
					m_Queue.push_back(next);
				}
			}
		}

		if (m_Parents[target] >= 0)
		{
			for (int cell = target; cell != start; cell = m_Parents[cell])
			{
				walker.route.push_back(cell);
			}

			// Include the current cell centre so turns never cut solid corners.
			walker.route.push_back(start);
			std::reverse(walker.route.begin(), walker.route.end());

			if (walker.route.size() > 1 && ClearLine(walker.actor->GetWorldPosition(), Center(walker.route[1])))
			{
				walker.next = 1;
			}
		}
		else
		{
			++m_PathFailures;
		}
	}

	if (walker.next >= walker.route.size())
	{
		return;
	}

	ActorPosition position = walker.actor->GetPosition();
	const ActorPosition goal = Center(walker.route[walker.next]);
	const float distance = Distance(position, goal);

	if (distance < ARRIVAL_DISTANCE)
	{
		++walker.next;
		return;
	}

	const float step = (std::min)(speed * deltaSeconds, distance);
	ActorPosition next = { position.x + (goal.x - position.x) * step / distance,
		position.y + (goal.y - position.y) * step / distance, position.z };
	next.z = m_World->GetElevation(next.x, next.y);

	if (ClearLine(position, next))
	{
		walker.actor->SetPosition(next.x, next.y, next.z);
	}
}

void VillageSimulation::FindWorkTarget(Resident& resident)
{
	const ActorPosition home = resident.walker.home;
	const float offset = resident.activity.occupation == OCCUPATION_GARDEN ? 0.65f : 1.0f;
	resident.workTarget = { home.x + resident.activity.facingX * offset, home.y + resident.activity.facingY * offset, home.z };

	if (resident.activity.occupation == OCCUPATION_FISH)
	{
		float nearest = 4.5f;
		resident.taskAvailable = false;

		for (int y = (int)home.y - 5; y <= (int)home.y + 5; ++y)
		{
			for (int x = (int)home.x - 5; x <= (int)home.x + 5; ++x)
			{
				const ActorPosition water = { x + 0.5f, y + 0.5f, m_World->GetElevation(x + 0.5f, y + 0.5f) };
				const float distance = Distance(home, water);

				if (m_World->GetTile(x, y) == TILE_WATER && distance < nearest && fabsf(water.z - home.z) < 0.6f)
				{
					nearest = distance;
					resident.workTarget = water;
					resident.taskAvailable = true;
				}
			}
		}
	}
	else if (resident.activity.occupation == OCCUPATION_GARDEN)
	{
		resident.workTarget.z = m_World->GetElevation(resident.workTarget.x, resident.workTarget.y);
		resident.taskAvailable = !m_World->IsBlocked(resident.workTarget.x, resident.workTarget.y, BODY_RADIUS)
			&& fabsf(resident.workTarget.z - home.z) < 0.1f;
	}

	AnalysisLog::Get().Event("village.workstation", resident.id + ";occupation="
		+ ResidentActivity::Name(resident.activity.occupation) + ";available=" + (resident.taskAvailable ? "1" : "0"));
}

bool VillageSimulation::UpdateDuty(Resident& resident, float deltaSeconds)
{
	const ResidentOccupation occupation = resident.activity.occupation;
	Walker& walker = resident.walker;
	const ActorPosition position = walker.actor->GetWorldPosition();
	const bool gardener = occupation == OCCUPATION_GARDEN;
	const bool anchored = occupation == OCCUPATION_FISH || occupation == OCCUPATION_FORGE
		|| (gardener && resident.behavior == STAY);

	if (!resident.taskAvailable || (!anchored && !gardener && !ResidentActivity::IsPatrol(occupation)))
	{
		resident.activity.working = occupation == OCCUPATION_PRAY || occupation == OCCUPATION_READ;
		return false;
	}

	// A helpful farmer still approaches the player through the existing follow behavior.
	if (resident.behavior == FOLLOW && Distance(position, m_Player->GetWorldPosition()) < RESIDENT_NOTICE_RANGE
		&& Distance(walker.home, m_Player->GetWorldPosition()) < resident.radius)
	{
		return false;
	}

	if (anchored)
	{
		resident.activity.working = true;
		return true;
	}

	if (Distance(position, m_Player->GetWorldPosition()) < RESIDENT_TALK_RANGE)
	{
		resident.activity.working = ResidentActivity::IsPatrol(occupation);
		return true;
	}

	if (resident.atDuty)
	{
		if (gardener && Distance(position, walker.home) > ARRIVAL_DISTANCE)
		{
			const float distance = Distance(position, walker.home);

			if (distance > ResidentActivity::ARRIVAL_RANGE || Cell(position) != Cell(walker.home))
			{
				Navigate(walker, Cell(walker.home), resident.speed, deltaSeconds);
			}
			else
			{
				const float step = (std::min)(resident.speed * deltaSeconds / distance, 1.0f);
				const ActorPosition next = { position.x + (walker.home.x - position.x) * step,
					position.y + (walker.home.y - position.y) * step, walker.home.z };

				if (ClearLine(position, next))
				{
					walker.actor->SetPosition(next.x, next.y, next.z);
				}
			}

			return true;
		}

		resident.activity.working = true;
		resident.dutyTimer -= deltaSeconds;

		if (resident.dutyTimer <= 0.0f && resident.speed > 0.0f)
		{
			resident.atDuty = false;
			resident.returning = !resident.returning;
			resident.destination = resident.returning && !gardener ? Cell(walker.home)
				: Nearby(walker.home, gardener ? (std::min)(resident.radius, 1.5f) : resident.radius);
			walker.repath = 0.0f;
			resident.dutyTimer = 12.0f;
			AnalysisLog::Get().Event("village.duty", resident.id + ";travel");
		}

		return true;
	}

	Navigate(walker, resident.destination, resident.speed, deltaSeconds);
	resident.dutyTimer -= deltaSeconds;

	if (Distance(walker.actor->GetWorldPosition(), Center(resident.destination)) < ResidentActivity::ARRIVAL_RANGE
		|| resident.dutyTimer <= 0.0f)
	{
		resident.atDuty = true;
		resident.dutyTimer = m_Rng.Range(ResidentActivity::PATROL_REST_MIN, ResidentActivity::PATROL_REST_MAX);
		walker.repath = 0.0f;
		AnalysisLog::Get().Event("village.duty", resident.id + ";work_or_watch");
	}

	return true;
}

void VillageSimulation::UpdateResident(Resident& resident, float deltaSeconds)
{
	if (resident.courier)
	{
		resident.activity.working = false;
		resident.activity.alerted = false;
		UpdateCourier(resident, deltaSeconds);
		return;
	}

	Walker& walker = resident.walker;
	walker.cooldown -= deltaSeconds;
	walker.decision -= deltaSeconds;
	resident.afraid = (std::max)(0.0f, resident.afraid - deltaSeconds);
	const ActorPosition position = walker.actor->GetWorldPosition();
	const ActorPosition player = m_Player->GetWorldPosition();
	const float distance = Distance(position, player);
	resident.activity.working = false;
	resident.activity.alerted = false;

	if (resident.behavior == HEALER && distance < HEAL_RANGE && walker.cooldown <= 0.0f && m_Health < MAX_HEALTH)
	{
		m_Health = (std::min)(MAX_HEALTH, m_Health + HEAL_AMOUNT);
		walker.cooldown = HEAL_INTERVAL;
		AnalysisLog::Get().Event("village.heal", resident.id);
	}

	if (resident.behavior == GUARD)
	{
		for (size_t i = 0; i < m_Monsters.size(); ++i)
		{
			const Monster& monster = m_Monsters[i];
			const ActorPosition enemy = monster.walker.actor->GetWorldPosition();

			const bool hunter = resident.activity.occupation == OCCUPATION_HUNT;
			const float notice = hunter ? ResidentActivity::HUNT_RANGE : GUARD_NOTICE_RANGE;

			if (monster.health > 0.0f && Distance(position, enemy) < notice
				&& Distance(walker.home, enemy) < resident.radius + GUARD_NOTICE_RANGE)
			{
				resident.activity.alerted = true;
				const float enemyDistance = Distance(position, enemy);
				resident.activity.facingX = (enemy.x - position.x) / (std::max)(enemyDistance, 0.001f);
				resident.activity.facingY = (enemy.y - position.y) / (std::max)(enemyDistance, 0.001f);

				if (hunter && ClearLine(position, enemy))
				{
					if (walker.cooldown <= 0.0f)
					{
						resident.activity.BeginShot();
						walker.cooldown = ResidentActivity::HUNT_INTERVAL;
					}

					return;
				}

				if (!hunter && enemyDistance < GUARD_ATTACK_RANGE && walker.cooldown <= 0.0f && ClearLine(position, enemy))
				{
					HitMonster(i, GUARD_DAMAGE);
					walker.cooldown = GUARD_ATTACK_INTERVAL;
					resident.activity.strike = 0.5f;
				}

				resident.destination = Cell(enemy);
				Navigate(walker, resident.destination, resident.speed, deltaSeconds);
				return;
			}
		}
	}

	if (UpdateDuty(resident, deltaSeconds) || resident.behavior == STAY)
	{
		return;
	}

	if (resident.behavior == BANDIT && resident.afraid <= 0.0f && !Safe(player)
		&& distance < BANDIT_ATTACK_RANGE && walker.cooldown <= 0.0f && ClearLine(position, player))
	{
		HurtPlayer(BANDIT_DAMAGE);
		walker.cooldown = BANDIT_ATTACK_INTERVAL;
	}

	if (walker.decision <= 0.0f)
	{
		walker.decision = m_Rng.Range(RESIDENT_DECISION_MIN, RESIDENT_DECISION_MAX);
		resident.destination = Nearby(walker.home, resident.radius);

		if (resident.behavior == PATROL)
		{
			resident.returning = !resident.returning;
			resident.destination = resident.returning ? Cell(walker.home) : resident.destination;
			walker.decision = resident.radius * 2.0f / (std::max)(resident.speed, 0.1f) + 2.0f;
		}

		walker.repath = 0.0f;
	}

	const bool fleeing = resident.behavior == FLEE || resident.afraid > 0.0f;
	const bool approaching = resident.behavior == MERCHANT || resident.behavior == HEALER
		|| resident.behavior == FOLLOW || resident.behavior == BANDIT;

	if (fleeing && distance < 5.0f)
	{
		float best = distance;

		for (int i = 0; i < FLEE_CANDIDATES; ++i)
		{
			const int candidate = Nearby(walker.home, resident.radius);
			const float away = Distance(Center(candidate), player);

			if (away > best)
			{
				best = away;
				resident.destination = candidate;
			}
		}
	}
	else if (!fleeing && approaching && distance < RESIDENT_NOTICE_RANGE && Distance(player, walker.home) < resident.radius)
	{
		if (distance < (resident.behavior == BANDIT ? BANDIT_ATTACK_RANGE * 0.8f : RESIDENT_STOP_RANGE))
		{
			return;
		}

		resident.destination = Cell(player);
	}

	// Neutral residents leave room to interact; bandits can be repelled by Space.
	if (!fleeing && resident.behavior != BANDIT && distance < RESIDENT_TALK_RANGE)
	{
		return;
	}

	Navigate(walker, resident.destination, resident.speed, deltaSeconds);
}

bool VillageSimulation::SpawnMonster(size_t index, bool initial)
{
	Monster& monster = m_Monsters[index];
	const std::vector<int>& candidates = m_Roads[monster.road].spawns;

	if (candidates.empty())
	{
		monster.respawn = RESPAWN_RETRY;
		return false;
	}

	const size_t first = static_cast<size_t>(m_Rng.RangeInt(0, static_cast<int>(candidates.size()) - 1));

	for (size_t attempt = 0; attempt < candidates.size(); ++attempt)
	{
		const ActorPosition position = Center(candidates[(first + attempt) % candidates.size()]);

		if (Distance(position, m_Player->GetWorldPosition()) < RESPAWN_DISTANCE
			|| (!initial && Distance(position, monster.lastDeath) < RESPAWN_RELOCATION))
		{
			continue;
		}

		bool occupied = false;

		for (const Resident& resident : m_Residents)
		{
			if (Distance(position, resident.walker.actor->GetWorldPosition()) < ATTACK_RANGE
				|| Distance(position, resident.walker.home) < ATTACK_RANGE)
			{
				occupied = true;
				break;
			}
		}

		for (const Monster& other : m_Monsters)
		{
			if (other.health > 0.0f && Distance(position, other.walker.actor->GetWorldPosition()) < MONSTER_SPAWN_SEPARATION)
			{
				occupied = true;
				break;
			}
		}

		if (occupied)
		{
			continue;
		}

		monster.walker.actor->SetPosition(position.x, position.y, position.z);
		monster.walker.actor->SetVisible(true);
		monster.walker.home = position;
		monster.walker.route.clear();
		monster.walker.repath = m_Rng.Range(0.0f, REPATH_INTERVAL);
		monster.walker.cooldown = 1.0f;
		monster.health = PROFILES[monster.kind].health;
		monster.flash = 0.0f;
		monster.respawn = 0.0f;

		if (!initial)
		{
			AnalysisLog::Get().Event("village.monster_respawn", "slot=" + std::to_string(index)
				+ ";road=" + std::to_string(monster.road) + ";from_x=" + std::to_string(monster.lastDeath.x)
				+ ";from_y=" + std::to_string(monster.lastDeath.y) + ";to_x=" + std::to_string(position.x)
				+ ";to_y=" + std::to_string(position.y));
		}

		return true;
	}

	monster.respawn = RESPAWN_RETRY;
	AnalysisLog::Get().Event("village.spawn_deferred", "slot=" + std::to_string(index));

	return false;
}

void VillageSimulation::UpdateMonster(size_t index, float deltaSeconds)
{
	Monster& monster = m_Monsters[index];
	Walker& walker = monster.walker;

	if (monster.health <= 0.0f)
	{
		monster.respawn -= deltaSeconds;

		if (monster.respawn <= 0.0f)
		{
			SpawnMonster(index, false);
		}

		return;
	}

	walker.cooldown -= deltaSeconds;
	monster.flash = (std::max)(0.0f, monster.flash - deltaSeconds);
	const ActorPosition player = m_Player->GetWorldPosition();
	const ActorPosition position = walker.actor->GetWorldPosition();
	const MonsterProfile& profile = PROFILES[monster.kind];
	const float distance = Distance(position, player);
	const bool chasing = !Safe(player) && distance < MONSTER_NOTICE_RANGE && Distance(player, walker.home) < MONSTER_LEASH_RANGE;

	if (chasing && distance < MONSTER_ATTACK_RANGE && walker.cooldown <= 0.0f && ClearLine(position, player))
	{
		HurtPlayer(profile.damage);
		walker.cooldown = MONSTER_ATTACK_INTERVAL;
	}

	Navigate(walker, Cell(chasing ? player : walker.home), profile.speed, deltaSeconds);
}

void VillageSimulation::HurtPlayer(float damage)
{
	if (m_Invulnerable > 0.0f)
	{
		return;
	}

	m_Health -= damage;
	m_Invulnerable = DAMAGE_GRACE_TIME;

	if (m_Health <= 0.0f)
	{
		m_Player->SetPosition(m_World->GetPlayerStartX(), m_World->GetPlayerStartY(),
			m_World->GetElevation(m_World->GetPlayerStartX(), m_World->GetPlayerStartY()));
		m_Health = MAX_HEALTH;
		m_Invulnerable = RECOVERY_GRACE_TIME;
		m_Bow.Clear();
		m_AttackMotion.Clear();
		AnalysisLog::Get().Event("village.player_recovered", "sanctuary;no_story_reset");
	}
}

void VillageSimulation::HitMonster(size_t index, float damage)
{
	Monster& monster = m_Monsters[index];
	monster.health -= damage;
	monster.flash = HIT_FLASH_TIME;

	if (monster.health <= 0.0f)
	{
		monster.walker.actor->SetVisible(false);
		monster.lastDeath = monster.walker.actor->GetWorldPosition();
		monster.respawn = m_Rng.Range(MONSTER_RESPAWN_MIN, MONSTER_RESPAWN_MAX);
		++m_Kills;
		AnalysisLog::Get().Event("village.monster_defeated", "slot=" + std::to_string(index));
	}
}

void VillageSimulation::Attack()
{
	if (m_AttackCooldown > 0.0f)
	{
		return;
	}

	const bool bow = m_Weapon == WEAPON_BOW;
	m_AttackCooldown = bow ? BOW_INTERVAL : ATTACK_INTERVAL;
	float facingX = m_FacingX;
	float facingY = m_FacingY;

	if (bow)
	{
		m_Bow.AimDirection(m_Player->GetWorldPosition(), facingX, facingY);
	}

	m_AttackMotion.Begin(bow, m_AttackCooldown, facingX, facingY, true);
	AnalysisLog::Get().Event("combat.attack_start", bow ? "village;bow" : "village;sword");
}

void VillageSimulation::SetFacing(float x, float y)
{
	m_FacingX = x;
	m_FacingY = y;
}

void VillageSimulation::ResolveAttack()
{
	const ActorPosition player = m_Player->GetWorldPosition();
	AnalysisLog::Get().Event("combat.attack_impact", m_AttackMotion.bow ? "village;bow" : "village;sword");

	if (m_AttackMotion.bow)
	{
		float x = m_AttackMotion.directionX;
		float y = m_AttackMotion.directionY;
		m_Bow.AimDirection(player, x, y);
		m_AttackMotion.SetDirection(x, y);
		m_Bow.Shoot(player, PLAYER_DAMAGE);
		return;
	}

	for (size_t i = 0; i < m_Monsters.size(); ++i)
	{
		const Monster& monster = m_Monsters[i];
		const ActorPosition position = monster.walker.actor->GetWorldPosition();

		if (monster.health > 0.0f && Distance(player, position) < ATTACK_RANGE && ClearLine(player, position))
		{
			HitMonster(i, PLAYER_DAMAGE);
		}
	}

	for (Resident& resident : m_Residents)
	{
		if (resident.behavior == BANDIT && Distance(player, resident.walker.actor->GetWorldPosition()) < ATTACK_RANGE
			&& ClearLine(player, resident.walker.actor->GetWorldPosition()))
		{
			resident.afraid = BANDIT_RETREAT_TIME;
			resident.walker.repath = 0.0f;
			AnalysisLog::Get().Event("village.bandit_repelled", resident.id);
		}
	}
}

void VillageSimulation::SelectWeapon(WeaponKind weapon)
{
	if (m_Weapon != weapon)
	{
		if (m_AttackMotion.active && !m_AttackMotion.impactApplied)
		{
			AnalysisLog::Get().Event("combat.attack_cancel", "village;weapon_switch");
		}

		m_Weapon = weapon;
		m_AttackMotion.Clear();
		AnalysisLog::Get().Event("combat.weapon", weapon == WEAPON_BOW ? "village;bow" : "village;sword");
	}
}

void VillageSimulation::Update(float deltaSeconds)
{
	m_AttackCooldown = (std::max)(0.0f, m_AttackCooldown - deltaSeconds);
	m_Invulnerable = (std::max)(0.0f, m_Invulnerable - deltaSeconds);

	if (m_AttackMotion.Advance(deltaSeconds))
	{
		ResolveAttack();
	}

	for (Resident& resident : m_Residents)
	{
		const ActorPosition before = resident.walker.actor->GetWorldPosition();
		UpdateResident(resident, deltaSeconds);
		const ActorPosition after = resident.walker.actor->GetWorldPosition();
		resident.moving = before.x != after.x || before.y != after.y;
		resident.activity.movement.Advance(deltaSeconds, after.x - before.x, after.y - before.y, false);
		resident.activity.working = resident.activity.working && !resident.moving;

		if (resident.moving && !resident.activity.alerted)
		{
			const float length = Distance(before, after);
			resident.activity.facingX = (after.x - before.x) / length;
			resident.activity.facingY = (after.y - before.y) / length;
		}
		else if (resident.activity.working && (resident.activity.occupation == OCCUPATION_GARDEN
			|| resident.activity.occupation == OCCUPATION_FISH))
		{
			const float length = Distance(after, resident.workTarget);
			resident.activity.facingX = (resident.workTarget.x - after.x) / (std::max)(length, 0.001f);
			resident.activity.facingY = (resident.workTarget.y - after.y) / (std::max)(length, 0.001f);
		}

		if (resident.activity.Advance(deltaSeconds) && resident.activity.alerted)
		{
			if (m_HuntingBow.Shoot(after, GUARD_DAMAGE))
			{
				AnalysisLog::Get().Event("village.hunter_shot", resident.id);
			}
		}
	}

	unsigned int alive = 0;

	for (size_t i = 0; i < m_Monsters.size(); ++i)
	{
		UpdateMonster(i, deltaSeconds);
		alive += m_Monsters[i].health > 0.0f ? 1 : 0;
	}

	m_LogTimer += deltaSeconds;
	m_ContactTimer += deltaSeconds;

	if (m_ContactTimer >= CONTACT_INTERVAL)
	{
		ShareRumors();
		UpdateAtmosphere();
		m_ContactTimer = 0.0f;
	}

	if (m_LogTimer >= LOG_INTERVAL)
	{
		AnalysisLog::Get().Event("village.population", "alive=" + std::to_string(alive)
			+ ";kills=" + std::to_string(m_Kills) + ";path_requests=" + std::to_string(m_PathRequests)
			+ ";path_failures=" + std::to_string(m_PathFailures));
		m_LogTimer = 0.0f;

		for (const Settlement& village : m_Settlements)
		{
			const MonitoredVillage& stats = village.statistics;
			AnalysisLog::Get().Event("village.atmosphere", village.id + ";mood=" + stats.mood
				+ ";fear=" + std::to_string(stats.fear) + ";trust=" + std::to_string(stats.trust)
				+ ";prosperity=" + std::to_string(stats.prosperity) + ";home_npcs=" + std::to_string(stats.homeNpcCount)
				+ ";present_npcs=" + std::to_string(stats.presentNpcCount) + ";travelers=" + std::to_string(stats.travelers)
				+ ";known_rumors=" + std::to_string(stats.knownRumors) + ";nearby_monsters=" + std::to_string(stats.nearbyMonsters));
		}

		m_PathRequests = m_PathFailures = 0;
	}
}

void VillageSimulation::SetState(bool active, bool visible)
{
	if (m_Group != NULL)
	{
		m_Group->SetActive(active);
		m_Group->SetVisible(visible);
	}
}

void VillageSimulation::CaptureMonitoring(MonitoredWorld& snapshot) const
{
	for (const Settlement& village : m_Settlements)
	{
		snapshot.villages.push_back(village.statistics);
	}

	snapshot.kills = m_Kills;
	const bool simulating = snapshot.active && snapshot.state == "play";

	for (const Resident& resident : m_Residents)
	{
		MonitoredEntity entity;
		entity.id = "npc:" + resident.id;
		const int currentVillage = VillageAt(resident.walker.actor->GetWorldPosition());
		entity.villageId = currentVillage < 0 ? "" : m_Settlements[currentVillage].id;
		entity.kind = "resident";
		entity.behavior = BEHAVIORS[resident.behavior];
		entity.state = !simulating ? "paused" : resident.moving ? "moving" : "idle";

		if (simulating && !resident.moving && resident.activity.working)
		{
			entity.state = ResidentActivity::Name(resident.activity.occupation);
		}
		else if (simulating && resident.activity.alerted)
		{
			entity.state = "alert";
		}

		if (simulating && resident.moving && (resident.afraid > 0.0f || resident.behavior == FLEE))
		{
			entity.state = "fleeing";
		}
		else if (simulating && resident.moving && resident.returning)
		{
			entity.state = "returning";
		}

		entity.position = resident.walker.actor->GetWorldPosition();

		if (simulating && resident.courier)
		{
			entity.state = resident.traveling ? "traveling" : "sharing_news";
		}

		snapshot.npcs.push_back(entity);
	}

	const char* const monsterKinds[] = { "wisp", "shade", "oni" };

	for (size_t i = 0; i < m_Monsters.size(); ++i)
	{
		const Monster& monster = m_Monsters[i];
		MonitoredEntity entity;
		entity.id = "monster:" + std::to_string(i);
		entity.villageId = "";
		entity.kind = monsterKinds[monster.kind];
		entity.behavior = "hostile";
		entity.alive = monster.health > 0.0f;
		entity.state = !entity.alive ? "respawning" : simulating ? "alive" : "paused";
		entity.position = monster.walker.actor->GetWorldPosition();
		entity.hasHealth = true;
		entity.health = (std::max)(0.0f, monster.health);
		entity.maxHealth = PROFILES[monster.kind].health;
		entity.respawnSeconds = (std::max)(0.0f, monster.respawn);
		snapshot.monsters.push_back(entity);
	}
}

bool VillageSimulation::IsArmed(const std::string& id) const
{
	for (const Resident& resident : m_Residents)
	{
		if (resident.id == id)
		{
			return resident.behavior == GUARD || resident.behavior == BANDIT;
		}
	}

	return false;
}

bool VillageSimulation::DrawResident(const std::string& id, Renderer& renderer, const Color& robe, float scale, bool hat)
{
	for (const Resident& resident : m_Residents)
	{
		if (resident.id != id)
		{
			continue;
		}

		const ActorPosition position = resident.walker.actor->GetWorldPosition();
		float x, y, targetX, targetY;
		renderer.WorldToScreen(position.x, position.y, position.z, &x, &y);
		renderer.WorldToScreen(resident.workTarget.x, resident.workTarget.y, resident.workTarget.z, &targetX, &targetY);
		ResidentActivityMotion motion = resident.activity;

		if (!resident.taskAvailable)
		{
			motion.occupation = OCCUPATION_NONE;
		}

		DrawResidentArt(renderer, *m_Models, x, y, position.x + position.y, scale,
			m_Lighting->Shade(position.x, position.y, robe), hat,
			resident.behavior == GUARD || resident.behavior == BANDIT, motion, targetX, targetY);
		return true;
	}

	return false;
}

void VillageSimulation::AddResidentLights(Lighting& lighting) const
{
	for (const Resident& resident : m_Residents)
	{
		if (resident.activity.occupation == OCCUPATION_TORCH && resident.walker.actor->IsVisibleInHierarchy())
		{
			const ActorPosition position = resident.walker.actor->GetWorldPosition();
			lighting.AddLight(position.x, position.y, 3.6f, 1.1f, TORCH_LIGHT_COLOR, true);
		}
	}
}

void VillageSimulation::DrawMonster(size_t index, Renderer& renderer)
{
	const Monster& monster = m_Monsters[index];
	const ActorPosition position = monster.walker.actor->GetWorldPosition();
	const MonsterProfile& profile = PROFILES[monster.kind];
	float x, y;
	renderer.WorldToScreen(position.x, position.y, position.z, &x, &y);
	const Color tint = monster.flash > 0.0f ? RGBA(1.0f, 0.35f, 0.25f) : RGBA(1.0f, 1.0f, 1.0f);
	DrawModel(&renderer, *m_Models->Find(profile.model), x, y, 1.0f, position.x + position.y,
		m_Lighting->Shade(position.x, position.y, tint));
	renderer.PushRect(x - 16.0f, y - 64.0f, 32.0f, 3.0f, RGBA(0.1f, 0.05f, 0.05f), position.x + position.y + 1.0f);
	renderer.PushRect(x - 16.0f, y - 64.0f, 32.0f * monster.health / profile.health, 3.0f,
		RGBA(0.8f, 0.22f, 0.15f), position.x + position.y + 1.01f);
}

void VillageSimulation::DrawSwing(Renderer& renderer)
{
	if (!m_AttackMotion.active || m_AttackMotion.bow)
	{
		return;
	}

	const ActorPosition player = m_Player->GetWorldPosition();
	float x, y;
	renderer.WorldToScreen(player.x, player.y, player.z, &x, &y);
	DrawFoxSlash(&renderer, x, y, player.x + player.y, m_AttackMotion);
}

void VillageSimulation::DrawHud(Renderer& renderer)
{
	const float x = -renderer.GetWidth() * 0.5f + VisualStyle::HUD_MARGIN;
	const float y = -renderer.GetHeight() * 0.5f + VisualStyle::HUD_MARGIN;
	const float weaponY = y + 24.0f;
	const float labelX = x + (VisualStyle::HUD_WEAPON_SIZE + VisualStyle::HUD_WEAPON_GAP) * 2.0f + 6.0f;
	const bool bow = m_Weapon == WEAPON_BOW;
	const bool safe = Safe(m_Player->GetWorldPosition());
	const int currentVillage = VillageAt(m_Player->GetWorldPosition());
	const std::wstring status = std::to_wstring(static_cast<int>(m_Health)) + L" / "
		+ std::to_wstring(static_cast<int>(MAX_HEALTH));

	VisualStyle::DrawPanel(&renderer, x - 8.0f, y - 8.0f,
		VisualStyle::HUD_WIDTH + 16.0f, VisualStyle::HUD_HEIGHT, 0.38f, DRAW_DEPTH);
	VisualStyle::DrawHealth(&renderer, x, y + 4.0f, VisualStyle::HUD_HEALTH_WIDTH,
		VisualStyle::HUD_HEALTH_HEIGHT, m_Health / MAX_HEALTH, DRAW_DEPTH + 0.2f);
	renderer.PushText(status, x + VisualStyle::HUD_WIDTH, y, VisualStyle::HUD_SMALL_FONT,
		FONT_UI, true, VisualStyle::COL_IVORY, ALIGN_RIGHT);
	VisualStyle::DrawWeaponSlots(&renderer, x, weaponY, bow, DRAW_DEPTH + 0.2f);
	renderer.PushText(m_Dialogue->Line(bow ? "ui_hud_bow" : "ui_hud_sword"), labelX, weaponY + 1.0f,
		VisualStyle::HUD_SMALL_FONT, FONT_UI, true, VisualStyle::COL_IVORY, ALIGN_LEFT);
	renderer.PushText(m_Dialogue->Line(safe ? "ui_hud_safe" : "ui_hud_outskirts"), labelX, weaponY + 19.0f,
		VisualStyle::HUD_SMALL_FONT, FONT_UI, false, safe ? VisualStyle::COL_MOSS : RGBA(0.88f, 0.64f, 0.46f), ALIGN_LEFT);

	if (currentVillage >= 0)
	{
		const Settlement& village = m_Settlements[currentVillage];
		renderer.PushText(m_Dialogue->Line("town_" + village.id) + L" / " + m_Dialogue->Line("mood_" + village.statistics.mood),
			x, y + VisualStyle::HUD_HEIGHT + 4.0f, VisualStyle::HUD_SMALL_FONT, FONT_UI, false,
			VisualStyle::COL_IVORY, ALIGN_LEFT);
	}
}
