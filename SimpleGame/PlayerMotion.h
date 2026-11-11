#pragma once

#include <cmath>

namespace PlayerMotion
{
	const float SWORD_DURATION = 0.40f;
	const float BOW_DURATION = 0.50f;
	const float SWORD_IMPACT = 0.46f;
	const float BOW_RELEASE = 0.66f;
	const float COOLDOWN_FRACTION = 0.90f;
	const float PI = 3.14159265f;
	const float HIP_Y = -18.0f;
	const float FORWARD_DOT = 0.20f;
	const float WALK_CYCLE_DISTANCE = 2.2f;
	const float RUN_CYCLE_DISTANCE = 2.8f;
	const float MOVE_START_RESPONSE = 14.0f;
	const float MOVE_STOP_RESPONSE = 18.0f;
	const float MOVE_TURN_RESPONSE = 16.0f;
	const float RUN_RESPONSE = 8.0f;

	inline float Ease(float value)
	{
		const float t = fminf(fmaxf(value, 0.0f), 1.0f);

		return t * t * (3.0f - 2.0f * t);
	}
}

struct PlayerLimbOffset
{
	float x = 0.0f;
	float y = 0.0f;
};

struct PlayerMovePose
{
	float bodyX = 0.0f;
	float bodyY = 0.0f;
	float tilt = 0.0f;
	float cape = 0.0f;
	PlayerLimbOffset feet[2];
	PlayerLimbOffset hands[2];
};

// Distance drives the gait after collision, so blocked input cannot keep stepping.
struct PlayerMoveMotion
{
	float phase = 0.0f;
	float blend = 0.0f;
	float runBlend = 0.0f;
	float directionX = 0.0f;
	float directionY = 0.5f;

	void Clear()
	{
		phase = 0.0f;
		blend = 0.0f;
		runBlend = 0.0f;
	}

	void Advance(float seconds, float movedX, float movedY, bool running)
	{
		if (!std::isfinite(seconds) || seconds <= 0.0f
			|| !std::isfinite(movedX) || !std::isfinite(movedY))
		{
			return;
		}

		const float distance = sqrtf(movedX * movedX + movedY * movedY);
		const bool moving = std::isfinite(distance) && distance > 0.00001f;
		const float target = moving ? fminf(distance / seconds / 0.65f, 1.0f) : 0.0f;
		const float response = moving ? PlayerMotion::MOVE_START_RESPONSE : PlayerMotion::MOVE_STOP_RESPONSE;
		blend += (target - blend) * (1.0f - expf(-seconds * response));
		runBlend += ((running && moving ? 1.0f : 0.0f) - runBlend)
			* (1.0f - expf(-seconds * PlayerMotion::RUN_RESPONSE));

		if (moving)
		{
			const float turn = 1.0f - expf(-seconds * PlayerMotion::MOVE_TURN_RESPONSE);
			const float projectedX = (movedX - movedY) / distance * 0.70710678f;
			const float projectedY = (movedX + movedY) / distance * 0.35355339f;
			directionX += (projectedX - directionX) * turn;
			directionY += (projectedY - directionY) * turn;
			const float cycle = running ? PlayerMotion::RUN_CYCLE_DISTANCE : PlayerMotion::WALK_CYCLE_DISTANCE;
			phase = fmodf(phase + distance / cycle * PlayerMotion::PI * 2.0f, PlayerMotion::PI * 2.0f);
		}
		else if (blend < 0.001f)
		{
			Clear();
		}
	}

