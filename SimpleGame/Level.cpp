#include "stdafx.h"
#include "Level.h"

#include <cmath>
#include <cstdio>
#include <iostream>

namespace
{
	const float TWO_PI = 6.2831853f;

	const int MAP_WIDTH = 56;
	const int MAP_HEIGHT = 46;
	const int CHUNK_SIZE = 8;
	const float GROUND_BOUND_HALF_WIDTH = 35.0f;
	const float GROUND_BOUND_HALF_HEIGHT = 19.0f;
	const float SCENERY_BOUND_HALF_WIDTH = 100.0f;
	const float SCENERY_BOUND_TOP = 160.0f;
	const float SCENERY_BOUND_BOTTOM = 40.0f;
	const float PLAYER_BOUND_HALF_WIDTH = 60.0f;
	const float PLAYER_BOUND_TOP = 110.0f;
	const float PLAYER_BOUND_BOTTOM = 35.0f;
	const float SWING_BOUND_HALF_WIDTH = 64.0f;
	const float SWING_BOUND_TOP = 56.0f;
	const float SWING_BOUND_BOTTOM = 40.0f;
	const float ENEMY_BOUND_HALF_WIDTH = 80.0f;
	const float ENEMY_BOUND_TOP = 130.0f;
	const float ENEMY_BOUND_BOTTOM = 48.0f;
	const float PICKUP_BOUND_HALF_WIDTH = 32.0f;
	const float PICKUP_BOUND_TOP = 48.0f;
	const float PICKUP_BOUND_BOTTOM = 24.0f;
	const float FLOATING_TEXT_BOUND_HORIZONTAL_MARGIN = 4.0f;
	const float FLOATING_TEXT_BOUND_TOP = 48.0f;
	const float FLOATING_TEXT_BOUND_HEIGHT_SCALE = 2.0f;
	const float FLOATING_TEXT_HEIGHT_OFFSET = 1.1f;

	const int ENEMY_POPULATION = 16;
	const float RESPAWN_MIN = 5.0f;
	const float RESPAWN_MAX = 9.0f;
	const float RESPAWN_MIN_DISTANCE = 11.0f;
	const float SPAWN_RETRY_DELAY = 1.0f;

	const float PLAYER_RADIUS = 0.28f;
	const float ENEMY_RADIUS = 0.25f;
	const float ATTACK_RANGE = 1.7f;
	const float ATTACK_ARC = 0.20f;			// dot product threshold, not an angle
	const float CRIT_CHANCE = 0.12f;
	const float CRIT_MULTIPLIER = 1.8f;
	const float SWING_DURATION = 0.18f;
	const float INVULNERABLE_TIME = 0.45f;

	const float ORB_MAGNET_RANGE = 2.6f;
	const float ORB_MAGNET_SPEED = 5.5f;
	const float PICKUP_RANGE = 0.65f;
	const float PICKUP_LIFETIME = 45.0f;
	const float PICKUP_SCATTER = 0.3f;

	const float POTION_HEAL = 34.0f;
	const float POTION_CHANCE = 0.18f;
	const float WHETSTONE_CHANCE = 0.05f;

	const float DOWNED_DURATION = 2.4f;
	const float RESPAWN_HEALTH_FRACTION = 0.6f;

	// --- palette ---------------------------------------------------------
	const Color COL_GRASS = { 0.34f, 0.41f, 0.30f, 1.0f };
	const Color COL_TALLGRASS = { 0.29f, 0.37f, 0.27f, 1.0f };
	const Color COL_DIRT = { 0.47f, 0.40f, 0.30f, 1.0f };
	const Color COL_STONE = { 0.47f, 0.48f, 0.46f, 1.0f };
	const Color COL_WATER = { 0.20f, 0.30f, 0.39f, 1.0f };
	const Color COL_SAND = { 0.56f, 0.51f, 0.41f, 1.0f };

	const Color COL_SHU = { 0.66f, 0.24f, 0.16f, 1.0f };
	const Color COL_SKY = { 0.035f, 0.048f, 0.068f, 1.0f };
	const Color COL_FOG = { 0.055f, 0.075f, 0.105f, 1.0f };
	const Color COL_INK = { 0.045f, 0.055f, 0.070f, 1.0f };
	const Color COL_FLAME = { 1.0f, 0.70f, 0.36f, 1.0f };
	const Color COL_WISP_LIGHT = { 0.50f, 1.0f, 0.85f, 1.0f };

	const Color COL_PLAYER_ROBE = { 0.20f, 0.24f, 0.31f, 1.0f };
	const Color COL_DAMAGE_DEALT = { 0.93f, 0.91f, 0.86f, 1.0f };
	const Color COL_DAMAGE_CRIT = { 0.98f, 0.80f, 0.36f, 1.0f };
	const Color COL_DAMAGE_TAKEN = { 0.92f, 0.38f, 0.30f, 1.0f };
	const Color COL_EXPERIENCE = { 0.66f, 0.85f, 1.0f, 1.0f };
	const Color COL_HEAL = { 0.55f, 0.86f, 0.55f, 1.0f };

	const float DEPTH_OVERLAY = 900000.0f;
	const float DEPTH_PANEL = 920000.0f;
	const float DEPTH_PANEL_TOP = 930000.0f;

	struct EnemyProfile
	{
		const char* model;
		float health;
		float damage;
		float speed;
		float aggroRange;
		float attackRange;
		float attackInterval;
		float scale;
		int experience;
		bool glows;
	};

	const EnemyProfile ENEMY_PROFILES[ENEMY_KIND_COUNT] =
	{
		{ "enemy_wisp",  18.0f,  5.0f, 3.30f, 8.0f, 0.95f, 1.1f, 0.95f,  8, true },
		{ "enemy_shade", 34.0f,  9.0f, 2.40f, 7.0f, 1.05f, 1.4f, 1.00f, 14, false },
		{ "enemy_oni",   70.0f, 18.0f, 1.75f, 6.5f, 1.25f, 1.9f, 1.10f, 30, false }
	};

	const char* const STAT_NAME_KEYS[STAT_COUNT] =
	{
		"ui_lv_stat_vit",
		"ui_lv_stat_str",
		"ui_lv_stat_grd",
		"ui_lv_stat_agi"
	};

	const char* const STAT_DESC_KEYS[STAT_COUNT] =
	{
		"ui_lv_desc_vit",
		"ui_lv_desc_str",
		"ui_lv_desc_grd",
		"ui_lv_desc_agi"
	};

	float Clamp(float value, float low, float high)
	{
		if (value < low)
		{
			return low;
		}

		if (value > high)
		{
			return high;
		}

		return value;
	}

	void SetWorldPosition(Actor& actor, float x, float y, float z)
	{
		const ActorPosition local = actor.GetPosition();
		const ActorPosition world = actor.GetWorldPosition();

		actor.SetPosition(x - world.x + local.x, y - world.y + local.y, z - world.z + local.z);
	}

	void SetWorldPosition(Actor& actor, float x, float y)
	{
		SetWorldPosition(actor, x, y, actor.GetWorldPosition().z);
	}

	unsigned int HashInt(int x, int y)
	{
		unsigned int h = (unsigned int)(x * 73856093) ^ (unsigned int)(y * 19349663);
		h ^= h >> 13;
		h *= 1274126177u;
		h ^= h >> 16;

		return h;
	}
}

Level::Level()
	: m_Renderer(NULL)
	, m_Models(NULL)
	, m_Dialogue(NULL)
	, m_SimulationActor(NULL)
	, m_PlayerActor(NULL)
	, m_PickupActors(NULL)
	, m_FloatingTextActors(NULL)
	, m_StatsActor(NULL)
	, m_DownedActor(NULL)
	, m_State(LEVEL_PLAY)
	, m_WantsExit(false)
	, m_Time(0.0f)
	, m_StateTime(0.0f)
	, m_Fade(1.0f)
	, m_FacingX(0.7071f)
	, m_FacingY(0.7071f)
	, m_Stride(0.0f)
	, m_Running(false)
	, m_Moving(false)
	, m_AttackHeld(false)
	, m_SwingCooldown(0.0f)
	, m_SwingAnim(0.0f)
	, m_HurtFlash(0.0f)
	, m_DownedTimer(0.0f)
	, m_LevelUpFlash(0.0f)
	, m_CameraX(0.0f)
	, m_CameraY(0.0f)
	, m_Kills(0)
{
	for (int i = 0; i < MOVE_COUNT; ++i)
	{
		m_MoveKey[i] = false;
	}
}

