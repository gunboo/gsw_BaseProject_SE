#include "stdafx.h"
#include "Game.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>

namespace
{
	// ---------------------------------------------------------------- palette
	// The four colours from the design document, plus the terrain they sit on.
	// Everything here is the UNLIT value; Game::Lit applies night and lantern
	// light on top, which is what actually carries the mood.
	const Color COL_GRASS = { 0.34f, 0.41f, 0.30f, 1.0f };
	const Color COL_TALLGRASS = { 0.29f, 0.37f, 0.27f, 1.0f };
	const Color COL_DIRT = { 0.47f, 0.40f, 0.30f, 1.0f };
	const Color COL_STONE = { 0.47f, 0.48f, 0.46f, 1.0f };
	const Color COL_WATER = { 0.20f, 0.30f, 0.39f, 1.0f };
	const Color COL_SAND = { 0.56f, 0.51f, 0.41f, 1.0f };

	const Color COL_TSUCHI = { 0.48f, 0.38f, 0.28f, 1.0f };		// earth
	const Color COL_SHIRO = { 0.79f, 0.77f, 0.70f, 1.0f };		// faded white
	const Color COL_SHU = { 0.66f, 0.24f, 0.16f, 1.0f };		// vermilion

	const Color COL_THATCH = { 0.31f, 0.28f, 0.24f, 1.0f };
	const Color COL_PLASTER = { 0.62f, 0.60f, 0.54f, 1.0f };
	const Color COL_TIMBER = { 0.25f, 0.21f, 0.18f, 1.0f };

	const Color COL_FOG = { 0.055f, 0.075f, 0.105f, 1.0f };
	const Color COL_SKY = { 0.035f, 0.048f, 0.068f, 1.0f };
	const Color COL_FLAME = { 1.0f, 0.70f, 0.36f, 1.0f };

	const Color COL_INK = { 0.045f, 0.055f, 0.070f, 1.0f };

	const float TWO_PI = 6.2831853f;

	// The person models are baked at this pixel height; other sizes are scales.
	const float PERSON_MODEL_HEIGHT = 36.0f;

	// The three clues, paired with the UI strings that name them, so that the
	// HUD pips and the clue list cannot drift out of step with the bit values.
	const int CLUE_TOTAL = 3;
	const int CLUE_BITS[CLUE_TOTAL] = { CLUE_SMITH, CLUE_FISHER, CLUE_CHILD };
	const char* const CLUE_UI_KEYS[CLUE_TOTAL] =
	{
		"ui_clue_smith",
		"ui_clue_fisher",
		"ui_clue_child"
	};

	const float PLAYER_RADIUS = 0.28f;
	const float WALK_SPEED = 3.1f;
	const float RUN_SPEED = 5.2f;
	const unsigned char TUTORIAL_SKIP_KEY = '1';
	const int SKIP_HINT_FONT_SIZE = 14;
	const float SKIP_HINT_SIDE_MARGIN = 22.0f;
	const float SKIP_HINT_BOTTOM_MARGIN = 34.0f;
	const float GUIDE_EDGE_MARGIN = 46.0f;
	const float GUIDE_VERTICAL_MARGIN = 120.0f;
	const float GUIDE_PANEL_WIDTH = 252.0f;
	const float GUIDE_PANEL_HEIGHT = 60.0f;
	const int SCENE_CHUNK_SIZE = 8;
	const int EMBER_COUNT = 44;
	const int MIST_COUNT = 16;
	const float SCENERY_BOUND_WIDTH = 100.0f;
	const float SCENERY_BOUND_TOP = 160.0f;
	const float SCENERY_BOUND_BOTTOM = 40.0f;
	const float CHARACTER_BOUND_WIDTH = 64.0f;
	const float CHARACTER_BOUND_TOP = 84.0f;
	const float CHARACTER_BOUND_BOTTOM = 32.0f;
	const float MIST_BOUND_WIDTH = 140.0f;
	const float MIST_BOUND_HEIGHT = 40.0f;
	const float EMBER_BOUND_RADIUS = 4.0f;
	const float PROP_BOUND_LEFT_MARGIN = 120.0f;
	const float PROP_BOUND_RIGHT_MARGIN = 160.0f;
	const float PROP_BOUND_TOP_MARGIN = 160.0f;
	const float PROP_BOUND_BOTTOM_MARGIN = 100.0f;

	const float DEPTH_OVERLAY = 900000.0f;
	const float DEPTH_PANEL = 920000.0f;
	const float DEPTH_PANEL_TOP = 930000.0f;

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

	unsigned int HashInt(int x, int y)
	{
		unsigned int h = (unsigned int)(x * 73856093) ^ (unsigned int)(y * 19349663);
		h ^= h >> 13;
		h *= 1274126177u;
		h ^= h >> 16;

		return h;
	}

	// Projects a world-space polygon and hands it to the batch.
	void PushPoly3(Renderer* renderer, const float* xyz, int count, const Color& color, float depth, float worldZ)
	{
		float points[32];

		if (count > 16)
		{
			count = 16;
		}

		for (int i = 0; i < count; ++i)
		{
			renderer->WorldToScreen(xyz[i * 3 + 0], xyz[i * 3 + 1], xyz[i * 3 + 2] + worldZ,
				&points[i * 2 + 0], &points[i * 2 + 1]);
		}

		renderer->PushPolygon(points, count, color, depth);
	}

	// Flat quad lying on a horizontal plane at height z.
	void PushFlatQuad(Renderer* renderer, float x0, float y0, float x1, float y1, float z,
		const Color& color, float depth, float worldZ)
	{
		const float xyz[12] =
		{
			x0, y0, z,
			x1, y0, z,
			x1, y1, z,
			x0, y1, z
		};

		PushPoly3(renderer, xyz, 4, color, depth, worldZ);
	}

	// Vertical face spanning two world points, from height z0 up to z1.
	void PushWallQuad(Renderer* renderer, float ax, float ay, float bx, float by,
		float z0, float z1, const Color& color, float depth, float worldZ)
	{
		const float xyz[12] =
		{
			ax, ay, z0,
			bx, by, z0,
			bx, by, z1,
			ax, ay, z1
		};

		PushPoly3(renderer, xyz, 4, color, depth, worldZ);
	}
}

Game::Game()
	: m_Renderer(NULL)
	, m_Models(NULL)
	, m_Dialogue(NULL)
	, m_EnvironmentActor(NULL)
	, m_CharactersActor(NULL)
	, m_EffectsActor(NULL)
	, m_InterfaceActor(NULL)
	, m_HudActor(NULL)
	, m_DialogueActor(NULL)
	, m_CluesActor(NULL)
	, m_IntroActor(NULL)
	, m_EndingActor(NULL)
	, m_State(STATE_INTRO)
	, m_WantsExit(false)
	, m_WantsNextLevel(false)
	, m_Time(0.0f)
	, m_StateTime(0.0f)
	, m_Fade(1.0f)
	, m_PlayerActor(NULL)
	, m_PlayerStride(0.0f)
	, m_Running(false)
	, m_Moving(false)
	, m_CameraX(1.0f)
	, m_CameraY(1.0f)
	, m_MetElder(false)
	, m_ClueMask(0)
	, m_TargetKind(INTERACT_NONE)
	, m_TargetIndex(-1)
	, m_TargetX(0.0f)
	, m_TargetY(0.0f)
	, m_ActiveBlock(NULL)
	, m_PageStart(0)
	, m_PageEnd(0)
	, m_ToastTimer(0.0f)
	, m_PlayerLightIndex(-1)
{
	for (int i = 0; i < 4; ++i)
	{
		m_MoveKey[i] = false;
	}
}

bool Game::Initialize(Renderer* renderer, ModelLibrary* models, DialogueDB* dialogue)
{
	m_Renderer = renderer;
	m_Models = models;
	m_Dialogue = dialogue;

	if (!m_World.Load("./Data/village.map"))
	{
		return false;
	}

	m_SceneGraph.Clear();
	m_Npcs.clear();
	m_PropActors.clear();
	m_Motes.clear();
	m_Mist.clear();
	m_Lighting.Clear();
	m_CameraX = m_World.GetPlayerStartX();
	m_CameraY = m_World.GetPlayerStartY();
	m_Lighting.SetViewer(m_CameraX, m_CameraY);
	m_State = STATE_INTRO;
	m_StateTime = 0.0f;
	m_Fade = 1.0f;
	CreateScene();
	UpdateSceneState();
	UpdateLighting();

	return true;
}

void Game::CreateScene()
{
	m_EnvironmentActor = m_SceneGraph.CreateActor("Environment");
	m_CharactersActor = m_SceneGraph.CreateActor("Characters");
	m_EffectsActor = m_SceneGraph.CreateActor("Effects");
	m_InterfaceActor = m_SceneGraph.CreateActor("Interface");
	m_EnvironmentActor->SetActive(false);

	CreateEnvironmentActors();
	CreateCharacterActors();
	CreateEffectActors();
	CreateInterfaceActors();

	// Effects follow Characters so camera/light positions use this frame's move.
	m_EffectsActor->SetUpdateCallback([this](Actor&, float deltaSeconds)
	{
		UpdateCamera(deltaSeconds);
		UpdateInteractionTarget();
	});
}

