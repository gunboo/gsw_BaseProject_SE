/*
Copyright 2022 Lee Taek Hee (Tech University of Korea)

This program is free software: you can redistribute it and/or modify
it under the terms of the What The Hell License. Do it plz.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY.
*/

#include "stdafx.h"
#include <algorithm>
#include <ctime>
#include <cstdlib>
#include <cerrno>
#include <iostream>
#include <limits>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "Platform.h"
#include "WindowInput.h"
#include <fstream>
#include <vector>


#include "Dialogue.h"
#include "AnalysisLog.h"
#include "Game.h"
#include "Level.h"
#include "Model.h"
#include "Renderer.h"
#include "Monitoring.h"

namespace
{
	const int WINDOW_WIDTH = 1024;
	const int WINDOW_HEIGHT = 640;

	const char* const DIALOGUE_PATH = "./Data/dialogue.txt";
	const char* const MODEL_CACHE_PATH = "./Data/models.cache";

	Renderer* g_Renderer = NULL;
	ModelLibrary* g_Models = NULL;
	DialogueDB* g_Dialogue = NULL;

	Game* g_Tutorial = NULL;
	Level* g_Level = NULL;
	MonitoringPublisher g_Monitor;

	GLFWwindow* g_Window = NULL;
	double g_PreviousTime = 0.0;
	WindowInput g_Input;
	std::string g_CapturePath;
	double g_TestStart = 0.0;
	double g_PreviousRenderMs = -1.0;
	double g_PreviousUpdateMs = 0.0;
	FrameSample g_FrameSample;

	// The tutorial hands over to level 1; from then on the level owns the session.
	bool InLevel()
	{
		return g_Level != NULL;
	}

	void EnterLevel()
	{
		unsigned int seed = (unsigned int)time(NULL);
		const std::string seedSetting = Platform::Environment("GAME_LEVEL_SEED");
		const char* seedText = seedSetting.empty() ? NULL : seedSetting.c_str();
		if (seedText != NULL)
		{
			char* end = NULL;
			errno = 0;
			const unsigned long parsed = strtoul(seedText, &end, 10);

			if (*seedText >= '0' && *seedText <= '9' && *end == '\0' && errno == 0
				&& parsed <= (std::numeric_limits<unsigned int>::max)())
			{
				seed = static_cast<unsigned int>(parsed);
			}
			else
			{
				AnalysisLog::Get().Event("level.seed_invalid", "using_clock_seed");
			}

		}

		AnalysisLog::Get().Event("level.begin", "seed=" + std::to_string(seed));
		const double begin = AnalysisLog::NowMs();

		g_Level = new Level();

		if (!g_Level->Initialize(g_Renderer, g_Models, g_Dialogue, seed))
		{
			std::cout << "Level 1 could not be generated; staying in the tutorial.\n";

			delete g_Level;
			g_Level = NULL;
			AnalysisLog::Get().Event("level.failed", "seed=" + std::to_string(seed));
		}
		else
		{
			AnalysisLog::Get().Event("level.ready", "seed=" + std::to_string(seed)
				+ ";initialization_ms=" + std::to_string(AnalysisLog::NowMs() - begin));
		}
	}
}