bool Level::Initialize(Renderer* renderer, ModelLibrary* models, DialogueDB* dialogue, unsigned int seed)
{
	m_Renderer = renderer;
	m_Models = models;
	m_Dialogue = dialogue;

	m_Rng.Seed(seed ^ 0xA5A5A5A5u);

	if (!m_World.Generate(seed, MAP_WIDTH, MAP_HEIGHT))
	{
		return false;
	}

	BuildSceneGraph();
	m_CameraX = PlayerPosition().x;
	m_CameraY = PlayerPosition().y;
	m_Lighting.SetViewer(m_CameraX, m_CameraY);

	m_Stats.Reset();

	// Dusk rather than deep night: the village sells mood, this level has to
	// stay readable while three things are swinging at once.
	m_Lighting.SetAmbient(RGBA(0.46f, 0.52f, 0.66f));
	m_Lighting.SetFog(COL_FOG, 10.0f, 22.0f, 0.85f);

	for (int i = 0; i < ENEMY_POPULATION; ++i)
	{
		SpawnEnemy(i, true);
	}

	m_State = LEVEL_PLAY;
	m_StateTime = 0.0f;
	m_Fade = 1.0f;
	SyncSceneState();
	UpdateLights();

	return true;
}

SceneGraph& Level::GetSceneGraph()
{
	return m_SceneGraph;
}

const SceneGraph& Level::GetSceneGraph() const
{
	return m_SceneGraph;
}

void Level::BuildSceneGraph()
{
	m_SceneGraph.Clear();
	m_Enemies.clear();
	m_Pickups.clear();
	m_FloatingText.clear();

	Actor* environment = m_SceneGraph.CreateActor("environment");
	environment->SetActive(false);
	BuildEnvironment(environment);

	m_SimulationActor = m_SceneGraph.CreateActor("simulation");
	m_SimulationActor->SetUpdateCallback([this](Actor&, float deltaSeconds)
	{
		UpdateSimulation(deltaSeconds);
	});

	m_PlayerActor = m_SceneGraph.CreateActor("player", m_SimulationActor);
	m_PlayerActor->SetPosition(m_World.GetPlayerStartX(), m_World.GetPlayerStartY());
	m_PlayerActor->SetRenderBounds(-PLAYER_BOUND_HALF_WIDTH, -PLAYER_BOUND_TOP,
		PLAYER_BOUND_HALF_WIDTH, PLAYER_BOUND_BOTTOM);
	m_PlayerActor->SetUpdateCallback([this](Actor&, float deltaSeconds)
	{
		if (m_State == LEVEL_PLAY)
		{
			UpdatePlayer(deltaSeconds);

			if (m_AttackHeld)
			{
				Attack();
			}
		}
	});
	m_PlayerActor->SetRenderCallback([this](const Actor&, Renderer&)
	{
		DrawPlayer();
	});

	Actor* swing = m_SceneGraph.CreateActor("swing", m_PlayerActor);
	swing->SetPosition(0.0f, 0.0f, 0.55f);
	swing->SetRenderBounds(-SWING_BOUND_HALF_WIDTH, -SWING_BOUND_TOP,
		SWING_BOUND_HALF_WIDTH, SWING_BOUND_BOTTOM);
	swing->SetRenderCallback([this](const Actor& actor, Renderer&)
	{
		DrawSwing(actor);
	});

	Actor* enemies = m_SceneGraph.CreateActor("enemies", m_SimulationActor);
	m_Enemies.resize(ENEMY_POPULATION);

	for (int i = 0; i < ENEMY_POPULATION; ++i)
	{
		Actor* actor = m_SceneGraph.CreateActor("enemy_" + std::to_string(i), enemies);
		m_Enemies[i].actor = actor;
		actor->SetRenderBounds(-ENEMY_BOUND_HALF_WIDTH, -ENEMY_BOUND_TOP,
			ENEMY_BOUND_HALF_WIDTH, ENEMY_BOUND_BOTTOM);
		actor->SetUpdateCallback([this, i](Actor&, float deltaSeconds)
		{
			UpdateEnemy(i, deltaSeconds);
		});
		actor->SetRenderCallback([this, i](const Actor&, Renderer&)
		{
			DrawEnemy(i);
		});
	}

	m_PickupActors = m_SceneGraph.CreateActor("pickups", m_SimulationActor);
	m_FloatingTextActors = m_SceneGraph.CreateActor("floating_text", m_SimulationActor);

	Actor* camera = m_SceneGraph.CreateActor("camera_and_lights", m_SimulationActor);
	camera->SetUpdateCallback([this](Actor&, float deltaSeconds)
	{
		UpdateCamera(deltaSeconds);
	});

	Actor* interfaceActors = m_SceneGraph.CreateActor("interface");
	interfaceActors->SetActive(false);

	Actor* atmosphere = m_SceneGraph.CreateActor("atmosphere", interfaceActors);
	atmosphere->SetRenderCallback([this](const Actor&, Renderer&)
	{
		DrawAtmosphere();
	});

	Actor* hud = m_SceneGraph.CreateActor("hud", interfaceActors);
	hud->SetRenderCallback([this](const Actor&, Renderer&)
	{
		DrawHud();
	});

	m_StatsActor = m_SceneGraph.CreateActor("stats_panel", interfaceActors);
	m_StatsActor->SetRenderCallback([this](const Actor&, Renderer&)
	{
		DrawStatsScreen();
	});

	m_DownedActor = m_SceneGraph.CreateActor("downed_panel", interfaceActors);
	m_DownedActor->SetRenderCallback([this](const Actor&, Renderer&)
	{
		DrawDownedScreen();
	});
}

void Level::BuildEnvironment(Actor* environment)
{
	const int width = m_World.GetWidth();
	const int height = m_World.GetHeight();
	const Model* treeModels[] =
	{
		m_Models->Find("tree_cedar_small"),
		m_Models->Find("tree_cedar_tall"),
		m_Models->Find("tree_broadleaf"),
		m_Models->Find("tree_cedar_mid")
	};
	const Model* bush = m_Models->Find("bush");
	const Model* rock = m_Models->Find("rock");

	for (int chunkY = 0; chunkY < height; chunkY += CHUNK_SIZE)
	{
		for (int chunkX = 0; chunkX < width; chunkX += CHUNK_SIZE)
		{
			Actor* chunk = m_SceneGraph.CreateActor("chunk_" + std::to_string(chunkX)
				+ "_" + std::to_string(chunkY), environment);
			chunk->SetPosition((float)chunkX, (float)chunkY);

			for (int y = chunkY; y < chunkY + CHUNK_SIZE && y < height; ++y)
			{
				for (int x = chunkX; x < chunkX + CHUNK_SIZE && x < width; ++x)
				{
					const std::string suffix = std::to_string(x) + "_" + std::to_string(y);
					Actor* ground = m_SceneGraph.CreateActor("ground_" + suffix, chunk);
					ground->SetPosition((float)(x - chunkX) + 0.5f, (float)(y - chunkY) + 0.5f);
					ground->SetRenderBounds(-GROUND_BOUND_HALF_WIDTH, -GROUND_BOUND_HALF_HEIGHT,
						GROUND_BOUND_HALF_WIDTH, GROUND_BOUND_HALF_HEIGHT);
					ground->SetRenderCallback([this, x, y](const Actor& actor, Renderer&)
					{
						DrawGround(actor, x, y);
					});

					const TileType tile = m_World.GetTile(x, y);

					if (tile != TILE_TREE && tile != TILE_BUSH && tile != TILE_ROCK)
					{
						continue;
					}

					const unsigned int hash = HashInt(x, y);
					const Model* model = rock;

					if (tile == TILE_TREE)
					{
						model = treeModels[(hash >> 16) % 4];
					}
					else if (tile == TILE_BUSH)
					{
						model = bush;
					}

					Actor* scenery = m_SceneGraph.CreateActor("scenery_" + suffix, chunk);
					scenery->SetPosition((float)(x - chunkX) + 0.30f + (float)(hash % 40) * 0.01f,
						(float)(y - chunkY) + 0.30f + (float)((hash >> 8) % 40) * 0.01f);
					scenery->SetRenderBounds(-SCENERY_BOUND_HALF_WIDTH, -SCENERY_BOUND_TOP,
						SCENERY_BOUND_HALF_WIDTH, SCENERY_BOUND_BOTTOM);
					scenery->SetRenderCallback([this, model](const Actor& actor, Renderer&)
					{
						DrawScenery(actor, model);
					});
				}
			}
		}
	}
}