void Game::CreateEnvironmentActors()
{
	const int chunkColumns = (m_World.GetWidth() + SCENE_CHUNK_SIZE - 1) / SCENE_CHUNK_SIZE;
	const int chunkRows = (m_World.GetHeight() + SCENE_CHUNK_SIZE - 1) / SCENE_CHUNK_SIZE;
	std::vector<Actor*> chunks;

	for (int y = 0; y < chunkRows; ++y)
	{
		for (int x = 0; x < chunkColumns; ++x)
		{
			Actor* chunk = m_SceneGraph.CreateActor("Chunk_" + std::to_string(x)
				+ "_" + std::to_string(y), m_EnvironmentActor);
			chunk->SetPosition((float)(x * SCENE_CHUNK_SIZE), (float)(y * SCENE_CHUNK_SIZE));
			chunk->SetActive(false);
			chunks.push_back(chunk);
		}
	}

	for (int y = 0; y < m_World.GetHeight(); ++y)
	{
		for (int x = 0; x < m_World.GetWidth(); ++x)
		{
			const TileType tile = m_World.GetTile(x, y);
			const unsigned int variation = HashInt(x, y);
			Actor* chunk = chunks[(y / SCENE_CHUNK_SIZE) * chunkColumns + x / SCENE_CHUNK_SIZE];
			const ActorPosition origin = chunk->GetWorldPosition();
			const std::string suffix = std::to_string(x) + "_" + std::to_string(y);
			Actor* ground = m_SceneGraph.CreateActor("Ground_" + suffix, chunk);
			ground->SetPosition((float)x + 0.5f - origin.x, (float)y + 0.5f - origin.y);
			ground->SetRenderBounds(-Renderer::TileHalfWidth(), -Renderer::TileHalfHeight(),
				Renderer::TileHalfWidth(), Renderer::TileHalfHeight());
			ground->SetRenderCallback([this, tile, variation](const Actor& actor, Renderer&)
			{
				DrawGround(actor, tile, variation);
			});

			if (tile != TILE_TREE && tile != TILE_BUSH && tile != TILE_ROCK)
			{
				continue;
			}

			Actor* scenery = m_SceneGraph.CreateActor("Scenery_" + suffix, chunk);
			scenery->SetPosition((float)x + 0.30f + (float)(variation % 40) * 0.01f - origin.x,
				(float)y + 0.30f + (float)((variation >> 8) % 40) * 0.01f - origin.y);
			scenery->SetRenderBounds(-SCENERY_BOUND_WIDTH, -SCENERY_BOUND_TOP,
				SCENERY_BOUND_WIDTH, SCENERY_BOUND_BOTTOM);
			scenery->SetRenderCallback([this, tile, variation](const Actor& actor, Renderer&)
			{
				DrawScenery(actor, tile, (int)((variation >> 16) % 5));
			});
		}
	}

	const std::vector<Prop>& props = m_World.GetProps();

	for (size_t i = 0; i < props.size(); ++i)
	{
		const Prop definition = props[i];
		const int chunkX = (int)Clamp(definition.x / SCENE_CHUNK_SIZE, 0.0f, (float)(chunkColumns - 1));
		const int chunkY = (int)Clamp(definition.y / SCENE_CHUNK_SIZE, 0.0f, (float)(chunkRows - 1));
		Actor* chunk = chunks[chunkY * chunkColumns + chunkX];
		const ActorPosition origin = chunk->GetWorldPosition();
		Actor* prop = m_SceneGraph.CreateActor("Prop_" + std::to_string(i), chunk);
		prop->SetPosition(definition.x - origin.x, definition.y - origin.y);
		prop->SetRenderBounds(-definition.sizeY * Renderer::TileHalfWidth() - PROP_BOUND_LEFT_MARGIN,
			-PROP_BOUND_TOP_MARGIN, definition.sizeX * Renderer::TileHalfWidth() + PROP_BOUND_RIGHT_MARGIN,
			(definition.sizeX + definition.sizeY) * Renderer::TileHalfHeight() + PROP_BOUND_BOTTOM_MARGIN);
		prop->SetRenderCallback([this, definition](const Actor& actor, Renderer&)
		{
			DrawProp(actor, definition);
		});

		m_PropActors.push_back(prop);
	}
}

void Game::CreateCharacterActors()
{
	const std::vector<SpawnPoint>& spawns = m_World.GetNpcSpawns();

	for (size_t i = 0; i < spawns.size(); ++i)
	{
		Npc npc;

		npc.id = spawns[i].id;
		npc.actor = m_SceneGraph.CreateActor("Npc_" + npc.id, m_CharactersActor);
		npc.actor->SetPosition(spawns[i].x, spawns[i].y);
		npc.actor->SetRenderBounds(-CHARACTER_BOUND_WIDTH, -CHARACTER_BOUND_TOP,
			CHARACTER_BOUND_WIDTH, CHARACTER_BOUND_BOTTOM);
		npc.phase = (float)i * 1.37f;
		npc.look = (int)(i % 8);
		npc.actor->SetRenderCallback([this, i](const Actor&, Renderer&)
		{
			DrawNpc(m_Npcs[i]);
		});

		m_Npcs.push_back(npc);
	}

	m_PlayerActor = m_SceneGraph.CreateActor("Player", m_CharactersActor);
	m_PlayerActor->SetPosition(m_World.GetPlayerStartX(), m_World.GetPlayerStartY());
	m_PlayerActor->SetRenderBounds(-CHARACTER_BOUND_WIDTH, -CHARACTER_BOUND_TOP,
		CHARACTER_BOUND_WIDTH, CHARACTER_BOUND_BOTTOM);
	m_PlayerActor->SetUpdateCallback([this](Actor&, float deltaSeconds)
	{
		UpdatePlayer(deltaSeconds);
	});

	m_PlayerActor->SetRenderCallback([this](const Actor& actor, Renderer&)
	{
		DrawPlayer(actor);
	});
}

void Game::CreateEffectActors()
{
	for (int i = 0; i < MIST_COUNT; ++i)
	{
		const unsigned int variation = HashInt(i * 17 + 3, i * 5 + 11);
		Mote wisp;

		wisp.actor = m_SceneGraph.CreateActor("Mist_" + std::to_string(i), m_EffectsActor);
		wisp.actor->SetPosition(4.0f + (float)(variation % 3600) * 0.01f,
			4.0f + (float)((variation >> 8) % 2800) * 0.01f,
			0.10f + (float)((variation >> 4) % 20) * 0.004f);
		wisp.drift = 0.10f + (float)((variation >> 12) % 40) * 0.004f;
		wisp.riseSpeed = 0.0f;
		wisp.life = 1.0f;
		wisp.maxLife = 1.0f;
		wisp.size = 2.6f + (float)((variation >> 16) % 26) * 0.08f;
		wisp.actor->SetRenderBounds(-MIST_BOUND_WIDTH, -MIST_BOUND_HEIGHT,
			MIST_BOUND_WIDTH, MIST_BOUND_HEIGHT);
		wisp.actor->SetUpdateCallback([this, i](Actor&, float deltaSeconds)
		{
			UpdateMist((size_t)i, deltaSeconds);
		});

		wisp.actor->SetRenderCallback([this, i](const Actor&, Renderer&)
		{
			DrawMist(m_Mist[i], (size_t)i);
		});

		m_Mist.push_back(wisp);
	}

	for (int i = 0; i < EMBER_COUNT; ++i)
	{
		Mote mote;

		mote.actor = m_SceneGraph.CreateActor("Ember_" + std::to_string(i), m_EffectsActor);
		mote.actor->SetVisible(false);
		mote.actor->SetRenderBounds(-EMBER_BOUND_RADIUS, -EMBER_BOUND_RADIUS,
			EMBER_BOUND_RADIUS, EMBER_BOUND_RADIUS);
		mote.life = 0.0f;
		mote.maxLife = 1.0f;
		mote.riseSpeed = 0.0f;
		mote.drift = 0.0f;
		mote.size = 1.0f;
		mote.actor->SetUpdateCallback([this, i](Actor&, float deltaSeconds)
		{
			UpdateMote((size_t)i, deltaSeconds);
		});

		mote.actor->SetRenderCallback([this, i](const Actor&, Renderer&)
		{
			DrawMote(m_Motes[i]);
		});

		m_Motes.push_back(mote);
	}
}

void Game::CreateInterfaceActors()
{
	// Window-relative panels deliberately have no world-culling bounds.
	Actor* atmosphere = m_SceneGraph.CreateActor("Atmosphere", m_InterfaceActor);
	atmosphere->SetRenderCallback([this](const Actor&, Renderer&)
	{
		DrawAtmosphere();
	});

	m_HudActor = m_SceneGraph.CreateActor("Hud", m_InterfaceActor);
	m_HudActor->SetRenderCallback([this](const Actor&, Renderer&)
	{
		DrawHud();
	});

	m_DialogueActor = m_SceneGraph.CreateActor("Dialogue", m_InterfaceActor);
	m_DialogueActor->SetRenderCallback([this](const Actor&, Renderer&)
	{
		DrawDialogue();
	});

	m_CluesActor = m_SceneGraph.CreateActor("Clues", m_InterfaceActor);
	m_CluesActor->SetRenderCallback([this](const Actor&, Renderer&)
	{
		DrawClueList();
	});

	m_IntroActor = m_SceneGraph.CreateActor("Intro", m_InterfaceActor);
	m_IntroActor->SetRenderCallback([this](const Actor&, Renderer&)
	{
		DrawIntro();
	});

	m_EndingActor = m_SceneGraph.CreateActor("Ending", m_InterfaceActor);
	m_EndingActor->SetRenderCallback([this](const Actor&, Renderer&)
	{
		DrawEnding();
	});

	Actor* skipHint = m_SceneGraph.CreateActor("TutorialSkipHint", m_InterfaceActor);
	skipHint->SetActive(false);
	skipHint->SetRenderCallback([this](const Actor&, Renderer&)
	{
		DrawTutorialSkipHint();
	});

	m_InterfaceActor->SetUpdateCallback([this](Actor&, float deltaSeconds)
	{
		if (m_ToastTimer > 0.0f)
		{
			m_ToastTimer -= deltaSeconds;
		}
	});
}

void Game::UpdateSceneState()
{
	const bool worldVisible = m_State != STATE_ENDING || m_Fade < 0.999f;
	const bool worldActive = m_State != STATE_INTRO && m_State != STATE_ENDING;
	m_EnvironmentActor->SetVisible(worldVisible);
	m_CharactersActor->SetVisible(worldVisible);
	m_CharactersActor->SetActive(worldActive);
	m_EffectsActor->SetVisible(worldVisible);
	m_EffectsActor->SetActive(worldActive);
	m_PlayerActor->SetActive(m_State == STATE_PLAY);
	m_HudActor->SetVisible(worldActive);
	m_DialogueActor->SetVisible(m_State == STATE_DIALOGUE);
	m_CluesActor->SetVisible(m_State == STATE_CLUES);
	m_IntroActor->SetVisible(m_State == STATE_INTRO);
	m_EndingActor->SetVisible(m_State == STATE_ENDING);
}

// ---------------------------------------------------------------- simulation

