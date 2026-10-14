#include "stdafx.h"
#include "Bow.h"
#include "Renderer.h"
#include "AnalysisLog.h"

#include <algorithm>
#include <cmath>

namespace
{
	const float RANGE = 12.0f;
	const float SPEED = 20.0f;
	const float COLLISION_STEP = 0.15f;
	const float ARROW_RADIUS = 0.05f;
	const float HIT_RADIUS = 0.6f;
	const float MIN_DISTANCE = 0.001f;
	const float DRAW_HEIGHT = 0.8f;
	const float TAIL_LENGTH = 0.65f;

	void Place(Actor& actor, const ActorPosition& position)
	{
		const ActorPosition world = actor.GetWorldPosition();
		const ActorPosition local = actor.GetPosition();
		actor.SetPosition(local.x + position.x - world.x, local.y + position.y - world.y,
			local.z + position.z - world.z);
	}
}

void Bow::Initialize(SceneGraph& graph, Actor* parent, World& world, size_t targetCount,
	std::function<Actor*(size_t)> target, std::function<void(size_t, float)> hit)
{
	m_World = &world;
	m_TargetCount = targetCount;
	m_Target = target;
	m_Hit = hit;

	for (size_t i = 0; i < m_Arrows.size(); ++i)
	{
		Arrow& arrow = m_Arrows[i];
		arrow = {};
		arrow.actor = graph.CreateActor("Arrow_" + std::to_string(i), parent);
		arrow.actor->SetRenderBounds(-50.0f, -60.0f, 50.0f, 30.0f);
		arrow.actor->SetUpdateCallback([this, i](Actor&, float deltaSeconds)
		{
			Update(i, deltaSeconds);
		});
		arrow.actor->SetRenderCallback([this, i](const Actor&, Renderer& renderer)
		{
			Draw(i, renderer);
		});
		Stop(arrow);
	}
}

bool Bow::ClearShot(const ActorPosition& origin, const ActorPosition& target) const
{
	const float dx = target.x - origin.x;
	const float dy = target.y - origin.y;
	const int steps = static_cast<int>(ceilf(sqrtf(dx * dx + dy * dy) / COLLISION_STEP));

	for (int i = 0; i <= steps; ++i)
	{
		const float t = steps == 0 ? 0.0f : static_cast<float>(i) / steps;
		const float x = origin.x + dx * t;
		const float y = origin.y + dy * t;
		const float shotHeight = origin.z + (target.z - origin.z) * t + DRAW_HEIGHT;

		if (m_World->IsBlocked(x, y, ARROW_RADIUS) || m_World->GetElevation(x, y) > shotHeight - ARROW_RADIUS)
		{
			return false;
		}
	}

	return true;
}

bool Bow::Shoot(const ActorPosition& origin, float damage)
{
	Actor* nearest = NULL;
	float best = RANGE * RANGE;

	for (size_t i = 0; i < m_TargetCount; ++i)
	{
		Actor* target = m_Target(i);

		if (target == NULL || !target->IsVisibleInHierarchy())
		{
			continue;
		}

		const ActorPosition position = target->GetWorldPosition();
		const float dx = position.x - origin.x;
		const float dy = position.y - origin.y;
		const float distance = dx * dx + dy * dy;

		if (distance < best && distance > MIN_DISTANCE && ClearShot(origin, position))
		{
			nearest = target;
			best = distance;
		}
	}

	if (nearest == NULL)
	{
		return false;
	}

	for (Arrow& arrow : m_Arrows)
	{
		if (arrow.remaining > 0.0f)
		{
			continue;
		}

		const ActorPosition target = nearest->GetWorldPosition();
		const float distance = sqrtf(best);
		arrow.dx = (target.x - origin.x) / distance;
		arrow.dy = (target.y - origin.y) / distance;
		arrow.dz = (target.z - origin.z) / distance;
		arrow.remaining = RANGE;
		arrow.damage = damage;
		Place(*arrow.actor, origin);
		arrow.actor->SetActive(true);
		arrow.actor->SetVisible(true);
		AnalysisLog::Get().Event("combat.bow_shot", "damage=" + std::to_string(damage));

		return true;
	}

	return false;
}

