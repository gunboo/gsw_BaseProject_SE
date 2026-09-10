/*
Copyright 2022 Lee Taek Hee (Tech University of Korea)

This program is free software: you can redistribute it and/or modify
it under the terms of the What The Hell License. Do it plz.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY.
*/

#include "stdafx.h"
#include <iostream>

#include "Dependencies\glew.h"
#include "Dependencies\freeglut.h"

#include "Renderer.h"
#include "Game.h"

namespace
{
	const int WINDOW_WIDTH = 1024;
	const int WINDOW_HEIGHT = 640;

	Renderer* g_Renderer = NULL;
	Game* g_Game = NULL;

	int g_PreviousTimeMs = 0;
}

void RenderScene(void)
{
	g_Game->Render();
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

	g_Game->Update(deltaSeconds);

	if (g_Game->WantsExit())
	{
		glutLeaveMainLoop();
		return;
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
	g_Game->OnKey(key, true, shift);
}

void KeyUpInput(unsigned char key, int x, int y)
{
	const bool shift = (glutGetModifiers() & GLUT_ACTIVE_SHIFT) != 0;
	g_Game->OnKey(key, false, shift);
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

	g_Game = new Game();
	if (!g_Game->Initialize(g_Renderer))
	{
		std::cout << "Game data could not be loaded. Check ./Data/village.map and ./Data/dialogue.txt\n";
		delete g_Game;
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

	delete g_Game;
	delete g_Renderer;

	return 0;
}
