#include "stdafx.h"
#include "VillageSimulation.h"
#include "AnalysisLog.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <set>

namespace
{
	const int MONSTER_COUNT = 72;
	const float BODY_RADIUS = 0.28f;
	const float MAX_HEALTH = 100.0f;
	const float ATTACK_RANGE = 2.4f;
	const float ATTACK_INTERVAL = 0.45f;
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
	const float MONSTER_RESPAWN_MIN = 5.0f;
	const float MONSTER_RESPAWN_MAX = 9.0f;
	const float DAMAGE_GRACE_TIME = 0.7f;
	const float RECOVERY_GRACE_TIME = 3.0f;
	const float HIT_FLASH_TIME = 0.2f;
	const float SWING_TIME = 0.18f;
	const BlockRect SAFE_BOUNDS = { 28.0f, 23.0f, 59.0f, 47.0f };
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
	m_AttackCooldown = m_Invulnerable = m_Swing = m_LogTimer = 0.0f;
	BuildNavigation();

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

	m_Group = graph.CreateActor("VillagePopulation");
	m_Group->SetUpdateCallback([this](Actor&, float deltaSeconds)
	{
		Update(deltaSeconds);
	});
	m_Monsters.resize(MONSTER_COUNT);

	for (size_t i = 0; i < m_Monsters.size(); ++i)
	{
		Monster& monster = m_Monsters[i];
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

	Actor* swing = graph.CreateActor("VillageAttack", player);
	swing->SetRenderBounds(-130.0f, -80.0f, 130.0f, 80.0f);
	swing->SetRenderCallback([this](const Actor&, Renderer& renderer)
	{
		DrawSwing(renderer);
	});
	AnalysisLog::Get().Event("village.ready", "width=" + std::to_string(world.GetWidth())
		+ ";height=" + std::to_string(world.GetHeight()) + ";npcs=" + std::to_string(m_Residents.size())
		+ ";monsters=" + std::to_string(MONSTER_COUNT) + ";reachable=" + std::to_string(m_Open.size()));

	return true;
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
		std::string extra;
		std::istringstream row(line);

		if (!(row >> resident.id >> behavior >> resident.radius >> resident.speed)
			|| (row >> extra) || !loaded.insert(resident.id).second
			|| !std::isfinite(resident.radius) || !std::isfinite(resident.speed)
			|| resident.radius < 0.0f || resident.radius > MAX_RESIDENT_RADIUS
			|| resident.speed < 0.0f || resident.speed > MAX_RESIDENT_SPEED)
		{
			return false;
		}

		int kind = -1;

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

		if (kind < 0 || resident.walker.actor == NULL)
		{
			return false;
		}

		resident.behavior = static_cast<Behavior>(kind);

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
			|| m_World->IsBlocked(resident.walker.home.x, resident.walker.home.y, BODY_RADIUS))
		{
			AnalysisLog::Get().Event("village.npc_blocked", resident.id);
			return false;
		}

		resident.destination = cell;
		resident.walker.decision = m_Rng.Range(0.0f, 3.0f);
		m_Residents.push_back(resident);
		AnalysisLog::Get().Event("village.npc_profile", resident.id + ";behavior=" + behavior
			+ ";radius=" + std::to_string(resident.radius) + ";speed=" + std::to_string(resident.speed));
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
	return { static_cast<float>(cell % m_World->GetWidth()) + 0.5f,
		static_cast<float>(cell / m_World->GetWidth()) + 0.5f, 0.0f };
}

bool VillageSimulation::Safe(const ActorPosition& position) const
{
	// The original settlement and all three clue witnesses remain a sanctuary.
	return position.x >= SAFE_BOUNDS.minX && position.x <= SAFE_BOUNDS.maxX
		&& position.y >= SAFE_BOUNDS.minY && position.y <= SAFE_BOUNDS.maxY;
}

void VillageSimulation::BuildNavigation()
{
	const int width = m_World->GetWidth();
	const int count = width * m_World->GetHeight();
	m_Walkable.assign(count, 0);
	m_Edges.assign(count, 0);
	m_Open.clear();
	m_Spawns.clear();
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

		if (!Safe(point) && Distance(point, start) >= 16.0f)
		{
			m_Spawns.push_back(cell);
		}
	}
}