void Game::Update(float deltaSeconds)
{
	m_Time += deltaSeconds;
	m_StateTime += deltaSeconds;

	if (m_State == STATE_INTRO)
	{
		m_Fade = Clamp(1.0f - m_StateTime / 1.8f, 0.0f, 1.0f);

		if (m_StateTime > 4.2f)
		{
			m_State = STATE_PLAY;
			m_StateTime = 0.0f;
		}
	}
	else if (m_State == STATE_ENDING)
	{
		m_Fade = Clamp(m_StateTime / 1.6f, 0.0f, 1.0f);
	}

	if (m_State != STATE_PLAY)
	{
		m_Moving = false;
	}

	UpdateSceneState();
	m_SceneGraph.Update(deltaSeconds);
}

void Game::UpdateCamera(float deltaSeconds)
{
	const ActorPosition player = m_PlayerActor->GetWorldPosition();
	const float follow = Clamp(deltaSeconds * 6.0f, 0.0f, 1.0f);
	m_CameraX += (player.x - m_CameraX) * follow;
	m_CameraY += (player.y - m_CameraY) * follow;
	m_Lighting.SetViewer(m_CameraX, m_CameraY);
	m_Lighting.SetTime(m_Time);
	UpdateLighting();
}

void Game::UpdateLighting()
{
	const ActorPosition player = m_PlayerActor->GetWorldPosition();
	const float playerIntensity = m_PlayerActor->IsVisibleInHierarchy() ? 1.15f : 0.0f;
	const std::vector<Prop>& props = m_World.GetProps();

	m_Lighting.Clear();

	// Keep slot 0 reserved even while hidden; ember emitters start at slot 1.
	m_PlayerLightIndex = m_Lighting.AddLight(player.x, player.y - 0.15f,
		4.2f, playerIntensity, COL_FLAME, true);

	for (size_t i = 0; i < props.size(); ++i)
	{
		if (props[i].type != PROP_LANTERN || !m_PropActors[i]->IsVisibleInHierarchy())
		{
			continue;
		}

		const ActorPosition position = m_PropActors[i]->GetWorldPosition();

		m_Lighting.AddLight(position.x, position.y, 5.0f, 1.45f, COL_FLAME, true);
	}

	for (size_t i = 0; i < m_Npcs.size(); ++i)
	{
		const Npc& npc = m_Npcs[i];

		if (npc.id != "smith" || !npc.actor->IsVisibleInHierarchy())
		{
			continue;
		}

		const ActorPosition position = npc.actor->GetWorldPosition();

		m_Lighting.AddLight(position.x + 0.6f, position.y + 0.3f, 3.4f, 1.6f,
			RGBA(1.0f, 0.45f, 0.20f), true);
	}
}

void Game::UpdatePlayer(float deltaSeconds)
{
	// WASD is screen-relative, not world-relative: W walks straight up the
	// screen, which in world space is the -x -y diagonal.
	float dx = 0.0f;
	float dy = 0.0f;

	if (m_MoveKey[MOVE_UP])
	{
		dx -= 1.0f;
		dy -= 1.0f;
	}

	if (m_MoveKey[MOVE_DOWN])
	{
		dx += 1.0f;
		dy += 1.0f;
	}

	if (m_MoveKey[MOVE_LEFT])
	{
		dx -= 1.0f;
		dy += 1.0f;
	}

	if (m_MoveKey[MOVE_RIGHT])
	{
		dx += 1.0f;
		dy -= 1.0f;
	}

	const float lengthSq = dx * dx + dy * dy;
	m_Moving = lengthSq > 0.0001f;

	if (!m_Moving)
	{
		m_PlayerStride *= 0.85f;
		return;
	}

	const float length = sqrtf(lengthSq);
	dx /= length;
	dy /= length;

	const float speed = m_Running ? RUN_SPEED : WALK_SPEED;
	const float stepX = dx * speed * deltaSeconds;
	const float stepY = dy * speed * deltaSeconds;
	const ActorPosition worldPosition = m_PlayerActor->GetWorldPosition();
	ActorPosition position = m_PlayerActor->GetPosition();
	float worldX = worldPosition.x;
	float worldY = worldPosition.y;

	// Axis-separated so that sliding along a wall still works.
	if (!m_World.IsBlocked(worldX + stepX, worldY, PLAYER_RADIUS))
	{
		worldX += stepX;
		position.x += stepX;
	}

	if (!m_World.IsBlocked(worldX, worldY + stepY, PLAYER_RADIUS))
	{
		position.y += stepY;
	}

	m_PlayerActor->SetPosition(position.x, position.y, position.z);
	m_PlayerStride += deltaSeconds * (m_Running ? 13.0f : 9.0f);
}

void Game::UpdateInteractionTarget()
{
	m_TargetKind = INTERACT_NONE;
	m_TargetIndex = -1;

	const ActorPosition player = m_PlayerActor->GetWorldPosition();
	float bestDistance = 1e9f;

	for (size_t i = 0; i < m_Npcs.size(); ++i)
	{
		if (!m_Npcs[i].actor->IsVisibleInHierarchy())
		{
			continue;
		}

		const ActorPosition position = m_Npcs[i].actor->GetWorldPosition();
		const float dx = position.x - player.x;
		const float dy = position.y - player.y;
		const float d = sqrtf(dx * dx + dy * dy);

		if (d < 1.6f && d < bestDistance)
		{
			bestDistance = d;
			m_TargetKind = INTERACT_NPC;
			m_TargetIndex = (int)i;
			m_TargetX = position.x;
			m_TargetY = position.y;
		}
	}

	// A nearby villager must remain selectable beside a cart or shrine.
	if (m_TargetKind == INTERACT_NPC)
	{
		return;
	}

	const std::vector<Prop>& props = m_World.GetProps();

	for (size_t i = 0; i < props.size(); ++i)
	{
		const Prop& prop = props[i];

		if (prop.type == PROP_LANTERN || !m_PropActors[i]->IsVisibleInHierarchy())
		{
			continue;
		}

		const ActorPosition position = m_PropActors[i]->GetWorldPosition();
		const float centerX = position.x + prop.sizeX * 0.5f;
		const float centerY = position.y + prop.sizeY * 0.5f;
		const float reach = 1.15f + (prop.sizeX > prop.sizeY ? prop.sizeX : prop.sizeY) * 0.5f;

		const float dx = centerX - player.x;
		const float dy = centerY - player.y;
		const float d = sqrtf(dx * dx + dy * dy);

		// Keep the existing relative ranking among nearby scenery props.
		const float score = d * 1.6f;

		if (d < reach && score < bestDistance)
		{
			bestDistance = score;
			m_TargetKind = INTERACT_PROP;
			m_TargetIndex = (int)i;
			m_TargetX = centerX;
			m_TargetY = centerY;
		}
	}
}

void Game::UpdateMote(size_t index, float deltaSeconds)
{
	Mote& mote = m_Motes[index];
	mote.life -= deltaSeconds;

	if (mote.life <= 0.0f)
	{
		mote.actor->SetVisible(false);

		if (m_Lighting.GetLightCount() < 2)
		{
			return;
		}

		// Light 0 belongs to the player; embers choose nearby fixed lanterns.
		const ActorPosition player = m_PlayerActor->GetWorldPosition();
		int best = 1;
		float bestDistance = 1e9f;

		for (int k = 1; k < m_Lighting.GetLightCount(); ++k)
		{
			const float dx = m_Lighting.GetLight(k).x - player.x;
			const float dy = m_Lighting.GetLight(k).y - player.y;
			const float distance = dx * dx + dy * dy;
			const unsigned int jitter = HashInt((int)(index * 31 + (size_t)k), (int)(m_Time * 3.0f));
			const float noisy = distance + (float)(jitter % 100) * 0.35f;

			if (noisy < bestDistance)
			{
				bestDistance = noisy;
				best = k;
			}
		}

		const unsigned int variation = HashInt((int)index, (int)(m_Time * 60.0f));
		const Actor* parent = mote.actor->GetParent();
		const ActorPosition origin = parent != NULL
			? parent->GetWorldPosition() : ActorPosition{ 0.0f, 0.0f, 0.0f };
		mote.actor->SetPosition(m_Lighting.GetLight(best).x
			+ ((float)(variation % 100) * 0.01f - 0.5f) * 0.7f - origin.x,
			m_Lighting.GetLight(best).y + ((float)((variation >> 7) % 100) * 0.01f - 0.5f) * 0.7f - origin.y,
			0.85f + (float)((variation >> 14) % 40) * 0.004f - origin.z);
		mote.riseSpeed = 0.22f + (float)((variation >> 20) % 40) * 0.006f;
		mote.drift = ((float)((variation >> 3) % 100) * 0.01f - 0.5f) * 0.18f;
		mote.maxLife = 2.4f + (float)((variation >> 11) % 30) * 0.06f;
		mote.life = mote.maxLife;
		mote.size = 1.4f + (float)((variation >> 17) % 20) * 0.07f;
		mote.actor->SetVisible(true);
		return;
	}

	const ActorPosition position = mote.actor->GetPosition();
	mote.actor->SetPosition(position.x + mote.drift * deltaSeconds,
		position.y, position.z + mote.riseSpeed * deltaSeconds);
}

void Game::UpdateMist(size_t index, float deltaSeconds)
{
	Mote& wisp = m_Mist[index];
	ActorPosition position = wisp.actor->GetPosition();
	position.x += wisp.drift * deltaSeconds;

	if (position.x > (float)m_World.GetWidth() + 4.0f)
	{
		position.x = -4.0f;
	}

	wisp.actor->SetPosition(position.x, position.y, position.z);
}

// ---------------------------------------------------------------- interaction

int Game::ClueBitForNpc(const std::string& id) const
{
	if (id == "smith")
	{
		return CLUE_SMITH;
	}

	if (id == "fisher")
	{
		return CLUE_FISHER;
	}

	if (id == "child")
	{
		return CLUE_CHILD;
	}

	return 0;
}

int Game::ClueCount() const
{
	int count = 0;

	for (int bit = CLUE_SMITH; bit <= CLUE_CHILD; bit <<= 1)
	{
		if ((m_ClueMask & bit) != 0)
		{
			++count;
		}
	}

	return count;
}

std::string Game::DialogueKeyForNpc(const Npc& npc) const
{
	if (npc.id == "elder")
	{
		if (!m_MetElder)
		{
			return "elder_intro";
		}

		if (m_ClueMask != CLUE_ALL)
		{
			return "elder_wait";
		}

		return "elder_final";
	}

	const int bit = ClueBitForNpc(npc.id);

	if (bit != 0)
	{
		if (!m_MetElder)
		{
			return npc.id + "_before";
		}

		if ((m_ClueMask & bit) == 0)
		{
			return npc.id + "_clue";
		}

		return npc.id + "_after";
	}

	return npc.id;
}

