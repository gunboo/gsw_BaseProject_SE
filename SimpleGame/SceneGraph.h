#pragma once

#include "Actor.h"

#include <unordered_set>

// A scene owns one stable root and its descendants. Returned Actor pointers are
// borrowed and become invalid when their actor or an ancestor is removed.
// Update inherits active flags; Render independently inherits visible flags.
// Traversal only submits geometry; Renderer retains depth sorting and batching.
class SceneGraph
{
public:
	SceneGraph();
	~SceneGraph();

	SceneGraph(const SceneGraph&) = delete;
	SceneGraph& operator=(const SceneGraph&) = delete;

	Actor& GetRoot();
	const Actor& GetRoot() const;
	Actor* CreateActor(const std::string& name, Actor* parent = NULL);
	Actor* AddActor(std::unique_ptr<Actor> actor, Actor* parent = NULL);
	bool RemoveActor(Actor* actor);
	bool Reparent(Actor* actor, Actor* parent = NULL, bool preserveWorldPosition = true);
	void Clear();

	// Only children of visited branches are snapshotted. Additions take part in
	// the next traversal; removals immediately stop further callbacks.
	// Reparenting is immediate but never causes a second visit in this traversal.
	// Moving into an already visited branch may defer a visit until next time.
	// Removed objects live until the outermost traversal returns, allowing an
	// actor to remove itself or an ancestor safely from its callback.
	void Update(float deltaSeconds);
	void Render(Renderer& renderer);

	// Render statistics count visited/cut branches, not every actor inside a
	// rejected branch. Culled includes hidden/empty branches as well as those
	// outside the viewport. Submitted counts actors with drawable content.
	std::size_t GetVisitedActorCount() const;
	std::size_t GetCulledActorCount() const;
	std::size_t GetSubmittedActorCount() const;

private:
	class TraversalScope;

	struct TraversalState
	{
		std::size_t insertionVersion;
		std::unordered_set<Actor*> visited;
	};

	static std::vector<Actor*> SnapshotChildren(Actor& actor);
	static void SetOwner(Actor& actor, SceneGraph* sceneGraph);
	static std::unique_ptr<Actor> DetachActor(Actor& actor);
	void RetireActor(std::unique_ptr<Actor> actor);
	void UpdateActor(Actor& actor, float deltaSeconds, TraversalState& state);
	void RenderActor(Actor& actor, Renderer& renderer, const Actor::Bounds& viewport, TraversalState& state);

	Actor m_Root;
	std::size_t m_TraversalDepth;
	std::size_t m_InsertionVersion;
	std::size_t m_VisitedActorCount;
	std::size_t m_CulledActorCount;
	std::size_t m_SubmittedActorCount;
	std::vector<std::unique_ptr<Actor>> m_RemovedActors;
};
