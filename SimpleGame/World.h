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
	PROP_UNKNOWN
};

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

class World
{
public:
	World();

	bool Load(const char* path);

	int GetWidth() const { return m_Width; }
	int GetHeight() const { return m_Height; }

	TileType GetTile(int x, int y) const;
	bool IsSolidTile(int x, int y) const;

	// Circle test against solid tiles and prop footprints.
	bool IsBlocked(float worldX, float worldY, float radius) const;

	const std::vector<Prop>& GetProps() const { return m_Props; }
	const std::vector<SpawnPoint>& GetNpcSpawns() const { return m_NpcSpawns; }

	float GetPlayerStartX() const { return m_PlayerStartX; }
	float GetPlayerStartY() const { return m_PlayerStartY; }

private:
	void BuildBlockers();
	void FlattenPropFootprints();

	int m_Width;
	int m_Height;
	std::vector<unsigned char> m_Tiles;
	std::vector<Prop> m_Props;
	std::vector<BlockRect> m_Blockers;
	std::vector<SpawnPoint> m_NpcSpawns;

	float m_PlayerStartX;
	float m_PlayerStartY;
};
