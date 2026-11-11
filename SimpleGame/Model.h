#pragma once

#include <map>
#include <string>
#include <vector>

#include "Renderer.h"
#include "PlayerMotion.h"

// A model is a flat soup of convex polygons in SCREEN-SPACE pixels, measured
// from the anchor at the object feet (+y down, same as the renderer).
//
// Screen space works because the quarter-view projection is affine: moving the
// camera translates a shape but never changes it. So a shape can be built once
// from circles, rectangles and ellipses, written to a cache file, and on every
// later run simply loaded and blitted at a projected anchor.

enum ModelPolyFlags
{
	MODEL_FLAG_NONE = 0,
	MODEL_FLAG_UNLIT = 1 << 0,		// a source, not a surface: skip lighting
	MODEL_FLAG_TINT = 1 << 1,		// multiply by the per-instance tint
	MODEL_FLAG_SHADED = 1 << 2,	// baked surface receives a top-left gradient
	MODEL_FLAG_RADIAL = 1 << 3,	// center-first fan with a transparent perimeter
	MODEL_FLAG_GROUND = 1 << 4,	// contact shadows stay beneath every actor
	MODEL_FLAG_PLAYER_UPPER = 1 << 5,
	MODEL_FLAG_PLAYER_LEFT_ARM = 1 << 6,
	MODEL_FLAG_PLAYER_RIGHT_ARM = 1 << 7,
	MODEL_FLAG_PLAYER_LEFT_LEG = 1 << 8,
	MODEL_FLAG_PLAYER_RIGHT_LEG = 1 << 9
};

struct ModelPoly
{
	int firstVertex;
	int vertexCount;
	Color color;
	float depthBias;
	unsigned int flags;
	int anim;
	float animPhase;
	float animStrength;
	float shadeBounds[4] = {}; // runtime gradient bounds
	int mesh = -1;			// runtime Renderer handle, not serialized in models.cache
};

struct Model
{
	std::vector<float> vertices;	// x, y pairs
	std::vector<ModelPoly> polys;
	float bounds[4] = {}; // runtime body bounds: minX, minY, maxX, maxY; excludes ground shadows
};

// Draws one instance. Lighting is evaluated by the caller once per object.
void DrawModel(Renderer* renderer, const Model& model, float screenX, float screenY,
	float scale, float depth, const ShadeParams& shade, float bodyTilt = 0.0f, unsigned int hiddenFlags = 0,
	const PlayerMovePose* movePose = NULL);

class ModelBuilder
{
public:
	ModelBuilder();

	// Applies to every shape added until ClearAnim.
	void SetAnim(AnimKind kind, float phase, float strength);
	void ClearAnim();
	void SetFlags(unsigned int flags);

	void AddPolygon(const float* xy, int vertexCount, const Color& color, float depthBias,
		unsigned int flags = MODEL_FLAG_NONE);
	void AddTriangle(float x0, float y0, float x1, float y1, float x2, float y2,
		const Color& color, float depthBias, unsigned int flags = MODEL_FLAG_NONE);
	void AddRect(float x, float y, float width, float height, const Color& color, float depthBias,
		unsigned int flags = MODEL_FLAG_NONE);
	void AddEllipse(float centerX, float centerY, float radiusX, float radiusY, int segments,
		const Color& color, float depthBias, unsigned int flags = MODEL_FLAG_NONE);
	void AddSoftEllipse(float centerX, float centerY, float radiusX, float radiusY,
		const Color& color, float depthBias, unsigned int flags = MODEL_FLAG_UNLIT);
	void AddLine(float x0, float y0, float x1, float y1, float thickness, const Color& color,
		float depthBias, unsigned int flags = MODEL_FLAG_NONE);

	const Model& Get() const
	{
		return m_Model;
	}

private:
	Model m_Model;
	int m_Anim;
	float m_AnimPhase;
	float m_AnimStrength;
	unsigned int m_Flags;
};

// Owns every baked shape in the game. Builds them on the first run and writes
// Data/models.cache; later runs read that file and skip the building entirely.
class ModelLibrary
{
public:
	ModelLibrary();

	bool LoadOrBuild(const char* cachePath);
	bool RegisterMeshes(Renderer* renderer);

	const Model* Find(const std::string& name) const;
	int GetCount() const
	{
		return (int)m_Models.size();
	}

	bool WasLoadedFromCache() const
	{
		return m_LoadedFromCache;
	}

private:
	void BuildAll();
	bool LoadCache(const char* path);
	bool SaveCache(const char* path) const;

	std::map<std::string, Model> m_Models;
	bool m_LoadedFromCache;
};

// Both scenes use the same player composition and equipment depth ordering.
void DrawFoxPlayer(Renderer* renderer, const ModelLibrary& models, float screenX, float screenY,
	float depth, const ShadeParams& shade, bool swordEquipped, const PlayerAttackMotion* motion = NULL,
	const PlayerMoveMotion* movement = NULL);

void DrawFoxSlash(Renderer* renderer, float screenX, float screenY, float depth,
	const PlayerAttackMotion& motion);