void Level::SyncSceneState()
{
	m_SimulationActor->SetActive(m_State != LEVEL_STATS);
	m_StatsActor->SetVisible(m_State == LEVEL_STATS);
	m_DownedActor->SetVisible(m_State == LEVEL_DOWNED);
}

ActorPosition Level::PlayerPosition() const
{
	return m_PlayerActor->GetWorldPosition();
}

std::wstring Level::Text(const char* key) const
{
	return m_Dialogue->Line(key);
}

const Model* Level::ModelFor(EnemyKind kind) const
{
	return m_Models->Find(ENEMY_PROFILES[kind].model);
}

const Model* Level::ModelFor(PickupKind kind) const
{
	switch (kind)
	{
	case PICKUP_POTION:    return m_Models->Find("item_potion");
	case PICKUP_WHETSTONE: return m_Models->Find("item_whetstone");
	default:               return m_Models->Find("item_orb");
	}
}

// ---------------------------------------------------------------- spawning

void Level::SpawnEnemy(int index, bool awayFromPlayer)
{
	const std::vector<int>& open = m_World.GetOpenCells();
	Enemy& enemy = m_Enemies[index];
	const ActorPosition player = PlayerPosition();

	// Defer spawning if every reachable tile is too close to the player.
	enemy.alive = false;
	enemy.actor->SetVisible(false);
	enemy.respawnTimer = SPAWN_RETRY_DELAY;

	if (open.empty())
	{
		return;
	}

	float x = 0.0f;
	float y = 0.0f;
	int candidateCount = 0;

	// Reservoir sampling chooses uniformly without discarding valid distant
	// cells after a fixed number of unlucky random attempts.
	for (size_t i = 0; i < open.size(); ++i)
	{
		const int cell = open[i];
		const float candidateX = (float)(cell % m_World.GetWidth()) + 0.5f;
		const float candidateY = (float)(cell / m_World.GetWidth()) + 0.5f;

		const float dx = candidateX - player.x;
		const float dy = candidateY - player.y;
		const float distanceSquared = dx * dx + dy * dy;

		if ((awayFromPlayer && distanceSquared < RESPAWN_MIN_DISTANCE * RESPAWN_MIN_DISTANCE)
			|| m_World.IsBlocked(candidateX, candidateY, ENEMY_RADIUS))
		{
			continue;
		}

		++candidateCount;

		if (m_Rng.RangeInt(1, candidateCount) == 1)
		{
			x = candidateX;
			y = candidateY;
		}
	}

	if (candidateCount == 0)
	{
		return;
	}

	// Tougher kinds get rarer, so the field stays mostly wisps and shades.
	const float roll = m_Rng.NextFloat();
	EnemyKind kind = ENEMY_WISP;

	if (roll > 0.82f)
	{
		kind = ENEMY_ONI;
	}
	else if (roll > 0.45f)
	{
		kind = ENEMY_SHADE;
	}

	const EnemyProfile& profile = ENEMY_PROFILES[kind];

	enemy.kind = kind;
	SetWorldPosition(*enemy.actor, x, y);
	enemy.maxHealth = profile.health;
	enemy.health = profile.health;
	enemy.attackTimer = 0.0f;
	enemy.hitFlash = 0.0f;
	enemy.wanderTimer = m_Rng.Range(0.5f, 2.5f);
	enemy.wanderX = 0.0f;
	enemy.wanderY = 0.0f;
	enemy.phase = m_Rng.Range(0.0f, TWO_PI);
	enemy.alive = true;
	enemy.actor->SetVisible(true);
	enemy.respawnTimer = 0.0f;
}

void Level::SpawnPickup(PickupKind kind, float x, float y, int value)
{
	size_t index = 0;

	while (index < m_Pickups.size() && m_Pickups[index].actor->IsActive())
	{
		++index;
	}

	if (index == m_Pickups.size())
	{
		Pickup pickup = {};
		pickup.actor = m_SceneGraph.CreateActor("pickup_" + std::to_string(index), m_PickupActors);
		pickup.actor->SetRenderBounds(-PICKUP_BOUND_HALF_WIDTH, -PICKUP_BOUND_TOP,
			PICKUP_BOUND_HALF_WIDTH, PICKUP_BOUND_BOTTOM);
		pickup.actor->SetUpdateCallback([this, index](Actor&, float deltaSeconds)
		{
			UpdatePickup((int)index, deltaSeconds);
		});
		pickup.actor->SetRenderCallback([this, index](const Actor&, Renderer&)
		{
			DrawPickup((int)index);
		});
		m_Pickups.push_back(pickup);
	}

	Pickup& pickup = m_Pickups[index];
	float worldX = x + m_Rng.Range(-PICKUP_SCATTER, PICKUP_SCATTER);
	float worldY = y + m_Rng.Range(-PICKUP_SCATTER, PICKUP_SCATTER);

	// Keep scattered drops on the reachable collision map.
	if (m_World.IsBlocked(worldX, worldY, PLAYER_RADIUS))
	{
		worldX = floorf(x) + 0.5f;
		worldY = floorf(y) + 0.5f;
	}

	SetWorldPosition(*pickup.actor, worldX, worldY);
	pickup.kind = kind;
	pickup.phase = m_Rng.Range(0.0f, TWO_PI);
	pickup.life = PICKUP_LIFETIME;
	pickup.value = value;
	pickup.actor->SetActive(true);
	pickup.actor->SetVisible(true);
}

void Level::AddFloatingText(const std::wstring& text, const ActorPosition& position, const Color& color, int size)
{
	size_t index = 0;

	while (index < m_FloatingText.size() && m_FloatingText[index].actor->IsActive())
	{
		++index;
	}

	if (index == m_FloatingText.size())
	{
		FloatingText entry = {};
		entry.actor = m_SceneGraph.CreateActor("floating_text_" + std::to_string(index), m_FloatingTextActors);
		entry.actor->SetUpdateCallback([this, index](Actor&, float deltaSeconds)
		{
			UpdateFloatingText((int)index, deltaSeconds);
		});
		entry.actor->SetRenderCallback([this, index](const Actor&, Renderer&)
		{
			DrawFloatingText((int)index);
		});
		m_FloatingText.push_back(entry);
	}

	// Slots never move logically: callbacks capture indices, not vector addresses.
	FloatingText& entry = m_FloatingText[index];
	entry.text = text;
	SetWorldPosition(*entry.actor, position.x, position.y, position.z + FLOATING_TEXT_HEIGHT_OFFSET);
	entry.rise = 0.0f;
	entry.maxLife = 1.1f;
	entry.life = entry.maxLife;
	entry.color = color;
	entry.size = size;

	const float halfWidth = m_Renderer->MeasureTextWidth(text, size, FONT_SERIF, true) * 0.5f
		+ FLOATING_TEXT_BOUND_HORIZONTAL_MARGIN;
	entry.actor->SetRenderBounds(-halfWidth, -FLOATING_TEXT_BOUND_TOP,
		halfWidth, (float)size * FLOATING_TEXT_BOUND_HEIGHT_SCALE);
	entry.actor->SetActive(true);
	entry.actor->SetVisible(true);
}

// ---------------------------------------------------------------- combat

