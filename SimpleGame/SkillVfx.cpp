#include "stdafx.h"
#include "SkillVfx.h"
#include "Model.h"
#include "Renderer.h"

#include <algorithm>
#include <cmath>

namespace
{
	const float PI = 3.14159265f;
	const size_t MAX_EFFECTS = 96;
	const size_t MAX_FRAGMENTS = 256;
	const float HIT_HEIGHT = 0.8f;
	const float GRAVITY = 8.0f;
	const Color WHITE = { 1.0f, 0.98f, 0.89f, 1.0f };
	const Color GOLD = { 1.0f, 0.58f, 0.15f, 1.0f };
	const Color CYAN = { 0.22f, 0.85f, 1.0f, 1.0f };
	const Color VIOLET = { 0.75f, 0.39f, 1.0f, 1.0f };
	const Color JADE = { 0.35f, 1.0f, 0.69f, 1.0f };

	Color Opacity(Color color, float alpha)
	{
		color.a = std::max(0.0f, std::min(1.0f, alpha));
		return color;
	}

	ActorPosition Lerp(const ActorPosition& from, const ActorPosition& to, float t)
	{
		return { from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * t,
			from.z + (to.z - from.z) * t };
	}

	ActorPosition Orbit(const ActorPosition& center, float radius, float angle, float height)
	{
		return { center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius, center.z + height };
	}

	void Project(Renderer& renderer, const ActorPosition& point, float& x, float& y)
	{
		renderer.WorldToScreen(point.x, point.y, point.z, &x, &y);
	}

	// Each segment deforms the renderer's resident quad; no new meshes are baked.
	void Arc(Renderer& renderer, const ActorPosition& center, float radius, float angle,
		float sweep, float width, Color color, float alpha, float height)
	{
		const int segments = 24;

		for (int i = 0; i < segments; ++i)
		{
			const float t = static_cast<float>(i) / segments;
			const float next = static_cast<float>(i + 1) / segments;
			const float a = angle + sweep * t;
			const float b = angle + sweep * next;
			const float taper = 0.15f + 0.85f * std::sin(PI * (t + next) * 0.5f);
			const ActorPosition outer = Orbit(center, radius, a, height);
			const ActorPosition outerNext = Orbit(center, radius, b, height);
			float points[8];
			Project(renderer, outer, points[0], points[1]);
			Project(renderer, outerNext, points[2], points[3]);
			Project(renderer, Orbit(center, radius - width * taper, b, height), points[4], points[5]);
			Project(renderer, Orbit(center, radius - width * taper, a, height), points[6], points[7]);
			const float depth = height < 0.1f ? -89000.0f + (center.x + center.y) * 0.01f
				: (outer.x + outer.y + outerNext.x + outerNext.y) * 0.5f + 0.3f;
			const Color colors[] = { Opacity(color, alpha * taper), Opacity(color, alpha * taper),
				Opacity(color, 0.0f), Opacity(color, 0.0f) };
			renderer.PushPolygonShaded(points, colors, 4, depth);
			renderer.PushLine(points[0], points[1], points[2], points[3], 1.4f,
				Opacity(WHITE, alpha * taper * 0.8f), depth + 0.001f);
		}
	}

	void Ribbon(Renderer& renderer, float x, float y, float tx, float ty, Color color,
		float width, float alpha, float depth)
	{
		renderer.PushLine(x, y, tx, ty, width * 3.5f, Opacity(color, alpha * 0.09f), depth);
		renderer.PushLine(x, y, tx, ty, width * 1.7f, Opacity(color, alpha * 0.27f), depth + 0.001f);
		renderer.PushLine(x, y, tx, ty, width, Opacity(color, alpha), depth + 0.002f);
		renderer.PushLine(x, y, tx, ty, std::max(1.0f, width * 0.32f), Opacity(WHITE, alpha), depth + 0.003f);
	}

	void ArrowHead(Renderer& renderer, float x, float y, float dx, float dy,
		float size, Color color, float alpha, float depth)
	{
		const float length = std::sqrt(dx * dx + dy * dy);

		if (length < 0.001f)
		{
			return;
		}

		dx /= length;
		dy /= length;
		const float points[] = { x + dx * size, y + dy * size,
			x - dx * size - dy * size * 0.55f, y - dy * size + dx * size * 0.55f,
			x - dx * size + dy * size * 0.55f, y - dy * size - dx * size * 0.55f };
		renderer.PushPolygon(points, 3, Opacity(color, alpha), depth);
	}

