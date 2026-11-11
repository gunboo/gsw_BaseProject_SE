#include "stdafx.h"
#include "GroundShadow.h"
#include "EnvironmentArt.h"
#include "VisualStyle.h"

#include <cmath>
#include <utility>

namespace
{
	const float CAST_X = 0.68f;
	const float CAST_Y = -0.14f;
	const float PENUMBRA = 0.48f;
	const float CAST_ALPHA = 0.36f;
	const float CONTACT_ALPHA = 0.62f;
	const float CONTACT_WIDTH = 0.32f;
	const float HEIGHT_TOLERANCE = 0.40f;
	const float TREE_PENUMBRA = 0.38f;
	const float TREE_CAST_ALPHA = 0.22f;
	const int RECEIVER_SUBDIVISIONS = World::STAIR_TREADS_PER_TILE;

	struct Patch
	{
		float x[4], y[4], z[4], alpha[4];
	};

	struct Caster
	{
		float width, depth, height, radius;
		bool tree;
	};

	float Fade(float distance, float width)
	{
		const float t = VisualStyle::Clamp(1.0f - distance / width, 0.0f, 1.0f);

		return t * t * (3.0f - 2.0f * t);
	}

	float RectangleDistance(float x, float y, float width, float depth)
	{
		const float dx = fmaxf(fmaxf(-x, x - width), 0.0f);
		const float dy = fmaxf(fmaxf(-y, y - depth), 0.0f);

		return sqrtf(dx * dx + dy * dy);
	}

	float Coverage(const Caster& caster, float x, float y, float receiverHeight)
	{
		const float height = caster.height - receiverHeight;

		if (height <= 0.0f)
		{
			return 0.0f;
		}

		const float dx = height * CAST_X;
		const float dy = height * CAST_Y;
		const float contact = fabsf(receiverHeight) <= HEIGHT_TOLERANCE
			? CONTACT_ALPHA * Fade(RectangleDistance(x, y, caster.width, caster.depth), CONTACT_WIDTH) : 0.0f;

		if (caster.tree)
		{
			// One broad canopy silhouette and a narrow trunk shadow share the light direction.
			const float centerX = dx * 0.76f;
			const float centerY = dy * 0.76f;
			const float crownDistance = sqrtf((x - centerX) * (x - centerX)
				+ (y - centerY) * (y - centerY)) - caster.radius;
			const float t = VisualStyle::Clamp((x * dx + y * dy) / (dx * dx + dy * dy), 0.0f, 0.76f);
			const float trunkDistance = sqrtf((x - dx * t) * (x - dx * t) + (y - dy * t) * (y - dy * t));

			return fmaxf(TREE_CAST_ALPHA * Fade(crownDistance, TREE_PENUMBRA),
				TREE_CAST_ALPHA * 0.7f * Fade(trunkDistance, CONTACT_WIDTH));
		}

		// Convex hull of a rectangular footprint swept by the directional light.
		const float hullX[6] = { 0.0f, dx, caster.width + dx, caster.width + dx, caster.width, 0.0f };
		const float hullY[6] = { 0.0f, dy, dy, caster.depth + dy, caster.depth, caster.depth };
		float distance = -1e9f;

		for (int i = 0; i < 6; ++i)
		{
			const int next = (i + 1) % 6;
			const float edgeX = hullX[next] - hullX[i];
			const float edgeY = hullY[next] - hullY[i];
			const float length = sqrtf(edgeX * edgeX + edgeY * edgeY);

			if (length > 0.001f)
			{
				distance = fmaxf(distance, (edgeY * (x - hullX[i]) - edgeX * (y - hullY[i])) / length);
			}
		}

		return fmaxf(contact, CAST_ALPHA * Fade(distance, PENUMBRA));
	}

