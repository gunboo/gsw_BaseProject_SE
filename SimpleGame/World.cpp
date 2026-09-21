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

	struct PropName
	{
		const char* name;
		PropType type;
	};

	// Keep this table in step with the PROPS section of Data/village.map.
	const PropName PROP_NAMES[] =
	{
		{ "shrine",  PROP_SHRINE },
		{ "torii",   PROP_TORII },
		{ "house",   PROP_HOUSE },
		{ "ruin",    PROP_RUIN },
		{ "well",    PROP_WELL },
		{ "cart",    PROP_CART },
		{ "dock",    PROP_DOCK },
		{ "stone",   PROP_STONE },
		{ "lantern", PROP_LANTERN }
	};

	PropType PropFromName(const std::string& name)
	{
		const int count = (int)(sizeof(PROP_NAMES) / sizeof(PROP_NAMES[0]));

		for (int i = 0; i < count; ++i)
		{
			if (name == PROP_NAMES[i].name)
			{
				return PROP_NAMES[i].type;
			}
		}

		return PROP_UNKNOWN;
	}

	float Clamp(float value, float low, float high)
	{
		if (value < low)
		{
			return low;
		}

		if (value > high)
		{
			return high;
		}

		return value;
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

// ---------------------------------------------------------------- random generation

void Rng::Seed(unsigned int value)
{
	state = value != 0 ? value : 0x9E3779B9u;
}

unsigned int Rng::Next()
{
	state ^= state << 13;
	state ^= state >> 17;
	state ^= state << 5;

	return state;
}

float Rng::NextFloat()
{
	return (float)(Next() & 0xFFFFFF) / (float)0x1000000;
}

float Rng::Range(float low, float high)
{
	return low + (high - low) * NextFloat();
}

int Rng::RangeInt(int low, int high)
{
	if (high <= low)
	{
		return low;
	}

	return low + (int)(Next() % (unsigned int)(high - low + 1));
}

void World::SetTile(int x, int y, TileType tile)
{
	if (x < 0 || y < 0 || x >= m_Width || y >= m_Height)
	{
		return;
	}

	m_Tiles[(size_t)y * m_Width + x] = (unsigned char)tile;
}

void World::PaintTerrain(Rng* rng)
{
	// Ground cover first, then the features that cut into it.
	for (int y = 0; y < m_Height; ++y)
	{
		for (int x = 0; x < m_Width; ++x)
		{
			SetTile(x, y, rng->NextFloat() < 0.18f ? TILE_TALLGRASS : TILE_GRASS);
		}
	}

	// Ponds, each with a sand rim so the shore does not read as a hard edge.
	const int pondCount = 2 + (int)(rng->Next() % 3);

	for (int p = 0; p < pondCount; ++p)
	{
		const float centerX = rng->Range(m_Width * 0.18f, m_Width * 0.82f);
		const float centerY = rng->Range(m_Height * 0.18f, m_Height * 0.82f);
		const float radiusX = rng->Range(3.5f, 7.0f);
		const float radiusY = rng->Range(3.0f, 5.5f);

		for (int y = 0; y < m_Height; ++y)
		{
			for (int x = 0; x < m_Width; ++x)
			{
				const float dx = (x - centerX) / radiusX;
				const float dy = (y - centerY) / radiusY;
				const float distance = dx * dx + dy * dy;

				if (distance < 1.0f)
				{
					SetTile(x, y, TILE_WATER);
				}
				else if (distance < 1.35f && GetTile(x, y) != TILE_WATER)
				{
					SetTile(x, y, TILE_SAND);
				}
			}
		}
	}

	// Forest clumps. Density falls off from each clump centre, which gives a
	// ragged edge instead of a circle of trees.
	const int clumpCount = 7 + (int)(rng->Next() % 6);

	for (int c = 0; c < clumpCount; ++c)
	{
		const float centerX = rng->Range(0.0f, (float)m_Width);
		const float centerY = rng->Range(0.0f, (float)m_Height);
		const float radius = rng->Range(4.0f, 9.0f);

		for (int y = 0; y < m_Height; ++y)
		{
			for (int x = 0; x < m_Width; ++x)
			{
				const float dx = x - centerX;
				const float dy = y - centerY;
				const float distance = sqrtf(dx * dx + dy * dy);

				if (distance > radius)
				{
					continue;
				}

				const TileType tile = GetTile(x, y);

				if (tile == TILE_WATER || tile == TILE_SAND)
				{
					continue;
				}

				const float density = 1.0f - distance / radius;

				if (rng->NextFloat() < density * 0.72f)
				{
					SetTile(x, y, TILE_TREE);
				}
				else if (rng->NextFloat() < density * 0.30f)
				{
					SetTile(x, y, TILE_BUSH);
				}
			}
		}
	}

	// Scattered boulders.
	for (int y = 0; y < m_Height; ++y)
	{
		for (int x = 0; x < m_Width; ++x)
		{
			if (GetTile(x, y) == TILE_GRASS && rng->NextFloat() < 0.020f)
			{
				SetTile(x, y, TILE_ROCK);
			}
		}
	}

	// A solid ring of forest so the player cannot walk off the map.
	for (int y = 0; y < m_Height; ++y)
	{
		for (int x = 0; x < m_Width; ++x)
		{
			if (x < 2 || y < 2 || x >= m_Width - 2 || y >= m_Height - 2)
			{
				SetTile(x, y, TILE_TREE);
			}
		}
	}

	// The starting clearing: stone underfoot, nothing standing on it.
	const int clearX = m_Width / 2;
	const int clearY = m_Height / 2;
	const float clearRadius = 4.5f;

	for (int y = 0; y < m_Height; ++y)
	{
		for (int x = 0; x < m_Width; ++x)
		{
			const float dx = (float)(x - clearX);
			const float dy = (float)(y - clearY);

			if (dx * dx + dy * dy <= clearRadius * clearRadius)
			{
				SetTile(x, y, TILE_STONE);
			}
		}
	}

	m_PlayerStartX = clearX + 0.5f;
	m_PlayerStartY = clearY + 0.5f;
}

void World::CarvePath(int fromX, int fromY, int toX, int toY)
{
	// An L-shaped corridor two tiles wide. Two wide matters: a single-tile gap
	// is narrower than the player collision circle in a corner.
	int x = fromX;
	int y = fromY;

	while (x != toX)
	{
		x += (toX > x) ? 1 : -1;

		SetTile(x, y, TILE_DIRT);
		SetTile(x, y + 1, TILE_DIRT);
	}

	while (y != toY)
	{
		y += (toY > y) ? 1 : -1;

		SetTile(x, y, TILE_DIRT);
		SetTile(x + 1, y, TILE_DIRT);
	}
}

int World::LabelComponents(std::vector<int>* labels) const
{
	labels->assign((size_t)m_Width * m_Height, -1);

	std::vector<int> stack;
	int nextLabel = 0;

	for (int startY = 0; startY < m_Height; ++startY)
	{
		for (int startX = 0; startX < m_Width; ++startX)
		{
			const int startIndex = startY * m_Width + startX;

			if (IsSolidTile(startX, startY) || (*labels)[startIndex] >= 0)
			{
				continue;
			}

			stack.clear();
			stack.push_back(startIndex);
			(*labels)[startIndex] = nextLabel;

			while (!stack.empty())
			{
				const int index = stack.back();
				stack.pop_back();

				const int x = index % m_Width;
				const int y = index / m_Width;

				const int neighbourX[4] = { x - 1, x + 1, x, x };
				const int neighbourY[4] = { y, y, y - 1, y + 1 };

				for (int n = 0; n < 4; ++n)
				{
					const int nx = neighbourX[n];
					const int ny = neighbourY[n];

					if (nx < 0 || ny < 0 || nx >= m_Width || ny >= m_Height)
					{
						continue;
					}

					const int neighbourIndex = ny * m_Width + nx;

					if (IsSolidTile(nx, ny) || (*labels)[neighbourIndex] >= 0)
					{
						continue;
					}

					(*labels)[neighbourIndex] = nextLabel;
					stack.push_back(neighbourIndex);
				}
			}

			++nextLabel;
		}
	}

	return nextLabel;
}

void World::ConnectComponents()
{
	// Requirement: no walkable pocket may be unreachable. This is stage one -
	// carve a corridor from every sizeable pocket to the largest region.
	// CollectOpenCells then seals whatever still failed to join.
	std::vector<int> labels;
	const int componentCount = LabelComponents(&labels);

	if (componentCount <= 1)
	{
		return;
	}

	std::vector<int> sizes((size_t)componentCount, 0);
	std::vector<int> representative((size_t)componentCount, -1);

	for (int index = 0; index < (int)labels.size(); ++index)
	{
		const int label = labels[index];

		if (label < 0)
		{
			continue;
		}

		++sizes[label];

		if (representative[label] < 0)
		{
			representative[label] = index;
		}
	}

	int mainLabel = 0;

	for (int label = 1; label < componentCount; ++label)
	{
		if (sizes[label] > sizes[mainLabel])
		{
			mainLabel = label;
		}
	}

	const int mainIndex = representative[mainLabel];
	const int mainX = mainIndex % m_Width;
	const int mainY = mainIndex / m_Width;

	const int MIN_POCKET_TO_CONNECT = 6;

	for (int label = 0; label < componentCount; ++label)
	{
		if (label == mainLabel || representative[label] < 0)
		{
			continue;
		}

		// A pocket of a few tiles is not worth a corridor across the map; it
		// gets filled in rather than connected.
		if (sizes[label] < MIN_POCKET_TO_CONNECT)
		{
			continue;
		}

		const int index = representative[label];

		CarvePath(index % m_Width, index / m_Width, mainX, mainY);
	}
}

void World::CollectOpenCells(int startX, int startY)
{
	m_OpenCells.clear();

	if (IsSolidTile(startX, startY))
	{
		return;
	}

	std::vector<unsigned char> visited((size_t)m_Width * m_Height, 0);
	std::vector<int> stack;

	const int startIndex = startY * m_Width + startX;

	stack.push_back(startIndex);
	visited[startIndex] = 1;

	while (!stack.empty())
	{
		const int index = stack.back();
		stack.pop_back();

		m_OpenCells.push_back(index);

		const int x = index % m_Width;
		const int y = index / m_Width;

		const int neighbourX[4] = { x - 1, x + 1, x, x };
		const int neighbourY[4] = { y, y, y - 1, y + 1 };

		for (int n = 0; n < 4; ++n)
		{
			const int nx = neighbourX[n];
			const int ny = neighbourY[n];

			if (nx < 0 || ny < 0 || nx >= m_Width || ny >= m_Height)
			{
				continue;
			}

			const int neighbourIndex = ny * m_Width + nx;

			if (visited[neighbourIndex] || IsSolidTile(nx, ny))
			{
				continue;
			}

			visited[neighbourIndex] = 1;
			stack.push_back(neighbourIndex);
		}
	}

	// Stage two. Anything walkable the flood fill never reached would be a
	// place the player can see but never stand in, so it is closed off. After
	// this sweep the walkable set and the reachable set are the same set.
	int sealed = 0;

	for (int y = 0; y < m_Height; ++y)
	{
		for (int x = 0; x < m_Width; ++x)
		{
			const int index = y * m_Width + x;

			if (!visited[index] && !IsSolidTile(x, y))
			{
				SetTile(x, y, TILE_TREE);
				++sealed;
			}
		}
	}

	if (sealed > 0)
	{
		std::cout << "level: sealed " << sealed << " unreachable tiles\n";
	}
}

bool World::Generate(unsigned int seed, int width, int height)
{
	const int MIN_DIMENSION = 16;

	if (width < MIN_DIMENSION || height < MIN_DIMENSION)
	{
		return false;
	}

	m_Width = width;
	m_Height = height;
	m_Tiles.assign((size_t)m_Width * m_Height, (unsigned char)TILE_GRASS);
	m_Props.clear();
	m_Blockers.clear();
	m_NpcSpawns.clear();
	m_OpenCells.clear();

	Rng rng;
	rng.Seed(seed);

	PaintTerrain(&rng);
	ConnectComponents();
	CollectOpenCells((int)m_PlayerStartX, (int)m_PlayerStartY);

	if (m_OpenCells.empty())
	{
		std::cout << "level: generation produced no walkable space\n";

		return false;
	}

	std::cout << "level: generated " << m_Width << "x" << m_Height
		<< " from seed " << seed << ", " << m_OpenCells.size() << " open tiles\n";

	return true;
}
