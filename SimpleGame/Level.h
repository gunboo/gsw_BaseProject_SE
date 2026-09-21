#pragma once

#include <string>
#include <vector>

#include "Dialogue.h"
#include "Input.h"
#include "Lighting.h"
#include "Model.h"
#include "Renderer.h"
#include "Stats.h"
#include "World.h"

enum LevelState
{
	LEVEL_PLAY = 0,
	LEVEL_STATS,
	LEVEL_DOWNED
};

enum EnemyKind
{
	ENEMY_WISP = 0,		// fast, weak, carries its own light
	ENEMY_SHADE,		// average
	ENEMY_ONI,			// slow, heavy hitter
	ENEMY_KIND_COUNT
};

enum PickupKind
{
	PICKUP_ORB = 0,		// experience
	PICKUP_POTION,		// heals
	PICKUP_WHETSTONE	// permanent attack bonus
};

struct Enemy
{
	EnemyKind kind;
	float x;
	float y;
	float health;
	float maxHealth;
	float attackTimer;
	float hitFlash;
	float wanderTimer;
	float wanderX;
	float wanderY;
	float phase;
	bool alive;
	float respawnTimer;
};

struct Pickup
{
	PickupKind kind;
	float x;
	float y;
	float phase;
	float life;
	int value;
	bool active;
};

struct FloatingText
{
	std::wstring text;
	float x;
	float y;
	float rise;
	float life;
	float maxLife;
	Color color;
	int size;
};

// Level 1: a random forest basin used purely to grind experience and spend the
// points it pays out. Characters, scenery and pickups use cached models;
// ground, interface and transient effects use runtime primitives.
class Level
{
public:
	Level();

	bool Initialize(Renderer* renderer, ModelLibrary* models, DialogueDB* dialogue, unsigned int seed);

	void Update(float deltaSeconds);
	void Render();
	void OnKey(unsigned char key, bool down, bool shift);

	bool WantsExit() const
	{
		return m_WantsExit;
	}

private:
	// --- simulation ---
	void UpdatePlayer(float deltaSeconds);
	void UpdateEnemies(float deltaSeconds);
	void UpdatePickups(float deltaSeconds);
	void UpdateEffects(float deltaSeconds);
	void UpdateLights();

	void Attack();
	void DamageEnemy(int index, float amount, bool critical);
	void DamagePlayer(float amount);
	void KillEnemy(int index);
	void SpawnEnemy(int index, bool awayFromPlayer);
	void SpawnPickup(PickupKind kind, float x, float y, int value);
	void CollectPickup(int index);
	void AddFloatingText(const std::wstring& text, float x, float y, const Color& color, int size);
	void GrantExperience(int amount);

	// --- rendering ---
	void DrawGround();
	void DrawScenery();
	void DrawEnemies();
	void DrawPickups();
	void DrawPlayer();
	void DrawSwing();
	void DrawFloatingText();
	void DrawAtmosphere();
	void DrawHud();
	void DrawStatsScreen();
	void DrawDownedScreen();
	void DrawBar(float x, float y, float width, float height, float fill,
		const Color& fillColor, const Color& backColor, float depth);
	void DrawPanel(float x, float y, float width, float height, float alpha, float depth);

	bool OnScreen(float screenX, float screenY, float margin) const;
	const Model* ModelFor(EnemyKind kind) const;
	const Model* ModelFor(PickupKind kind) const;
	std::wstring Text(const char* key) const;

	Renderer* m_Renderer;
	ModelLibrary* m_Models;
	DialogueDB* m_Dialogue;

	World m_World;
	Lighting m_Lighting;
	PlayerStats m_Stats;
	Rng m_Rng;

	LevelState m_State;
	bool m_WantsExit;
	float m_Time;
	float m_StateTime;
	float m_Fade;

	float m_PlayerX;
	float m_PlayerY;
	float m_FacingX;
	float m_FacingY;
	float m_Stride;
	bool m_MoveKey[MOVE_COUNT];
	bool m_Running;
	bool m_Moving;
	bool m_AttackHeld;

	float m_SwingCooldown;
	float m_SwingAnim;
	float m_HurtFlash;
	float m_DownedTimer;
	float m_LevelUpFlash;

	float m_CameraX;
	float m_CameraY;

	int m_Kills;

	std::vector<Enemy> m_Enemies;
	std::vector<Pickup> m_Pickups;
	std::vector<FloatingText> m_FloatingText;
};
