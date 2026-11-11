#pragma once

#include <GLFW/glfw3.h>

class WindowInput
{
public:
	static int GameplayKey(int key)
	{
		if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z)
		{
			return 'a' + key - GLFW_KEY_A;
		}

		if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9)
		{
			return '0' + key - GLFW_KEY_0;
		}

		switch (key)
		{
		case GLFW_KEY_SPACE: return ' ';
		case GLFW_KEY_ESCAPE: return 27;
		case GLFW_KEY_TAB: return '\t';
		case GLFW_KEY_ENTER: return '\r';
		default: return -1;
		}
	}

	template <typename Dispatch>
	void Key(int key, int action, bool shift, Dispatch dispatch)
	{
		if (action == GLFW_REPEAT)
		{
			return;
		}

		const int mapped = GameplayKey(key);

		if (mapped >= 0)
		{
			m_Held[mapped] = action == GLFW_PRESS;
			dispatch(static_cast<unsigned char>(mapped), action == GLFW_PRESS, shift);
		}
		else if (key == GLFW_KEY_LEFT_SHIFT || key == GLFW_KEY_RIGHT_SHIFT)
		{
			dispatch(0, action == GLFW_PRESS, shift);
		}
	}

	template <typename Dispatch>
	void ReleaseAll(Dispatch dispatch)
	{
		for (int key = 0; key < 256; ++key)
		{
			if (m_Held[key])
			{
				dispatch(static_cast<unsigned char>(key), false, false);
				m_Held[key] = false;
			}
		}

		dispatch(0, false, false);
	}

private:
	bool m_Held[256] = {};
};
