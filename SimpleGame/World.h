#pragma once

#include <string>
#include <vector>

enum TileType
{
	TILE_GRASS = 0,
	TILE_TALLGRASS,
	TILE_DIRT,
	TILE_STONE,
	TILE_WATER,
	TILE_SAND,
	TILE_TREE,
	TILE_BUSH,
	TILE_ROCK
};

enum PropType
{
	PROP_SHRINE = 0,
	PROP_TORII,
	PROP_HOUSE,
	PROP_RUIN,
	PROP_WELL,
	PROP_CART,
	PROP_DOCK,
	PROP_STONE,
	PROP_LANTERN,
	PROP_KEEP,
	PROP_TOWER,
	PROP_WALL,
	PROP_GATE,
	PROP_UNKNOWN
};

namespace StructureDimensions
{
	const float GATE_PIER_WIDTH = 0.65f;
	const float TORII_HALF_SPAN = 0.9f;
	const float TORII_BASE_HALF_WIDTH = 0.22f;
	const float TORII_BASE_HALF_DEPTH = 0.24f;
}

// For sized props (house, shrine, ruin) x/y is the minimum corner of the
// footprint and sizeX/sizeY its extent. Point props keep size 0 and are
// positioned at x/y directly.
struct Prop
{
	PropType type;
	float x;
	float y;
	float sizeX;
	float sizeY;
	int variant;
};

struct SpawnPoint
{
	std::string id;
	float x;
	float y;
};

struct BlockRect
{
	float minX;
	float minY;
	float maxX;
	float maxY;
};

struct TerrainSurface
{
	float height = 0.0f;
	float slopeX = 0.0f;
	float slopeY = 0.0f;
};

// Small xorshift generator. Deterministic for a given seed so a level can be
// reproduced from its seed alone.
struct Rng
{
	unsigned int state;

	void Seed(unsigned int value);
	unsigned int Next();
	float NextFloat();					// [0, 1)
	float Range(float low, float high);
	int RangeInt(int low, int high);	// inclusive
};

class World
{
public:
	World();

	bool Load(const char* path);

	// Builds a random map and guarantees that every walkable cell is reachable
	// from the player start. See World::Generate for how that is enforced.
	bool Generate(unsigned int seed, int width, int height);

	// Reachable walkable cells, as y * width + x. Filled by Generate and used
	// for spawn placement.
	const std::vector<int>& GetOpenCells() const { return m_OpenCells; }

	int GetWidth() const { return m_Width; }
	int GetHeight() const { return m_Height; }

	TileType GetTile(int x, int y) const;
	bool IsSolidTile(int x, int y) const;

	// Circle test against solid tiles and prop footprints.
	bool IsBlocked(float worldX, float worldY, float radius) const;
	bool CanMove(float fromX, float fromY, float toX, float toY, float radius) const;
	float GetElevation(float worldX, float worldY) const;
	float GetTileElevation(int x, int y, float localX = 0.5f, float localY = 0.5f) const;
	bool IsStairTile(int x, int y) const;
	bool StairsRunAlongX(int x, int y) const;
	static const int STAIR_TREADS_PER_TILE = 2;

	const std::vector<Prop>& GetProps() const { return m_Props; }
	const std::vector<SpawnPoint>& GetNpcSpawns() const { return m_NpcSpawns; }

	float GetPlayerStartX() const { return m_PlayerStartX; }
	float GetPlayerStartY() const { return m_PlayerStartY; }

private:
	void BuildBlockers();
	void FlattenPropFootprints();

	void SetTile(int x, int y, TileType tile);
	void PaintTerrain(Rng* rng);
	void CarvePath(int fromX, int fromY, int toX, int toY);
	int LabelComponents(std::vector<int>* labels) const;
	void ConnectComponents();
	void CollectOpenCells(int startX, int startY);

	int m_Width;
	int m_Height;
	std::vector<unsigned char> m_Tiles;
	std::vector<TerrainSurface> m_Surfaces;
	std::vector<Prop> m_Props;
	std::vector<BlockRect> m_Blockers;
	std::vector<SpawnPoint> m_NpcSpawns;
	std::vector<int> m_OpenCells;

	float m_PlayerStartX;
	float m_PlayerStartY;
};
