#include "stdafx.h"
#include "ResidentArt.h"

#include <cmath>

namespace
{
	Color Material(const Color& color, const ShadeParams& shade)
	{
		Color result = Mix(Modulate(color, shade.light), shade.fogColor, shade.fog);
		result.a = color.a * shade.tint.a;

		return result;
	}

	struct Painter
	{
		Renderer& renderer;
		float x;
		float y;
		float scale;
		float depth;

		void Line(float x0, float y0, float x1, float y1, float width, const Color& color, float bias = 0.02f)
		{
			renderer.PushLine(x + x0 * scale, y + y0 * scale, x + x1 * scale, y + y1 * scale,
				width * scale, color, depth + bias);
		}

		void Ellipse(float px, float py, float rx, float ry, const Color& color, float bias = 0.02f)
		{
			renderer.PushEllipse(x + px * scale, y + py * scale, rx * scale, ry * scale, color, depth + bias, 10);
		}

		void Quad(float px, float py, float width, float height, const Color& color, float bias = 0.02f)
		{
			renderer.PushRect(x + px * scale, y + py * scale, width * scale, height * scale, color, depth + bias);
		}

		void Triangle(float x0, float y0, float x1, float y1, float x2, float y2, const Color& color)
		{
			const float points[] = { x + x0 * scale, y + y0 * scale,
				x + x1 * scale, y + y1 * scale, x + x2 * scale, y + y2 * scale };
			renderer.PushPolygon(points, 3, color, depth + 0.03f);
		}
	};

	void Rotate(float x, float y, float tilt, float& px, float& py)
	{
		px = x * cosf(tilt) - (y - PlayerMotion::HIP_Y) * sinf(tilt);
		py = PlayerMotion::HIP_Y + x * sinf(tilt) + (y - PlayerMotion::HIP_Y) * cosf(tilt);
	}

	void Arm(Painter& painter, float side, float handX, float handY, float tilt, const ShadeParams& shade)
	{
		float shoulderX, shoulderY;
		Rotate(side * 5.4f, -22.3f, tilt, shoulderX, shoulderY);
		const float elbowX = (shoulderX + handX) * 0.5f + side;
		const float elbowY = (shoulderY + handY) * 0.5f + 2.0f;
		const Color cloth = Material(shade.tint, shade);
		painter.Line(shoulderX, shoulderY, elbowX, elbowY, 4.5f, cloth, 0.01f);
		painter.Line(elbowX, elbowY, handX, handY, 2.8f, cloth, 0.011f);
		painter.Ellipse(handX, handY, 1.8f, 2.0f, Material(RGBA(0.67f, 0.55f, 0.40f), shade), 0.012f);
	}
}

