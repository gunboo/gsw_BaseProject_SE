#pragma once

// Held-key slots shared by every scene. The order is the physical key layout,
// not WASD spelling, so a movement table reads up-left-down-right.
enum MoveKey
{
	MOVE_UP = 0,		// W
	MOVE_LEFT,			// A
	MOVE_DOWN,			// S
	MOVE_RIGHT,			// D
	MOVE_COUNT
};