std::string Game::DialogueKeyForProp(const Prop& prop) const
{
	switch (prop.type)
	{
	case PROP_WELL:   return "prop_well";
	case PROP_STONE:  return "prop_stone";
	case PROP_TORII:  return "prop_torii";
	case PROP_RUIN:   return "prop_ruin";
	case PROP_DOCK:   return "prop_dock";
	case PROP_CART:   return "prop_cart";
	case PROP_SHRINE: return "prop_shrine";
	case PROP_HOUSE:  return "prop_house";
	default:          return std::string();
	}
}

void Game::TryInteract()
{
	if (m_TargetKind == INTERACT_NPC && m_TargetIndex >= 0)
	{
		OpenDialogue(DialogueKeyForNpc(m_Npcs[m_TargetIndex]));
		return;
	}

	if (m_TargetKind == INTERACT_PROP && m_TargetIndex >= 0)
	{
		const std::string key = DialogueKeyForProp(m_World.GetProps()[m_TargetIndex]);

		if (!key.empty())
		{
			OpenDialogue(key);
		}
	}
}

void Game::OpenDialogue(const std::string& key)
{
	const DialogueBlock* block = m_Dialogue->Find(key);

	if (block == NULL || block->lines.empty())
	{
		std::cout << "dialogue key missing: " << key << "\n";
		return;
	}

	m_ActiveBlock = block;
	m_ActiveKey = key;
	m_PageStart = 0;
	m_PageEnd = 0;
	m_State = STATE_DIALOGUE;
	m_StateTime = 0.0f;

	AdvanceDialogue();
}

void Game::AdvanceDialogue()
{
	if (m_ActiveBlock == NULL)
	{
		CloseDialogue();
		return;
	}

	m_PageStart = m_PageEnd;

	if (m_PageStart >= m_ActiveBlock->lines.size())
	{
		CloseDialogue();
		return;
	}

	// A page is the run of consecutive lines that share a speaker, capped at
	// three so the box never overflows.
	const std::wstring& speaker = m_ActiveBlock->lines[m_PageStart].speaker;
	m_PageEnd = m_PageStart + 1;

	while (m_PageEnd < m_ActiveBlock->lines.size()
		&& m_ActiveBlock->lines[m_PageEnd].speaker == speaker
		&& (m_PageEnd - m_PageStart) < 3)
	{
		++m_PageEnd;
	}
}

void Game::CloseDialogue()
{
	const std::string key = m_ActiveKey;

	m_ActiveBlock = NULL;
	m_ActiveKey.clear();
	m_PageStart = 0;
	m_PageEnd = 0;
	m_State = STATE_PLAY;
	m_StateTime = 0.0f;

	// Quest effects land when the conversation ends, not when it opens, so the
	// player always reads the line that earned the clue.
	if (key == "elder_intro")
	{
		m_MetElder = true;
		return;
	}

	if (key == "elder_final")
	{
		m_State = STATE_ENDING;
		m_StateTime = 0.0f;
		return;
	}

	if (key == "smith_clue")
	{
		m_ClueMask |= CLUE_SMITH;
		ShowToast(m_Dialogue->Line("ui_clue_gained"));
	}

	if (key == "fisher_clue")
	{
		m_ClueMask |= CLUE_FISHER;
		ShowToast(m_Dialogue->Line("ui_clue_gained"));
	}

	if (key == "child_clue")
	{
		m_ClueMask |= CLUE_CHILD;
		ShowToast(m_Dialogue->Line("ui_clue_gained"));
	}
}

void Game::ShowToast(const std::wstring& text)
{
	m_ToastText = text;
	m_ToastTimer = 2.6f;
}

bool Game::ConsumeNextLevelRequest()
{
	const bool requested = m_WantsNextLevel;
	m_WantsNextLevel = false;

	return requested;
}

void Game::OnKey(unsigned char key, bool down, bool shift)
{
	m_Running = shift;

	// Normalise so that holding Shift does not leave a movement key stuck down.
	if (key >= 'A' && key <= 'Z')
	{
		key = (unsigned char)(key - 'A' + 'a');
	}

	if (key == 27)			// Esc
	{
		if (down)
		{
			m_WantsExit = true;
		}

		return;
	}

	switch (key)
	{
	case 'w': m_MoveKey[0] = down; break;
	case 'a': m_MoveKey[1] = down; break;
	case 's': m_MoveKey[2] = down; break;
	case 'd': m_MoveKey[3] = down; break;
	default: break;
	}

	if (!down)
	{
		return;
	}

	// Check before state-specific input so dialogue and clue panels can be skipped too.
	if (key == TUTORIAL_SKIP_KEY)
	{
		m_WantsNextLevel = true;
		return;
	}

	if (m_State == STATE_INTRO)
	{
		if (key == 'e' || key == ' ' || key == 13)
		{
			m_State = STATE_PLAY;
			m_StateTime = 0.0f;
			m_Fade = 0.0f;
		}

		return;
	}

	if (m_State == STATE_ENDING)
	{
		if (key == 'e' || key == ' ' || key == 13)
		{
			m_WantsNextLevel = true;
		}

		return;
	}

	if (m_State == STATE_DIALOGUE)
	{
		if (key == 'e' || key == ' ' || key == 13)
		{
			AdvanceDialogue();
		}

		return;
	}

	if (m_State == STATE_CLUES)
	{
		if (key == '\t' || key == 'e' || key == ' ')
		{
			m_State = STATE_PLAY;
		}

		return;
	}

	if (m_State == STATE_PLAY)
	{
		if (key == 'e' || key == ' ' || key == 13)
		{
			TryInteract();
		}
		else if (key == '\t')
		{
			m_State = STATE_CLUES;
		}
	}
}

// ---------------------------------------------------------------- lighting

float Game::FogFactor(float worldX, float worldY) const
{
	return m_Lighting.FogFactor(worldX, worldY);
}

Color Game::Lit(const Color& base, float worldX, float worldY) const
{
	return m_Lighting.Apply(base, worldX, worldY);
}

void Game::PushShadow(float worldX, float worldY, float radiusX, float radiusY, float depth, float worldZ)
{
	float sx, sy;
	m_Renderer->WorldToScreen(worldX, worldY, worldZ, &sx, &sy);

	Color shadow = RGBA(0.0f, 0.0f, 0.0f, 0.30f * (1.0f - FogFactor(worldX, worldY)));
	m_Renderer->PushEllipse(sx, sy, radiusX, radiusY, shadow, depth, 12);
}

// ---------------------------------------------------------------- world draw

void Game::DrawGround(const Actor& actor, TileType tile, unsigned int variation)
{
	const ActorPosition position = actor.GetWorldPosition();
	float screenX = 0.0f;
	float screenY = 0.0f;
	m_Renderer->WorldToScreen(position.x, position.y, position.z, &screenX, &screenY);

	Color base;

	switch (tile)
	{
	case TILE_TALLGRASS: base = COL_TALLGRASS; break;
	case TILE_DIRT:      base = COL_DIRT; break;
	case TILE_STONE:     base = COL_STONE; break;
	case TILE_SAND:      base = COL_SAND; break;
	case TILE_WATER:     base = COL_WATER; break;
	default:            base = COL_GRASS; break;
	}

	if (tile == TILE_WATER)
	{
		const float x = position.x - 0.5f;
		const float y = position.y - 0.5f;
		const float ripple = sinf(m_Time * 1.3f + x * 0.62f + y * 0.44f) * 0.035f
			+ sinf(m_Time * 0.7f + x * 0.21f - y * 0.33f) * 0.02f;
		base.g += ripple;
		base.b += ripple * 1.4f;
	}
	else
	{
		const float shade = 1.0f + ((float)(variation % 100) * 0.01f - 0.5f) * 0.09f;
		base.r *= shade;
		base.g *= shade;
		base.b *= shade;
	}

	const Color lit = Lit(base, position.x, position.y);
	m_Renderer->PushDiamond(screenX, screenY, Renderer::TileHalfWidth(), Renderer::TileHalfHeight(),
		lit, -100000.0f + (position.x + position.y - 1.0f) * 0.01f);
}

void Game::DrawScenery(const Actor& actor, TileType tile, int variant)
{
	const ActorPosition position = actor.GetWorldPosition();

	switch (tile)
	{
	case TILE_TREE: DrawTree(position.x, position.y, variant, position.z); break;
	case TILE_BUSH: DrawBush(position.x, position.y, position.z); break;
	case TILE_ROCK: DrawRock(position.x, position.y, position.z); break;
	default: break;
	}
}

void Game::DrawProp(const Actor& actor, const Prop& definition)
{
	const ActorPosition position = actor.GetWorldPosition();
	Prop prop = definition;

	prop.x = position.x;
	prop.y = position.y;

	switch (prop.type)
	{
	case PROP_HOUSE:   DrawBuilding(prop, COL_PLASTER, COL_THATCH, true, position.z); break;
	case PROP_SHRINE:  DrawBuilding(prop, RGBA(0.55f, 0.47f, 0.42f), RGBA(0.26f, 0.23f, 0.22f), false, position.z); break;
	case PROP_RUIN:    DrawRuin(prop, position.z); break;
	case PROP_TORII:   DrawTorii(prop, position.z); break;
	case PROP_LANTERN: DrawLantern(prop, position.z); break;
	case PROP_WELL:    DrawWell(prop, position.z); break;
	case PROP_CART:    DrawCart(prop, position.z); break;
	case PROP_DOCK:    DrawDock(prop, position.z); break;
	case PROP_STONE:   DrawStone(prop, position.z); break;
	default: break;
	}
}

