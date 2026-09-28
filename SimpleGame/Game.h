#pragma once

#include <string>
#include <vector>

#include "Renderer.h"
#include "World.h"
#include "Dialogue.h"
#include "Input.h"
#include "Lighting.h"
#include "Model.h"
#include "SceneGraph.h"

enum GameState
{
	STATE_INTRO = 0,
	STATE_PLAY,
	STATE_DIALOGUE,
	STATE_CLUES,
	STATE_ENDING
};

enum InteractKind
{
	INTERACT_NONE = 0,
	INTERACT_NPC,
	INTERACT_PROP
};

// One bit per villager who saw the crest. CLUE_ALL is the gate on the ending.
enum ClueBit
{
	CLUE_SMITH = 1,
	CLUE_FISHER = 2,
	CLUE_CHILD = 4,
	CLUE_ALL = CLUE_SMITH | CLUE_FISHER | CLUE_CHILD
};

struct Npc
{
	std::string id;
	Actor* actor;
	float phase;		// idle sway offset so the crowd is not in lockstep
	int look;
};

struct Mote
{
	Actor* actor;
	float riseSpeed;
	float drift;
	float life;
	float maxLife;
	float size;
};

// The tutorial level: one village, one short quest, about five minutes of play.
class Game
{
public:
	Game();

	bool Initialize(Renderer* renderer, ModelLibrary* models, DialogueDB* dialogue);

	void Update(float deltaSeconds);
	void Render();

	// Game borrows its gameplay actor pointers from this graph. Before removing
	// those actors or their ancestors, clear the associated Game references.
	SceneGraph& GetSceneGraph();
	const SceneGraph& GetSceneGraph() const;

	void OnKey(unsigned char key, bool down, bool shift);
	bool WantsExit() const { return m_WantsExit; }
	bool ConsumeNextLevelRequest();
	GameState GetState() const
	{
		return m_State;
	}

private:
	// --- simulation ---
	void CreateScene();
	void CreateEnvironmentActors();
	void CreateCharacterActors();
	void CreateEffectActors();
	void CreateInterfaceActors();
	void UpdateSceneState();
	void UpdatePlayer(float deltaSeconds);
	void UpdateCamera(float deltaSeconds);
	void UpdateLighting();
	void UpdateInteractionTarget();
	void UpdateMote(size_t index, float deltaSeconds);
	void UpdateMist(size_t index, float deltaSeconds);
	void TryInteract();
	void OpenDialogue(const std::string& key);
	void AdvanceDialogue();
	void CloseDialogue();
	void ShowToast(const std::wstring& text);
	std::string DialogueKeyForNpc(const Npc& npc) const;
	std::string DialogueKeyForProp(const Prop& prop) const;
	int ClueBitForNpc(const std::string& id) const;
	int ClueCount() const;

	// --- rendering ---
	Color Lit(const Color& base, float worldX, float worldY) const;
	float FogFactor(float worldX, float worldY) const;
	void PushShadow(float worldX, float worldY, float radiusX, float radiusY, float depth, float worldZ);

	void DrawGround(const Actor& actor, TileType tile, unsigned int variation);
	void DrawScenery(const Actor& actor, TileType tile, int variant);
	void DrawProp(const Actor& actor, const Prop& definition);
	void DrawNpc(const Npc& npc);
	void DrawPlayer(const Actor& actor);
	void DrawMote(const Mote& mote);
	void DrawMist(const Mote& wisp, size_t index);
	void DrawAtmosphere();
	void DrawHud();
	void DrawTutorialSkipHint();
	void DrawQuestGuide();
	void DrawDialogue();
	void DrawClueList();
	void DrawIntro();
	void DrawEnding();
	void DrawPanel(float x, float y, float width, float height, float alpha, float depth);
	void DrawAccentPanel(float x, float y, float width, float height, const Color& accent, float depth);

	void DrawTree(float worldX, float worldY, int variant, float worldZ);
	void DrawBush(float worldX, float worldY, float worldZ);
	void DrawRock(float worldX, float worldY, float worldZ);
	void DrawBuilding(const Prop& prop, const Color& wall, const Color& roof, bool litWindow, float worldZ);
	void DrawRuin(const Prop& prop, float worldZ);
	void DrawTorii(const Prop& prop, float worldZ);
	void DrawLantern(const Prop& prop, float worldZ);
	void DrawWell(const Prop& prop, float worldZ);
	void DrawCart(const Prop& prop, float worldZ);
	void DrawDock(const Prop& prop, float worldZ);
	void DrawStone(const Prop& prop, float worldZ);
	void DrawPerson(float worldX, float worldY, float bodyHeight, const Color& robe,
		const Color& trim, bool hat, bool sword, bool lantern, float bobPixels, float worldZ);

	Renderer* m_Renderer;
	ModelLibrary* m_Models;
	DialogueDB* m_Dialogue;
	World m_World;
	SceneGraph m_SceneGraph;
	Actor* m_EnvironmentActor;
	Actor* m_CharactersActor;
	Actor* m_EffectsActor;
	Actor* m_InterfaceActor;
	Actor* m_HudActor;
	Actor* m_DialogueActor;
	Actor* m_CluesActor;
	Actor* m_IntroActor;
	Actor* m_EndingActor;
	std::vector<Actor*> m_PropActors;

	GameState m_State;
	bool m_WantsExit;
	bool m_WantsNextLevel;
	float m_Time;
	float m_StateTime;
	float m_Fade;			// 1 = fully black

	// player
	Actor* m_PlayerActor;
	float m_PlayerStride;	// drives the walk bob
	bool m_MoveKey[MOVE_COUNT];
	bool m_Running;
	bool m_Moving;

	// the camera trails the player instead of snapping to it
	float m_CameraX;
	float m_CameraY;

	// quest
	bool m_MetElder;
	int m_ClueMask;

	// interaction
	InteractKind m_TargetKind;
	int m_TargetIndex;
	float m_TargetX;
	float m_TargetY;

	// dialogue playback: one page is the run of consecutive lines sharing a speaker
	const DialogueBlock* m_ActiveBlock;
	size_t m_PageStart;
	size_t m_PageEnd;
	std::string m_ActiveKey;

	float m_ToastTimer;
	std::wstring m_ToastText;

	std::vector<Npc> m_Npcs;
	Lighting m_Lighting;
	int m_PlayerLightIndex;
	std::vector<Mote> m_Motes;
	std::vector<Mote> m_Mist;
};
