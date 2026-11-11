#include "../SimpleGame/stdafx.h"
#include "../SimpleGame/PainterOrder.h"
#include "../SimpleGame/Lighting.h"

#include <algorithm>
#include "TestAssertions.h"
#include <cmath>
#include <cstdio>
#include <limits>
#include <numeric>
#include <random>

Color ReferenceLight(const Lighting& lighting, float x, float y, float time)
{
	Color result = RGBA(0.11f, 0.12f, 0.13f);

	for (int i = 0; i < lighting.GetLightCount(); ++i)
	{
		const LightSource& light = lighting.GetLight(i);
		const float dx = x - light.x;
		const float dy = y - light.y;
		const float distance = dx * dx + dy * dy;

		if (distance >= light.radius * light.radius)
		{
			continue;
		}

		float falloff = 1.0f - sqrtf(distance) / light.radius;
		falloff *= falloff;
		float amount = falloff * light.intensity;

		if (light.flicker)
		{
			amount *= 1.0f + 0.09f * sinf(time * 6.3f + i * 2.1f) + 0.05f * sinf(time * 11.7f + i);
		}

		result.r += light.color.r * amount;
		result.g += light.color.g * amount;
		result.b += light.color.b * amount;
	}

	result.r = fminf(fmaxf(result.r, 0.0f), 1.0f);
	result.g = fminf(fmaxf(result.g, 0.0f), 1.0f);
	result.b = fminf(fmaxf(result.b, 0.0f), 1.0f);

	return result;
}

int main()
{
	std::mt19937 random(4112026u);
	PainterOrder painter;

	for (int count : { 0, 1, 2, 255, 16000, 50000 })
	{
		std::vector<float> depths(count);
		std::vector<int> order(count);
		std::iota(order.begin(), order.end(), 0);

		for (float& depth : depths)
		{
			depth = (static_cast<int>(random() % 200000) - 100000) * 0.0001f;
		}

		if (count > 10)
		{
			depths[0] = -100000.0f;
			depths[1] = depths[2] = 0.0f;
			depths[3] = -0.0f;
			depths[4] = std::numeric_limits<float>::infinity();
			depths[5] = -std::numeric_limits<float>::infinity();
			depths[6] = depths[7] = -90000.0f;
		}

		std::shuffle(order.begin(), order.end(), random);
		std::vector<int> expected = order;
		std::stable_sort(expected.begin(), expected.end(), [&depths](int a, int b) { return depths[a] < depths[b]; });
		painter.Sort(order, [&depths](int index) { return depths[index]; });
		assert(order == expected);
	}

	Lighting lighting;
	lighting.SetAmbient(RGBA(0.11f, 0.12f, 0.13f));

	for (int frame = 0; frame < 4; ++frame)
	{
		if (frame == 0 || frame == 3)
		{
			lighting.Clear();

			for (int i = 0; i < 100; ++i)
			{
				lighting.AddLight((int)(random() % 420) - 40.0f, (int)(random() % 240) - 40.0f,
					3.6f + (i % 4), 0.7f, RGBA(0.9f, 0.6f, 0.3f), i % 2 != 0);
			}
		}

		const float time = frame * 0.713f;
		lighting.SetTime(time);

		for (int sample = 0; sample < 5000; ++sample)
		{
			if (sample == 2500)
			{
				lighting.MoveLight(0, -8.0f, 8.0f);
				lighting.MoveLight(1, 8.0f, -8.0f);
			}

			const float x = (int)(random() % 840) * 0.5f - 40.0f;
			const float y = (int)(random() % 480) * 0.5f - 40.0f;
			const Color expected = ReferenceLight(lighting, x, y, time);
			const Color actual = lighting.SampleLight(x, y);
			assert(fabsf(actual.r - expected.r) < 0.000001f);
			assert(fabsf(actual.g - expected.g) < 0.000001f);
			assert(fabsf(actual.b - expected.b) < 0.000001f);
		}
	}

	std::puts("PASS: stable painter order including ties/signed zero; indexed lighting matches full scan; moving lights/time/reset");

	return 0;
}