void Game::DrawNpc(const Npc& npc)
{
	static const Color ROBES[8] =
	{
		{ 0.34f, 0.30f, 0.25f, 1.0f },
		{ 0.26f, 0.29f, 0.34f, 1.0f },
		{ 0.31f, 0.34f, 0.27f, 1.0f },
		{ 0.39f, 0.34f, 0.29f, 1.0f },
		{ 0.24f, 0.25f, 0.29f, 1.0f },
		{ 0.36f, 0.28f, 0.24f, 1.0f },
		{ 0.28f, 0.33f, 0.31f, 1.0f },
		{ 0.41f, 0.38f, 0.32f, 1.0f }
	};
	const ActorPosition position = npc.actor->GetWorldPosition();
	const float bob = sinf(m_Time * 1.6f + npc.phase) * 1.6f;

	if (npc.id == "ghost")
	{
		Color pale = RGBA(0.62f, 0.72f, 0.78f, 0.52f);
		pale = Mix(pale, COL_FOG, FogFactor(position.x, position.y) * 0.6f);
		const float hover = sinf(m_Time * 1.1f + npc.phase) * 4.0f - 6.0f;
		DrawPerson(position.x, position.y, 1.30f, pale, RGBA(0.75f, 0.83f, 0.88f, 0.45f),
			false, false, false, hover, position.z);
		return;
	}

	const bool hat = npc.id == "farmer" || npc.id == "fisher";
	const bool small = npc.id == "child";
	DrawPerson(position.x, position.y, small ? 0.92f : 1.32f, ROBES[npc.look],
		Mix(ROBES[npc.look], COL_SHIRO, 0.35f), hat, false, false, bob, position.z);
}

void Game::DrawPlayer(const Actor& actor)
{
	const ActorPosition position = actor.GetWorldPosition();
	const float stride = m_Moving ? fabsf(sinf(m_PlayerStride)) * 2.6f : 0.0f;

	DrawPerson(position.x, position.y, 1.38f, RGBA(0.20f, 0.24f, 0.31f),
		COL_SHU, false, true, true, -stride, position.z);
}

// Vegetation and boulders come from the cached library. The shapes used to be
// rebuilt from triangles every frame here; now this only picks a model and
// shades it once.
void Game::DrawTree(float worldX, float worldY, int variant, float worldZ)
{
	static const char* const CEDAR_NAMES[3] =
	{
		"tree_cedar_small",
		"tree_cedar_mid",
		"tree_cedar_tall"
	};

	const char* name = (variant == 4) ? "tree_broadleaf" : CEDAR_NAMES[variant % 3];
	const Model* model = m_Models->Find(name);

	if (model == NULL)
	{
		return;
	}

	float screenX = 0.0f;
	float screenY = 0.0f;
	m_Renderer->WorldToScreen(worldX, worldY, worldZ, &screenX, &screenY);

	const ShadeParams shade = m_Lighting.Shade(worldX, worldY, RGBA(1.0f, 1.0f, 1.0f));

	DrawModel(m_Renderer, *model, screenX, screenY, 1.0f, worldX + worldY, shade);
}

void Game::DrawBush(float worldX, float worldY, float worldZ)
{
	const Model* model = m_Models->Find("bush");

	if (model == NULL)
	{
		return;
	}

	float screenX = 0.0f;
	float screenY = 0.0f;
	m_Renderer->WorldToScreen(worldX, worldY, worldZ, &screenX, &screenY);

	const ShadeParams shade = m_Lighting.Shade(worldX, worldY, RGBA(1.0f, 1.0f, 1.0f));

	DrawModel(m_Renderer, *model, screenX, screenY, 1.0f, worldX + worldY, shade);
}

void Game::DrawRock(float worldX, float worldY, float worldZ)
{
	const Model* model = m_Models->Find("rock");

	if (model == NULL)
	{
		return;
	}

	float screenX = 0.0f;
	float screenY = 0.0f;
	m_Renderer->WorldToScreen(worldX, worldY, worldZ, &screenX, &screenY);

	const ShadeParams shade = m_Lighting.Shade(worldX, worldY, RGBA(1.0f, 1.0f, 1.0f));

	DrawModel(m_Renderer, *model, screenX, screenY, 1.0f, worldX + worldY, shade);
}

void Game::DrawBuilding(const Prop& prop, const Color& wall, const Color& roof, bool litWindow, float worldZ)
{
	Renderer* r = m_Renderer;

	const float x0 = prop.x;
	const float y0 = prop.y;
	const float x1 = prop.x + prop.sizeX;
	const float y1 = prop.y + prop.sizeY;
	const float yc = (y0 + y1) * 0.5f;

	const float centerX = (x0 + x1) * 0.5f;
	const float centerY = (y0 + y1) * 0.5f;
	const float depth = x1 + y1;

	const float eave = 0.38f;
	const float wallTop = 1.45f;
	const float eaveHeight = 1.30f;
	const float ridgeHeight = 3.35f;

	// Stone plinth the building stands on.
	PushFlatQuad(r, x0 - 0.16f, y0 - 0.16f, x1 + 0.16f, y1 + 0.16f, 0.02f,
		Lit(RGBA(0.30f, 0.29f, 0.27f), centerX, centerY), depth - 0.30f, worldZ);

	// The two walls that face the camera: the +x face and the +y face.
	const Color wallEast = Lit(Scale(wall, 0.78f), x1, centerY);
	const Color wallSouth = Lit(wall, centerX, y1);

	PushWallQuad(r, x1, y0, x1, y1, 0.0f, wallTop, wallEast, depth - 0.25f, worldZ);
	PushWallQuad(r, x1, y1, x0, y1, 0.0f, wallTop, wallSouth, depth - 0.24f, worldZ);

	// Timber posts at the corners of the visible faces.
	const Color timber = Lit(COL_TIMBER, centerX, centerY);
	PushWallQuad(r, x1, y0, x1, y0 + 0.12f, 0.0f, wallTop, timber, depth - 0.23f, worldZ);
	PushWallQuad(r, x1, y1 - 0.12f, x1, y1, 0.0f, wallTop, timber, depth - 0.23f, worldZ);
	PushWallQuad(r, x0, y1, x0 + 0.12f, y1, 0.0f, wallTop, timber, depth - 0.23f, worldZ);

	// Door on the south face.
	const float doorX = x0 + prop.sizeX * 0.42f;
	PushWallQuad(r, doorX + 0.62f, y1, doorX, y1, 0.0f, 0.92f,
		Lit(RGBA(0.16f, 0.14f, 0.13f), centerX, y1), depth - 0.22f, worldZ);

	if (litWindow)
	{
		// Shoji glow. Deliberately NOT run through Lit: the window is a source,
		// not a surface, and it is most of what makes the village feel inhabited.
		const float fog = FogFactor(x1, centerY);
		const Color glow = Mix(RGBA(0.98f, 0.70f, 0.36f), COL_FOG, fog * 0.7f);
		PushWallQuad(r, x1, y0 + 0.55f, x1, y0 + 1.55f, 0.55f, 1.20f, glow, depth - 0.21f, worldZ);

		const Color mullion = Lit(COL_TIMBER, x1, centerY);
		PushWallQuad(r, x1, y0 + 1.02f, x1, y0 + 1.08f, 0.55f, 1.20f, mullion, depth - 0.20f, worldZ);
	}

	// Roof. The ridge runs along x, which is the long axis of every building in
	// this level, so the visible planes are the south slope and the east gable.
	const Color roofSouth = Lit(roof, centerX, y1 + eave);
	const Color roofEast = Lit(Scale(roof, 0.74f), x1 + eave, centerY);

	const float gable[9] =
	{
		x1 + eave, y0 - eave, eaveHeight,
		x1 + eave, y1 + eave, eaveHeight,
		x1 + eave, yc,        ridgeHeight
	};
	PushPoly3(r, gable, 3, roofEast, depth - 0.10f, worldZ);

	const float slope[12] =
	{
		x0 - eave, y1 + eave, eaveHeight,
		x1 + eave, y1 + eave, eaveHeight,
		x1 + eave, yc,        ridgeHeight,
		x0 - eave, yc,        ridgeHeight
	};
	PushPoly3(r, slope, 4, roofSouth, depth - 0.05f, worldZ);

	// Ridge cap and eave line, the two edges that give thatch its silhouette.
	float ax, ay, bx, by;
	r->WorldToScreen(x0 - eave, yc, ridgeHeight + worldZ, &ax, &ay);
	r->WorldToScreen(x1 + eave, yc, ridgeHeight + worldZ, &bx, &by);
	r->PushLine(ax, ay, bx, by, 5.0f, Lit(Scale(roof, 1.35f), centerX, yc), depth - 0.04f);

	r->WorldToScreen(x0 - eave, y1 + eave, eaveHeight + worldZ, &ax, &ay);
	r->WorldToScreen(x1 + eave, y1 + eave, eaveHeight + worldZ, &bx, &by);
	r->PushLine(ax, ay, bx, by, 3.0f, Lit(Scale(roof, 0.55f), centerX, y1), depth - 0.03f);
}

void Game::DrawRuin(const Prop& prop, float worldZ)
{
	Renderer* r = m_Renderer;

	const float x0 = prop.x;
	const float y0 = prop.y;
	const float x1 = prop.x + prop.sizeX;
	const float y1 = prop.y + prop.sizeY;
	const float centerX = (x0 + x1) * 0.5f;
	const float centerY = (y0 + y1) * 0.5f;
	const float depth = x1 + y1;

	PushFlatQuad(r, x0 - 0.1f, y0 - 0.1f, x1 + 0.1f, y1 + 0.1f, 0.02f,
		Lit(RGBA(0.17f, 0.15f, 0.14f), centerX, centerY), depth - 0.30f, worldZ);

	// Wall stubs: the fire took the top of everything.
	const Color charred = Lit(RGBA(0.20f, 0.17f, 0.15f), centerX, centerY);
	PushWallQuad(r, x1, y0, x1, y0 + 1.1f, 0.0f, 0.75f, charred, depth - 0.25f, worldZ);
	PushWallQuad(r, x1, y1, x1 - 1.3f, y1, 0.0f, 0.55f, charred, depth - 0.24f, worldZ);

	// Standing posts, snapped at different heights.
	const float postHeights[4] = { 1.9f, 1.1f, 2.3f, 0.8f };
	const float postX[4] = { x0 + 0.2f, x1 - 0.2f, x0 + 1.6f, x1 - 1.1f };
	const float postY[4] = { y1 - 0.2f, y1 - 0.15f, y0 + 0.3f, y0 + 0.2f };

	for (int i = 0; i < 4; ++i)
	{
		PushWallQuad(r, postX[i], postY[i], postX[i] + 0.16f, postY[i],
			0.0f, postHeights[i], Lit(RGBA(0.13f, 0.11f, 0.10f), postX[i], postY[i]),
			postX[i] + postY[i] + 0.01f, worldZ);
	}

	// Rubble.
	for (int i = 0; i < 5; ++i)
	{
		const unsigned int h = HashInt((int)(prop.x * 10.0f) + i, (int)(prop.y * 10.0f) + i * 7);
		const float rx = x0 + (float)(h % 100) * 0.01f * prop.sizeX;
		const float ry = y0 + (float)((h >> 9) % 100) * 0.01f * prop.sizeY;

		float sx, sy;
		r->WorldToScreen(rx, ry, worldZ, &sx, &sy);
		r->PushEllipse(sx, sy, 7.0f, 3.5f, Lit(RGBA(0.16f, 0.14f, 0.13f), rx, ry), rx + ry, 8);
	}
}

