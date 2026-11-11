#pragma once

#include "SceneGraph.h"
#include "World.h"
#include "Model.h"
#include "Lighting.h"
#include "Dialogue.h"
#include "Bow.h"
#include "Monitoring.h"
#include "ResidentActivity.h"

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
	void SetFacing(float x, float y);
	const PlayerAttackMotion& AttackMotion() const
	{
		return m_AttackMotion;
	}
	void SelectWeapon(WeaponKind weapon);
	bool HasBow() const
	{
		return m_Weapon == WEAPON_BOW;
	}
	void DrawHud(Renderer& renderer);
	bool IsArmed(const std::string& id) const;
	bool DrawResident(const std::string& id, Renderer& renderer, const Color& robe, float scale, bool hat);
	void AddResidentLights(Lighting& lighting) const;
	void CaptureMonitoring(MonitoredWorld& snapshot) const;
	std::string RumorDialogue(const std::string& id) const;

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
		int plannedTarget = -1;
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
		bool moving = false;
		ResidentActivityMotion activity;
		ActorPosition workTarget = {};
		float dutyTimer = 0.0f;
		bool atDuty = true;
		bool taskAvailable = true;
		int homeVillage = -1;
		int destinationVillage = -1;
		bool courier = false;
		bool traveling = false;
		float visitTimer = 10.0f;
		unsigned int rumors = 0;
	};

	struct Monster
	{
		Walker walker;
		int kind = 0;
		float health = 0.0f;
		float respawn = 0.0f;
		float flash = 0.0f;
		int road = 0;
		ActorPosition lastDeath = {};
	};

	struct Settlement
	{
		std::string id;
		std::string name;
		ActorPosition center = {};
		float radius = 14.0f;
		float baseFear = 0.0f;
		float baseTrust = 0.0f;
		float baseProsperity = 0.0f;
		MonitoredVillage statistics;
		unsigned int rumors = 0;
		std::vector<int> neighbors;
	};

	struct Road
	{
		int from = -1;
		int to = -1;
		std::vector<int> spawns;
	};

	struct Rumor
	{
		std::string id;
		std::string dialogue;
		float fear = 0.0f;
		float trust = 0.0f;
		float prosperity = 0.0f;
	};

	bool LoadSettlements();
	int VillageAt(const ActorPosition& position) const;
	void UpdateCourier(Resident& resident, float deltaSeconds);
	void ShareRumors();
	void UpdateAtmosphere();
	bool LoadResidents(const std::vector<Actor*>& actors);
	void BuildNavigation();
	int Cell(const ActorPosition& position) const;
	ActorPosition Center(int cell) const;
	int Nearby(const ActorPosition& origin, float radius);
	bool Safe(const ActorPosition& position) const;
	bool ClearLine(const ActorPosition& a, const ActorPosition& b) const;
	void Navigate(Walker& walker, int target, float speed, float deltaSeconds);
	void UpdateResident(Resident& resident, float deltaSeconds);
	bool UpdateDuty(Resident& resident, float deltaSeconds);
	void FindWorkTarget(Resident& resident);
	void UpdateMonster(size_t index, float deltaSeconds);
	bool SpawnMonster(size_t index, bool initial);
	void HitMonster(size_t index, float damage);
	void ResolveAttack();
	void HurtPlayer(float damage);
	void DrawMonster(size_t index, Renderer& renderer);
	void DrawSwing(Renderer& renderer);

	World* m_World = NULL;
	Bow m_Bow;
	Bow m_HuntingBow;
	WeaponKind m_Weapon = WEAPON_SWORD;
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
	std::vector<Settlement> m_Settlements;
	std::vector<Road> m_Roads;
	std::vector<Rumor> m_Rumors;
	float m_ContactTimer = 0.0f;
	Rng m_Rng;
	float m_Health = 100.0f;
	float m_AttackCooldown = 0.0f;
	float m_Invulnerable = 0.0f;
	PlayerAttackMotion m_AttackMotion;
	float m_FacingX = 0.7071f;
	float m_FacingY = 0.7071f;
	float m_LogTimer = 0.0f;
	unsigned int m_Kills = 0;
	unsigned int m_PathRequests = 0;
	unsigned int m_PathFailures = 0;
};