void Bow::Stop(Arrow& arrow)
{
	arrow.remaining = 0.0f;
	arrow.actor->SetActive(false);
	arrow.actor->SetVisible(false);
}

void Bow::Clear()
{
	for (Arrow& arrow : m_Arrows)
	{
		if (arrow.actor != NULL)
		{
			Stop(arrow);
		}
	}
}

void Bow::Update(size_t index, float deltaSeconds)
{
	Arrow& arrow = m_Arrows[index];
	float travel = (std::min)(SPEED * deltaSeconds, arrow.remaining);
	ActorPosition position = arrow.actor->GetWorldPosition();

	while (travel > 0.0f)
	{
		const float step = (std::min)(travel, COLLISION_STEP);
		position.x += arrow.dx * step;
		position.y += arrow.dy * step;
		position.z += arrow.dz * step;
		travel -= step;
		arrow.remaining -= step;

		if (m_World->IsBlocked(position.x, position.y, ARROW_RADIUS)
			|| m_World->GetElevation(position.x, position.y) > position.z + DRAW_HEIGHT - ARROW_RADIUS)
		{
			Stop(arrow);
			return;
		}

		for (size_t i = 0; i < m_TargetCount; ++i)
		{
			Actor* target = m_Target(i);

			if (target == NULL || !target->IsVisibleInHierarchy())
			{
				continue;
			}

			const ActorPosition enemy = target->GetWorldPosition();
			const float dx = enemy.x - position.x;
			const float dy = enemy.y - position.y;

			if (dx * dx + dy * dy <= HIT_RADIUS * HIT_RADIUS
				&& fabsf(enemy.z - position.z) <= HIT_RADIUS && ClearShot(position, enemy))
			{
				const float damage = arrow.damage;
				Stop(arrow);
				m_Hit(i, damage);
				AnalysisLog::Get().Event("combat.bow_hit", "target_slot=" + std::to_string(i));
				return;
			}
		}
	}

	Place(*arrow.actor, position);

	if (arrow.remaining <= 0.0f)
	{
		Stop(arrow);
	}
}

void Bow::Draw(size_t index, Renderer& renderer)
{
	const Arrow& arrow = m_Arrows[index];
	const ActorPosition position = arrow.actor->GetWorldPosition();
	float x, y, tailX, tailY;
	renderer.WorldToScreen(position.x, position.y, position.z + DRAW_HEIGHT, &x, &y);
	renderer.WorldToScreen(position.x - arrow.dx * TAIL_LENGTH,
		position.y - arrow.dy * TAIL_LENGTH, position.z - arrow.dz * TAIL_LENGTH + DRAW_HEIGHT, &tailX, &tailY);
	const float depth = position.x + position.y + 0.2f;
	renderer.PushLine(tailX, tailY, x, y, 2.5f, RGBA(0.9f, 0.77f, 0.45f), depth);
	renderer.PushDiamond(x, y, 3.0f, 3.0f, RGBA(1.0f, 0.95f, 0.8f), depth + 0.01f);
}

void Bow::DrawEquipped(Renderer& renderer, const ActorPosition& position)
{
	float x, y;
	renderer.WorldToScreen(position.x, position.y, position.z, &x, &y);
	const float depth = position.x + position.y + 0.3f;
	const Color wood = RGBA(0.64f, 0.43f, 0.24f);
	renderer.PushLine(x + 10.0f, y - 42.0f, x + 21.0f, y - 28.0f, 3.0f, wood, depth);
	renderer.PushLine(x + 21.0f, y - 28.0f, x + 10.0f, y - 14.0f, 3.0f, wood, depth);
	renderer.PushLine(x + 10.0f, y - 42.0f, x + 10.0f, y - 14.0f, 1.0f,
		RGBA(0.85f, 0.84f, 0.7f), depth);
}
