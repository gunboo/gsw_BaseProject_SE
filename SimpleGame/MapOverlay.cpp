#include "stdafx.h"
#include "MapOverlay.h"
#include "AnalysisLog.h"
#include "VisualStyle.h"

namespace
{
	const float MAP_DEPTH = 950000.0f;
	const float MARGIN = 20.0f;
	const float NPC_ZOOM = 2.0f;
	const float LABEL_ZOOM = 4.0f;
	const int LABEL_FONT = 12;
	const Color INK = { 0.035f, 0.055f, 0.05f, 1.0f };
	const Color GOLD = { 0.91f, 0.77f, 0.46f, 1.0f };
	const Color HOSTILE = { 0.94f, 0.40f, 0.32f, 1.0f };
	const Color TERRAIN[] = {
		{0.23f, 0.30f, 0.22f, 1}, {0.28f, 0.35f, 0.23f, 1},
		{0.56f, 0.49f, 0.34f, 1}, {0.48f, 0.51f, 0.46f, 1},
		{0.17f, 0.35f, 0.41f, 1}, {0.64f, 0.60f, 0.42f, 1},
		{0.12f, 0.21f, 0.16f, 1}, {0.19f, 0.27f, 0.17f, 1},
		{0.35f, 0.38f, 0.36f, 1}
	};
	const char* const PROP_LABELS[] = {
		"ui_map_shrine", "ui_map_torii", "ui_map_house", "ui_map_ruin",
		"ui_map_well", "ui_map_cart", "ui_map_dock", "ui_map_stone",
		"ui_map_lantern", "ui_map_keep", "ui_map_tower", "ui_map_wall", "ui_map_gate"
	};

	void ClippedRect(Renderer& renderer, const MapRect& area, float x, float y,
		float width, float height, const Color& color, float depth)
	{
		const float right = std::min(area.x + area.width, x + width);
		const float bottom = std::min(area.y + area.height, y + height);
		x = std::max(area.x, x);
		y = std::max(area.y, y);

		if (right > x && bottom > y)
		{
			renderer.PushRect(x, y, right - x, bottom - y, color, depth);
		}
	}

	bool Overlaps(const MapRect& a, const MapRect& b)
	{
		return a.x < b.x + b.width && a.x + a.width > b.x
			&& a.y < b.y + b.height && a.y + a.height > b.y;
	}

	void Label(Renderer& renderer, const MapRect& area, std::vector<MapRect>& occupied,
		const std::wstring& text, float x, float y, const Color& color)
	{
		if (text.empty() || !area.Contains(x, y))
		{
			return;
		}

		const float width = renderer.MeasureTextWidth(text, LABEL_FONT, FONT_UI, false) + 8.0f;
		const float height = renderer.MeasureTextHeight(text, LABEL_FONT, FONT_UI, false) + 4.0f;
		MapRect box = { x + 8.0f, y - height * 0.5f, width, height };

		if (!area.Contains(box.x + width, box.y + height))
		{
			box.x = x - width - 8.0f;
		}

		if (!area.Contains(box.x, box.y) || !area.Contains(box.x + width, box.y + height))
		{
			return;
		}

		for (const MapRect& other : occupied)
		{
			if (Overlaps(box, other))
			{
				return;
			}
		}

		occupied.push_back(box);
		renderer.PushRect(box.x, box.y, width, height, INK, MAP_DEPTH + 8.0f);
		renderer.PushText(text, box.x + 4.0f, box.y + 2.0f, LABEL_FONT, FONT_UI, false, color);
	}
}

void MapOverlay::Initialize(const World& world, const DialogueDB& dialogue, const std::string& titleKey)
{
	m_World = &world;
	m_Dialogue = &dialogue;
	m_Title = dialogue.Line(titleKey);
	m_Open = false;
	m_Dragging = false;
	m_Markers.clear();
	m_Terrain.clear();
	m_View.Fit(world.GetWidth(), world.GetHeight());

	// Run-length terrain is cached once; no map textures or per-frame mesh rebuilds.
	for (int y = 0; y < world.GetHeight(); ++y)
	{
		for (int x = 0; x < world.GetWidth();)
		{
			const TileType tile = world.GetTile(x, y);
			const int start = x++;

			while (x < world.GetWidth() && world.GetTile(x, y) == tile)
			{
				++x;
			}

			m_Terrain.push_back({ start, y, x - start, tile });
		}
	}

	AnalysisLog::Get().Event("map.ready", "terrain_runs=" + std::to_string(m_Terrain.size()));
}