void Level::Attack()
{
	if (m_SwingCooldown > 0.0f || m_State != LEVEL_PLAY || !m_PlayerActor->IsActiveInHierarchy())
	{
		return;
	}

	const ActorPosition player = PlayerPosition();
	m_SwingCooldown = m_Stats.AttackInterval();
	m_SwingAnim = SWING_DURATION;

	for (size_t i = 0; i < m_Enemies.size(); ++i)
	{
		const Enemy& enemy = m_Enemies[i];

		if (!enemy.alive || !enemy.actor->IsActiveInHierarchy())
		{
			continue;
		}

		const ActorPosition position = enemy.actor->GetWorldPosition();
		const float dx = position.x - player.x;
		const float dy = position.y - player.y;
		const float distance = sqrtf(dx * dx + dy * dy);

		if (distance > ATTACK_RANGE || distance < 0.0001f)
		{
			continue;
		}

		// A forward arc, not a circle: facing has to mean something.
		const float dot = (dx / distance) * m_FacingX + (dy / distance) * m_FacingY;

		if (dot < ATTACK_ARC)
		{
			continue;
		}

		const bool critical = m_Rng.NextFloat() < CRIT_CHANCE;
		float damage = m_Stats.AttackPower() * m_Rng.Range(0.88f, 1.12f);

		if (critical)
		{
			damage *= CRIT_MULTIPLIER;
		}

		DamageEnemy((int)i, damage, critical);
	}
}

void Level::DamageEnemy(int index, float amount, bool critical)
{
	Enemy& enemy = m_Enemies[index];

	enemy.health -= amount;
	enemy.hitFlash = 0.16f;

	wchar_t buffer[32];
	swprintf_s(buffer, 32, L"%d", (int)(amount + 0.5f));

	AddFloatingText(buffer, enemy.actor->GetWorldPosition(), critical ? COL_DAMAGE_CRIT : COL_DAMAGE_DEALT,
		critical ? 22 : 18);

	// A shove, so a hit reads even when it does not kill.
	const float dx = enemy.actor->GetWorldPosition().x - PlayerPosition().x;
	const float dy = enemy.actor->GetWorldPosition().y - PlayerPosition().y;
	const float distance = sqrtf(dx * dx + dy * dy);

	if (distance > 0.0001f)
	{
		const float knockback = 0.22f;
		const float pushX = enemy.actor->GetWorldPosition().x + dx / distance * knockback;
		const float pushY = enemy.actor->GetWorldPosition().y + dy / distance * knockback;

		if (!m_World.IsBlocked(pushX, enemy.actor->GetWorldPosition().y, ENEMY_RADIUS))
		{
			SetWorldPosition(*enemy.actor, pushX, enemy.actor->GetWorldPosition().y);
		}

		if (!m_World.IsBlocked(enemy.actor->GetWorldPosition().x, pushY, ENEMY_RADIUS))
		{
			SetWorldPosition(*enemy.actor, enemy.actor->GetWorldPosition().x, pushY);
		}
	}

	if (enemy.health <= 0.0f)
	{
		KillEnemy(index);
	}
}

void Level::KillEnemy(int index)
{
	Enemy& enemy = m_Enemies[index];
	const ActorPosition position = enemy.actor->GetWorldPosition();
	const EnemyProfile& profile = ENEMY_PROFILES[enemy.kind];

	enemy.alive = false;
	enemy.actor->SetVisible(false);
	enemy.respawnTimer = m_Rng.Range(RESPAWN_MIN, RESPAWN_MAX);
	++m_Kills;

	SpawnPickup(PICKUP_ORB, position.x, position.y, profile.experience);

	const float roll = m_Rng.NextFloat();

	if (roll < WHETSTONE_CHANCE)
	{
		SpawnPickup(PICKUP_WHETSTONE, position.x, position.y, 1);
	}
	else if (roll < WHETSTONE_CHANCE + POTION_CHANCE)
	{
		SpawnPickup(PICKUP_POTION, position.x, position.y, (int)POTION_HEAL);
	}
}

void Level::DamagePlayer(float amount)
{
	if (m_HurtFlash > 0.0f || m_State != LEVEL_PLAY || !m_PlayerActor->IsActiveInHierarchy())
	{
		return;
	}

	const float reduced = amount * (1.0f - m_Stats.DamageReduction());

	m_Stats.health -= reduced;
	m_HurtFlash = INVULNERABLE_TIME;

	wchar_t buffer[32];
	swprintf_s(buffer, 32, L"-%d", (int)(reduced + 0.5f));

	AddFloatingText(buffer, PlayerPosition(), COL_DAMAGE_TAKEN, 20);

	if (m_Stats.health <= 0.0f)
	{
		m_Stats.health = 0.0f;
		m_State = LEVEL_DOWNED;
		m_StateTime = 0.0f;
		m_DownedTimer = DOWNED_DURATION;
		m_AttackHeld = false;
	}
}

void Level::GrantExperience(int amount)
{
	const int beforeLevel = m_Stats.level;

	if (m_Stats.AddExperience(amount))
	{
		m_LevelUpFlash = 1.4f;

		wchar_t buffer[64];
		swprintf_s(buffer, 64, L"%s  %d", Text("ui_lv_levelup").c_str(), m_Stats.level);

		AddFloatingText(buffer, PlayerPosition(), COL_DAMAGE_CRIT, 24);
	}
	else if (beforeLevel == m_Stats.level)
	{
		wchar_t buffer[32];
		swprintf_s(buffer, 32, L"+%d", amount);

		AddFloatingText(buffer, PlayerPosition(), COL_EXPERIENCE, 16);
	}
}

void Level::CollectPickup(int index)
{
	Pickup& pickup = m_Pickups[index];

	pickup.actor->SetActive(false);
	pickup.actor->SetVisible(false);

	if (pickup.kind == PICKUP_ORB)
	{
		GrantExperience(pickup.value);
		return;
	}

	if (pickup.kind == PICKUP_POTION)
	{
		const float before = m_Stats.health;
		m_Stats.health = Clamp(m_Stats.health + (float)pickup.value, 0.0f, m_Stats.MaxHealth());

		wchar_t buffer[32];
		swprintf_s(buffer, 32, L"+%d", (int)(m_Stats.health - before + 0.5f));

		AddFloatingText(buffer, PlayerPosition(), COL_HEAL, 18);

		return;
	}

	// Whetstone: a free permanent point, not a consumable.
	++m_Stats.invested[STAT_STRENGTH];
	AddFloatingText(Text("ui_lv_got_whetstone"), PlayerPosition(), COL_DAMAGE_CRIT, 18);
}

// ---------------------------------------------------------------- update

void Level::Update(float deltaSeconds)
{
	SyncSceneState();
	m_SceneGraph.Update(deltaSeconds);
	SyncSceneState();
}

void Level::UpdateSimulation(float deltaSeconds)
{
	m_Time += deltaSeconds;
	m_StateTime += deltaSeconds;
	m_Fade = Clamp(m_Fade - deltaSeconds / 1.2f, 0.0f, 1.0f);

	if (m_SwingCooldown > 0.0f)
	{
		m_SwingCooldown -= deltaSeconds;
	}

	if (m_SwingAnim > 0.0f)
	{
		m_SwingAnim -= deltaSeconds;
	}

	if (m_HurtFlash > 0.0f)
	{
		m_HurtFlash -= deltaSeconds;
	}

	if (m_LevelUpFlash > 0.0f)
	{
		m_LevelUpFlash -= deltaSeconds;
	}

	if (m_State == LEVEL_DOWNED)
	{
		m_DownedTimer -= deltaSeconds;

		if (m_DownedTimer <= 0.0f)
		{
			SetWorldPosition(*m_PlayerActor, m_World.GetPlayerStartX(), m_World.GetPlayerStartY());
			m_Stats.health = m_Stats.MaxHealth() * RESPAWN_HEALTH_FRACTION;
			m_HurtFlash = INVULNERABLE_TIME;
			m_State = LEVEL_PLAY;
			m_StateTime = 0.0f;
		}
	}
}