	void Star(Renderer& renderer, float x, float y, float radius, float angle,
		Color color, float alpha, float depth)
	{
		for (int i = 0; i < 4; ++i)
		{
			const float a = angle + i * PI * 0.5f;
			const float size = radius * (i % 2 ? 0.65f : 1.0f);
			const float dx = std::cos(a), dy = std::sin(a);
			const float points[] = { x + dx * size, y + dy * size,
				x - dy * 2.5f, y + dx * 2.5f, x - dx * 3.0f, y - dy * 3.0f,
				x + dy * 2.5f, y - dx * 2.5f };
			renderer.PushPolygon(points, 4, Opacity(color, alpha), depth);
		}
	}
}

void SkillVfx::Initialize(const ModelLibrary* models)
{
	m_Models = models;
	m_Effects.reserve(MAX_EFFECTS);
	m_Fragments.reserve(MAX_FRAGMENTS);
	Clear();
}

void SkillVfx::Clear()
{
	m_Effects.clear();
	m_Fragments.clear();
	m_Time = 0.0f;
}

void SkillVfx::Add(Kind kind, const ActorPosition& from, const ActorPosition& to, float radius,
	float angle, float duration, bool bow, bool strong, float delay)
{
	if (m_Effects.size() < MAX_EFFECTS)
	{
		m_Effects.push_back({ kind, from, to, radius, angle, duration + delay, duration, bow, strong });
	}
}

void SkillVfx::Fragments(const ActorPosition& origin, int count, float speed, bool bow, bool violet)
{
	for (int i = 0; i < count && m_Fragments.size() < MAX_FRAGMENTS; ++i)
	{
		const float angle = m_VisualRng.Range(0.0f, PI * 2.0f);
		const float velocity = m_VisualRng.Range(speed * 0.35f, speed);
		const float duration = m_VisualRng.Range(0.28f, 0.6f);
		const ActorPosition start = { origin.x, origin.y, origin.z + HIT_HEIGHT };
		const ActorPosition direction = { std::cos(angle) * velocity, std::sin(angle) * velocity,
			m_VisualRng.Range(1.0f, 3.8f) };
		m_Fragments.push_back({ start, direction, duration, duration, angle,
			m_VisualRng.Range(1.8f, 4.0f), bow, violet });
	}
}

void SkillVfx::Slash(const ActorPosition& center, float x, float y, float radius)
{
	const float angle = std::atan2(y, x);
	Add(SLASH, center, center, radius, angle, 0.38f, false);
	Add(SLASH, center, center, radius * 0.78f, angle + PI, 0.32f, false, true, 0.07f);
}

void SkillVfx::Dash(const ActorPosition& from, const ActorPosition& to, bool shadow, bool bow)
{
	const float dx = to.x - from.x, dy = to.y - from.y;
	const float distance = std::sqrt(dx * dx + dy * dy);
	const int count = std::min(7, static_cast<int>(std::ceil(distance / 0.55f)));

	for (int i = 0; i < count; ++i)
	{
		const float t = static_cast<float>(i) / count;
		const ActorPosition position = Lerp(from, to, t);
		Add(GHOST, position, to, 0.0f, std::atan2(dy, dx), 0.3f + t * 0.25f, bow, shadow);
	}

	Fragments(to, shadow ? 14 : 7, 2.0f, bow, shadow);
}

void SkillVfx::Ray(const ActorPosition& from, const ActorPosition& to, bool bow, bool pierce)
{
	Add(bow ? ARROW : BLADE_WAVE, from, to, pierce ? 1.0f : 0.5f,
		std::atan2(to.y - from.y, to.x - from.x), pierce ? 0.48f : 0.32f, bow, pierce);
}