void Game::DrawTorii(const Prop& prop, float worldZ)
{
	Renderer* r = m_Renderer;

	const float x = prop.x;
	const float y = prop.y;
	const float depth = x + y + 0.4f;

	const float span = 0.75f;
	const float postTop = 2.30f;
	const float nuki = 1.70f;			// lower tie beam
	const float kasagi = 2.15f;			// upper lintel

	// Weathered vermilion: the one place the red is allowed to be large.
	const Color pillar = Lit(Scale(COL_SHU, 0.85f), x, y);
	const Color beam = Lit(COL_SHU, x, y);

	PushShadow(x - span, y, 8.0f, 4.0f, depth - 0.02f, worldZ);
	PushShadow(x + span, y, 8.0f, 4.0f, depth - 0.02f, worldZ);

	PushWallQuad(r, x - span - 0.09f, y, x - span + 0.09f, y, 0.0f, postTop, pillar, depth, worldZ);
	PushWallQuad(r, x + span - 0.09f, y, x + span + 0.09f, y, 0.0f, postTop, pillar, depth, worldZ);

	PushWallQuad(r, x - span - 0.10f, y, x + span + 0.10f, y, nuki, nuki + 0.16f, beam, depth + 0.01f, worldZ);
	PushWallQuad(r, x - span - 0.34f, y, x + span + 0.34f, y, kasagi, kasagi + 0.22f, beam, depth + 0.02f, worldZ);
}

void Game::DrawLantern(const Prop& prop, float worldZ)
{
	Renderer* r = m_Renderer;

	const float x = prop.x;
	const float y = prop.y;
	const float depth = x + y;
	const float fog = FogFactor(x, y);

	PushShadow(x, y, 7.0f, 3.0f, depth - 0.02f, worldZ);

	// Post.
	PushWallQuad(r, x - 0.06f, y, x + 0.06f, y, 0.0f, 1.05f,
		Lit(RGBA(0.22f, 0.19f, 0.16f), x, y), depth, worldZ);

	// The halo goes down first so the paper box sits inside it.
	const float flicker = 1.0f + 0.10f * sinf(m_Time * 7.1f + x * 3.0f) + 0.06f * sinf(m_Time * 13.0f + y);
	float gx, gy;
	r->WorldToScreen(x, y, 1.28f + worldZ, &gx, &gy);

	for (int i = 3; i >= 1; --i)
	{
		const float radius = 16.0f * i * flicker;
		const Color halo = RGBA(1.0f, 0.66f, 0.30f, 0.13f / i * (1.0f - fog));
		r->PushEllipse(gx, gy, radius, radius * 0.78f, halo, depth - 0.01f, 16);
	}

	// Paper box, self-lit.
	const Color paper = Mix(RGBA(1.0f, 0.78f, 0.44f), COL_FOG, fog * 0.65f);
	PushWallQuad(r, x - 0.17f, y, x + 0.17f, y, 1.05f, 1.52f, paper, depth + 0.01f, worldZ);

	const Color cap = Lit(RGBA(0.20f, 0.17f, 0.15f), x, y);
	PushWallQuad(r, x - 0.21f, y, x + 0.21f, y, 1.52f, 1.60f, cap, depth + 0.02f, worldZ);
}

void Game::DrawWell(const Prop& prop, float worldZ)
{
	Renderer* r = m_Renderer;

	const float x = prop.x;
	const float y = prop.y;
	const float depth = x + y;

	float bx, by;
	r->WorldToScreen(x, y, worldZ, &bx, &by);

	PushShadow(x, y, 20.0f, 9.0f, depth - 0.03f, worldZ);

	const Color stone = Lit(RGBA(0.38f, 0.38f, 0.36f), x, y);
	const Color hole = Lit(RGBA(0.05f, 0.05f, 0.06f), x, y);

	r->PushEllipse(bx, by - 6.0f, 20.0f, 10.0f, stone, depth - 0.02f, 16);
	r->PushEllipse(bx, by - 8.0f, 13.0f, 6.5f, hole, depth - 0.01f, 16);

	const Color post = Lit(RGBA(0.24f, 0.20f, 0.17f), x, y);
	PushWallQuad(r, x - 0.5f, y, x - 0.38f, y, 0.25f, 1.5f, post, depth, worldZ);
	PushWallQuad(r, x + 0.38f, y, x + 0.5f, y, 0.25f, 1.5f, post, depth, worldZ);

	// Small roof over the shaft.
	float ax, ay, cx2, cy2, apexX, apexY;
	r->WorldToScreen(x - 0.72f, y, 1.5f + worldZ, &ax, &ay);
	r->WorldToScreen(x + 0.72f, y, 1.5f + worldZ, &cx2, &cy2);
	r->WorldToScreen(x, y, 2.0f + worldZ, &apexX, &apexY);

	const float roof[6] = { ax, ay, cx2, cy2, apexX, apexY };
	r->PushPolygon(roof, 3, Lit(COL_THATCH, x, y), depth + 0.02f);
}

void Game::DrawCart(const Prop& prop, float worldZ)
{
	Renderer* r = m_Renderer;

	const float x = prop.x;
	const float y = prop.y;
	const float depth = x + y;

	PushShadow(x, y, 17.0f, 7.0f, depth - 0.03f, worldZ);

	const Color wood = Lit(RGBA(0.33f, 0.27f, 0.21f), x, y);
	const Color darkWood = Lit(RGBA(0.22f, 0.18f, 0.14f), x, y);

	PushFlatQuad(r, x - 0.55f, y - 0.35f, x + 0.55f, y + 0.35f, 0.42f, wood, depth, worldZ);
	PushWallQuad(r, x + 0.55f, y - 0.35f, x + 0.55f, y + 0.35f, 0.30f, 0.55f, darkWood, depth + 0.01f, worldZ);
	PushWallQuad(r, x + 0.55f, y + 0.35f, x - 0.55f, y + 0.35f, 0.30f, 0.55f, darkWood, depth + 0.01f, worldZ);

	float wx1, wy1, wx2, wy2;
	r->WorldToScreen(x - 0.4f, y + 0.36f, 0.22f + worldZ, &wx1, &wy1);
	r->WorldToScreen(x + 0.4f, y + 0.36f, 0.22f + worldZ, &wx2, &wy2);
	r->PushEllipse(wx1, wy1, 8.0f, 8.0f, darkWood, depth + 0.02f, 12);
	r->PushEllipse(wx2, wy2, 8.0f, 8.0f, darkWood, depth + 0.02f, 12);
}

void Game::DrawDock(const Prop& prop, float worldZ)
{
	Renderer* r = m_Renderer;

	const float x = prop.x;
	const float y = prop.y;

	const Color plank = Lit(RGBA(0.30f, 0.25f, 0.20f), x, y);
	const Color darkPlank = Lit(RGBA(0.22f, 0.18f, 0.15f), x, y);

	for (int i = 0; i < 5; ++i)
	{
		const float px = x + (float)i * 0.52f;
		PushFlatQuad(r, px, y - 0.62f, px + 0.44f, y + 0.62f, 0.14f,
			(i % 2 == 0) ? plank : darkPlank, px + y, worldZ);
	}

	// Mooring post.
	PushWallQuad(r, x + 2.5f, y + 0.5f, x + 2.62f, y + 0.5f, 0.0f, 0.7f,
		Lit(RGBA(0.20f, 0.17f, 0.14f), x, y), x + 2.5f + y + 0.5f + 0.01f, worldZ);
}

void Game::DrawStone(const Prop& prop, float worldZ)
{
	Renderer* r = m_Renderer;

	const float x = prop.x;
	const float y = prop.y;
	const float depth = x + y;

	PushShadow(x, y, 9.0f, 4.0f, depth - 0.02f, worldZ);

	float bx, by;
	r->WorldToScreen(x, y, worldZ, &bx, &by);

	const Color stone = Lit(RGBA(0.40f, 0.41f, 0.38f), x, y);
	const float body[8] =
	{
		bx - 8.0f, by,
		bx + 8.0f, by,
		bx + 6.0f, by - 26.0f,
		bx - 6.0f, by - 26.0f
	};
	r->PushPolygon(body, 4, stone, depth);

	r->PushEllipse(bx, by - 27.0f, 6.5f, 4.5f, stone, depth + 0.01f, 10);

	// Someone still leaves offerings here.
	const Color moss = Lit(RGBA(0.26f, 0.34f, 0.24f), x, y);
	r->PushEllipse(bx - 2.0f, by - 12.0f, 4.0f, 5.0f, moss, depth + 0.02f, 8);
}

