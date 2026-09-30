#pragma once

#include "SceneGraph.h"
#include "World.h"
#include "Model.h"
#include "Lighting.h"
#include "Dialogue.h"

// Borrowed actors are owned by Game's scene graph. This simulation has no
// story progression flags and never changes the separate farming level.
class VillageSimulation
{
public:
	bool Initialize(World& world, SceneGraph& graph, Actor* player,
		const std::vector<Actor*>& npcs, ModelLibrary& models, Lighting& lighting, DialogueDB& dialogue);
	void Update(float deltaSeconds);
	void SetState(bool active, bool visible);
	void Attack();
	void DrawHud(Renderer& renderer);
	bool IsArmed(const std::string& id) const;

private:
	enum Behavior
	{
		STAY, WANDER, PATROL, MERCHANT, HEALER, GUARD, BANDIT, FLEE, FOLLOW
	};

	struct Walker
	{
		Actor* actor = NULL;
		ActorPosition home = {};
		std::vector<int> route;
		size_t next = 0;
		float repath = 0.0f;
		float decision = 0.0f;
		float cooldown = 0.0f;
	};

	struct Resident
	{
		std::string id;
		Behavior behavior = STAY;
		float radius = 0.0f;
		float speed = 0.0f;
		Walker walker;
		int destination = -1;
		float afraid = 0.0f;
		bool returning = false;
	};

	struct Monster
	{
		Walker walker;
		int kind = 0;
		float health = 0.0f;
		float respawn = 0.0f;
		float flash = 0.0f;
	};

	bool LoadResidents(const std::vector<Actor*>& actors);
	void BuildNavigation();
	int Cell(const ActorPosition& position) const;
	ActorPosition Center(int cell) const;
	int Nearby(const ActorPosition& origin, float radius);
	bool Safe(const ActorPosition& position) const;
	bool ClearLine(const ActorPosition& a, const ActorPosition& b) const;
	void Navigate(Walker& walker, int target, float speed, float deltaSeconds);
	void UpdateResident(Resident& resident, float deltaSeconds);
	void UpdateMonster(size_t index, float deltaSeconds);
	bool SpawnMonster(size_t index, bool initial);
	void HitMonster(size_t index, float damage);
	void HurtPlayer(float damage);
	void DrawMonster(size_t index, Renderer& renderer);
	void DrawSwing(Renderer& renderer);

	World* m_World = NULL;
	Actor* m_Player = NULL;
	Actor* m_Group = NULL;
	ModelLibrary* m_Models = NULL;
	Lighting* m_Lighting = NULL;
	DialogueDB* m_Dialogue = NULL;
	std::vector<unsigned char> m_Walkable;
	std::vector<unsigned char> m_Edges;
	std::vector<int> m_Open;
	std::vector<int> m_Spawns;
	std::vector<int> m_Parents;
	std::vector<int> m_Queue;
	std::vector<Resident> m_Residents;
	std::vector<Monster> m_Monsters;
	Rng m_Rng;
	float m_Health = 100.0f;
	float m_AttackCooldown = 0.0f;
	float m_Invulnerable = 0.0f;
	float m_Swing = 0.0f;
	float m_LogTimer = 0.0f;
	unsigned int m_Kills = 0;
	unsigned int m_PathRequests = 0;
	unsigned int m_PathFailures = 0;
};
