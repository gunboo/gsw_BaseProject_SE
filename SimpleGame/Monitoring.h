#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "Actor.h"

// Plain snapshots keep the protocol independent of scene ownership and transport.
struct MonitoredEntity
{
	std::string id;
	std::string villageId;
	std::string kind;
	std::string behavior;
	std::string state;
	ActorPosition position = {};
	bool alive = true;
	bool hasHealth = false;
	float health = 0.0f;
	float maxHealth = 0.0f;
	float respawnSeconds = 0.0f;
};

struct MonitoredVillage
{
	std::string id;
	std::string name;
};

struct MonitoredWorld
{
	std::string id;
	std::string name;
	std::string state;
	bool active = false;
	int width = 0;
	int height = 0;
	unsigned int kills = 0;
	std::vector<MonitoredVillage> villages;
	std::vector<MonitoredEntity> npcs;
	std::vector<MonitoredEntity> monsters;
};

class MonitoringPublisher
{
public:
	~MonitoringPublisher();
	void Start();
	void Stop();
	bool IsDue(double nowMs);
	void Publish(const std::vector<MonitoredWorld>& worlds);

private:
	void Run();

	std::string m_SourceId;
	std::string m_InstanceId;
	unsigned short m_Port = 8765;
	std::uint64_t m_Sequence = 0;
	double m_NextPublishMs = 0.0;
	std::mutex m_Mutex;
	std::condition_variable m_Wake;
	std::thread m_Worker;
	std::string m_Pending;
	bool m_Stopping = false;
	bool m_Enabled = false;
	std::atomic<int> m_Connection{ 0 };
	int m_LastConnection = 0;
};