void RenderScene(void)
{
	const double begin = AnalysisLog::NowMs();
	g_FrameSample.intervalMs = g_PreviousRenderMs < 0.0 ? -1.0 : begin - g_PreviousRenderMs;
	g_PreviousRenderMs = begin;

	if (InLevel())
	{
		g_Level->Render();
	}
	else
	{
		g_Tutorial->Render();
	}

	if (!g_CapturePath.empty() && glfwGetTime() - g_TestStart >= 8.0)
	{
		int width, height;
		glfwGetFramebufferSize(g_Window, &width, &height);
		std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 3);
		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
		glPixelStorei(GL_PACK_ALIGNMENT, 4);
		std::ofstream capture(g_CapturePath, std::ios::binary);
		capture << "P6\n" << width << ' ' << height << "\n255\n";

		for (int y = height - 1; y >= 0; --y)
		{
			capture.write(reinterpret_cast<const char*>(pixels.data() + static_cast<size_t>(y) * width * 3), width * 3);
		}

		AnalysisLog::Get().Event("smoke.capture", "frame=" + std::to_string(g_Renderer->GetStats().frameId));
		g_CapturePath.clear();
	}

	g_FrameSample.renderCpuMs = AnalysisLog::NowMs() - begin;
	const double swapStart = AnalysisLog::NowMs();
	glfwSwapBuffers(g_Window);
	g_FrameSample.swapCpuMs = AnalysisLog::NowMs() - swapStart;

	if (InLevel())
	{
		AnalysisLog::Get().RecordFrame(g_FrameSample, *g_Renderer, g_Level->GetSceneGraph(),
			"level1", static_cast<int>(g_Level->GetState()));
	}
	else
	{
		AnalysisLog::Get().RecordFrame(g_FrameSample, *g_Renderer, g_Tutorial->GetSceneGraph(),
			"tutorial", static_cast<int>(g_Tutorial->GetState()));
	}

	g_FrameSample = {};
}

// Update and render are separate: the loop advances the simulation by the time
// that actually elapsed, so movement speed does not depend on the frame rate.
void Idle(void)
{
	const double begin = AnalysisLog::NowMs();
	const double wallMs = begin - g_PreviousUpdateMs;
	g_PreviousUpdateMs = begin;
	++g_FrameSample.updates;
	g_FrameSample.updateWallMs += wallMs;

	if (wallMs > g_FrameSample.maxUpdateWallMs)
	{
		g_FrameSample.maxUpdateWallMs = wallMs;
	}

	const double now = glfwGetTime();
	float deltaSeconds = static_cast<float>(now - g_PreviousTime);
	g_PreviousTime = now;

	if (deltaSeconds < 0.0f)
	{
		deltaSeconds = 0.0f;
	}

	if (deltaSeconds > 0.05f)
	{
		++g_FrameSample.clampedUpdates;
		// A long stall (dragging the window, a breakpoint) must not teleport
		// the player through a wall.
		deltaSeconds = 0.05f;
	}

	g_FrameSample.simulationMs += deltaSeconds * 1000.0;

	if (InLevel())
	{
		g_Level->Update(deltaSeconds);

		if (g_Level->WantsExit())
		{
			glfwSetWindowShouldClose(g_Window, GLFW_TRUE);

			return;
		}
	}
	else
	{
		g_Tutorial->Update(deltaSeconds);

		// Consume once so a failed transition waits for another explicit request.
		if (g_Tutorial->ConsumeNextLevelRequest())
		{
			EnterLevel();
		}

		if (g_Tutorial->WantsExit())
		{
			glfwSetWindowShouldClose(g_Window, GLFW_TRUE);

			return;
		}
	}

	if (g_Monitor.IsDue(AnalysisLog::NowMs()))
	{
		std::vector<MonitoredWorld> worlds;
		worlds.push_back(g_Tutorial->CaptureMonitoring(!InLevel()));

		if (InLevel())
		{
			worlds.push_back(g_Level->CaptureMonitoring(true));
		}

		g_Monitor.Publish(worlds);
	}

	g_FrameSample.updateCpuMs += AnalysisLog::NowMs() - begin;
}

void Reshape(int width, int height)
{
	g_Renderer->Resize(width, height);
	AnalysisLog::Get().Event("window.resize", "width=" + std::to_string(g_Renderer->GetWidth())
		+ ";height=" + std::to_string(g_Renderer->GetHeight()));
}

void DispatchKey(unsigned char key, bool down, bool shift)
{
	if (InLevel())
	{
		g_Level->OnKey(key, down, shift);
	}
	else if (g_Tutorial != NULL)
	{
		g_Tutorial->OnKey(key, down, shift);
	}
}