	void Create(SceneGraph& graph, Actor* casterActor, const World& world,
		const Lighting& lighting, const Caster& caster)
	{
		const ActorPosition origin = casterActor->GetWorldPosition();
		const float maximumHeight = caster.height + fmaxf(origin.z, 0.0f);
		const float padding = caster.tree ? caster.radius + TREE_PENUMBRA : PENUMBRA;
		const int minX = (int)fmaxf(0.0f, floorf(origin.x - padding));
		const int minY = (int)fmaxf(0.0f, floorf(origin.y - maximumHeight * -CAST_Y - padding));
		const int maxX = (int)fminf((float)world.GetWidth(), ceilf(origin.x + caster.width + maximumHeight * CAST_X + padding));
		const int maxY = (int)fminf((float)world.GetHeight(), ceilf(origin.y + caster.depth + padding));
		std::vector<Patch> patches;
		float minScreenX = 1e9f, minScreenY = 1e9f, maxScreenX = -1e9f, maxScreenY = -1e9f;

		for (int y = minY; y < maxY; ++y)
		{
			for (int x = minX; x < maxX; ++x)
			{
				if (world.GetTile(x, y) == TILE_WATER)
				{
					continue;
				}

				for (int row = 0; row < RECEIVER_SUBDIVISIONS; ++row)
				{
					for (int column = 0; column < RECEIVER_SUBDIVISIONS; ++column)
					{
						const float step = 1.0f / RECEIVER_SUBDIVISIONS;
						const float localX[4] = { column * step, (column + 1) * step, (column + 1) * step, column * step };
						const float localY[4] = { row * step, row * step, (row + 1) * step, (row + 1) * step };
						const float treadZ = world.GetTileElevation(x, y, (column + 0.5f) * step, (row + 0.5f) * step);
						Patch patch;
						float maximumAlpha = 0.0f;

						for (int corner = 0; corner < 4; ++corner)
						{
							patch.x[corner] = x + localX[corner] - origin.x;
							patch.y[corner] = y + localY[corner] - origin.y;
							patch.z[corner] = (world.IsStairTile(x, y) ? treadZ
								: world.GetTileElevation(x, y, localX[corner], localY[corner])) - origin.z;
							patch.alpha[corner] = Coverage(caster, patch.x[corner], patch.y[corner], patch.z[corner]);
							maximumAlpha = fmaxf(maximumAlpha, patch.alpha[corner]);
						}

						if (maximumAlpha < 0.005f)
						{
							continue;
						}

						for (int corner = 0; corner < 4; ++corner)
						{
							const float sx = (patch.x[corner] - patch.y[corner]) * Renderer::TileHalfWidth();
							const float sy = (patch.x[corner] + patch.y[corner]) * Renderer::TileHalfHeight()
								- patch.z[corner] * Renderer::HeightScale();
							minScreenX = fminf(minScreenX, sx);
							minScreenY = fminf(minScreenY, sy);
							maxScreenX = fmaxf(maxScreenX, sx);
							maxScreenY = fmaxf(maxScreenY, sy);
						}

						patches.push_back(patch);
					}
				}
			}
		}

		if (patches.empty())
		{
			return;
		}

		// A sibling remains visible when only the cast shadow reaches the viewport.
		Actor* shadow = graph.CreateActor(casterActor->GetName() + "_Shadow", casterActor->GetParent());
		const ActorPosition local = casterActor->GetPosition();
		shadow->SetPosition(local.x, local.y, local.z);
		shadow->SetRenderBounds(minScreenX, minScreenY, maxScreenX, maxScreenY);
		shadow->SetRenderCallback([patches = std::move(patches), &lighting](const Actor& actor, Renderer& renderer)
		{
			const ActorPosition position = actor.GetWorldPosition();
			const float fog = 1.0f - lighting.FogFactor(position.x, position.y);

			for (const Patch& patch : patches)
			{
				float points[8];
				Color colors[4];

				for (int corner = 0; corner < 4; ++corner)
				{
					renderer.WorldToScreen(position.x + patch.x[corner], position.y + patch.y[corner],
						position.z + patch.z[corner], &points[corner * 2], &points[corner * 2 + 1]);
					colors[corner] = RGBA(0.025f, 0.04f, 0.035f, patch.alpha[corner] * fog);
				}

				renderer.PushPolygonShaded(points, colors, 4, VisualStyle::SHADOW_DEPTH);
			}
		});
	}
}

void GroundShadow::CreateStructure(SceneGraph& graph, Actor* caster, const World& world,
	const Lighting& lighting, const Prop& prop)
{
	if (prop.type != PROP_HOUSE && prop.type != PROP_SHRINE && prop.type != PROP_KEEP
		&& prop.type != PROP_TOWER && prop.type != PROP_WALL && prop.type != PROP_GATE && prop.type != PROP_RUIN)
	{
		return;
	}

	const Caster shape = { prop.sizeX, prop.sizeY, EnvironmentArt::StructureHeight(prop), 0.0f, false };
	Create(graph, caster, world, lighting, shape);
}

void GroundShadow::CreateTree(SceneGraph& graph, Actor* caster, const World& world,
	const Lighting& lighting, const Model& model)
{
	float top = 0.0f, halfWidth = 0.0f;

	for (size_t i = 0; i < model.vertices.size(); i += 2)
	{
		top = fminf(top, model.vertices[i + 1]);
		halfWidth = fmaxf(halfWidth, fabsf(model.vertices[i]));
	}

	const Caster shape = { 0.0f, 0.0f, -top / Renderer::HeightScale(),
		halfWidth / (Renderer::TileHalfWidth() * 1.5f), true };
	Create(graph, caster, world, lighting, shape);
}