	PlayerMovePose Sample(float upperWeight = 1.0f) const
	{
		PlayerMovePose pose;
		const float stride = 4.2f + runBlend * 2.2f;
		const float lift = 2.8f + runBlend * 2.0f;
		const float sway = sinf(phase);
		const float travel = cosf(phase);
		pose.bodyX = sway * (0.45f + runBlend * 0.35f) * blend * upperWeight;
		pose.bodyY = -(1.0f - cosf(phase * 2.0f)) * (0.40f + runBlend * 0.35f)
			* blend * (0.35f + upperWeight * 0.65f);
		pose.tilt = (-directionX * (0.025f + runBlend * 0.07f) + sway * 0.022f)
			* blend * upperWeight;
		pose.cape = (-directionX * (1.5f + runBlend * 2.0f) + sinf(phase - 0.65f) * (0.8f + runBlend))
			* blend * upperWeight;

		for (int side = 0; side < 2; ++side)
		{
			const float sign = side == 0 ? 1.0f : -1.0f;
			const float step = travel * sign * stride * blend;
			const float raised = fmaxf(sway * sign, 0.0f) * lift * blend;
			pose.feet[side].x = directionX * step;
			pose.feet[side].y = directionY * step - raised;
			pose.hands[side].x = -directionX * travel * sign * (1.2f + runBlend) * blend * upperWeight;
			pose.hands[side].y = -directionY * travel * sign * (2.0f + runBlend * 1.5f) * blend * upperWeight;
		}

		return pose;
	}
};

// A single impact edge survives a long frame; changing equipment cancels it.
struct PlayerAttackMotion
{
	bool active = false;
	bool bow = false;
	bool fullCircle = false;
	bool impactApplied = false;
	float elapsed = 0.0f;
	float duration = 0.0f;
	float directionX = 0.7071f;
	float directionY = 0.7071f;
	int side = -1;

	void Begin(bool useBow, float interval, float facingX, float facingY, bool circle = false)
	{
		bow = useBow;
		fullCircle = circle;
		elapsed = 0.0f;
		duration = fminf(bow ? PlayerMotion::BOW_DURATION : PlayerMotion::SWORD_DURATION,
			fmaxf(interval, 0.01f) * PlayerMotion::COOLDOWN_FRACTION);
		impactApplied = false;
		active = true;

		if (!bow)
		{
			side = -side;
		}

		SetDirection(facingX, facingY);
	}

	void SetDirection(float x, float y)
	{
		const float length = sqrtf(x * x + y * y);

		if (std::isfinite(length) && length > 0.0001f)
		{
			directionX = x / length;
			directionY = y / length;
		}
	}

	void Clear()
	{
		active = false;
		impactApplied = false;
		elapsed = 0.0f;
	}

	bool Advance(float seconds)
	{
		if (!active || !std::isfinite(seconds) || seconds <= 0.0f)
		{
			return false;
		}

		elapsed = fminf(elapsed + seconds, duration);
		const bool impact = !impactApplied && elapsed >= duration * ImpactProgress();
		impactApplied = impactApplied || impact;
		active = elapsed < duration;

		return impact;
	}

	float ImpactProgress() const
	{
		return bow ? PlayerMotion::BOW_RELEASE : PlayerMotion::SWORD_IMPACT;
	}

	float Progress() const
	{
		return duration > 0.0f ? fminf(elapsed / duration, 1.0f) : 0.0f;
	}

	float Angle() const
	{
		// Quarter-view projection compresses world Y relative to screen X.
		return atan2f((directionX + directionY) * 16.0f, (directionX - directionY) * 32.0f);
	}

	float BowPull() const
	{
		if (!active || !bow)
		{
			return 0.0f;
		}

		const float progress = Progress();

		return progress < PlayerMotion::BOW_RELEASE
			? PlayerMotion::Ease(progress / PlayerMotion::BOW_RELEASE)
			: 1.0f - PlayerMotion::Ease((progress - PlayerMotion::BOW_RELEASE) / 0.12f);
	}

	float SwordSweep() const
	{
		return PlayerMotion::Ease((Progress() - 0.24f) / 0.46f);
	}

	float Tilt() const
	{
		if (!active)
		{
			return 0.0f;
		}

		if (bow)
		{
			return -0.10f * BowPull();
		}

		const float progress = Progress();
		const float recovery = 1.0f - PlayerMotion::Ease((progress - 0.70f) / 0.30f);

		return side * (-0.15f * PlayerMotion::Ease(progress / 0.24f) + 0.34f * SwordSweep()) * recovery;
	}
};
