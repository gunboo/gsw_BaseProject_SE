#include "../SimpleGame/stdafx.h"
#include "../SimpleGame/MapOverlay.h"
#include "../SimpleGame/WindowInput.h"
#include "TestAssertions.h"

#include <cmath>
#include <cstdio>
#include <limits>

namespace
{
	void Near(float actual, float expected)
	{
		assert(std::fabs(actual - expected) < 0.001f);
	}
}

int main()
{
	MapView view;
	const MapRect area = { -400.0f, -200.0f, 800.0f, 400.0f };
	view.Fit(200, 100);
	Near(view.Scale(area, 200, 100), 4.0f);
	Near(view.centerX, 100.0f);
	Near(view.centerY, 50.0f);

	// The world point underneath the mouse stays fixed during zoom.
	const float anchorX = 100.0f, anchorY = 40.0f;
	const float worldX = view.centerX + anchorX / view.Scale(area, 200, 100);
	const float worldY = view.centerY + anchorY / view.Scale(area, 200, 100);
	view.ZoomAt(3.5f, anchorX, anchorY, area, 200, 100);
	Near(view.centerX + anchorX / view.Scale(area, 200, 100), worldX);
	Near(view.centerY + anchorY / view.Scale(area, 200, 100), worldY);

	const float zoom = view.zoom;
	view.ZoomAt(4.0f, 999.0f, 0.0f, area, 200, 100);
	Near(view.zoom, zoom);
	view.ZoomAt(std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f, area, 200, 100);
	Near(view.zoom, zoom);
	view.ZoomAt(1000.0f, 0.0f, 0.0f, area, 200, 100);
	Near(view.zoom, 16.0f);
	view.Pan(100000.0f, -100000.0f, area, 200, 100);
	Near(view.centerX, 6.25f);
	Near(view.centerY, 96.875f);
	view.ZoomAt(-1000.0f, 0.0f, 0.0f, area, 200, 100);
	Near(view.zoom, 1.0f);
	Near(view.centerX, 100.0f);
	Near(view.centerY, 50.0f);

	// A tall viewport must letterbox the short world axis instead of stretching it.
	const MapRect portrait = { -120.0f, -300.0f, 240.0f, 600.0f };
	view.Pan(3000.0f, -3000.0f, portrait, 200, 100);
	Near(view.Scale(portrait, 200, 100), 1.2f);
	Near(view.centerX, 100.0f);
	Near(view.centerY, 50.0f);

	World world;
	DialogueDB dialogue;
	assert(world.Generate(7, 32, 32));
	assert(dialogue.Load("./Data/dialogue.txt"));
	assert(!dialogue.Line("ui_map_village").empty());
	assert(!dialogue.Line("ui_map_names").empty());
	MapOverlay map;
	map.Initialize(world, dialogue, "ui_map_village");
	const ActorPosition player = { 16.0f, 16.0f, 0.0f };
	assert(!map.OnKey('m', true, false, player));
	assert(!map.IsOpen());
	WindowInput input;
	const auto dispatch = [&](unsigned char key, bool down, bool)
	{
		map.OnKey(key, down, true, player);
	};
	input.Key(GLFW_KEY_M, GLFW_PRESS, false, dispatch);
	assert(map.IsOpen());
	input.Key(GLFW_KEY_M, GLFW_REPEAT, false, dispatch);
	assert(map.IsOpen());
	input.ReleaseAll(dispatch);
	assert(map.IsOpen());
	assert(map.OnKey('w', true, true, player));
	assert(map.OnKey(' ', false, true, player));
	assert(map.OnKey('k', true, true, player));
	assert(map.OnKey(27, true, false, player));
	assert(!map.IsOpen());
	assert(!map.OnKey(27, false, true, player));
	assert(map.OnKey('m', true, true, player));
	assert(map.OnKey('m', true, true, player));
	assert(!map.IsOpen());
	map.OnKey('m', true, true, player);
	map.Initialize(world, dialogue, "ui_lv_name");
	assert(!map.IsOpen());
	std::puts("PASS: map zoom anchor, limits, pan bounds, aspect ratio, modal input and scene reset");
	return 0;
}