void DrawResidentArt(Renderer& renderer, const ModelLibrary& models, float x, float y,
	float depth, float scale, const ShadeParams& shade, bool hat, bool armed,
	const ResidentActivityMotion& motion, float targetX, float targetY)
{
	const Model* body = models.Find("resident_active");

	if (body == NULL)
	{
		return;
	}

	PlayerMovePose pose = motion.movement.Sample();
	const float stroke = motion.WorkStroke();
	const float look = sinf(motion.time * 0.7f) * motion.workBlend;
	const float facing = (motion.facingX - motion.facingY) < -0.1f ? -1.0f : 1.0f;

	if (motion.occupation == OCCUPATION_GARDEN || motion.occupation == OCCUPATION_FORGE)
	{
		pose.tilt += facing * (0.08f * motion.workBlend + stroke * 0.15f);
		pose.bodyY += stroke * 1.2f;
	}
	else if (motion.occupation == OCCUPATION_READ || motion.occupation == OCCUPATION_PRAY)
	{
		pose.tilt += look * 0.035f;
		pose.bodyY += motion.workBlend * 0.5f;
	}
	else if (ResidentActivity::IsPatrol(motion.occupation))
	{
		pose.tilt += look * 0.045f;
	}

	DrawModel(&renderer, *body, x, y, scale, depth, shade, pose.tilt, MODEL_FLAG_PLAYER_LEFT_ARM, &pose);
	const Model* kasa = hat ? models.Find("resident_kasa") : NULL;

	if (kasa != NULL)
	{
		DrawModel(&renderer, *kasa, x, y, scale, depth, shade, pose.tilt, 0, &pose);
	}

	Painter painter = { renderer, x + pose.bodyX * scale, y + pose.bodyY * scale, scale, depth };
	const Color wood = Material(RGBA(0.39f, 0.28f, 0.15f), shade);
	const Color steel = Material(RGBA(0.69f, 0.73f, 0.65f), shade);
	const Color rope = Material(RGBA(0.71f, 0.66f, 0.47f), shade);
	float leftX, leftY, rightX, rightY;
	Rotate(-8.0f, -15.0f, pose.tilt, leftX, leftY);
	Rotate(8.0f, -15.0f, pose.tilt, rightX, rightY);
	leftX += pose.hands[0].x;
	leftY += pose.hands[0].y;
	rightX += pose.hands[1].x;
	rightY += pose.hands[1].y;

	switch (motion.occupation)
	{
	case OCCUPATION_GARDEN:
	case OCCUPATION_FORGE:
	{
		rightX = facing * (10.0f + stroke * 4.0f);
		rightY = -13.0f - stroke * 13.0f;
		leftX = facing * 5.0f;
		leftY = -13.0f - stroke * 5.0f;
		const float tipX = rightX + facing * (13.0f - stroke * 5.0f);
		const float tipY = rightY + 9.0f - stroke * 17.0f;
		painter.Line(leftX, leftY, tipX, tipY, 2.0f, wood);
		painter.Quad(tipX - 4.0f, tipY - 2.0f, 8.0f, 4.0f, steel);

		if (motion.occupation == OCCUPATION_FORGE)
		{
			painter.Quad(facing * 22.0f - 6.0f, -7.0f, 12.0f, 7.0f, wood, -0.01f);
			painter.Quad(facing * 22.0f - 10.0f, -10.0f, 20.0f, 4.0f, steel);
		}

		if (stroke < 0.16f && motion.workBlend > 0.8f)
		{
			const Color debris = motion.occupation == OCCUPATION_FORGE
				? RGBA(1.0f, 0.68f, 0.22f, 0.8f) : Material(RGBA(0.39f, 0.31f, 0.17f), shade);
			painter.Triangle(tipX + 2.0f, tipY, tipX + 3.0f, tipY - 4.0f, tipX + 5.0f, tipY - 1.0f, debris);
			painter.Triangle(tipX - 4.0f, tipY - 1.0f, tipX - 5.0f, tipY - 5.0f, tipX - 2.0f, tipY - 3.0f, debris);
		}
		break;
	}
	case OCCUPATION_FISH:
	{
		const float cycle = fmodf(motion.time, ResidentActivity::FISH_CYCLE);
		const float cast = PlayerMotion::Ease(cycle / 1.2f);
		const float tug = sinf(PlayerMotion::Ease((cycle - 6.0f) / 3.0f) * PlayerMotion::PI);
		rightX = 8.0f;
		rightY = -18.0f - tug * 4.0f;
		leftX = 3.0f;
		leftY = -16.0f - tug * 2.0f;
		const float waterX = (targetX - painter.x) / scale;
		const float waterY = (targetY - painter.y) / scale;
		const float tipX = waterX * 0.52f;
		const float tipY = waterY * 0.45f - 35.0f - tug * 13.0f - (1.0f - cast) * 10.0f;
		const float bobberX = rightX + (waterX - rightX) * cast;
		const float bobberY = rightY + (waterY - rightY) * cast - sinf(cast * PlayerMotion::PI) * 22.0f;
		painter.Line(leftX, leftY, rightX, rightY, 2.5f, wood);
		painter.Line(rightX, rightY, tipX, tipY, 1.8f, wood);
		painter.Line(tipX, tipY, bobberX, bobberY, 0.65f, rope);
		painter.Ellipse(bobberX, bobberY + sinf(motion.time * 2.0f), 1.8f, 1.0f, RGBA(0.85f, 0.35f, 0.22f));

		if (cycle > 6.0f && cycle < 8.0f)
		{
			painter.Line(waterX - 5.0f, waterY + 2.0f, waterX - 2.0f, waterY + 1.0f, 0.7f, rope);
			painter.Line(waterX + 2.0f, waterY + 1.0f, waterX + 6.0f, waterY + 2.0f, 0.7f, rope);
		}
		break;
	}
	case OCCUPATION_HUNT:
	{
		const float angle = atan2f((motion.facingX + motion.facingY) * 16.0f, (motion.facingX - motion.facingY) * 32.0f);
		const float cosine = cosf(angle);
		const float sine = sinf(angle);
		const float pull = motion.BowPull();
		rightX = 10.0f;
		rightY = -20.0f;
		leftX = rightX - cosine * (4.0f + pull * 7.0f);
		leftY = rightY - sine * (4.0f + pull * 7.0f);
		const float topX = rightX + sine * 13.0f;
		const float topY = rightY - cosine * 13.0f;
		const float bottomX = rightX - sine * 13.0f;
		const float bottomY = rightY + cosine * 13.0f;
		painter.Line(topX, topY, rightX + cosine * 6.0f, rightY + sine * 6.0f, 2.0f, wood);
		painter.Line(bottomX, bottomY, rightX + cosine * 6.0f, rightY + sine * 6.0f, 2.0f, wood);
		painter.Line(topX, topY, leftX, leftY, 0.7f, rope);
		painter.Line(bottomX, bottomY, leftX, leftY, 0.7f, rope);
		painter.Line(-9.0f, -18.0f, -12.0f, -31.0f, 5.0f, wood, -0.02f);

		if (!motion.shotReleased)
		{
			painter.Line(leftX, leftY, rightX + cosine * 17.0f, rightY + sine * 17.0f, 1.0f, steel);
		}
		break;
	}
	case OCCUPATION_TORCH:
		leftX = -11.0f + pose.hands[0].x * 0.3f;
		leftY = -23.0f + pose.hands[0].y * 0.3f;
		painter.Line(leftX, leftY + 8.0f, leftX - 2.0f, leftY - 16.0f, 2.8f, wood);
		painter.Line(leftX - 3.0f, leftY - 12.0f, leftX - 1.0f, leftY - 17.0f, 4.0f, rope);
		renderer.SetAnim(ANIM_FLAME, motion.time, 0.7f * scale);
		renderer.PushSoftEllipse(painter.x + (leftX - 2.0f) * scale, painter.y + (leftY - 18.0f) * scale,
			23.0f * scale, 20.0f * scale, RGBA(1.0f, 0.60f, 0.20f, 0.15f * (1.0f - shade.fog)), depth - 0.03f);
		painter.Triangle(leftX - 6.0f, leftY - 15.0f, leftX + 2.0f, leftY - 15.0f,
			leftX - 1.0f, leftY - 29.0f, RGBA(1.0f, 0.42f, 0.12f));
		painter.Triangle(leftX - 4.0f, leftY - 16.0f, leftX, leftY - 16.0f,
			leftX - 2.0f, leftY - 24.0f, RGBA(1.0f, 0.88f, 0.39f));
		renderer.ClearAnim();
		break;
	case OCCUPATION_SENTRY:
	{
		const float thrust = sinf(motion.strike / 0.5f * PlayerMotion::PI) * 12.0f;
		rightX = 11.0f + thrust * facing;
		rightY = -16.0f;
		painter.Line(rightX, 2.0f, rightX + thrust * facing, -43.0f, 2.3f, wood);
		painter.Triangle(rightX + thrust * facing - 3.0f, -41.0f, rightX + thrust * facing + 3.0f, -41.0f,
			rightX + thrust * facing, -51.0f, steel);
		painter.Ellipse(-9.0f, -13.0f, 5.5f, 9.0f, wood);
		painter.Line(-9.0f, -20.0f, -9.0f, -6.0f, 1.3f, steel);
		break;
	}
	case OCCUPATION_PRAY:
		leftX = -1.5f;
		rightX = 1.5f;
		leftY = rightY = -19.0f + look * 0.8f;
		break;
	case OCCUPATION_READ:
		leftX = -7.0f;
		rightX = 9.0f;
		leftY = rightY = -17.0f;
		painter.Quad(-8.0f, -20.0f, 17.0f, 9.0f, Material(RGBA(0.77f, 0.70f, 0.49f), shade));
		painter.Line(-7.0f, -21.0f, -7.0f, -11.0f, 2.0f, wood);
		painter.Line(9.0f, -21.0f, 9.0f, -11.0f, 2.0f, wood);
		painter.Line(-4.0f, -17.0f, 6.0f, -17.0f, 0.65f, wood);
		painter.Line(-4.0f, -14.0f, 3.0f, -14.0f, 0.65f, wood);
		break;
	case OCCUPATION_CARRY:
		leftX = -9.0f;
		rightX = 9.0f;
		leftY = rightY = -15.0f;
		painter.Quad(-9.0f, -21.0f, 18.0f, 13.0f, wood);
		painter.Line(-8.0f, -20.0f, 8.0f, -9.0f, 1.4f, rope);
		painter.Line(8.0f, -20.0f, -8.0f, -9.0f, 1.4f, rope);
		break;
	default: break;
	}

	Arm(painter, -1.0f, leftX, leftY, pose.tilt, shade);
	Arm(painter, 1.0f, rightX, rightY, pose.tilt, shade);

	if (armed && motion.occupation != OCCUPATION_HUNT && motion.occupation != OCCUPATION_SENTRY)
	{
		painter.Line(rightX, rightY, rightX + 16.0f, rightY + 11.0f, 2.2f, steel, 0.024f);
	}
}

void DrawResidentGarden(Renderer& renderer, float x, float y, float depth, const ShadeParams& shade)
{
	const Color earth = Material(RGBA(0.29f, 0.23f, 0.14f), shade);
	const Color green = Material(RGBA(0.36f, 0.51f, 0.22f), shade);
	const float patch[] = { x - 24.0f, y, x, y - 12.0f, x + 24.0f, y, x, y + 12.0f };
	renderer.PushPolygon(patch, 4, earth, -100000.0f + depth * 0.01f + 0.004f);

	for (int row = 0; row < 2; ++row)
	{
		for (int column = 0; column < 3; ++column)
		{
			const float px = x + (column - 1) * 11.0f + row * 4.0f - 2.0f;
			const float py = y + row * 5.0f - 2.0f;
			renderer.PushLine(px, py, px, py - 6.0f, 1.3f, green, depth - 0.015f);
			renderer.PushEllipse(px - 2.0f, py - 4.0f, 3.0f, 1.5f, green, depth - 0.014f, 8);
			renderer.PushEllipse(px + 2.0f, py - 6.0f, 3.0f, 1.5f, green, depth - 0.013f, 8);
		}
	}
}
