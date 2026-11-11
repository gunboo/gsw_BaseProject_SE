#include "../SimpleGame/stdafx.h"
#include "../SimpleGame/WindowInput.h"
#include "TestAssertions.h"

#include <cstdio>
#include <vector>

int main()
{
	assert(WindowInput::GameplayKey(GLFW_KEY_W) == 'w');
	assert(WindowInput::GameplayKey(GLFW_KEY_1) == '1');
	assert(WindowInput::GameplayKey(GLFW_KEY_SPACE) == ' ');
	assert(WindowInput::GameplayKey(GLFW_KEY_TAB) == '\t');
	assert(WindowInput::GameplayKey(GLFW_KEY_ESCAPE) == 27);
	assert(WindowInput::GameplayKey(GLFW_KEY_F9) == -1);
	struct Event
	{
		unsigned char key;
		bool down;
		bool shift;
	};
	std::vector<Event> events;
	const auto dispatch = [&events](unsigned char key, bool down, bool shift)
	{
		events.push_back({ key, down, shift });
	};
	WindowInput input;
	input.Key(GLFW_KEY_W, GLFW_PRESS, false, dispatch);
	input.Key(GLFW_KEY_SPACE, GLFW_PRESS, false, dispatch);
	input.Key(GLFW_KEY_SPACE, GLFW_REPEAT, false, dispatch);
	assert(events.size() == 2);
	input.Key(GLFW_KEY_LEFT_SHIFT, GLFW_PRESS, true, dispatch);
	assert(events.back().shift);
	input.Key(GLFW_KEY_LEFT_SHIFT, GLFW_RELEASE, false, dispatch);
	assert(!events.back().shift);
	input.ReleaseAll(dispatch);
	assert(events.size() == 7);
	assert(!events[4].down && !events[5].down && !events[6].down);
	assert(!events[4].shift && !events[5].shift && !events[6].shift);
	events.clear();
	input.ReleaseAll(dispatch);
	assert(events.size() == 1 && events[0].key == 0);
	std::puts("PASS: physical WASD/number/space/control keys; no repeat attacks; Shift transitions; focus-loss releases");

	return 0;
}