void SkillVfx::Area(const ActorPosition& center, float radius, bool bow, bool storm)
{
	Add(storm ? STORM : SPIN, center, center, radius, m_VisualRng.Range(0.0f, PI),
		storm ? 0.95f : 0.7f, bow, storm);
	Fragments(center, storm ? 18 : 12, radius, bow, !bow);

	if (bow)
	{
		const int count = storm ? 20 : 12;

		for (int i = 0; i < count; ++i)
		{
			// A golden-angle spiral covers the interior as well as the boundary.
			const float angle = i * 2.399963f;
			const float distance = radius * 0.9f * std::sqrt((i + 0.5f) / count);
			const ActorPosition to = Orbit(center, distance, angle, 0.08f);
			const ActorPosition from = { to.x - 0.65f, to.y - 0.65f, to.z + (storm ? 5.0f : 3.5f) };
			Add(RAIN, from, to, 0.0f, angle, 0.4f, true, storm, (i % 5) * 0.045f);
		}
	}
}

void SkillVfx::Ward(const ActorPosition& center, bool bow)
{
	Add(WARD, center, center, 1.4f, 0.0f, 0.65f, bow);
	Fragments(center, 12, 1.0f, bow);
}

void SkillVfx::Impact(const ActorPosition& position, bool critical, bool bow)
{
	Add(IMPACT, position, position, critical ? 37.0f : 24.0f,
		m_VisualRng.Range(-0.6f, 0.6f), 0.3f, bow, critical);
	Fragments(position, critical ? 18 : 10, critical ? 5.0f : 3.2f, bow);
}

void SkillVfx::Update(float seconds)
{
	m_Time = std::fmod(m_Time + seconds, PI * 120.0f);

	for (Effect& effect : m_Effects)
	{
		effect.remaining -= seconds;
	}

	m_Effects.erase(std::remove_if(m_Effects.begin(), m_Effects.end(),
		[](const Effect& effect) { return effect.remaining <= 0.0f; }), m_Effects.end());

	for (Fragment& fragment : m_Fragments)
	{
		fragment.remaining -= seconds;
	}

	m_Fragments.erase(std::remove_if(m_Fragments.begin(), m_Fragments.end(),
		[](const Fragment& fragment) { return fragment.remaining <= 0.0f; }), m_Fragments.end());
}

