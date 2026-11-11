#pragma once

#include "PlayerMotion.h"

enum ResidentOccupation
{
	OCCUPATION_NONE = 0,
	OCCUPATION_GARDEN,
	OCCUPATION_TORCH,
	OCCUPATION_SENTRY,
	OCCUPATION_FISH,
	OCCUPATION_HUNT,
	OCCUPATION_FORGE,
	OCCUPATION_PRAY,
	OCCUPATION_READ,
	OCCUPATION_CARRY,
	OCCUPATION_COUNT
};

namespace ResidentActivity
{
	const float FISH_CYCLE = 9.0f;
	const float SHOT_RELEASE = 0.55f;
	const float SHOT_DURATION = 1.1f;
	const float WORK_CYCLE = 2.4f;
	const float ARRIVAL_RANGE = 0.85f;
	const float HUNT_RANGE = 7.0f;
	const float HUNT_INTERVAL = 2.4f;
	const float PATROL_REST_MIN = 3.0f;
	const float PATROL_REST_MAX = 6.0f;

	inline const char* Name(ResidentOccupation occupation)
	{
		static const char* const names[] =
		{
			"none", "garden", "torch", "sentry", "fish", "hunt", "forge", "pray", "read", "carry"
		};

		return occupation >= OCCUPATION_NONE && occupation < OCCUPATION_COUNT ? names[occupation] : "none";
	}

	inline bool IsPatrol(ResidentOccupation occupation)
	{
		return occupation == OCCUPATION_TORCH || occupation == OCCUPATION_SENTRY || occupation == OCCUPATION_HUNT;
	}
}

struct ResidentActivityMotion
{
	ResidentOccupation occupation = OCCUPATION_NONE;
	PlayerMoveMotion movement;
	float time = 0.0f;
	float workBlend = 0.0f;
	float shotTime = ResidentActivity::SHOT_DURATION;
	bool shotReleased = true;
	float strike = 0.0f;
	bool working = false;
	bool alerted = false;
	float facingX = 0.7071f;
	float facingY = 0.7071f;

	void BeginShot()
	{
		shotTime = 0.0f;
		shotReleased = false;
	}

	bool Advance(float seconds)
	{
		if (!std::isfinite(seconds) || seconds <= 0.0f)
		{
			return false;
		}

		time = fmodf(time + seconds, 360.0f);
		workBlend += ((working ? 1.0f : 0.0f) - workBlend) * (1.0f - expf(-seconds * 9.0f));
		strike = fmaxf(0.0f, strike - seconds);
		shotTime = fminf(shotTime + seconds, ResidentActivity::SHOT_DURATION);
		const bool release = !shotReleased && shotTime >= ResidentActivity::SHOT_RELEASE;
		shotReleased = shotReleased || release;

		return release;
	}

	float WorkStroke() const
	{
		return (sinf(time / ResidentActivity::WORK_CYCLE * PlayerMotion::PI * 2.0f) * 0.5f + 0.5f) * workBlend;
	}

	float BowPull() const
	{
		return shotTime < ResidentActivity::SHOT_RELEASE
			? PlayerMotion::Ease(shotTime / ResidentActivity::SHOT_RELEASE)
			: 1.0f - PlayerMotion::Ease((shotTime - ResidentActivity::SHOT_RELEASE) / 0.18f);
	}
};