void MapOverlay::AddMarker(const Actor* actor, const std::wstring& name, bool hostile)
{
	m_Markers.push_back({ actor, name, hostile });
}

bool MapOverlay::OnKey(unsigned char key, bool down, bool canOpen, const ActorPosition& player)
{
	if (m_World == NULL)
	{
		return false;
	}

	if (down && ((key == 'm' && (canOpen || m_Open)) || (key == 27 && m_Open)))
	{
		m_Open = !m_Open;
		m_Dragging = false;

		if (m_Open)
		{
			m_View.Fit(m_World->GetWidth(), m_World->GetHeight());
		}

		AnalysisLog::Get().Event("map.toggle", m_Open ? "open;paused" : "closed");
		return true;
	}

	if (m_Open && down && key == 'r')
	{
		m_View.Fit(m_World->GetWidth(), m_World->GetHeight());
	}
	else if (m_Open && down && key == 'f')
	{
		m_View.centerX = player.x;
		m_View.centerY = player.y;
	}

	return m_Open;
}

float MapOverlay::MiniHeight(const Renderer& renderer)
{
	return std::min(160.0f, renderer.GetHeight() * 0.28f);
}

MapRect MapOverlay::FullArea(const Renderer& renderer) const
{
	return { -renderer.GetWidth() * 0.5f + MARGIN, -renderer.GetHeight() * 0.5f + 64.0f,
		std::max(1.0f, renderer.GetWidth() - MARGIN * 2.0f), std::max(1.0f, renderer.GetHeight() - 156.0f) };
}

void MapOverlay::OnScroll(float steps, float x, float y, const Renderer& renderer)
{
	if (m_Open)
	{
		m_View.ZoomAt(steps, x, y, FullArea(renderer), m_World->GetWidth(), m_World->GetHeight());
	}
}

void MapOverlay::OnPointer(float x, float y, bool down, const Renderer& renderer)
{
	if (m_Open && down)
	{
		const MapRect area = FullArea(renderer);

		if (m_Dragging)
		{
			m_View.Pan(x - m_PointerX, y - m_PointerY, area, m_World->GetWidth(), m_World->GetHeight());
		}
		else
		{
			m_Dragging = area.Contains(x, y);
		}
	}
	else
	{
		m_Dragging = false;
	}

	m_PointerX = x;
	m_PointerY = y;
}

void MapOverlay::Draw(Renderer& renderer, const ActorPosition& player)
{
	if (m_World == NULL)
	{
		return;
	}

	renderer.ClearAnim();
	const float top = -renderer.GetHeight() * 0.5f;
	const float right = renderer.GetWidth() * 0.5f;
	MapRect area = FullArea(renderer);
	MapView view = m_View;

	if (m_Open)
	{
		m_View.Clamp(m_World->GetWidth(), m_World->GetHeight(), area);
		view = m_View;
		renderer.PushText(m_Title, -right + MARGIN, top + 20.0f, 22, FONT_UI, true, GOLD);
		renderer.PushText(m_Dialogue->Line("ui_map_close"), right - MARGIN, top + 26.0f,
			13, FONT_UI, false, VisualStyle::COL_IVORY, ALIGN_RIGHT);
		const float footer = area.y + area.height + 12.0f;
		renderer.PushText(m_Dialogue->Line("ui_map_controls"), area.x, footer, 12, FONT_UI, false, GOLD);
		renderer.PushText(m_Dialogue->Line("ui_map_legend"), area.x, footer + 24.0f, 12, FONT_UI, false, VisualStyle::COL_IVORY);
		const char* detail = view.zoom < NPC_ZOOM ? "ui_map_overview" : view.zoom < LABEL_ZOOM ? "ui_map_people" : "ui_map_names";
		renderer.PushText(m_Dialogue->Line(detail), area.x, footer + 48.0f, 12, FONT_UI, false, VisualStyle::COL_MOSS);
	}
	else
	{
		const float width = std::min(210.0f, renderer.GetWidth() * 0.34f);
		const float height = MiniHeight(renderer);
		const float x = right - MARGIN - width;
		renderer.DiscardTextInRect(x - 1.0f, top + MARGIN - 1.0f, width + 2.0f, height + 2.0f);
		renderer.PushRect(x - 1.0f, top + MARGIN - 1.0f, width + 2.0f, height + 2.0f, GOLD, MAP_DEPTH);
		renderer.PushRect(x, top + MARGIN, width, height, INK, MAP_DEPTH + 1.0f);
		renderer.PushText(m_Title + L"  [M]", x + 8.0f, top + MARGIN + 5.0f,
			12, FONT_UI, true, GOLD);
		area = { x + 6.0f, top + MARGIN + 26.0f, width - 12.0f, height - 32.0f };
		view.centerX = player.x;
		view.centerY = player.y;
		view.zoom = std::max(1.0f, m_World->GetWidth() / 64.0f);
		view.Clamp(m_World->GetWidth(), m_World->GetHeight(), area);
	}

	renderer.PushRect(area.x, area.y, area.width, area.height, INK, MAP_DEPTH + 1.0f);
	DrawMap(renderer, area, view, player);
	renderer.PushText(L"N", area.x + 8.0f, area.y + 6.0f, 12, FONT_UI, true, GOLD);
}