void Level::UpdateCamera(float deltaSeconds)
{
	const float follow = Clamp(deltaSeconds * 6.0f, 0.0f, 1.0f);
	m_CameraX += (PlayerPosition().x - m_CameraX) * follow;
	m_CameraY += (PlayerPosition().y - m_CameraY) * follow;
	m_Lighting.SetViewer(m_CameraX, m_CameraY);
	m_Lighting.SetTime(m_Time);
	UpdateLights();
}

void Level::UpdatePlayer(float deltaSeconds)
{
	float dx = 0.0f;
	float dy = 0.0f;

	if (m_MoveKey[MOVE_UP])
	{
		dx -= 1.0f;
		dy -= 1.0f;
	}

	if (m_MoveKey[MOVE_DOWN])
	{
		dx += 1.0f;
		dy += 1.0f;
	}

	if (m_MoveKey[MOVE_LEFT])
	{
		dx -= 1.0f;
		dy += 1.0f;
	}

	if (m_MoveKey[MOVE_RIGHT])
	{
		dx += 1.0f;
		dy -= 1.0f;
	}

	const float lengthSq = dx * dx + dy * dy;
	m_Moving = lengthSq > 0.0001f;

	if (!m_Moving)
	{
		m_Stride *= 0.85f;
		return;
	}

	const float length = sqrtf(lengthSq);
	dx /= length;
	dy /= length;

	m_FacingX = dx;
	m_FacingY = dy;

	const float speed = m_Stats.MoveSpeed() * (m_Running ? 1.45f : 1.0f);
	const float stepX = dx * speed * deltaSeconds;
	const float stepY = dy * speed * deltaSeconds;

	if (!m_World.IsBlocked(PlayerPosition().x + stepX, PlayerPosition().y, PLAYER_RADIUS))
	{
		SetWorldPosition(*m_PlayerActor, PlayerPosition().x + stepX, PlayerPosition().y);
	}

	if (!m_World.IsBlocked(PlayerPosition().x, PlayerPosition().y + stepY, PLAYER_RADIUS))
	{
		SetWorldPosition(*m_PlayerActor, PlayerPosition().x, PlayerPosition().y + stepY);
	}

	m_Stride += deltaSeconds * (m_Running ? 13.0f : 9.0f);
}

void Level::UpdateEnemy(int index, float deltaSeconds)
{
	Enemy& enemy = m_Enemies[index];

	if (m_State != LEVEL_PLAY)
	{
		return;
	}

	if (!enemy.alive)
	{
		enemy.respawnTimer -= deltaSeconds;

		if (enemy.respawnTimer <= 0.0f)
		{
			SpawnEnemy(index, true);
		}

		return;
	}

	if (enemy.hitFlash > 0.0f)
	{
		enemy.hitFlash -= deltaSeconds;
	}

	if (enemy.attackTimer > 0.0f)
	{
		enemy.attackTimer -= deltaSeconds;
	}

	const EnemyProfile& profile = ENEMY_PROFILES[enemy.kind];

	const float dx = PlayerPosition().x - enemy.actor->GetWorldPosition().x;
	const float dy = PlayerPosition().y - enemy.actor->GetWorldPosition().y;
	const float distance = sqrtf(dx * dx + dy * dy);

	float moveX = 0.0f;
	float moveY = 0.0f;

	if (distance < profile.aggroRange && distance > 0.0001f)
	{
		moveX = dx / distance;
		moveY = dy / distance;

		if (distance <= profile.attackRange)
		{
			moveX = 0.0f;
			moveY = 0.0f;

			if (enemy.attackTimer <= 0.0f)
			{
				enemy.attackTimer = profile.attackInterval;
				DamagePlayer(profile.damage);
			}
		}
	}
	else
	{
		enemy.wanderTimer -= deltaSeconds;

		if (enemy.wanderTimer <= 0.0f)
		{
			const float angle = m_Rng.Range(0.0f, TWO_PI);
			enemy.wanderX = cosf(angle);
			enemy.wanderY = sinf(angle);
			enemy.wanderTimer = m_Rng.Range(1.2f, 3.2f);
		}

		moveX = enemy.wanderX * 0.45f;
		moveY = enemy.wanderY * 0.45f;
	}

	const float stepX = moveX * profile.speed * deltaSeconds;
	const float stepY = moveY * profile.speed * deltaSeconds;

	if (!m_World.IsBlocked(enemy.actor->GetWorldPosition().x + stepX, enemy.actor->GetWorldPosition().y, ENEMY_RADIUS))
	{
		SetWorldPosition(*enemy.actor, enemy.actor->GetWorldPosition().x + stepX, enemy.actor->GetWorldPosition().y);
	}
	else
	{
		enemy.wanderTimer = 0.0f;
	}

	if (!m_World.IsBlocked(enemy.actor->GetWorldPosition().x, enemy.actor->GetWorldPosition().y + stepY, ENEMY_RADIUS))
	{
		SetWorldPosition(*enemy.actor, enemy.actor->GetWorldPosition().x, enemy.actor->GetWorldPosition().y + stepY);
	}
	else
	{
		enemy.wanderTimer = 0.0f;
	}
}

void Level::UpdatePickup(int index, float deltaSeconds)
{
	if (m_State != LEVEL_PLAY)
	{
		return;
	}

	Pickup& pickup = m_Pickups[index];
	pickup.life -= deltaSeconds;

	if (pickup.life <= 0.0f)
	{
		pickup.actor->SetActive(false);
		pickup.actor->SetVisible(false);
		return;
	}

	const ActorPosition position = pickup.actor->GetWorldPosition();
	const float dx = PlayerPosition().x - position.x;
	const float dy = PlayerPosition().y - position.y;
	const float distance = sqrtf(dx * dx + dy * dy);

	if (pickup.kind == PICKUP_ORB && distance < ORB_MAGNET_RANGE && distance > 0.0001f)
	{
		const float pull = Clamp(ORB_MAGNET_SPEED * deltaSeconds
			* (1.0f - distance / ORB_MAGNET_RANGE), 0.0f, distance);
		const float nextX = position.x + dx / distance * pull;
		const float nextY = position.y + dy / distance * pull;

		if (!m_World.IsBlocked(nextX, position.y, PLAYER_RADIUS))
		{
			SetWorldPosition(*pickup.actor, nextX, position.y, position.z);
		}

		if (!m_World.IsBlocked(pickup.actor->GetWorldPosition().x, nextY, PLAYER_RADIUS))
		{
			SetWorldPosition(*pickup.actor, pickup.actor->GetWorldPosition().x, nextY, position.z);
		}
	}

	if (distance < PICKUP_RANGE)
	{
		CollectPickup(index);
	}
}

void Level::UpdateFloatingText(int index, float deltaSeconds)
{
	FloatingText& entry = m_FloatingText[index];
	entry.life -= deltaSeconds;
	entry.rise += deltaSeconds * 34.0f;

	if (entry.life <= 0.0f)
	{
		entry.actor->SetActive(false);
		entry.actor->SetVisible(false);
	}
}

void Level::UpdateLights()
{
	// Few enough lights that rebuilding beats tracking indices across respawns.
	m_Lighting.Clear();

	if (m_PlayerActor->IsVisibleInHierarchy())
	{
		m_Lighting.AddLight(PlayerPosition().x, PlayerPosition().y - 0.15f, 5.0f, 1.25f, COL_FLAME, true);
	}

	for (size_t i = 0; i < m_Enemies.size(); ++i)
	{
		const Enemy& enemy = m_Enemies[i];

		if (!enemy.alive || !enemy.actor->IsVisibleInHierarchy() || !ENEMY_PROFILES[enemy.kind].glows)
		{
			continue;
		}

		m_Lighting.AddLight(enemy.actor->GetWorldPosition().x, enemy.actor->GetWorldPosition().y, 4.0f, 1.15f, COL_WISP_LIGHT, true);
	}
}

// ---------------------------------------------------------------- input