// One baked person, tinted per villager. The kasa, the sword and the carried
// lantern are separate models stacked on top, so a villager is a small
// composition rather than its own baked shape.
void Game::DrawPerson(float worldX, float worldY, float bodyHeight, const Color& robe,
	const Color& trim, bool hat, bool sword, bool lantern, float bobPixels, float worldZ)
{
	const Model* person = m_Models->Find("person");

	if (person == NULL)
	{
		return;
	}

	float screenX = 0.0f;
	float screenY = 0.0f;
	m_Renderer->WorldToScreen(worldX, worldY, worldZ, &screenX, &screenY);
	screenY += bobPixels;

	// Callers supply world-height units, while cached vertices are in pixels.
	// Convert first so an adult stays about 36 pixels tall rather than one.
	const float scale = bodyHeight * Renderer::HeightScale() / PERSON_MODEL_HEIGHT;
	const float depth = worldX + worldY;

	ShadeParams shade = m_Lighting.Shade(worldX, worldY, robe);

	if (robe.a < 0.99f)
	{
		// Spirits are not lit by lanterns; they carry their own pallor.
		shade.light = RGBA(1.0f, 1.0f, 1.0f);
	}

	DrawModel(m_Renderer, *person, screenX, screenY, scale, depth, shade);

	if (hat)
	{
		const Model* kasa = m_Models->Find("person_kasa");

		if (kasa != NULL)
		{
			DrawModel(m_Renderer, *kasa, screenX, screenY, scale, depth, shade);
		}
	}

	if (sword)
	{
		const Model* blade = m_Models->Find("person_sword");

		if (blade != NULL)
		{
			ShadeParams swordShade = shade;
			swordShade.tint = trim;

			DrawModel(m_Renderer, *blade, screenX, screenY, scale, depth, swordShade);
		}
	}

	if (lantern)
	{
		const Model* lamp = m_Models->Find("person_lantern");

		if (lamp != NULL)
		{
			DrawModel(m_Renderer, *lamp, screenX, screenY, scale, depth, shade);
		}
	}
}

void Game::DrawMist(const Mote& wisp, size_t index)
{
	const ActorPosition position = wisp.actor->GetWorldPosition();
	float screenX = 0.0f;
	float screenY = 0.0f;
	m_Renderer->WorldToScreen(position.x, position.y, position.z, &screenX, &screenY);

	const float breathe = 1.0f + 0.12f * sinf(m_Time * 0.4f + (float)index);
	const Color haze = RGBA(0.55f, 0.62f, 0.70f, 0.055f);
	m_Renderer->PushEllipse(screenX, screenY, 62.0f * wisp.size * 0.4f * breathe,
		20.0f * wisp.size * 0.4f, haze, position.x + position.y + 0.05f, 16);
}

void Game::DrawMote(const Mote& mote)
{
	const ActorPosition position = mote.actor->GetWorldPosition();
	float screenX = 0.0f;
	float screenY = 0.0f;
	m_Renderer->WorldToScreen(position.x, position.y, position.z, &screenX, &screenY);

	const float remaining = mote.life / mote.maxLife;
	const float alpha = remaining * remaining * 0.75f * (1.0f - FogFactor(position.x, position.y));
	const Color ember = RGBA(1.0f, 0.62f + 0.2f * remaining, 0.30f, alpha);
	m_Renderer->PushEllipse(screenX, screenY, mote.size, mote.size, ember,
		position.x + position.y + 1.0f, 6);
}

void Game::DrawAtmosphere()
{
	const float halfWidth = m_Renderer->GetWidth() * 0.5f;
	const float halfHeight = m_Renderer->GetHeight() * 0.5f;

	const float outer = sqrtf(halfWidth * halfWidth + halfHeight * halfHeight) * 1.06f;
	const float inner = outer * 0.50f;

	const int segments = 30;

	for (int i = 0; i < segments; ++i)
	{
		const float a0 = TWO_PI * (float)i / (float)segments;
		const float a1 = TWO_PI * (float)(i + 1) / (float)segments;

		const float xy[8] =
		{
			cosf(a0) * inner, sinf(a0) * inner,
			cosf(a1) * inner, sinf(a1) * inner,
			cosf(a1) * outer, sinf(a1) * outer,
			cosf(a0) * outer, sinf(a0) * outer
		};

		const Color clear = RGBA(0.02f, 0.03f, 0.045f, 0.0f);
		const Color edge = RGBA(0.02f, 0.03f, 0.045f, 0.80f);
		const Color colors[4] = { clear, clear, edge, edge };

		m_Renderer->PushPolygonShaded(xy, colors, 4, DEPTH_OVERLAY);
	}

	if (m_Fade > 0.001f)
	{
		m_Renderer->PushRect(-halfWidth, -halfHeight, halfWidth * 2.0f, halfHeight * 2.0f,
			RGBA(0.0f, 0.0f, 0.0f, m_Fade), DEPTH_OVERLAY + 1.0f);
	}
}

// ---------------------------------------------------------------- interface

void Game::DrawPanel(float x, float y, float width, float height, float alpha, float depth)
{
	m_Renderer->PushRect(x, y, width, height, RGBA(COL_INK.r, COL_INK.g, COL_INK.b, alpha), depth);
	m_Renderer->PushRect(x, y, width, 1.0f, RGBA(0.55f, 0.55f, 0.52f, alpha * 0.28f), depth + 0.1f);
}

void Game::DrawAccentPanel(float x, float y, float width, float height, const Color& accent, float depth)
{
	DrawPanel(x, y, width, height, 0.80f, depth);
	m_Renderer->PushRect(x, y, 2.0f, height, accent, depth + 0.2f);
}

void Game::DrawQuestGuide()
{
	if (m_State != STATE_PLAY)
	{
		return;
	}

	const Npc* target = NULL;
	const ActorPosition player = m_PlayerActor->GetWorldPosition();
	float nearestDistance = 1e9f;
	const bool needsElder = !m_MetElder || m_ClueMask == CLUE_ALL;

	for (size_t i = 0; i < m_Npcs.size(); ++i)
	{
		const Npc& npc = m_Npcs[i];
		const int bit = ClueBitForNpc(npc.id);
		const bool relevant = needsElder ? npc.id == "elder"
			: bit != 0 && (m_ClueMask & bit) == 0;

		if (!relevant || !npc.actor->IsVisibleInHierarchy())
		{
			continue;
		}

		const ActorPosition position = npc.actor->GetWorldPosition();
		const float dx = position.x - player.x;
		const float dy = position.y - player.y;
		const float distance = dx * dx + dy * dy;

		if (distance < nearestDistance)
		{
			nearestDistance = distance;
			target = &npc;
		}
	}

	if (target == NULL)
	{
		return;
	}

	Renderer* r = m_Renderer;
	const float halfWidth = r->GetWidth() * 0.5f;
	const float halfHeight = r->GetHeight() * 0.5f;
	const Color gold = RGBA(0.95f, 0.80f, 0.45f, 0.95f);
	float screenX = 0.0f;
	float screenY = 0.0f;
	const ActorPosition position = target->actor->GetWorldPosition();
	r->WorldToScreen(position.x, position.y, position.z + 2.4f, &screenX, &screenY);

	// Keep the destination visible even when its villager is off screen.
	const float limitX = fmaxf(1.0f, halfWidth - GUIDE_EDGE_MARGIN);
	const float limitY = fmaxf(1.0f, halfHeight - GUIDE_VERTICAL_MARGIN);
	const float extent = fmaxf(fabsf(screenX) / limitX, fabsf(screenY) / limitY);

	if (extent > 1.0f)
	{
		screenX /= extent;
		screenY /= extent;
	}

	r->PushDiamond(screenX, screenY, 8.0f, 11.0f, gold, DEPTH_PANEL_TOP);
	r->PushDiamond(screenX, screenY, 3.0f, 5.0f, COL_INK, DEPTH_PANEL_TOP + 0.1f);

	const float panelX = halfWidth - GUIDE_PANEL_WIDTH - 22.0f;
	const float panelY = -halfHeight + 20.0f;
	DrawAccentPanel(panelX, panelY, GUIDE_PANEL_WIDTH, GUIDE_PANEL_HEIGHT, gold, DEPTH_PANEL);
	r->PushText(m_Dialogue->Line("ui_guide_title"), panelX + 14.0f, panelY + 10.0f,
		13, FONT_UI, false, RGBA(0.72f, 0.72f, 0.68f), ALIGN_LEFT);
	r->PushText(m_Dialogue->Line("ui_guide_" + target->id), panelX + 14.0f, panelY + 31.0f,
		16, FONT_UI, false, gold, ALIGN_LEFT);
}

void Game::DrawHud()
{
	Renderer* r = m_Renderer;

	const float left = -r->GetWidth() * 0.5f;
	const float top = -r->GetHeight() * 0.5f;
	const float bottom = r->GetHeight() * 0.5f;

	DrawQuestGuide();

	// --- quest panel, top left ---
	const std::wstring questTitle = m_Dialogue->Line("ui_quest_title");

	std::wstring objective;

	if (!m_MetElder)
	{
		objective = m_Dialogue->Line("ui_obj_elder");
	}
	else if (m_ClueMask != CLUE_ALL)
	{
		objective = m_Dialogue->Line("ui_obj_collect");
	}
	else
	{
		objective = m_Dialogue->Line("ui_obj_return");
	}

	const float panelX = left + 22.0f;
	const float panelY = top + 20.0f;
	const float panelW = 292.0f;
	const float panelH = m_MetElder ? 98.0f : 74.0f;

	DrawAccentPanel(panelX, panelY, panelW, panelH, COL_SHU, DEPTH_PANEL);

	r->PushText(questTitle, panelX + 16.0f, panelY + 12.0f, 15, FONT_SERIF, true,
		RGBA(0.80f, 0.66f, 0.52f, 0.95f), ALIGN_LEFT);
	r->PushText(objective, panelX + 16.0f, panelY + 36.0f, 17, FONT_UI, false,
		RGBA(0.90f, 0.90f, 0.87f, 0.95f), ALIGN_LEFT);

	if (m_MetElder)
	{
		// Three clue pips: filled once the villager has been asked.
		const float pipY = panelY + 68.0f;

		for (int i = 0; i < CLUE_TOTAL; ++i)
		{
			const bool have = (m_ClueMask & CLUE_BITS[i]) != 0;
			const float pipX = panelX + 18.0f + i * 26.0f;
			r->PushDiamond(pipX, pipY + 6.0f, 7.0f, 7.0f,
				have ? COL_SHU : RGBA(0.32f, 0.33f, 0.32f, 0.85f), DEPTH_PANEL_TOP);
		}

		wchar_t counter[32];
		swprintf_s(counter, 32, L"%d / %d", ClueCount(), CLUE_TOTAL);
		r->PushText(counter, panelX + panelW - 16.0f, pipY - 3.0f, 15, FONT_UI, false,
			RGBA(0.70f, 0.70f, 0.67f, 0.85f), ALIGN_RIGHT);
	}

	// --- controls, bottom left ---
	r->PushText(m_Dialogue->Line("ui_controls"), left + 22.0f, bottom - 34.0f, 14, FONT_UI, false,
		RGBA(0.62f, 0.64f, 0.62f, 0.65f), ALIGN_LEFT);

	// --- interaction prompt ---
	if (m_State == STATE_PLAY && m_TargetKind != INTERACT_NONE)
	{
		float sx, sy;
		r->WorldToScreen(m_TargetX, m_TargetY, 1.75f, &sx, &sy);

		const float pulse = sinf(m_Time * 4.0f) * 2.5f;
		r->PushDiamond(sx, sy + pulse, 6.0f, 8.0f, RGBA(0.95f, 0.80f, 0.45f, 0.9f), DEPTH_PANEL);

		const std::wstring verb = (m_TargetKind == INTERACT_NPC)
			? m_Dialogue->Line("ui_prompt_talk")
			: m_Dialogue->Line("ui_prompt_examine");

		const std::wstring prompt = L"[E]  " + verb;
		const float width = r->MeasureTextWidth(prompt, 16, FONT_UI, false);

		DrawPanel(-width * 0.5f - 14.0f, bottom - 92.0f, width + 28.0f, 32.0f, 0.72f, DEPTH_PANEL);
		r->PushText(prompt, 0.0f, bottom - 85.0f, 16, FONT_UI, false,
			RGBA(0.93f, 0.90f, 0.84f, 0.95f), ALIGN_CENTER);
	}

	// --- clue toast ---
	if (m_ToastTimer > 0.0f && !m_ToastText.empty())
	{
		const float alpha = Clamp(m_ToastTimer / 0.6f, 0.0f, 1.0f);
		const float width = r->MeasureTextWidth(m_ToastText, 18, FONT_SERIF, true);

		DrawPanel(-width * 0.5f - 20.0f, top + 132.0f, width + 40.0f, 38.0f, 0.80f * alpha, DEPTH_PANEL);
		m_Renderer->PushRect(-width * 0.5f - 20.0f, top + 132.0f, 2.0f, 38.0f,
			RGBA(COL_SHU.r, COL_SHU.g, COL_SHU.b, alpha), DEPTH_PANEL_TOP);
		r->PushText(m_ToastText, 0.0f, top + 140.0f, 18, FONT_SERIF, true,
			RGBA(0.93f, 0.86f, 0.72f, alpha), ALIGN_CENTER);
	}
}