void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	(void)scancode;
	(void)mods;

	if (action == GLFW_REPEAT)
	{
		return;
	}

	const bool shift = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS
		|| glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;

	if (key == GLFW_KEY_F9 && action == GLFW_PRESS)
	{
		AnalysisLog::Get().Event("marker", "F9;frame=" + std::to_string(g_Renderer->GetStats().frameId));
	}

	g_Input.Key(key, action, shift, DispatchKey);
}

MapOverlay* ActiveMap()
{
	return InLevel() ? &g_Level->GetMap() : g_Tutorial != NULL ? &g_Tutorial->GetMap() : NULL;
}

// GLFW cursor positions use window pixels; the renderer can use DPI-scaled pixels.
void MapCursor(GLFWwindow* window, double x, double y, float& screenX, float& screenY)
{
	int width, height;
	glfwGetWindowSize(window, &width, &height);
	screenX = static_cast<float>(x / std::max(1, width) - 0.5) * g_Renderer->GetWidth();
	screenY = static_cast<float>(y / std::max(1, height) - 0.5) * g_Renderer->GetHeight();
}

void ScrollCallback(GLFWwindow* window, double, double steps)
{
	MapOverlay* map = ActiveMap();

	if (map == NULL || g_Renderer == NULL)
	{
		return;
	}

	double x, y;
	float screenX, screenY;
	glfwGetCursorPos(window, &x, &y);
	MapCursor(window, x, y, screenX, screenY);
	map->OnScroll(static_cast<float>(steps), screenX, screenY, *g_Renderer);
}

void CursorCallback(GLFWwindow* window, double x, double y)
{
	MapOverlay* map = ActiveMap();

	if (map != NULL && g_Renderer != NULL)
	{
		float screenX, screenY;
		MapCursor(window, x, y, screenX, screenY);
		map->OnPointer(screenX, screenY, glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS, *g_Renderer);
	}
}

void MouseButtonCallback(GLFWwindow* window, int button, int, int)
{
	if (button == GLFW_MOUSE_BUTTON_LEFT)
	{
		double x, y;
		glfwGetCursorPos(window, &x, &y);
		CursorCallback(window, x, y);
	}
}

void FocusCallback(GLFWwindow*, int focused)
{
	if (focused == GLFW_TRUE)
	{
		return;
	}

	g_Input.ReleaseAll(DispatchKey);

	if (ActiveMap() != NULL)
	{
		ActiveMap()->CancelDrag();
	}
}

void RefreshViewport(GLFWwindow* window)
{
	if (g_Renderer == NULL)
	{
		return;
	}

	int width, height, framebufferWidth, framebufferHeight;
	glfwGetWindowSize(window, &width, &height);
	glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
	float scaleX = 1.0f;
	float scaleY = 1.0f;
	if (glfwGetPlatform() == GLFW_PLATFORM_WIN32 || glfwGetPlatform() == GLFW_PLATFORM_X11)
	{
		glfwGetWindowContentScale(window, &scaleX, &scaleY);
	}
	if (scaleX <= 0.0f)
	{
		scaleX = 1.0f;
	}

	if (scaleY <= 0.0f)
	{
		scaleY = 1.0f;
	}
	const int logicalWidth = static_cast<int>(width / scaleX + 0.5f);
	const int logicalHeight = static_cast<int>(height / scaleY + 0.5f);
	g_Renderer->Resize(logicalWidth, logicalHeight, framebufferWidth, framebufferHeight);
	AnalysisLog::Get().Event("window.resize", "width=" + std::to_string(logicalWidth)
		+ ";height=" + std::to_string(logicalHeight) + ";framebuffer_width=" + std::to_string(framebufferWidth)
		+ ";framebuffer_height=" + std::to_string(framebufferHeight));
}

void ErrorCallback(int code, const char* description)
{
	std::cerr << "GLFW " << code << ": " << description << "\n";
}