void Level::OnKey(unsigned char key, bool down, bool shift)
{
	m_Running = shift;

	if (key >= 'A' && key <= 'Z')
	{
		key = (unsigned char)(key - 'A' + 'a');
	}

	if (key == 27)
	{
		if (down)
		{
			m_WantsExit = true;
		}

		return;
	}

	switch (key)
	{
	case 'w': m_MoveKey[MOVE_UP] = down; break;
	case 'a': m_MoveKey[MOVE_LEFT] = down; break;
	case 's': m_MoveKey[MOVE_DOWN] = down; break;
	case 'd': m_MoveKey[MOVE_RIGHT] = down; break;
	default: break;
	}

	if (key == ' ')
	{
		m_AttackHeld = down;
	}

	if (!down)
	{
		return;
	}

	if (m_State == LEVEL_DOWNED)
	{
		m_AttackHeld = false;
		return;
	}

	if (key == 'c')
	{
		m_State = (m_State == LEVEL_STATS) ? LEVEL_PLAY : LEVEL_STATS;
		m_AttackHeld = false;
		m_Moving = false;
		SyncSceneState();
		return;
	}

	if (m_State == LEVEL_STATS)
	{
		if (key >= '1' && key <= '4')
		{
			const StatKind stat = (StatKind)(key - '1');

			if (m_Stats.Spend(stat))
			{
				AddFloatingText(Text(STAT_NAME_KEYS[stat]), PlayerPosition(), COL_DAMAGE_CRIT, 18);
			}
		}

		return;
	}

	if (m_State == LEVEL_PLAY && key == ' ')
	{
		Attack();
	}
}

// ---------------------------------------------------------------- drawing

void Level::DrawGround(const Actor& actor, int x, int y)
{
	const ActorPosition position = actor.GetWorldPosition();
	const TileType tile = m_World.GetTile(x, y);
	Color base;

	switch (tile)
	{
	case TILE_TALLGRASS: base = COL_TALLGRASS; break;
	case TILE_DIRT:      base = COL_DIRT; break;
	case TILE_STONE:     base = COL_STONE; break;
	case TILE_SAND:      base = COL_SAND; break;
	case TILE_WATER:     base = COL_WATER; break;
	default:             base = COL_GRASS; break;
	}

	if (tile != TILE_WATER)
	{
		const float shade = 1.0f + ((float)(HashInt(x, y) % 100) * 0.01f - 0.5f) * 0.09f;
		base.r *= shade;
		base.g *= shade;
		base.b *= shade;
	}

	float screenX = 0.0f;
	float screenY = 0.0f;
	m_Renderer->WorldToScreen(position.x, position.y, position.z, &screenX, &screenY);
	const Color lit = m_Lighting.Apply(base, position.x, position.y);

	if (tile == TILE_WATER)
	{
		m_Renderer->SetAnim(ANIM_WATER, (float)x * 0.62f + (float)y * 0.44f, 1.6f);
	}

	m_Renderer->PushDiamond(screenX, screenY, Renderer::TileHalfWidth(), Renderer::TileHalfHeight(),
		lit, -100000.0f + (position.x + position.y - 1.0f) * 0.01f);

	if (tile == TILE_WATER)
	{
		m_Renderer->ClearAnim();
	}
}

void Level::DrawScenery(const Actor& actor, const Model* model)
{
	if (model == NULL)
	{
		return;
	}

	const ActorPosition position = actor.GetWorldPosition();
	float screenX = 0.0f;
	float screenY = 0.0f;
	m_Renderer->WorldToScreen(position.x, position.y, position.z, &screenX, &screenY);
	const ShadeParams shade = m_Lighting.Shade(position.x, position.y, RGBA(1.0f, 1.0f, 1.0f));

	DrawModel(m_Renderer, *model, screenX, screenY, 1.0f, position.x + position.y, shade);
}

void Level::DrawEnemy(int index)
{
	const Enemy& enemy = m_Enemies[index];

	if (!enemy.alive)
	{
		return;
	}

	const ActorPosition position = enemy.actor->GetWorldPosition();
	float screenX = 0.0f;
	float screenY = 0.0f;
	m_Renderer->WorldToScreen(position.x, position.y, position.z, &screenX, &screenY);

	const Model* model = ModelFor(enemy.kind);

	if (model == NULL)
	{
		return;
	}

	const EnemyProfile& profile = ENEMY_PROFILES[enemy.kind];

	Color tint = RGBA(1.0f, 1.0f, 1.0f);

	if (enemy.hitFlash > 0.0f)
	{
		tint = RGBA(2.4f, 2.0f, 2.0f);
	}

	ShadeParams shade = m_Lighting.Shade(position.x, position.y, tint);

	if (enemy.hitFlash > 0.0f)
	{
		// Flash the whole silhouette, tinted or not.
		shade.light = RGBA(2.2f, 1.9f, 1.9f);
	}

	const float bob = sinf(m_Time * 2.2f + enemy.phase) * 2.0f;

	DrawModel(m_Renderer, *model, screenX, screenY + bob, profile.scale,
		position.x + position.y, shade);

	// Health pip above anything that has been hurt.
	if (enemy.health < enemy.maxHealth)
	{
		const float barWidth = 30.0f;
		const float fill = Clamp(enemy.health / enemy.maxHealth, 0.0f, 1.0f);
		const float barY = screenY - 52.0f * profile.scale;

		m_Renderer->PushRect(screenX - barWidth * 0.5f, barY, barWidth, 4.0f,
			RGBA(0.06f, 0.06f, 0.08f, 0.80f), position.x + position.y + 0.5f);
		m_Renderer->PushRect(screenX - barWidth * 0.5f, barY, barWidth * fill, 4.0f,
			COL_SHU, position.x + position.y + 0.51f);
	}
}

void Level::DrawPickup(int index)
{
	const Pickup& pickup = m_Pickups[index];
	const ActorPosition position = pickup.actor->GetWorldPosition();

	float screenX = 0.0f;
	float screenY = 0.0f;
	m_Renderer->WorldToScreen(position.x, position.y, position.z, &screenX, &screenY);

	const Model* model = ModelFor(pickup.kind);

	if (model == NULL)
	{
		return;
	}

	// Blink out over the last two seconds so a vanishing drop is not a
	// surprise.
	float alpha = 1.0f;

	if (pickup.life < 2.0f)
	{
		alpha = (sinf(pickup.life * 18.0f) * 0.5f + 0.5f) * 0.8f + 0.2f;
	}

	const float bob = sinf(m_Time * 2.6f + pickup.phase) * 2.5f;
	const ShadeParams shade = m_Lighting.Shade(position.x, position.y, RGBA(1.0f, 1.0f, 1.0f, alpha));

	DrawModel(m_Renderer, *model, screenX, screenY + bob, 1.0f, position.x + position.y, shade);
}

void Level::DrawPlayer()
{
	const ActorPosition position = PlayerPosition();
	const Model* person = m_Models->Find("person");
	const Model* sword = m_Models->Find("person_sword");
	const Model* lantern = m_Models->Find("person_lantern");

	float screenX = 0.0f;
	float screenY = 0.0f;
	m_Renderer->WorldToScreen(position.x, position.y, position.z, &screenX, &screenY);

	const float stride = m_Moving ? fabsf(sinf(m_Stride)) * 2.6f : 0.0f;
	const float depth = position.x + position.y;

	Color tint = COL_PLAYER_ROBE;

	if (m_HurtFlash > 0.0f)
	{
		tint = Mix(COL_PLAYER_ROBE, RGBA(1.0f, 0.35f, 0.30f), Clamp(m_HurtFlash / INVULNERABLE_TIME, 0.0f, 1.0f));
	}

	const ShadeParams shade = m_Lighting.Shade(position.x, position.y, tint);

	if (person != NULL)
	{
		DrawModel(m_Renderer, *person, screenX, screenY - stride, 1.0f, depth, shade);
	}

	if (sword != NULL)
	{
		DrawModel(m_Renderer, *sword, screenX, screenY - stride, 1.0f, depth, shade);
	}

	if (lantern != NULL)
	{
		DrawModel(m_Renderer, *lantern, screenX, screenY - stride, 1.0f, depth, shade);
	}
}

