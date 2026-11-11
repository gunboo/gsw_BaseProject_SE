#pragma once

#include <algorithm>
#include <cmath>

// North-up map coordinates, independent of the quarter-view world camera.
struct MapRect
{
	float x, y, width, height;

	bool Contains(float px, float py) const
	{
		return px >= x && py >= y && px <= x + width && py <= y + height;
	}
};

struct MapView
{
	static constexpr float MAX_ZOOM = 16.0f;
	float centerX = 0.0f;
	float centerY = 0.0f;
	float zoom = 1.0f;

	float Scale(const MapRect& area, int width, int height) const
	{
		return std::min(area.width / std::max(1, width), area.height / std::max(1, height)) * zoom;
	}

	void Fit(int width, int height)
	{
		centerX = width * 0.5f;
		centerY = height * 0.5f;
		zoom = 1.0f;
	}

	void Clamp(int width, int height, const MapRect& area)
	{
		const float scale = Scale(area, width, height);
		const float halfX = std::min(width * 0.5f, area.width / (2.0f * scale));
		const float halfY = std::min(height * 0.5f, area.height / (2.0f * scale));
		centerX = std::max(halfX, std::min(width - halfX, centerX));
		centerY = std::max(halfY, std::min(height - halfY, centerY));
	}

	void ZoomAt(float steps, float x, float y, const MapRect& area, int width, int height)
	{
		if (!std::isfinite(steps) || !area.Contains(x, y))
		{
			return;
		}

		const float oldScale = Scale(area, width, height);
		const float offsetX = x - area.x - area.width * 0.5f;
		const float offsetY = y - area.y - area.height * 0.5f;
		zoom = std::max(1.0f, std::min(float(MAX_ZOOM), zoom * std::pow(1.25f, std::max(-32.0f, std::min(32.0f, steps)))));
		const float scale = Scale(area, width, height);
		centerX += offsetX / oldScale - offsetX / scale;
		centerY += offsetY / oldScale - offsetY / scale;
		Clamp(width, height, area);
	}

	void Pan(float dx, float dy, const MapRect& area, int width, int height)
	{
		const float scale = Scale(area, width, height);
		centerX -= dx / scale;
		centerY -= dy / scale;
		Clamp(width, height, area);
	}
};