void SkillVfx::DrawEffect(Renderer& renderer, const Effect& effect) const
{
	const float age = effect.duration - effect.remaining;

	if (age < 0.0f)
	{
		return;
	}

	const float t = age / effect.duration;
	const float fade = (1.0f - t) * (1.0f - t);
	const Color color = effect.bow ? CYAN : GOLD;
	float x, y, tx, ty;
	Project(renderer, effect.from, x, y);
	Project(renderer, effect.to, tx, ty);
	const float margin = effect.radius * 48.0f + 90.0f;
	const float halfWidth = renderer.GetWidth() * 0.5f;
	const float halfHeight = renderer.GetHeight() * 0.5f;

	if (std::max(x, tx) + margin < -halfWidth || std::min(x, tx) - margin > halfWidth
		|| std::max(y, ty) + margin < -halfHeight || std::min(y, ty) - margin > halfHeight)
	{
		return;
	}

	const float depth = std::max(effect.from.x + effect.from.y, effect.to.x + effect.to.y) + 0.7f;

	switch (effect.kind)
	{
	case SLASH:
		Arc(renderer, effect.from, effect.radius * (0.8f + t * 0.2f),
			effect.angle - PI * 0.65f + t * 2.0f, PI * 1.3f, 0.55f,
			effect.strong ? WHITE : GOLD, fade, HIT_HEIGHT);
		break;
	case BLADE_WAVE:
	{
		// The full streak marks the immediate hit; crescents carry its fading energy forward.
		Ribbon(renderer, x, y - 21.0f, tx, ty - 21.0f, GOLD, 5.0f, fade, depth);

		for (int i = 0; i < 3; ++i)
		{
			const float travel = std::min(1.0f, t * 1.8f + i * 0.2f);
			Arc(renderer, Lerp(effect.from, effect.to, travel), 0.85f + i * 0.1f,
				effect.angle - PI * 0.48f, PI * 0.96f, 0.3f, GOLD, fade * (1.0f - i * 0.2f), HIT_HEIGHT);
		}

		break;
	}
	case ARROW:
	{
		y -= HIT_HEIGHT * Renderer::HeightScale();
		ty -= HIT_HEIGHT * Renderer::HeightScale();
		Ribbon(renderer, x, y, tx, ty, effect.strong ? CYAN : GOLD,
			effect.strong ? 4.0f : 2.5f, fade, depth);
		ArrowHead(renderer, tx, ty, tx - x, ty - y, effect.strong ? 10.0f : 7.0f, WHITE, fade, depth + 0.01f);

		for (int i = 1; i <= 3; ++i)
		{
			const float tail = std::max(0.0f, 1.0f - i * 0.18f - t * 0.25f);
			ArrowHead(renderer, x + (tx - x) * tail, y + (ty - y) * tail, tx - x, ty - y,
				4.0f, CYAN, fade * 0.35f, depth);
		}

		if (effect.strong)
		{
			Star(renderer, tx, ty, 18.0f + t * 10.0f, PI * 0.25f, CYAN, fade, depth + 0.02f);
		}

		break;
	}
	case RAIN:
	{
		const float travel = std::min(1.0f, t * 1.65f);
		const float headX = x + (tx - x) * travel, headY = y + (ty - y) * travel;
		const float tail = std::max(0.0f, travel - 0.38f);
		Ribbon(renderer, x + (tx - x) * tail, y + (ty - y) * tail, headX, headY,
			effect.strong ? CYAN : GOLD, 2.0f, 1.0f - t, depth);
		ArrowHead(renderer, headX, headY, tx - x, ty - y, 5.0f, WHITE, 1.0f - t, depth + 0.01f);

		if (travel >= 1.0f)
		{
			Star(renderer, tx, ty, 12.0f, effect.angle, color, fade, depth + 0.02f);
		}

		break;
	}
	case SPIN:
	case STORM:
	{
		const bool storm = effect.kind == STORM;
		const Color areaColor = effect.bow ? (storm ? CYAN : GOLD) : VIOLET;
		const float radius = effect.radius * (0.6f + 0.4f * std::min(1.0f, t * 3.0f));
		Arc(renderer, effect.from, radius, effect.angle, PI * 2.0f, 0.12f, areaColor, fade * 0.8f, 0.03f);

		if (!effect.bow)
		{
			const int layers = storm ? 4 : 2;

			for (int i = 0; i < layers; ++i)
			{
				const float rotation = effect.angle + t * (storm ? 12.0f : 8.0f) + i * PI;
				Arc(renderer, effect.from, radius * (1.0f - i * 0.13f), rotation,
					PI * 1.3f, 0.45f, i % 2 ? CYAN : VIOLET, fade, 0.6f + i * 0.48f);
			}
		}
		else
		{
			// Orbiting points make the rain's target area legible between volleys.
			for (int i = 0; i < 6; ++i)
			{
				const ActorPosition point = Orbit(effect.from, radius, effect.angle + t + i * PI / 3.0f, 0.15f);
				float px, py;
				Project(renderer, point, px, py);
				Star(renderer, px, py, storm ? 12.0f : 8.0f, t * 2.0f, areaColor, fade, point.x + point.y + 0.2f);
			}
		}

		break;
	}
	case GHOST:
	{
		const Color ghostColor = effect.strong ? VIOLET : CYAN;
		const Model* model = m_Models != NULL ? m_Models->Find("player_fox") : NULL;

		if (model != NULL)
		{
			ShadeParams shade = DefaultShade();
			shade.fog = 1.0f;
			shade.fogColor = ghostColor;
			shade.tint.a = fade * 0.4f;
			DrawModel(&renderer, *model, x, y, 1.0f, effect.from.x + effect.from.y,
				shade, 0.0f, MODEL_FLAG_GROUND);
		}

		const float dx = std::cos(effect.angle), dy = std::sin(effect.angle);
		const float screenX = (dx - dy) * 18.0f, screenY = (dx + dy) * 9.0f;
		Ribbon(renderer, x - screenX, y - 22.0f - screenY, x, y - 22.0f, ghostColor, 2.0f, fade * 0.5f, depth);

		if (!effect.bow)
		{
			Ribbon(renderer, x + 10.0f, y - 20.0f, x + 10.0f + screenX, y - 20.0f + screenY,
				ghostColor, 2.0f, fade * 0.7f, depth);
		}

		break;
	}
	case WARD:
		Arc(renderer, effect.from, effect.radius * (0.5f + t), t * 2.0f,
			PI * 2.0f, 0.3f, JADE, fade, 0.04f);
		renderer.PushSoftEllipse(x, y - 22.0f, 34.0f + t * 20.0f, 45.0f + t * 15.0f,
			Opacity(JADE, fade * 0.45f), depth);
		break;
	case IMPACT:
	{
		y -= HIT_HEIGHT * Renderer::HeightScale();
		const float flash = std::max(0.0f, 1.0f - age / 0.1f);
		const float radius = effect.radius * (0.7f + t);
		renderer.PushSoftEllipse(x, y, radius * 1.5f, radius, Opacity(color, fade * 0.65f), depth);
		Star(renderer, x, y, radius, effect.angle, color, fade, depth + 0.01f);
		Star(renderer, x, y, radius * 0.75f, effect.angle + PI * 0.25f, WHITE, fade, depth + 0.02f);
		renderer.PushEllipse(x, y, 10.0f * flash, 10.0f * flash, Opacity(WHITE, flash), depth + 0.03f);
		break;
	}
	}
}