void Level::DrawSwing(const Actor& actor)
{
	if (m_SwingAnim <= 0.0f)
	{
		return;
	}

	const float progress = 1.0f - m_SwingAnim / SWING_DURATION;
	const float alpha = (1.0f - progress) * 0.55f;
	const ActorPosition position = actor.GetWorldPosition();

	float originX = 0.0f;
	float originY = 0.0f;
	m_Renderer->WorldToScreen(position.x, position.y, position.z, &originX, &originY);

	// The arc is drawn in screen space, swept through the facing direction.
	const float facingAngle = atan2f((m_FacingX + m_FacingY) * Renderer::TileHalfHeight(),
		(m_FacingX - m_FacingY) * Renderer::TileHalfWidth());

	const float sweep = 2.1f;
	const float start = facingAngle - sweep * 0.5f + sweep * progress * 0.6f;
	const int segments = 9;
	const float innerRadius = 22.0f;
	const float outerRadius = 58.0f;

	for (int i = 0; i < segments; ++i)
	{
		const float a0 = start + sweep * (float)i / (float)segments;
		const float a1 = start + sweep * (float)(i + 1) / (float)segments;

		const float taper = 1.0f - (float)i / (float)segments * 0.55f;

		const float xy[8] =
		{
			originX + cosf(a0) * innerRadius, originY + sinf(a0) * innerRadius * 0.55f,
			originX + cosf(a1) * innerRadius, originY + sinf(a1) * innerRadius * 0.55f,
			originX + cosf(a1) * outerRadius * taper, originY + sinf(a1) * outerRadius * taper * 0.55f,
			originX + cosf(a0) * outerRadius * taper, originY + sinf(a0) * outerRadius * taper * 0.55f
		};

		m_Renderer->PushPolygon(xy, 4, RGBA(0.92f, 0.94f, 0.98f, alpha * taper),
			position.x + position.y + 0.6f);
	}
}

void Level::DrawFloatingText(int index)
{
	const FloatingText& entry = m_FloatingText[index];
	const ActorPosition position = entry.actor->GetWorldPosition();

	float screenX = 0.0f;
	float screenY = 0.0f;
	m_Renderer->WorldToScreen(position.x, position.y, position.z, &screenX, &screenY);

	const float fade = Clamp(entry.life / entry.maxLife, 0.0f, 1.0f);

	Color color = entry.color;
	color.a = fade;

	m_Renderer->PushText(entry.text, screenX, screenY - entry.rise, entry.size, FONT_SERIF, true,
		color, ALIGN_CENTER);
}

void Level::DrawAtmosphere()
{
	const float halfWidth = m_Renderer->GetWidth() * 0.5f;
	const float halfHeight = m_Renderer->GetHeight() * 0.5f;

	const float outer = sqrtf(halfWidth * halfWidth + halfHeight * halfHeight) * 1.06f;
	const float inner = outer * 0.54f;

	const int segments = 30;

	for (int i = 0; i < segments; ++i)
	{
		const float a0 = TWO_PI * (float)i / (float)segments;
		const float a1 = TWO_PI * (float)(i + 1) / (float)segments;

		const float xy[8] =
		{
			cosf(a0) * inner, sinf(a0) * inner,
			cosf(a1) * inner, sinf(a1) * inner,
			cosf(a1) * outer, sinf(a1) * outer,
			cosf(a0) * outer, sinf(a0) * outer
		};

		const Color clear = RGBA(0.02f, 0.03f, 0.045f, 0.0f);
		const Color edge = RGBA(0.02f, 0.03f, 0.045f, 0.74f);
		const Color colors[4] = { clear, clear, edge, edge };

		m_Renderer->PushPolygonShaded(xy, colors, 4, DEPTH_OVERLAY);
	}

	if (m_HurtFlash > 0.0f)
	{
		const float strength = Clamp(m_HurtFlash / INVULNERABLE_TIME, 0.0f, 1.0f) * 0.30f;

		m_Renderer->PushRect(-halfWidth, -halfHeight, halfWidth * 2.0f, halfHeight * 2.0f,
			RGBA(0.55f, 0.05f, 0.05f, strength), DEPTH_OVERLAY + 1.0f);
	}

	if (m_LevelUpFlash > 0.0f)
	{
		const float strength = Clamp(m_LevelUpFlash / 1.4f, 0.0f, 1.0f) * 0.22f;

		m_Renderer->PushRect(-halfWidth, -halfHeight, halfWidth * 2.0f, halfHeight * 2.0f,
			RGBA(0.95f, 0.80f, 0.42f, strength), DEPTH_OVERLAY + 2.0f);
	}

	if (m_Fade > 0.001f)
	{
		m_Renderer->PushRect(-halfWidth, -halfHeight, halfWidth * 2.0f, halfHeight * 2.0f,
			RGBA(0.0f, 0.0f, 0.0f, m_Fade), DEPTH_OVERLAY + 3.0f);
	}
}

// ---------------------------------------------------------------- interface

void Level::DrawPanel(float x, float y, float width, float height, float alpha, float depth)
{
	m_Renderer->PushRect(x, y, width, height, RGBA(COL_INK.r, COL_INK.g, COL_INK.b, alpha), depth);
	m_Renderer->PushRect(x, y, width, 1.0f, RGBA(0.55f, 0.55f, 0.52f, alpha * 0.28f), depth + 0.1f);
}

void Level::DrawBar(float x, float y, float width, float height, float fill,
	const Color& fillColor, const Color& backColor, float depth)
{
	m_Renderer->PushRect(x, y, width, height, backColor, depth);
	m_Renderer->PushRect(x, y, width * Clamp(fill, 0.0f, 1.0f), height, fillColor, depth + 0.1f);
}

void Level::DrawHud()
{
	Renderer* r = m_Renderer;

	const float left = -r->GetWidth() * 0.5f;
	const float top = -r->GetHeight() * 0.5f;
	const float right = r->GetWidth() * 0.5f;
	const float bottom = r->GetHeight() * 0.5f;

	// --- level and experience, top left ---
	const float panelX = left + 22.0f;
	const float panelY = top + 20.0f;
	const float panelW = 268.0f;
	const float panelH = 96.0f;

	DrawPanel(panelX, panelY, panelW, panelH, 0.80f, DEPTH_PANEL);
	r->PushRect(panelX, panelY, 2.0f, panelH, COL_SHU, DEPTH_PANEL_TOP);

	r->PushText(Text("ui_lv_name"), panelX + 16.0f, panelY + 11.0f, 15, FONT_SERIF, true,
		RGBA(0.80f, 0.66f, 0.52f, 0.95f), ALIGN_LEFT);

	wchar_t buffer[96];
	swprintf_s(buffer, 96, L"%s  %d", Text("ui_lv_level").c_str(), m_Stats.level);
	r->PushText(buffer, panelX + 16.0f, panelY + 33.0f, 20, FONT_SERIF, true,
		RGBA(0.93f, 0.92f, 0.88f, 0.98f), ALIGN_LEFT);

	const float experienceFill = m_Stats.IsMaxLevel() ? 1.0f
		: (float)m_Stats.experience / (float)m_Stats.ExperienceToNext();
	DrawBar(panelX + 16.0f, panelY + 64.0f, panelW - 32.0f, 7.0f, experienceFill,
		RGBA(0.45f, 0.68f, 0.92f), RGBA(0.10f, 0.12f, 0.16f, 0.9f), DEPTH_PANEL_TOP);

	std::wstring experienceText = Text("ui_lv_max_level");

	if (!m_Stats.IsMaxLevel())
	{
		swprintf_s(buffer, 96, L"%d / %d", m_Stats.experience, m_Stats.ExperienceToNext());
		experienceText = buffer;
	}

	r->PushText(experienceText, panelX + panelW - 16.0f, panelY + 74.0f, 13, FONT_UI, false,
		RGBA(0.68f, 0.70f, 0.74f, 0.85f), ALIGN_RIGHT);

	swprintf_s(buffer, 96, L"%s  %d", Text("ui_lv_kills").c_str(), m_Kills);
	r->PushText(buffer, panelX + 16.0f, panelY + 74.0f, 13, FONT_UI, false,
		RGBA(0.68f, 0.70f, 0.74f, 0.85f), ALIGN_LEFT);

	// --- health, bottom left ---
	const float healthX = left + 22.0f;
	const float healthY = bottom - 66.0f;
	const float healthW = 240.0f;

	r->PushText(Text("ui_lv_hp"), healthX, healthY - 20.0f, 13, FONT_UI, false,
		RGBA(0.70f, 0.72f, 0.70f, 0.85f), ALIGN_LEFT);

	DrawBar(healthX, healthY, healthW, 12.0f, m_Stats.health / m_Stats.MaxHealth(),
		RGBA(0.72f, 0.22f, 0.18f), RGBA(0.10f, 0.10f, 0.12f, 0.9f), DEPTH_PANEL);

	swprintf_s(buffer, 96, L"%d / %d", (int)(m_Stats.health + 0.5f), (int)(m_Stats.MaxHealth() + 0.5f));
	r->PushText(buffer, healthX + healthW, healthY - 20.0f, 13, FONT_UI, false,
		RGBA(0.70f, 0.72f, 0.70f, 0.85f), ALIGN_RIGHT);

	// --- unspent points nag, top right ---
	if (m_Stats.unspentPoints > 0)
	{
		const float blink = 0.55f + 0.45f * sinf(m_Time * 4.0f);

		swprintf_s(buffer, 96, L"%s  %d   [C]", Text("ui_lv_points").c_str(), m_Stats.unspentPoints);

		const float width = r->MeasureTextWidth(buffer, 16, FONT_UI, true);

		DrawPanel(right - width - 46.0f, top + 20.0f, width + 24.0f, 34.0f, 0.82f, DEPTH_PANEL);
		r->PushRect(right - width - 46.0f, top + 20.0f, 2.0f, 34.0f, COL_DAMAGE_CRIT, DEPTH_PANEL_TOP);
		r->PushText(buffer, right - width - 34.0f, top + 27.0f, 16, FONT_UI, true,
			RGBA(0.96f, 0.86f, 0.55f, blink), ALIGN_LEFT);
	}

	// --- controls ---
	r->PushText(Text("ui_lv_controls"), left + 22.0f, bottom - 32.0f, 14, FONT_UI, false,
		RGBA(0.62f, 0.64f, 0.62f, 0.62f), ALIGN_LEFT);
}

