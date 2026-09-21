#include "stdafx.h"
#include "SceneGraph.h"
#include "Renderer.h"

#include <typeinfo>
#include <utility>

class SceneGraph::TraversalScope
{
public:
	explicit TraversalScope(SceneGraph& sceneGraph)
		: m_SceneGraph(sceneGraph)
	{
		++m_SceneGraph.m_TraversalDepth;
	}

	~TraversalScope()
	{
		--m_SceneGraph.m_TraversalDepth;

		if (m_SceneGraph.m_TraversalDepth == 0)
		{
			m_SceneGraph.m_RemovedActors.clear();
		}
	}

private:
	SceneGraph& m_SceneGraph;
};

SceneGraph::SceneGraph()
	: m_Root("Root")
	, m_TraversalDepth(0)
	, m_InsertionVersion(0)
	, m_VisitedActorCount(0)
	, m_CulledActorCount(0)
	, m_SubmittedActorCount(0)
{
	m_Root.m_SceneGraph = this;
}

SceneGraph::~SceneGraph()
{
}

Actor& SceneGraph::GetRoot()
{
	return m_Root;
}

const Actor& SceneGraph::GetRoot() const
{
	return m_Root;
}

Actor* SceneGraph::CreateActor(const std::string& name, Actor* parent)
{
	return AddActor(std::unique_ptr<Actor>(new Actor(name)), parent);
}

Actor* SceneGraph::AddActor(std::unique_ptr<Actor> actor, Actor* parent)
{
	if (parent == NULL)
	{
		parent = &m_Root;
	}

	if (!actor || actor->m_SceneGraph != NULL || parent->m_SceneGraph != this)
	{
		return NULL;
	}

	Actor* result = actor.get();

	actor->m_Parent = parent;
	actor->m_InsertionVersion = ++m_InsertionVersion;
	// A subclass may override Render without installing a callback. Treat it
	// conservatively as drawable until it supplies explicit render bounds.
	actor->m_HasCustomRender = typeid(*actor) != typeid(Actor);
	SetOwner(*actor, this);
	parent->m_Children.push_back(std::move(actor));
	parent->InvalidateSubtreeBounds();

	return result;
}

bool SceneGraph::RemoveActor(Actor* actor)
{
	if (actor == NULL || actor == &m_Root || actor->m_SceneGraph != this)
	{
		return false;
	}

	if (m_TraversalDepth != 0)
	{
		m_RemovedActors.reserve(m_RemovedActors.size() + 1);
	}

	RetireActor(DetachActor(*actor));

	return true;
}

bool SceneGraph::Reparent(Actor* actor, Actor* parent, bool preserveWorldPosition)
{
	if (parent == NULL)
	{
		parent = &m_Root;
	}

	if (actor == NULL || actor == &m_Root || actor->m_SceneGraph != this || parent->m_SceneGraph != this)
	{
		return false;
	}

	for (const Actor* ancestor = parent; ancestor != NULL; ancestor = ancestor->m_Parent)
	{
		if (ancestor == actor)
		{
			return false;
		}
	}

	if (actor->m_Parent == parent)
	{
		return true;
	}

	ActorPosition worldPosition = actor->GetWorldPosition();
	ActorPosition parentPosition = parent->GetWorldPosition();

	// Allocate before detaching so allocation failure cannot destroy an actor
	// that may currently be executing its own callback.
	parent->m_Children.reserve(parent->m_Children.size() + 1);
	std::unique_ptr<Actor> ownership = DetachActor(*actor);
	actor->m_Parent = parent;
	parent->m_Children.push_back(std::move(ownership));
	parent->InvalidateSubtreeBounds();

	if (preserveWorldPosition)
	{
		actor->SetPosition(worldPosition.x - parentPosition.x,
			worldPosition.y - parentPosition.y, worldPosition.z - parentPosition.z);
	}

	return true;
}

void SceneGraph::Clear()
{
	if (m_TraversalDepth != 0)
	{
		m_RemovedActors.reserve(m_RemovedActors.size() + m_Root.m_Children.size());
	}

	// Move children out first so graph queries inside destructors see an empty
	// root; the root itself and its transform/flags remain stable across clears.
	std::vector<std::unique_ptr<Actor>> children;

	children.swap(m_Root.m_Children);
	m_Root.InvalidateSubtreeBounds();

	for (std::unique_ptr<Actor>& child : children)
	{
		child->m_Parent = NULL;
		RetireActor(std::move(child));
	}
}

void SceneGraph::Update(float deltaSeconds)
{
	TraversalScope traversal(*this);
	TraversalState state;

	state.insertionVersion = m_InsertionVersion;
	UpdateActor(m_Root, deltaSeconds, state);
}