void SkillVfx::Draw(Renderer& renderer, const ActorPosition& player, float ward, bool invulnerable) const
{
	renderer.ClearAnim();

	for (const Effect& effect : m_Effects)
	{
		DrawEffect(renderer, effect);
	}

	for (const Fragment& fragment : m_Fragments)
	{
		const float age = fragment.duration - fragment.remaining;
		const float fade = fragment.remaining / fragment.duration;
		const ActorPosition position = { fragment.origin.x + fragment.velocity.x * age,
			fragment.origin.y + fragment.velocity.y * age,
			std::max(fragment.origin.z - HIT_HEIGHT,
				fragment.origin.z + fragment.velocity.z * age - GRAVITY * age * age * 0.5f) };
		float x, y;
		Project(renderer, position, x, y);
		const float depth = position.x + position.y + 0.8f;
		const Color color = fragment.violet ? VIOLET : (fragment.bow ? CYAN : GOLD);
		const float dx = std::cos(fragment.angle + age * 9.0f) * fragment.size;
		const float dy = std::sin(fragment.angle + age * 9.0f) * fragment.size;
		const float points[] = { x + dx * 1.5f, y + dy * 1.5f, x - dy, y + dx, x + dy, y - dx };
		renderer.PushPolygon(points, 3, Opacity(color, fade), depth);
		renderer.PushLine(x, y, x - (fragment.velocity.x - fragment.velocity.y) * 1.8f,
			y - (fragment.velocity.x + fragment.velocity.y) * 0.9f + (fragment.velocity.z - GRAVITY * age),
			1.2f, Opacity(WHITE, fade * 0.7f), depth + 0.01f);
	}

	if (ward > 0.0f || invulnerable)
	{
		float x, y;
		Project(renderer, player, x, y);
		const Color color = invulnerable ? VIOLET : JADE;
		const float opacity = invulnerable ? 0.8f : std::min(1.0f, ward * 2.0f);
		const float depth = player.x + player.y + 0.6f;
		renderer.PushSoftEllipse(x, y - 24.0f, 30.0f, 40.0f, Opacity(color, opacity * 0.2f), depth);
		Arc(renderer, player, 0.9f, m_Time * 1.6f, PI * 2.0f, 0.12f, color, opacity, 0.04f);

		for (int i = 0; i < 6; ++i)
		{
			const float a = i * PI / 3.0f + 0.12f * std::sin(m_Time * 2.0f);
			const float b = a + PI / 3.0f;
			renderer.PushLine(x + std::cos(a) * 26.0f, y - 24.0f + std::sin(a) * 35.0f,
				x + std::cos(b) * 26.0f, y - 24.0f + std::sin(b) * 35.0f,
				1.5f, Opacity(color, opacity * 0.65f), depth);
			const ActorPosition rune = Orbit(player, 1.0f, m_Time * 1.6f + i * PI / 3.0f, 0.6f);
			float rx, ry;
			Project(renderer, rune, rx, ry);
			renderer.PushDiamond(rx, ry, 3.0f, 6.0f, Opacity(WHITE, opacity * 0.8f), rune.x + rune.y + 0.3f);
		}
	}

	renderer.ClearAnim();
}
