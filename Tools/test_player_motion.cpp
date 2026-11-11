#include "../SimpleGame/PlayerMotion.h"

#include "TestAssertions.h"
#include <cstdio>

int main()
{
	PlayerAttackMotion motion;
	motion.Begin(false, 0.45f, 1.0f, 1.0f);
	assert(!motion.Advance(motion.duration * (PlayerMotion::SWORD_IMPACT - 0.01f)));
	assert(motion.Advance(motion.duration * 0.02f));
	assert(!motion.Advance(1.0f));
	assert(!motion.active);
	assert(!motion.Advance(1.0f));

	motion.Begin(true, 0.65f, 1.0f, -1.0f);
	assert(!motion.Advance(motion.duration * 0.5f));
	assert(motion.BowPull() > 0.5f);
	assert(motion.Advance(motion.duration * 0.3f));
	assert(!motion.Advance(1.0f));

	motion.Begin(false, 0.45f, 1.0f, 0.0f);
	assert(motion.Advance(1.0f)); // A stall crosses both impact and end exactly once.
	assert(!motion.Advance(1.0f));
	motion.Begin(false, 0.45f, 1.0f, 0.0f);
	motion.Advance(0.02f);
	motion.Clear(); // Weapon switch or death cancels a pending hit.
	assert(!motion.Advance(1.0f));

	const float intervals[] = { 0.62f, 0.45f, 0.22f, 0.18f };

	for (float interval : intervals)
	{
		for (int bow = 0; bow < 2; ++bow)
		{
			motion.Begin(bow != 0, interval, 0.0f, 0.0f);
			assert(motion.duration < interval);
			assert(std::isfinite(motion.Angle()));
			assert(!motion.Advance(0.0f));
			assert(!motion.Advance(-1.0f));
			int impacts = 0;

			while (motion.active)
			{
				impacts += motion.Advance(0.05f) ? 1 : 0;
				assert(std::isfinite(motion.Tilt()));
				assert(motion.BowPull() >= 0.0f && motion.BowPull() <= 1.0f);
			}

			assert(impacts == 1);
		}
	}

	std::puts("PASS: one impact per attack; cancellation; long frames; faster intervals; finite poses");

	PlayerMoveMotion walk;
	PlayerMoveMotion fineWalk;
	const float diagonal = 0.70710678f;

	for (int frame = 0; frame < 60; ++frame)
	{
		walk.Advance(1.0f / 60.0f, diagonal * 3.1f / 60.0f, -diagonal * 3.1f / 60.0f, false);
	}

	for (int frame = 0; frame < 240; ++frame)
	{
		fineWalk.Advance(1.0f / 240.0f, diagonal * 3.1f / 240.0f, -diagonal * 3.1f / 240.0f, false);
	}

	assert(fabsf(walk.phase - fineWalk.phase) < 0.0001f);
	assert(fabsf(walk.blend - fineWalk.blend) < 0.0001f);
	assert(walk.directionX > 0.99f);
	const float stoppedPhase = walk.phase;

	for (int frame = 0; frame < 60; ++frame)
	{
		// Released keys and a fully blocked wall both report zero displacement.
		walk.Advance(1.0f / 60.0f, 0.0f, 0.0f, true);
		assert(walk.phase == stoppedPhase || walk.phase == 0.0f);
	}

	assert(walk.blend == 0.0f);
	assert(walk.runBlend == 0.0f);
	assert(walk.Sample().feet[0].x == 0.0f);
	assert(walk.Sample().bodyY == 0.0f);

	PlayerMoveMotion run;

	for (int frame = 0; frame < 120; ++frame)
	{
		run.Advance(1.0f / 60.0f, diagonal * 5.2f / 60.0f, diagonal * 5.2f / 60.0f, true);
	}

	assert(run.runBlend > 0.99f);
	assert(run.directionY > 0.49f && fabsf(run.directionX) < 0.0001f);
	const float beforeInvalid = run.phase;
	run.Advance(-1.0f, 1.0f, 1.0f, true);
	assert(run.phase == beforeInvalid);

	for (int angle = 0; angle < 8; ++angle)
	{
		for (int frame = 0; frame < 120; ++frame)
		{
			run.directionX = cosf(angle * PlayerMotion::PI / 4.0f);
			run.directionY = sinf(angle * PlayerMotion::PI / 4.0f) * 0.5f;
			run.phase = frame / 120.0f * PlayerMotion::PI * 2.0f;
			const PlayerMovePose pose = run.Sample();
			const PlayerMovePose attackPose = run.Sample(0.15f);
			assert(std::isfinite(pose.tilt) && fabsf(pose.tilt) < 0.12f);
			assert(fabsf(pose.feet[0].x + pose.feet[1].x) < 0.0001f);
			assert(fabsf(pose.feet[0].x) < 6.5f && fabsf(pose.feet[0].y) < 8.1f);
			assert(fabsf(attackPose.feet[0].x - pose.feet[0].x) < 0.0001f);
			assert(fabsf(attackPose.tilt) <= fabsf(pose.tilt));
		}
	}

	std::puts("PASS: distance-based gait; frame-rate-independent blend; blocked stop; run; eight directions; attack overlay");

	return 0;
}