void SceneGraph::Render(Renderer& renderer)
{
	TraversalScope traversal(*this);
	TraversalState state;
	float offsetX;
	float offsetY;
	float halfWidth = static_cast<float>(renderer.GetWidth()) * 0.5f;
	float halfHeight = static_cast<float>(renderer.GetHeight()) * 0.5f;

	renderer.WorldToScreen(0.0f, 0.0f, 0.0f, &offsetX, &offsetY);
	Actor::Bounds viewport = { -halfWidth - offsetX, -halfHeight - offsetY,
		halfWidth - offsetX, halfHeight - offsetY, false, false };

	state.insertionVersion = m_InsertionVersion;
	m_VisitedActorCount = 0;
	m_CulledActorCount = 0;
	m_SubmittedActorCount = 0;
	RenderActor(m_Root, renderer, viewport, state);
}

std::size_t SceneGraph::GetVisitedActorCount() const
{
	return m_VisitedActorCount;
}

std::size_t SceneGraph::GetCulledActorCount() const
{
	return m_CulledActorCount;
}

std::size_t SceneGraph::GetSubmittedActorCount() const
{
	return m_SubmittedActorCount;
}

void SceneGraph::UpdateActor(Actor& actor, float deltaSeconds, TraversalState& state)
{
	if (actor.m_SceneGraph != this || actor.m_InsertionVersion > state.insertionVersion
		|| !actor.IsActiveInHierarchy() || !state.visited.insert(&actor).second)
	{
		return;
	}

	actor.Update(deltaSeconds);

	if (actor.m_SceneGraph != this || !actor.IsActiveInHierarchy())
	{
		return;
	}

	std::vector<Actor*> children = SnapshotChildren(actor);

	for (Actor* child : children)
	{
		if (actor.m_SceneGraph != this || !actor.IsActiveInHierarchy())
		{
			break;
		}

		if (child->m_Parent == &actor)
		{
			UpdateActor(*child, deltaSeconds, state);
		}
	}
}

void SceneGraph::RenderActor(Actor& actor, Renderer& renderer, const Actor::Bounds& viewport, TraversalState& state)
{
	if (actor.m_SceneGraph != this || actor.m_InsertionVersion > state.insertionVersion
		|| !state.visited.insert(&actor).second)
	{
		return;
	}

	++m_VisitedActorCount;

	if (!actor.IsVisibleInHierarchy())
	{
		++m_CulledActorCount;
		return;
	}

	const Actor::Bounds& bounds = actor.GetSubtreeBounds();

	if (bounds.empty)
	{
		++m_CulledActorCount;
		return;
	}

	if (!bounds.unbounded)
	{
		ActorPosition position = actor.GetWorldPosition();
		float worldX = (position.x - position.y) * Renderer::TileHalfWidth();
		float worldY = (position.x + position.y) * Renderer::TileHalfHeight() - position.z * Renderer::HeightScale();

		if (bounds.maxX + worldX < viewport.minX || bounds.minX + worldX > viewport.maxX
			|| bounds.maxY + worldY < viewport.minY || bounds.minY + worldY > viewport.maxY)
		{
			++m_CulledActorCount;
			return;
		}
	}

	if (actor.HasRenderContent())
	{
		++m_SubmittedActorCount;
		actor.Render(renderer);
	}

	if (actor.m_SceneGraph != this || !actor.IsVisibleInHierarchy())
	{
		return;
	}

	std::vector<Actor*> children = SnapshotChildren(actor);

	for (Actor* child : children)
	{
		if (actor.m_SceneGraph != this || !actor.IsVisibleInHierarchy())
		{
			break;
		}

		if (child->m_Parent == &actor)
		{
			RenderActor(*child, renderer, viewport, state);
		}
	}
}

std::vector<Actor*> SceneGraph::SnapshotChildren(Actor& actor)
{
	std::vector<Actor*> children;

	children.reserve(actor.m_Children.size());

	for (const std::unique_ptr<Actor>& child : actor.m_Children)
	{
		children.push_back(child.get());
	}

	return children;
}

void SceneGraph::SetOwner(Actor& actor, SceneGraph* sceneGraph)
{
	actor.m_SceneGraph = sceneGraph;

	for (const std::unique_ptr<Actor>& child : actor.m_Children)
	{
		SetOwner(*child, sceneGraph);
	}
}

std::unique_ptr<Actor> SceneGraph::DetachActor(Actor& actor)
{
	std::vector<std::unique_ptr<Actor>>& siblings = actor.m_Parent->m_Children;

	for (auto child = siblings.begin(); child != siblings.end(); ++child)
	{
		if (child->get() == &actor)
		{
			std::unique_ptr<Actor> ownership = std::move(*child);

			siblings.erase(child);
			actor.m_Parent->InvalidateSubtreeBounds();
			actor.m_Parent = NULL;

			return ownership;
		}
	}

	return std::unique_ptr<Actor>();
}

void SceneGraph::RetireActor(std::unique_ptr<Actor> actor)
{
	if (!actor)
	{
		return;
	}

	SetOwner(*actor, NULL);

	if (m_TraversalDepth != 0)
	{
		m_RemovedActors.push_back(std::move(actor));
	}
}
