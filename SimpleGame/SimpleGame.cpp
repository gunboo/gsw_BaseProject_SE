/*
Copyright 2022 Lee Taek Hee (Tech University of Korea)

This program is free software: you can redistribute it and/or modify
it under the terms of the What The Hell License. Do it plz.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY.
*/

#include "stdafx.h"
#include <ctime>
#include <cstdlib>
#include <cerrno>
#include <iostream>

#include "Dependencies\glew.h"
#include "Dependencies\freeglut.h"

#include "Dialogue.h"
#include "AnalysisLog.h"
#include "Game.h"
#include "Level.h"
#include "Model.h"
#include "Renderer.h"

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

	int g_PreviousTimeMs = 0;
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
		char* seedText = NULL;
		size_t seedLength = 0;
		_dupenv_s(&seedText, &seedLength, "GAME_LEVEL_SEED");

		if (seedText != NULL)
		{
			char* end = NULL;
			errno = 0;
			const unsigned long parsed = strtoul(seedText, &end, 10);

			if (*seedText >= '0' && *seedText <= '9' && *end == '\0' && errno == 0)
			{
				seed = static_cast<unsigned int>(parsed);
			}
			else
			{
				AnalysisLog::Get().Event("level.seed_invalid", "using_clock_seed");
			}

			free(seedText);
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

	g_FrameSample.renderCpuMs = AnalysisLog::NowMs() - begin;
	const double swapStart = AnalysisLog::NowMs();
	glutSwapBuffers();
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

	const int nowMs = glutGet(GLUT_ELAPSED_TIME);
	float deltaSeconds = (float)(nowMs - g_PreviousTimeMs) / 1000.0f;
	g_PreviousTimeMs = nowMs;

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
			glutLeaveMainLoop();

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
			glutLeaveMainLoop();

			return;
		}
	}

	g_FrameSample.updateCpuMs += AnalysisLog::NowMs() - begin;
	glutPostRedisplay();
}

void Reshape(int width, int height)
{
	g_Renderer->Resize(width, height);
	AnalysisLog::Get().Event("window.resize", "width=" + std::to_string(g_Renderer->GetWidth())
		+ ";height=" + std::to_string(g_Renderer->GetHeight()));
}

void SpecialKeyInput(int key, int x, int y)
{
	if (key == GLUT_KEY_F9)
	{
		AnalysisLog::Get().Event("marker", "F9;frame=" + std::to_string(g_Renderer->GetStats().frameId));
	}
}

void KeyInput(unsigned char key, int x, int y)
{
	const bool shift = (glutGetModifiers() & GLUT_ACTIVE_SHIFT) != 0;

	if (InLevel())
	{
		g_Level->OnKey(key, true, shift);
	}
	else
	{
		g_Tutorial->OnKey(key, true, shift);
	}
}

void KeyUpInput(unsigned char key, int x, int y)
{
	const bool shift = (glutGetModifiers() & GLUT_ACTIVE_SHIFT) != 0;

	if (InLevel())
	{
		g_Level->OnKey(key, false, shift);
	}
	else
	{
		g_Tutorial->OnKey(key, false, shift);
	}
}

int main(int argc, char** argv)
{
	AnalysisLog::Get().Start();
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);
	glutInitWindowPosition(120, 60);
	glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
	glutCreateWindow("Game Software Engineering KPU");

	// Closing the window should unwind main() rather than kill the process, so
	// the destructors below actually run.
	glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);

	glewInit();
	const GLenum gpuKeys[] = { GL_VENDOR, GL_RENDERER, GL_VERSION, GL_SHADING_LANGUAGE_VERSION };
	const char* const gpuNames[] = { "gpu.vendor", "gpu.renderer", "gpu.opengl", "gpu.glsl" };

	for (int i = 0; i < 4; ++i)
	{
		const GLubyte* value = glGetString(gpuKeys[i]);
		AnalysisLog::Get().Event(gpuNames[i], value != NULL ? reinterpret_cast<const char*>(value) : "unavailable");
	}

	if (glewIsSupported("GL_VERSION_3_3"))
	{
		std::cout << "GLEW: OpenGL 3.3 is available.\n";
	}
	else
	{
		std::cout << "GLEW: OpenGL 3.3 is NOT supported. The shaders will fail to compile.\n";
	}

	double phaseStart = AnalysisLog::NowMs();
	g_Renderer = new Renderer(WINDOW_WIDTH, WINDOW_HEIGHT);
	AnalysisLog::Get().Event("renderer.initialize", "cpu_ms=" + std::to_string(AnalysisLog::NowMs() - phaseStart));

	if (!g_Renderer->IsInitialized())
	{
		std::cout << "Renderer could not be initialized. Check that the working directory is the\n"
			<< "project folder, so that ./Shaders and ./Data resolve.\n";
		delete g_Renderer;
		AnalysisLog::Get().Stop("renderer_initialization_failed");

		return 1;
	}

	// Shapes are built from primitives on the first run only; every later run
	// just reads Data/models.cache back.
	g_Models = new ModelLibrary();
	phaseStart = AnalysisLog::NowMs();

	if (!g_Models->LoadOrBuild(MODEL_CACHE_PATH) || !g_Models->RegisterMeshes(g_Renderer))
	{
		std::cout << "Model library could not be prepared.\n";
		delete g_Models;
		delete g_Renderer;
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
		AnalysisLog::Get().Stop("tutorial_initialization_failed");

		return 1;
	}

	AnalysisLog::Get().Event("tutorial.ready", "cpu_ms=" + std::to_string(AnalysisLog::NowMs() - phaseStart));

	// Without this, holding a key produces a stream of down/up pairs and the
	// movement state flickers.
	glutIgnoreKeyRepeat(1);

	glutDisplayFunc(RenderScene);
	glutIdleFunc(Idle);
	glutReshapeFunc(Reshape);
	glutKeyboardFunc(KeyInput);
	glutKeyboardUpFunc(KeyUpInput);
	glutSpecialFunc(SpecialKeyInput);

	g_PreviousTimeMs = glutGet(GLUT_ELAPSED_TIME);
	g_PreviousUpdateMs = AnalysisLog::NowMs();

	glutMainLoop();

	delete g_Level;
	delete g_Tutorial;
	delete g_Dialogue;
	delete g_Models;
	delete g_Renderer;
	AnalysisLog::Get().Stop("normal_exit");

	return 0;
}
