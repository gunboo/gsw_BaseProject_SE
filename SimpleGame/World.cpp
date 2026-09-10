#include "stdafx.h"
#include "World.h"

#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>

namespace
{
	TileType TileFromChar(char c)
	{
		switch (c)
		{
		case '.': return TILE_GRASS;
		case ',': return TILE_TALLGRASS;
		case ':': return TILE_DIRT;
		case '=': return TILE_STONE;
		case '~': return TILE_WATER;
		case '_': return TILE_SAND;
		case 'T': return TILE_TREE;
		case 't': return TILE_BUSH;
		case 'o': return TILE_ROCK;
		default:  return TILE_GRASS;
		}
	}

	PropType PropFromName(const std::string& name)
	{
		if (name == "shrine")  return PROP_SHRINE;
		if (name == "torii")   return PROP_TORII;
		if (name == "house")   return PROP_HOUSE;
		if (name == "ruin")    return PROP_RUIN;
		if (name == "well")    return PROP_WELL;
		if (name == "cart")    return PROP_CART;
		if (name == "dock")    return PROP_DOCK;
		if (name == "stone")   return PROP_STONE;
		if (name == "lantern") return PROP_LANTERN;
		return PROP_UNKNOWN;
	}

	float Clamp(float v, float low, float high)
	{
		if (v < low) return low;
		if (v > high) return high;
		return v;
	}
}

World::World()
	: m_Width(0)
	, m_Height(0)
	, m_PlayerStartX(1.0f)
	, m_PlayerStartY(1.0f)
{
}

bool World::Load(const char* path)
{
	std::ifstream file(path);
	if (file.fail())
	{
		std::cout << path << " could not be opened.\n";
		return false;
	}

	enum Section { SECTION_NONE, SECTION_TILES, SECTION_PROPS, SECTION_NPCS };
	Section section = SECTION_NONE;
	int rowsRead = 0;

	std::string line;
	while (std::getline(file, line))
	{
		if (!line.empty() && line[line.size() - 1] == '\r')
		{
			line.erase(line.size() - 1);
		}

		const bool insideTiles = (section == SECTION_TILES && rowsRead < m_Height);
		if (!insideTiles)
		{
			if (line.empty() || line[0] == '#')
			{
				continue;
			}
		}

		if (!insideTiles)
		{
			std::istringstream stream(line);
			std::string keyword;
			stream >> keyword;

			if (keyword == "SIZE")
			{
				stream >> m_Width >> m_Height;
				if (m_Width <= 0 || m_Height <= 0)
				{
					std::cout << path << " has an invalid SIZE.\n";
					return false;
				}
				m_Tiles.assign((size_t)m_Width * m_Height, (unsigned char)TILE_GRASS);
				continue;
			}
			if (keyword == "TILES")
			{
				section = SECTION_TILES;
				rowsRead = 0;
				continue;
			}
			if (keyword == "PROPS")
			{
				section = SECTION_PROPS;
				continue;
			}
			if (keyword == "NPCS")
			{
				section = SECTION_NPCS;
				continue;
			}
			if (keyword == "PLAYER")
			{
				stream >> m_PlayerStartX >> m_PlayerStartY;
				continue;
			}

			if (section == SECTION_PROPS)
			{
				Prop prop;
				prop.type = PropFromName(keyword);
				stream >> prop.x >> prop.y >> prop.sizeX >> prop.sizeY >> prop.variant;
				if (prop.type != PROP_UNKNOWN)
				{
					m_Props.push_back(prop);
				}
				continue;
			}

			if (section == SECTION_NPCS)
			{
				SpawnPoint spawn;
				spawn.id = keyword;
				stream >> spawn.x >> spawn.y;
				m_NpcSpawns.push_back(spawn);
				continue;
			}

			continue;
		}

		// Inside the tile grid: one row of characters per line.
		for (int x = 0; x < m_Width && x < (int)line.size(); ++x)
		{
			m_Tiles[(size_t)rowsRead * m_Width + x] = (unsigned char)TileFromChar(line[x]);
		}
		++rowsRead;
	}

	if (m_Width <= 0 || rowsRead < m_Height)
	{
		std::cout << path << " is incomplete: expected " << m_Height << " tile rows, read " << rowsRead << ".\n";
		return false;
	}

	FlattenPropFootprints();
	BuildBlockers();

	std::cout << path << " loaded: " << m_Width << "x" << m_Height
		<< ", props " << m_Props.size() << ", npcs " << m_NpcSpawns.size() << "\n";
	return true;
}

