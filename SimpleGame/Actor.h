#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class Renderer;
class SceneGraph;

struct ActorPosition
{
	float x;
	float y;
	float z;
};

// SceneGraph owns actors. World actors use local translations in world units.
// UI callbacks may interpret positions as pixels, but must leave render bounds
// unset because bounds currently use the world's quarter-view projection.
class Actor
{
public:
	explicit Actor(std::string name = "");
	virtual ~Actor();

	Actor(const Actor&) = delete;
	Actor& operator=(const Actor&) = delete;

	const std::string& GetName() const;
	void SetPosition(float x, float y, float z = 0.0f);
	ActorPosition GetPosition() const;
	ActorPosition GetWorldPosition() const;

	// Projected pixel offsets from this world actor's anchor, including all
	// effects and animation margins. An unset drawable is never view-culled.
	void SetRenderBounds(float minX, float minY, float maxX, float maxY);
	void ClearRenderBounds();
	bool HasRenderBounds() const;

	void SetVisible(bool visible);
	bool IsVisible() const;
	bool IsVisibleInHierarchy() const;
	void SetActive(bool active);
	bool IsActive() const;
	bool IsActiveInHierarchy() const;

	Actor* GetParent() const;
	std::size_t GetChildCount() const;
	Actor* GetChild(std::size_t index) const;

	void SetUpdateCallback(std::function<void(Actor&, float)> callback);
	void SetRenderCallback(std::function<void(const Actor&, Renderer&)> callback);
	virtual void Update(float deltaSeconds);
	virtual void Render(Renderer& renderer) const;

private:
	friend class SceneGraph;

	struct Bounds
	{
		float minX;
		float minY;
		float maxX;
		float maxY;
		bool empty;
		bool unbounded;
	};

	void InvalidateSubtreeBounds();
	const Bounds& GetSubtreeBounds();
	bool HasRenderContent() const;

	std::string m_Name;
	ActorPosition m_Position;
	bool m_Visible;
	bool m_Active;
	Actor* m_Parent;
	SceneGraph* m_SceneGraph;
	std::size_t m_InsertionVersion;
	bool m_HasRenderBounds;
	bool m_HasCustomRender;
	bool m_SubtreeBoundsDirty;
	Bounds m_RenderBounds;
	Bounds m_SubtreeBounds;
	std::vector<std::unique_ptr<Actor>> m_Children;
	std::shared_ptr<std::function<void(Actor&, float)>> m_UpdateCallback;
	std::shared_ptr<std::function<void(const Actor&, Renderer&)>> m_RenderCallback;
};