bool VillageSimulation::ClearLine(const ActorPosition& a, const ActorPosition& b) const
{
	const int steps = static_cast<int>(ceilf(Distance(a, b) / COLLISION_SAMPLE_STEP));

	for (int i = 0; i <= steps; ++i)
	{
		const float t = steps == 0 ? 0.0f : static_cast<float>(i) / steps;

		if (m_World->IsBlocked(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, BODY_RADIUS))
		{
			return false;
		}
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

	if (walker.repath <= 0.0f)
	{
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

	if (ClearLine(position, next))
	{
		walker.actor->SetPosition(next.x, next.y, next.z);
	}
}

void VillageSimulation::UpdateResident(Resident& resident, float deltaSeconds)
{
	Walker& walker = resident.walker;
	walker.cooldown -= deltaSeconds;
	walker.decision -= deltaSeconds;
	resident.afraid = (std::max)(0.0f, resident.afraid - deltaSeconds);
	const ActorPosition position = walker.actor->GetWorldPosition();
	const ActorPosition player = m_Player->GetWorldPosition();
	const float distance = Distance(position, player);

	if (resident.behavior == STAY)
	{
		return;
	}

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

			if (monster.health > 0.0f && Distance(position, enemy) < GUARD_NOTICE_RANGE
				&& Distance(walker.home, enemy) < resident.radius + GUARD_NOTICE_RANGE)
			{
				if (Distance(position, enemy) < GUARD_ATTACK_RANGE && walker.cooldown <= 0.0f && ClearLine(position, enemy))
				{
					HitMonster(i, GUARD_DAMAGE);
					walker.cooldown = GUARD_ATTACK_INTERVAL;
				}

				resident.destination = Cell(enemy);
				Navigate(walker, resident.destination, resident.speed, deltaSeconds);
				return;
			}
		}
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
	const size_t first = static_cast<size_t>(m_Rng.RangeInt(0, static_cast<int>(m_Spawns.size()) - 1));

	for (size_t attempt = 0; attempt < m_Spawns.size(); ++attempt)
	{
		const ActorPosition position = Center(m_Spawns[(first + attempt) % m_Spawns.size()]);

		if (Distance(position, m_Player->GetWorldPosition()) < RESPAWN_DISTANCE)
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

		monster.walker.actor->SetPosition(position.x, position.y);
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
			AnalysisLog::Get().Event("village.monster_respawn", "slot=" + std::to_string(index));
		}

		return true;
	}

	monster.respawn = 1.0f;
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
		m_Player->SetPosition(m_World->GetPlayerStartX(), m_World->GetPlayerStartY());
		m_Health = MAX_HEALTH;
		m_Invulnerable = RECOVERY_GRACE_TIME;
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

	m_AttackCooldown = ATTACK_INTERVAL;
	m_Swing = SWING_TIME;
	const ActorPosition player = m_Player->GetWorldPosition();

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

void VillageSimulation::Update(float deltaSeconds)
{
	m_AttackCooldown = (std::max)(0.0f, m_AttackCooldown - deltaSeconds);
	m_Invulnerable = (std::max)(0.0f, m_Invulnerable - deltaSeconds);
	m_Swing = (std::max)(0.0f, m_Swing - deltaSeconds);

	for (Resident& resident : m_Residents)
	{
		UpdateResident(resident, deltaSeconds);
	}

	unsigned int alive = 0;

	for (size_t i = 0; i < m_Monsters.size(); ++i)
	{
		UpdateMonster(i, deltaSeconds);
		alive += m_Monsters[i].health > 0.0f ? 1 : 0;
	}

	m_LogTimer += deltaSeconds;

	if (m_LogTimer >= LOG_INTERVAL)
	{
		AnalysisLog::Get().Event("village.population", "alive=" + std::to_string(alive)
			+ ";kills=" + std::to_string(m_Kills) + ";path_requests=" + std::to_string(m_PathRequests)
			+ ";path_failures=" + std::to_string(m_PathFailures));
		m_LogTimer = 0.0f;
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
	if (m_Swing <= 0.0f)
	{
		return;
	}

	const ActorPosition player = m_Player->GetWorldPosition();
	float x, y;
	renderer.WorldToScreen(player.x, player.y, 0.0f, &x, &y);
	renderer.PushEllipse(x, y - 10.0f, ATTACK_RANGE * 40.0f, ATTACK_RANGE * 20.0f,
		RGBA(0.8f, 0.85f, 0.95f, m_Swing * 1.4f), player.x + player.y + 0.5f, 24);
}

void VillageSimulation::DrawHud(Renderer& renderer)
{
	const float x = renderer.GetWidth() * 0.5f - 24.0f;
	const float y = -renderer.GetHeight() * 0.5f + 24.0f;
	const std::wstring status = m_Dialogue->Line("ui_village_health") + L" "
		+ std::to_wstring(static_cast<int>(m_Health)) + L" / 100";
	renderer.PushRect(x - 242.0f, y - 6.0f, 252.0f, 82.0f, RGBA(0.04f, 0.05f, 0.07f, 0.9f), DRAW_DEPTH);
	renderer.PushText(status, x, y, 16, FONT_UI, true, RGBA(0.9f, 0.8f, 0.7f), ALIGN_RIGHT);
	renderer.PushText(m_Dialogue->Line("ui_village_combat"), x, y + 25.0f, 14, FONT_UI, false,
		RGBA(0.8f, 0.8f, 0.7f), ALIGN_RIGHT);
	renderer.PushText(m_Dialogue->Line("ui_village_region"), x, y + 49.0f, 13, FONT_UI, false,
		RGBA(0.7f, 0.8f, 0.7f), ALIGN_RIGHT);
}