void World::FlattenPropFootprints()
{
	// Buildings clear whatever scattered terrain the generator left under them,
	// so props can be placed without hand-editing the tile grid.
	for (size_t i = 0; i < m_Props.size(); ++i)
	{
		const Prop& prop = m_Props[i];
		if (prop.type != PROP_HOUSE && prop.type != PROP_RUIN && prop.type != PROP_SHRINE)
		{
			continue;
		}

		const int minX = (int)floorf(prop.x) - 1;
		const int minY = (int)floorf(prop.y) - 1;
		const int maxX = (int)ceilf(prop.x + prop.sizeX);
		const int maxY = (int)ceilf(prop.y + prop.sizeY);

		for (int y = minY; y <= maxY; ++y)
		{
			for (int x = minX; x <= maxX; ++x)
			{
				if (x < 0 || y < 0 || x >= m_Width || y >= m_Height)
				{
					continue;
				}
				const TileType tile = GetTile(x, y);
				if (tile == TILE_TREE || tile == TILE_BUSH || tile == TILE_ROCK)
				{
					m_Tiles[(size_t)y * m_Width + x] = (unsigned char)TILE_DIRT;
				}
			}
		}
	}
}

void World::BuildBlockers()
{
	for (size_t i = 0; i < m_Props.size(); ++i)
	{
		const Prop& prop = m_Props[i];

		BlockRect rect;
		switch (prop.type)
		{
		case PROP_HOUSE:
		case PROP_RUIN:
		case PROP_SHRINE:
			rect.minX = prop.x;
			rect.minY = prop.y;
			rect.maxX = prop.x + prop.sizeX;
			rect.maxY = prop.y + prop.sizeY;
			break;

		case PROP_WELL:
		case PROP_CART:
		case PROP_STONE:
			rect.minX = prop.x - 0.45f;
			rect.minY = prop.y - 0.45f;
			rect.maxX = prop.x + 0.45f;
			rect.maxY = prop.y + 0.45f;
			break;

		default:
			// Torii, lanterns and the dock are walked through or under.
			continue;
		}

		m_Blockers.push_back(rect);
	}
}

TileType World::GetTile(int x, int y) const
{
	if (x < 0 || y < 0 || x >= m_Width || y >= m_Height)
	{
		return TILE_TREE;		// outside the map reads as solid forest
	}
	return (TileType)m_Tiles[(size_t)y * m_Width + x];
}

bool World::IsSolidTile(int x, int y) const
{
	const TileType tile = GetTile(x, y);
	return tile == TILE_TREE || tile == TILE_WATER || tile == TILE_ROCK;
}

bool World::IsBlocked(float worldX, float worldY, float radius) const
{
	const int minX = (int)floorf(worldX - radius);
	const int maxX = (int)floorf(worldX + radius);
	const int minY = (int)floorf(worldY - radius);
	const int maxY = (int)floorf(worldY + radius);

	for (int y = minY; y <= maxY; ++y)
	{
		for (int x = minX; x <= maxX; ++x)
		{
			if (!IsSolidTile(x, y))
			{
				continue;
			}

			const float nearestX = Clamp(worldX, (float)x, (float)x + 1.0f);
			const float nearestY = Clamp(worldY, (float)y, (float)y + 1.0f);
			const float dx = worldX - nearestX;
			const float dy = worldY - nearestY;
			if (dx * dx + dy * dy < radius * radius)
			{
				return true;
			}
		}
	}

	for (size_t i = 0; i < m_Blockers.size(); ++i)
	{
		const BlockRect& rect = m_Blockers[i];
		const float nearestX = Clamp(worldX, rect.minX, rect.maxX);
		const float nearestY = Clamp(worldY, rect.minY, rect.maxY);
		const float dx = worldX - nearestX;
		const float dy = worldY - nearestY;
		if (dx * dx + dy * dy < radius * radius)
		{
			return true;
		}
	}

	return false;
}