void Game::DrawTutorialSkipHint()
{
	const float right = m_Renderer->GetWidth() * 0.5f;
	const float bottom = m_Renderer->GetHeight() * 0.5f;

	m_Renderer->PushText(m_Dialogue->Line("ui_tutorial_skip"), right - SKIP_HINT_SIDE_MARGIN,
		bottom - SKIP_HINT_BOTTOM_MARGIN, SKIP_HINT_FONT_SIZE, FONT_UI, false,
		RGBA(0.90f, 0.82f, 0.65f, 0.95f), ALIGN_RIGHT);
}

void Game::DrawDialogue()
{
	if (m_ActiveBlock == NULL || m_PageStart >= m_ActiveBlock->lines.size())
	{
		return;
	}

	Renderer* r = m_Renderer;

	const float width = (float)r->GetWidth();
	const float height = (float)r->GetHeight();

	const float boxWidth = (width - 120.0f) < 760.0f ? (width - 120.0f) : 760.0f;
	const float boxHeight = 156.0f;
	const float boxX = -boxWidth * 0.5f;
	const float boxY = height * 0.5f - boxHeight - 46.0f;

	DrawPanel(boxX, boxY, boxWidth, boxHeight, 0.88f, DEPTH_PANEL);
	r->PushRect(boxX, boxY, 3.0f, boxHeight, COL_SHU, DEPTH_PANEL_TOP);

	const std::wstring& speaker = m_ActiveBlock->lines[m_PageStart].speaker;
	float textTop = boxY + 22.0f;

	if (!speaker.empty())
	{
		r->PushText(speaker, boxX + 26.0f, boxY + 16.0f, 17, FONT_SERIF, true,
			RGBA(0.84f, 0.70f, 0.52f, 0.98f), ALIGN_LEFT);
		textTop = boxY + 50.0f;
	}

	for (size_t i = m_PageStart; i < m_PageEnd && i < m_ActiveBlock->lines.size(); ++i)
	{
		const float lineY = textTop + (float)(i - m_PageStart) * 30.0f;
		r->PushText(m_ActiveBlock->lines[i].text, boxX + 26.0f, lineY, 20, FONT_SERIF, false,
			RGBA(0.92f, 0.91f, 0.87f, 0.98f), ALIGN_LEFT);
	}

	// Advance hint, blinking gently.
	const float blink = 0.55f + 0.45f * sinf(m_Time * 3.4f);
	r->PushText(m_Dialogue->Line("ui_advance"), boxX + boxWidth - 26.0f, boxY + boxHeight - 30.0f,
		14, FONT_UI, false, RGBA(0.75f, 0.72f, 0.66f, blink), ALIGN_RIGHT);
}

void Game::DrawClueList()
{
	Renderer* r = m_Renderer;

	const float width = (float)r->GetWidth();
	const float height = (float)r->GetHeight();

	r->PushRect(-width * 0.5f, -height * 0.5f, width, height,
		RGBA(0.0f, 0.0f, 0.0f, 0.62f), DEPTH_PANEL - 1.0f);

	const float boxWidth = 460.0f;
	const float boxHeight = 250.0f;
	const float boxX = -boxWidth * 0.5f;
	const float boxY = -boxHeight * 0.5f;

	DrawPanel(boxX, boxY, boxWidth, boxHeight, 0.94f, DEPTH_PANEL);
	r->PushRect(boxX, boxY, 3.0f, boxHeight, COL_SHU, DEPTH_PANEL_TOP);

	r->PushText(m_Dialogue->Line("ui_clues_header"), boxX + 28.0f, boxY + 22.0f, 22, FONT_SERIF, true,
		RGBA(0.86f, 0.72f, 0.54f, 0.98f), ALIGN_LEFT);

	if (ClueCount() == 0)
	{
		r->PushText(m_Dialogue->Line("ui_clues_empty"), boxX + 28.0f, boxY + 76.0f, 17, FONT_UI, false,
			RGBA(0.62f, 0.62f, 0.60f, 0.9f), ALIGN_LEFT);
	}
	else
	{
		int row = 0;

		for (int i = 0; i < CLUE_TOTAL; ++i)
		{
			if ((m_ClueMask & CLUE_BITS[i]) == 0)
			{
				continue;
			}

			const float lineY = boxY + 76.0f + row * 40.0f;

			r->PushDiamond(boxX + 36.0f, lineY + 11.0f, 6.0f, 6.0f, COL_SHU, DEPTH_PANEL_TOP);
			r->PushText(m_Dialogue->Line(CLUE_UI_KEYS[i]), boxX + 54.0f, lineY, 18, FONT_UI, false,
				RGBA(0.90f, 0.89f, 0.86f, 0.96f), ALIGN_LEFT);

			++row;
		}
	}

	r->PushText(m_Dialogue->Line("ui_advance"), boxX + boxWidth - 26.0f, boxY + boxHeight - 32.0f,
		14, FONT_UI, false, RGBA(0.70f, 0.68f, 0.64f, 0.8f), ALIGN_RIGHT);
}

void Game::DrawIntro()
{
	Renderer* r = m_Renderer;

	const DialogueBlock* block = m_Dialogue->Find("intro");

	if (block == NULL)
	{
		return;
	}

	// Text rises out of the black a beat after the fade begins.
	const float appear = Clamp((m_StateTime - 0.5f) / 1.2f, 0.0f, 1.0f);
	const float leave = 1.0f - Clamp((m_StateTime - 3.2f) / 0.8f, 0.0f, 1.0f);
	const float alpha = appear * leave;

	r->PushText(m_Dialogue->Line("ui_title"), 0.0f, -78.0f, 46, FONT_SERIF, true,
		RGBA(0.88f, 0.84f, 0.76f, alpha), ALIGN_CENTER);
	r->PushText(m_Dialogue->Line("ui_subtitle"), 0.0f, -18.0f, 15, FONT_UI, false,
		RGBA(0.66f, 0.60f, 0.52f, alpha * 0.9f), ALIGN_CENTER);

	for (size_t i = 0; i < block->lines.size(); ++i)
	{
		r->PushText(block->lines[i].text, 0.0f, 40.0f + (float)i * 32.0f, 19, FONT_SERIF, false,
			RGBA(0.78f, 0.78f, 0.74f, alpha * 0.92f), ALIGN_CENTER);
	}
}

void Game::DrawEnding()
{
	Renderer* r = m_Renderer;

	const float alpha = Clamp((m_StateTime - 1.4f) / 1.2f, 0.0f, 1.0f);

	if (alpha <= 0.0f)
	{
		return;
	}

	r->PushText(m_Dialogue->Line("ui_end_title"), 0.0f, -92.0f, 34, FONT_SERIF, true,
		RGBA(0.88f, 0.84f, 0.76f, alpha), ALIGN_CENTER);

	// A single vermilion rule under the chapter title.
	r->PushRect(-58.0f, -40.0f, 116.0f, 2.0f,
		RGBA(COL_SHU.r, COL_SHU.g, COL_SHU.b, alpha * 0.9f), DEPTH_PANEL);

	const DialogueBlock* body = m_Dialogue->Find("ui_end_body");

	if (body != NULL)
	{
		for (size_t i = 0; i < body->lines.size(); ++i)
		{
			r->PushText(body->lines[i].text, 0.0f, -6.0f + (float)i * 34.0f, 20, FONT_SERIF, false,
				RGBA(0.82f, 0.81f, 0.77f, alpha * 0.95f), ALIGN_CENTER);
		}
	}

	r->PushText(m_Dialogue->Line("ui_end_hint"), 0.0f, 128.0f, 14, FONT_UI, false,
		RGBA(0.60f, 0.60f, 0.57f, alpha * 0.8f), ALIGN_CENTER);
}

// ---------------------------------------------------------------- frame

void Game::Render()
{
	m_Renderer->SetCamera(m_CameraX, m_CameraY);
	m_Renderer->BeginFrame(COL_SKY, m_Time);
	UpdateSceneState();
	m_SceneGraph.Render(*m_Renderer);
	m_Renderer->EndFrame();
}

SceneGraph& Game::GetSceneGraph()
{
	return m_SceneGraph;
}

const SceneGraph& Game::GetSceneGraph() const
{
	return m_SceneGraph;
}
