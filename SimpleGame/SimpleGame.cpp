/*
Copyright 2022 Lee Taek Hee (Tech University of Korea)

This program is free software: you can redistribute it and/or modify
it under the terms of the What The Hell License. Do it plz.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY.
*/

#include "stdafx.h"
#include <ctime>
#include <iostream>

#include "Dependencies\glew.h"
#include "Dependencies\freeglut.h"

#include "Dialogue.h"
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

	// The tutorial hands over to level 1; from then on the level owns the session.
	bool InLevel()
	{
		return g_Level != NULL;
	}

	void EnterLevel()
	{
		const unsigned int seed = (unsigned int)time(NULL);

		g_Level = new Level();

		if (!g_Level->Initialize(g_Renderer, g_Models, g_Dialogue, seed))
		{
			std::cout << "Level 1 could not be generated; staying in the tutorial.\n";

			delete g_Level;
			g_Level = NULL;
		}
	}
}

void RenderScene(void)
{
	if (InLevel())
	{
		g_Level->Render();
	}
	else
	{
		g_Tutorial->Render();
	}

	glutSwapBuffers();
}

// Update and render are separate: the loop advances the simulation by the time
// that actually elapsed, so movement speed does not depend on the frame rate.
void Idle(void)
{
	const int nowMs = glutGet(GLUT_ELAPSED_TIME);
	float deltaSeconds = (float)(nowMs - g_PreviousTimeMs) / 1000.0f;
	g_PreviousTimeMs = nowMs;

	if (deltaSeconds < 0.0f)
	{
		deltaSeconds = 0.0f;
	}

	if (deltaSeconds > 0.05f)
	{
		// A long stall (dragging the window, a breakpoint) must not teleport
		// the player through a wall.
		deltaSeconds = 0.05f;
	}

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

	glutPostRedisplay();
}

void Reshape(int width, int height)
{
	g_Renderer->Resize(width, height);
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
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);
	glutInitWindowPosition(120, 60);
	glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
	glutCreateWindow("Game Software Engineering KPU");

	// Closing the window should unwind main() rather than kill the process, so
	// the destructors below actually run.
	glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);

	glewInit();

	if (glewIsSupported("GL_VERSION_3_3"))
	{
		std::cout << "GLEW: OpenGL 3.3 is available.\n";
	}
	else
	{
		std::cout << "GLEW: OpenGL 3.3 is NOT supported. The shaders will fail to compile.\n";
	}

	g_Renderer = new Renderer(WINDOW_WIDTH, WINDOW_HEIGHT);

	if (!g_Renderer->IsInitialized())
	{
		std::cout << "Renderer could not be initialized. Check that the working directory is the\n"
			<< "project folder, so that ./Shaders and ./Data resolve.\n";
		delete g_Renderer;

		return 1;
	}

	// Shapes are built from primitives on the first run only; every later run
	// just reads Data/models.cache back.
	g_Models = new ModelLibrary();

	if (!g_Models->LoadOrBuild(MODEL_CACHE_PATH))
	{
		std::cout << "Model library could not be prepared.\n";
		delete g_Models;
		delete g_Renderer;

		return 1;
	}

	g_Dialogue = new DialogueDB();

	if (!g_Dialogue->Load(DIALOGUE_PATH))
	{
		std::cout << "Dialogue could not be loaded. Check " << DIALOGUE_PATH << "\n";
		delete g_Dialogue;
		delete g_Models;
		delete g_Renderer;

		return 1;
	}

	g_Tutorial = new Game();

	if (!g_Tutorial->Initialize(g_Renderer, g_Models, g_Dialogue))
	{
		std::cout << "Tutorial data could not be loaded. Check ./Data/village.map\n";
		delete g_Tutorial;
		delete g_Dialogue;
		delete g_Models;
		delete g_Renderer;

		return 1;
	}

	// Without this, holding a key produces a stream of down/up pairs and the
	// movement state flickers.
	glutIgnoreKeyRepeat(1);

	glutDisplayFunc(RenderScene);
	glutIdleFunc(Idle);
	glutReshapeFunc(Reshape);
	glutKeyboardFunc(KeyInput);
	glutKeyboardUpFunc(KeyUpInput);

	g_PreviousTimeMs = glutGet(GLUT_ELAPSED_TIME);

	glutMainLoop();

	delete g_Level;
	delete g_Tutorial;
	delete g_Dialogue;
	delete g_Models;
	delete g_Renderer;

	return 0;
}