void Level::DrawStatsScreen()
{
	Renderer* r = m_Renderer;

	const float width = (float)r->GetWidth();
	const float height = (float)r->GetHeight();

	r->PushRect(-width * 0.5f, -height * 0.5f, width, height,
		RGBA(0.0f, 0.0f, 0.0f, 0.66f), DEPTH_PANEL - 1.0f);

	const float boxWidth = 520.0f;
	const float boxHeight = 392.0f;
	const float boxX = -boxWidth * 0.5f;
	const float boxY = -boxHeight * 0.5f;

	DrawPanel(boxX, boxY, boxWidth, boxHeight, 0.94f, DEPTH_PANEL);
	r->PushRect(boxX, boxY, 3.0f, boxHeight, COL_SHU, DEPTH_PANEL_TOP);

	r->PushText(Text("ui_lv_stats_title"), boxX + 28.0f, boxY + 22.0f, 22, FONT_SERIF, true,
		RGBA(0.86f, 0.72f, 0.54f, 0.98f), ALIGN_LEFT);

	wchar_t buffer[96];
	swprintf_s(buffer, 96, L"%s  %d", Text("ui_lv_points").c_str(), m_Stats.unspentPoints);
	r->PushText(buffer, boxX + boxWidth - 28.0f, boxY + 26.0f, 16, FONT_UI, true,
		m_Stats.unspentPoints > 0 ? RGBA(0.96f, 0.86f, 0.55f, 0.98f) : RGBA(0.55f, 0.56f, 0.54f, 0.85f),
		ALIGN_RIGHT);

	// Derived values, so the player can see what a point actually buys.
	const float derived[STAT_COUNT] =
	{
		m_Stats.MaxHealth(),
		m_Stats.AttackPower(),
		m_Stats.DamageReduction() * 100.0f,
		m_Stats.MoveSpeed()
	};

	for (int i = 0; i < STAT_COUNT; ++i)
	{
		const float rowY = boxY + 72.0f + i * 58.0f;

		r->PushRect(boxX + 24.0f, rowY + 44.0f, boxWidth - 48.0f, 1.0f,
			RGBA(0.35f, 0.36f, 0.34f, 0.35f), DEPTH_PANEL_TOP);

		swprintf_s(buffer, 96, L"%d", i + 1);
		r->PushText(buffer, boxX + 34.0f, rowY + 6.0f, 15, FONT_UI, true,
			RGBA(0.70f, 0.62f, 0.48f, 0.9f), ALIGN_LEFT);

		r->PushText(Text(STAT_NAME_KEYS[i]), boxX + 60.0f, rowY, 19, FONT_SERIF, true,
			RGBA(0.93f, 0.92f, 0.88f, 0.98f), ALIGN_LEFT);

		r->PushText(Text(STAT_DESC_KEYS[i]), boxX + 60.0f, rowY + 25.0f, 13, FONT_UI, false,
			RGBA(0.62f, 0.64f, 0.62f, 0.80f), ALIGN_LEFT);

		swprintf_s(buffer, 96, L"%d", m_Stats.GetPoints((StatKind)i));
		r->PushText(buffer, boxX + boxWidth - 118.0f, rowY + 2.0f, 19, FONT_SERIF, true,
			RGBA(0.90f, 0.89f, 0.85f, 0.95f), ALIGN_RIGHT);

		if (i == STAT_GUARD)
		{
			swprintf_s(buffer, 96, L"%.0f%%", derived[i]);
		}
		else if (i == STAT_AGILITY)
		{
			swprintf_s(buffer, 96, L"%.1f", derived[i]);
		}
		else
		{
			swprintf_s(buffer, 96, L"%.0f", derived[i]);
		}

		r->PushText(buffer, boxX + boxWidth - 28.0f, rowY + 4.0f, 16, FONT_UI, false,
			RGBA(0.66f, 0.78f, 0.92f, 0.92f), ALIGN_RIGHT);
	}

	swprintf_s(buffer, 96, L"%s  %d", Text("ui_lv_total_experience").c_str(), m_Stats.totalExperience);
	r->PushText(buffer, boxX + 28.0f, boxY + boxHeight - 82.0f, 14, FONT_UI, false,
		COL_EXPERIENCE, ALIGN_LEFT);

	swprintf_s(buffer, 96, L"%s  %.2f s", Text("ui_lv_attack_interval").c_str(), m_Stats.AttackInterval());
	r->PushText(buffer, boxX + 28.0f, boxY + boxHeight - 58.0f, 14, FONT_UI, false,
		COL_EXPERIENCE, ALIGN_LEFT);

	r->PushText(Text("ui_lv_stat_hint"), boxX + 28.0f, boxY + boxHeight - 34.0f, 14, FONT_UI, false,
		RGBA(0.68f, 0.68f, 0.64f, 0.80f), ALIGN_LEFT);
}

void Level::DrawDownedScreen()
{
	Renderer* r = m_Renderer;

	const float alpha = Clamp(m_StateTime / 0.5f, 0.0f, 1.0f);

	r->PushText(Text("ui_lv_downed"), 0.0f, -30.0f, 34, FONT_SERIF, true,
		RGBA(0.88f, 0.84f, 0.76f, alpha), ALIGN_CENTER);
	r->PushText(Text("ui_lv_respawn"), 0.0f, 20.0f, 16, FONT_UI, false,
		RGBA(0.68f, 0.66f, 0.62f, alpha * 0.9f), ALIGN_CENTER);
}

// ---------------------------------------------------------------- frame

void Level::Render()
{
	m_Renderer->SetCamera(m_CameraX, m_CameraY);
	m_Renderer->BeginFrame(COL_SKY, m_Time);
	m_SceneGraph.Render(*m_Renderer);
	m_Renderer->EndFrame();
}
