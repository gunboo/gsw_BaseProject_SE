#pragma once

#include "Model.h"
#include "ResidentActivity.h"

void DrawResidentArt(Renderer& renderer, const ModelLibrary& models, float x, float y,
	float depth, float scale, const ShadeParams& shade, bool hat, bool armed,
	const ResidentActivityMotion& motion, float targetX, float targetY);

void DrawResidentGarden(Renderer& renderer, float x, float y, float depth, const ShadeParams& shade);