void MapOverlay::DrawMap(Renderer& renderer, const MapRect& area, const MapView& view, const ActorPosition& player)
{
	const float scale = view.Scale(area, m_World->GetWidth(), m_World->GetHeight());
	const float originX = area.x + area.width * 0.5f - view.centerX * scale;
	const float originY = area.y + area.height * 0.5f - view.centerY * scale;
	const auto screenX = [&](float x) { return originX + x * scale; };
	const auto screenY = [&](float y) { return originY + y * scale; };

	for (const TerrainRun& run : m_Terrain)
	{
		ClippedRect(renderer, area, screenX(static_cast<float>(run.x)), screenY(static_cast<float>(run.y)),
			run.length * scale, scale, TERRAIN[run.tile], MAP_DEPTH + 2.0f);
	}

	for (const Prop& prop : m_World->GetProps())
	{
		const bool building = prop.sizeX > 0.0f && prop.sizeY > 0.0f;

		if (!building && (!m_Open || view.zoom < NPC_ZOOM))
		{
			continue;
		}

		const float width = building ? prop.sizeX * scale : 4.0f;
		const float height = building ? prop.sizeY * scale : 4.0f;
		ClippedRect(renderer, area, screenX(prop.x) - (building ? 0.0f : 2.0f),
			screenY(prop.y) - (building ? 0.0f : 2.0f), width, height, GOLD, MAP_DEPTH + 3.0f);
	}

	const bool people = m_Open && view.zoom >= NPC_ZOOM;
	const MapRect inset = { area.x + 7.0f, area.y + 7.0f, area.width - 14.0f, area.height - 14.0f };

	if (people)
	{
		for (const Marker& marker : m_Markers)
		{
			if (!marker.actor->IsVisibleInHierarchy())
			{
				continue;
			}

			const ActorPosition position = marker.actor->GetWorldPosition();
			const float x = screenX(position.x), y = screenY(position.y);

			if (inset.Contains(x, y))
			{
				renderer.PushEllipse(x, y, 5.0f, 5.0f, INK, MAP_DEPTH + 4.0f);
				renderer.PushEllipse(x, y, 3.0f, 3.0f, marker.hostile ? HOSTILE : VisualStyle::COL_SPIRIT, MAP_DEPTH + 5.0f);
			}
		}
	}

	const float px = screenX(player.x), py = screenY(player.y);

	if (area.Contains(px, py))
	{
		// Keep the whole player marker readable even against the world's edge.
		const float markerX = std::max(inset.x, std::min(inset.x + inset.width, px));
		const float markerY = std::max(inset.y, std::min(inset.y + inset.height, py));
		renderer.PushDiamond(markerX, markerY, 7.0f, 7.0f, INK, MAP_DEPTH + 6.0f);
		renderer.PushDiamond(markerX, markerY, 5.0f, 5.0f, VisualStyle::COL_IVORY, MAP_DEPTH + 7.0f);
	}

	if (m_Open && view.zoom >= LABEL_ZOOM)
	{
		std::vector<MapRect> occupied;
		Label(renderer, area, occupied, m_Dialogue->Line("ui_map_player"), px, py, VisualStyle::COL_IVORY);

		for (const Marker& marker : m_Markers)
		{
			if (marker.actor->IsVisibleInHierarchy())
			{
				const ActorPosition position = marker.actor->GetWorldPosition();
				Label(renderer, inset, occupied, marker.name, screenX(position.x), screenY(position.y),
					marker.hostile ? HOSTILE : VisualStyle::COL_SPIRIT);
			}
		}

		for (const Prop& prop : m_World->GetProps())
		{
			if (prop.type >= PROP_SHRINE && prop.type < PROP_UNKNOWN)
			{
				Label(renderer, inset, occupied, m_Dialogue->Line(PROP_LABELS[prop.type]),
					screenX(prop.x + prop.sizeX * 0.5f), screenY(prop.y + prop.sizeY * 0.5f), GOLD);
			}
		}
	}
}