int main(int argc, char** argv)
{
	(void)argc;
	if (!Platform::SelectAssetDirectory(argv[0]))
	{
		std::cerr << "Cannot locate Data and Shaders beside the executable.\n";
		return 1;
	}

	AnalysisLog::Get().Start();
	glfwSetErrorCallback(ErrorCallback);

	if (!glfwInit())
	{
		AnalysisLog::Get().Stop("glfw_initialization_failed");
		return 1;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	// Match the original driver-selected desktop profile; macOS requires core.
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);
	glfwWindowHint(GLFW_SAMPLES, 0);
	glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_FALSE);
	glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
#ifdef __APPLE__
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
	g_Window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Game Software Engineering KPU", NULL, NULL);

	if (g_Window == NULL)
	{
		glfwTerminate();
		AnalysisLog::Get().Stop("window_creation_failed");
		return 1;
	}

	glfwSetWindowPos(g_Window, 120, 60);
	glfwMakeContextCurrent(g_Window);
	// The previous loop was uncapped. Do not introduce a silent 60/30 FPS limit.
	glfwSwapInterval(0);
	glewExperimental = GL_TRUE;
	const GLenum loaderStatus = glewInit();
	glGetError(); // GLEW may probe a legacy token in a core context.

	if (loaderStatus != GLEW_OK || !GLEW_VERSION_3_3)
	{
		std::cerr << "OpenGL 3.3 and its entry points are required.\n";
		glfwDestroyWindow(g_Window);
		glfwTerminate();
		AnalysisLog::Get().Stop("opengl_loader_failed");
		return 1;
	}

	AnalysisLog::Get().Event("window.backend", std::string("glfw=") + glfwGetVersionString() + ";swap_interval=0");
	const GLenum gpuKeys[] = { GL_VENDOR, GL_RENDERER, GL_VERSION, GL_SHADING_LANGUAGE_VERSION };
	const char* const gpuNames[] = { "gpu.vendor", "gpu.renderer", "gpu.opengl", "gpu.glsl" };

	for (int i = 0; i < 4; ++i)
	{
		const GLubyte* value = glGetString(gpuKeys[i]);
		AnalysisLog::Get().Event(gpuNames[i], value != NULL ? reinterpret_cast<const char*>(value) : "unavailable");
	}

	double phaseStart = AnalysisLog::NowMs();
	g_Renderer = new Renderer(WINDOW_WIDTH, WINDOW_HEIGHT);
	AnalysisLog::Get().Event("renderer.initialize", "cpu_ms=" + std::to_string(AnalysisLog::NowMs() - phaseStart));

	if (!g_Renderer->IsInitialized())
	{
		std::cout << "Renderer could not be initialized. Check that the working directory is the\n"
			<< "project folder, so that ./Shaders and ./Data resolve.\n";
		delete g_Renderer;
		glfwDestroyWindow(g_Window);
		glfwTerminate();
		AnalysisLog::Get().Stop("renderer_initialization_failed");

		return 1;
	}

	AnalysisLog::Get().Event("fonts.backend", g_Renderer->FontBackend());

	// Shapes are built from primitives on the first run only; every later run
	// just reads Data/models.cache back.
	g_Models = new ModelLibrary();
	phaseStart = AnalysisLog::NowMs();

	if (!g_Models->LoadOrBuild(MODEL_CACHE_PATH) || !g_Models->RegisterMeshes(g_Renderer))
	{
		std::cout << "Model library could not be prepared.\n";
		delete g_Models;
		delete g_Renderer;
		glfwDestroyWindow(g_Window);
		glfwTerminate();
		AnalysisLog::Get().Stop("model_initialization_failed");

		return 1;
	}

	AnalysisLog::Get().Event("models.ready", "loaded=" + std::to_string(g_Models->WasLoadedFromCache())
		+ ";count=" + std::to_string(g_Models->GetCount()) + ";cpu_ms=" + std::to_string(AnalysisLog::NowMs() - phaseStart));
	g_Dialogue = new DialogueDB();

	if (!g_Dialogue->Load(DIALOGUE_PATH))
	{
		std::cout << "Dialogue could not be loaded. Check " << DIALOGUE_PATH << "\n";
		delete g_Dialogue;
		delete g_Models;
		delete g_Renderer;
		glfwDestroyWindow(g_Window);
		glfwTerminate();
		AnalysisLog::Get().Stop("dialogue_initialization_failed");

		return 1;
	}

	g_Tutorial = new Game();
	phaseStart = AnalysisLog::NowMs();

	if (!g_Tutorial->Initialize(g_Renderer, g_Models, g_Dialogue))
	{
		std::cout << "Tutorial data could not be loaded. Check ./Data/village.map\n";
		delete g_Tutorial;
		delete g_Dialogue;
		delete g_Models;
		delete g_Renderer;
		glfwDestroyWindow(g_Window);
		glfwTerminate();
		AnalysisLog::Get().Stop("tutorial_initialization_failed");

		return 1;
	}

	AnalysisLog::Get().Event("tutorial.ready", "cpu_ms=" + std::to_string(AnalysisLog::NowMs() - phaseStart));
	g_Monitor.Start();

	RefreshViewport(g_Window);
	glfwSetKeyCallback(g_Window, KeyCallback);
	glfwSetScrollCallback(g_Window, ScrollCallback);
	glfwSetCursorPosCallback(g_Window, CursorCallback);
	glfwSetMouseButtonCallback(g_Window, MouseButtonCallback);
	glfwSetWindowFocusCallback(g_Window, FocusCallback);
	glfwSetFramebufferSizeCallback(g_Window, [](GLFWwindow* window, int, int) { RefreshViewport(window); });
	glfwSetWindowContentScaleCallback(g_Window, [](GLFWwindow* window, float, float) { RefreshViewport(window); });
	g_PreviousTime = glfwGetTime();
	g_PreviousUpdateMs = AnalysisLog::NowMs();
	g_TestStart = glfwGetTime();
	g_CapturePath = Platform::Environment("GAME_SMOKE_CAPTURE");
	const std::string smokeSetting = Platform::Environment("GAME_SMOKE_SECONDS");
	char* smokeEnd = NULL;
	const double smokeSeconds = smokeSetting.empty() ? 0.0 : std::strtod(smokeSetting.c_str(), &smokeEnd);
	const bool smoke = !smokeSetting.empty() && *smokeEnd == '\0' && smokeSeconds > 0.0 && smokeSeconds <= 120.0;

	if (Platform::Environment("GAME_SMOKE_SCENE") == "level1")
	{
		EnterLevel();
		g_PreviousTime = glfwGetTime();
		g_PreviousUpdateMs = AnalysisLog::NowMs();
	}

	while (!glfwWindowShouldClose(g_Window))
	{
		glfwPollEvents();
		Idle();

		if (glfwWindowShouldClose(g_Window))
		{
			break;
		}

		int width, height;
		glfwGetFramebufferSize(g_Window, &width, &height);

		if (width > 0 && height > 0 && glfwGetWindowAttrib(g_Window, GLFW_ICONIFIED) != GLFW_TRUE)
		{
			RenderScene();
		}
		else
		{
			glfwWaitEventsTimeout(0.01);
		}

		if (smoke && glfwGetTime() - g_TestStart >= smokeSeconds)
		{
			glfwSetWindowShouldClose(g_Window, GLFW_TRUE);
		}
	}

	int exitCode = 0;

	if (smoke)
	{
		const GLenum error = glGetError();
		const RenderStats& stats = g_Renderer->GetStats();
		exitCode = error == GL_NO_ERROR && stats.worldDrawCalls > 0 && stats.textDrawCalls > 0 ? 0 : 2;
		AnalysisLog::Get().Event("smoke.result", "gl_error=" + std::to_string(error)
			+ ";draws=" + std::to_string(stats.worldDrawCalls + stats.textDrawCalls) + ";exit=" + std::to_string(exitCode));
	}

	g_Monitor.Stop();

	delete g_Level;
	delete g_Tutorial;
	delete g_Dialogue;
	delete g_Models;
	delete g_Renderer;
	glfwDestroyWindow(g_Window);
	glfwTerminate();
	AnalysisLog::Get().Stop("normal_exit");

	return exitCode;
}
