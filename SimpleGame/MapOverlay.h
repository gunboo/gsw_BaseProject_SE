#pragma once

#include "Actor.h"
#include "Dialogue.h"
#include "MapView.h"
#include "Renderer.h"
#include "World.h"

class MapOverlay
{
public:
	void Initialize(const World& world, const DialogueDB& dialogue, const std::string& titleKey);
	void AddMarker(const Actor* actor, const std::wstring& name, bool hostile = false);
	bool IsOpen() const { return m_Open; }
	// Returns true when map input has consumed the event, including key releases.
	bool OnKey(unsigned char key, bool down, bool canOpen, const ActorPosition& player);
	void OnScroll(float steps, float x, float y, const Renderer& renderer);
	void OnPointer(float x, float y, bool down, const Renderer& renderer);
	void CancelDrag() { m_Dragging = false; }
	void Draw(Renderer& renderer, const ActorPosition& player);
	static float MiniHeight(const Renderer& renderer);

private:
	struct TerrainRun
	{
		int x, y, length;
		TileType tile;
	};
	struct Marker
	{
		const Actor* actor;
		std::wstring name;
		bool hostile;
	};

	MapRect FullArea(const Renderer& renderer) const;
	void DrawMap(Renderer& renderer, const MapRect& area, const MapView& view, const ActorPosition& player);

	const World* m_World = NULL;
	const DialogueDB* m_Dialogue = NULL;
	std::wstring m_Title;
	std::vector<TerrainRun> m_Terrain;
	std::vector<Marker> m_Markers;
	MapView m_View;
	bool m_Open = false;
	bool m_Dragging = false;
	float m_PointerX = 0.0f;
	float m_PointerY = 0.0f;
};
