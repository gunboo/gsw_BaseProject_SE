#include "stdafx.h"
#include "Actor.h"
#include "Renderer.h"

#include <algorithm>
#include <cmath>
#include <utility>

Actor::Actor(std::string name)
	: m_Name(std::move(name))
	, m_Position{ 0.0f, 0.0f, 0.0f }
	, m_Visible(true)
	, m_Active(true)
	, m_Parent(NULL)
	, m_SceneGraph(NULL)
	, m_InsertionVersion(0)
	, m_HasRenderBounds(false)
	, m_HasCustomRender(false)
	, m_SubtreeBoundsDirty(true)
	, m_RenderBounds{ 0.0f, 0.0f, 0.0f, 0.0f, true, false }
	, m_SubtreeBounds{ 0.0f, 0.0f, 0.0f, 0.0f, true, false }
{
}

Actor::~Actor()
{
}

const std::string& Actor::GetName() const
{
	return m_Name;
}

void Actor::SetPosition(float x, float y, float z)
{
	if (m_Position.x == x && m_Position.y == y && m_Position.z == z)
	{
		return;
	}

	m_Position = { x, y, z };

	// Cached bounds are relative to this actor, so moving a whole branch only
	// invalidates its ancestors, not the unchanged geometry beneath it.
	if (m_Parent != NULL)
	{
		m_Parent->InvalidateSubtreeBounds();
	}
}

ActorPosition Actor::GetPosition() const
{
	return m_Position;
}

ActorPosition Actor::GetWorldPosition() const
{
	ActorPosition position = m_Position;
	const Actor* parent = m_Parent;

	while (parent != NULL)
	{
		position.x += parent->m_Position.x;
		position.y += parent->m_Position.y;
		position.z += parent->m_Position.z;
		parent = parent->m_Parent;
	}

	return position;
}

void Actor::SetRenderBounds(float minX, float minY, float maxX, float maxY)
{
	if (!std::isfinite(minX) || !std::isfinite(minY) || !std::isfinite(maxX) || !std::isfinite(maxY))
	{
		ClearRenderBounds();
		return;
	}

	m_RenderBounds = { (std::min)(minX, maxX), (std::min)(minY, maxY),
		(std::max)(minX, maxX), (std::max)(minY, maxY), false, false };
	m_HasRenderBounds = true;
	InvalidateSubtreeBounds();
}

void Actor::ClearRenderBounds()
{
	m_HasRenderBounds = false;
	InvalidateSubtreeBounds();
}

bool Actor::HasRenderBounds() const
{
	return m_HasRenderBounds;
}

void Actor::SetVisible(bool visible)
{
	if (m_Visible == visible)
	{
		return;
	}

	m_Visible = visible;
	InvalidateSubtreeBounds();
}

bool Actor::IsVisible() const
{
	return m_Visible;
}

bool Actor::IsVisibleInHierarchy() const
{
	const Actor* actor = this;

	while (actor != NULL)
	{
		if (!actor->m_Visible)
		{
			return false;
		}

		actor = actor->m_Parent;
	}

	return true;
}

void Actor::SetActive(bool active)
{
	m_Active = active;
}

bool Actor::IsActive() const
{
	return m_Active;
}

bool Actor::IsActiveInHierarchy() const
{
	const Actor* actor = this;

	while (actor != NULL)
	{
		if (!actor->m_Active)
		{
			return false;
		}

		actor = actor->m_Parent;
	}

	return true;
}

Actor* Actor::GetParent() const
{
	return m_Parent;
}

std::size_t Actor::GetChildCount() const
{
	return m_Children.size();
}

Actor* Actor::GetChild(std::size_t index) const
{
	if (index >= m_Children.size())
	{
		return NULL;
	}

	return m_Children[index].get();
}

void Actor::SetUpdateCallback(std::function<void(Actor&, float)> callback)
{
	if (callback)
	{
		m_UpdateCallback = std::make_shared<std::function<void(Actor&, float)>>(std::move(callback));
	}
	else
	{
		m_UpdateCallback.reset();
	}
}

void Actor::SetRenderCallback(std::function<void(const Actor&, Renderer&)> callback)
{
	if (callback)
	{
		m_RenderCallback = std::make_shared<std::function<void(const Actor&, Renderer&)>>(std::move(callback));
	}
	else
	{
		m_RenderCallback.reset();
	}

	InvalidateSubtreeBounds();
}

void Actor::Update(float deltaSeconds)
{
	// Retain the callable when it replaces itself, without copying mutable
	// lambda state or allocating a new callable every frame.
	std::shared_ptr<std::function<void(Actor&, float)>> callback = m_UpdateCallback;

	if (callback)
	{
		(*callback)(*this, deltaSeconds);
	}
}

void Actor::InvalidateSubtreeBounds()
{
	Actor* actor = this;

	while (actor != NULL)
	{
		// A dirty ancestor already propagated invalidation when it changed.
		if (actor->m_SubtreeBoundsDirty)
		{
			break;
		}

		actor->m_SubtreeBoundsDirty = true;
		actor = actor->m_Parent;
	}
}

const Actor::Bounds& Actor::GetSubtreeBounds()
{
	if (!m_SubtreeBoundsDirty)
	{
		return m_SubtreeBounds;
	}

	m_SubtreeBounds = { 0.0f, 0.0f, 0.0f, 0.0f, true, false };
	m_SubtreeBoundsDirty = false;

	if (!m_Visible)
	{
		return m_SubtreeBounds;
	}

	if (HasRenderContent())
	{
		m_SubtreeBounds = m_RenderBounds;
		m_SubtreeBounds.empty = false;
		m_SubtreeBounds.unbounded = !m_HasRenderBounds;
	}

	for (const std::unique_ptr<Actor>& child : m_Children)
	{
		const Bounds& childBounds = child->GetSubtreeBounds();

		if (childBounds.empty)
		{
			continue;
		}

		if (childBounds.unbounded)
		{
			m_SubtreeBounds.empty = false;
			m_SubtreeBounds.unbounded = true;
			continue;
		}

		float offsetX = (child->m_Position.x - child->m_Position.y) * Renderer::TileHalfWidth();
		float offsetY = (child->m_Position.x + child->m_Position.y) * Renderer::TileHalfHeight()
			- child->m_Position.z * Renderer::HeightScale();
		Bounds translated = childBounds;

		translated.minX += offsetX;
		translated.maxX += offsetX;
		translated.minY += offsetY;
		translated.maxY += offsetY;

		if (m_SubtreeBounds.empty)
		{
			m_SubtreeBounds = translated;
		}
		else
		{
			m_SubtreeBounds.minX = (std::min)(m_SubtreeBounds.minX, translated.minX);
			m_SubtreeBounds.minY = (std::min)(m_SubtreeBounds.minY, translated.minY);
			m_SubtreeBounds.maxX = (std::max)(m_SubtreeBounds.maxX, translated.maxX);
			m_SubtreeBounds.maxY = (std::max)(m_SubtreeBounds.maxY, translated.maxY);
		}
	}

	return m_SubtreeBounds;
}

bool Actor::HasRenderContent() const
{
	return m_RenderCallback || m_HasCustomRender;
}

void Actor::Render(Renderer& renderer) const
{
	std::shared_ptr<std::function<void(const Actor&, Renderer&)>> callback = m_RenderCallback;

	if (callback)
	{
		(*callback)(*this, renderer);
	}
}
