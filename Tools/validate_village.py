"""Validate tutorial map geometry, elevation seams and NPC access without running the game."""

from collections import deque
from math import ceil, floor, hypot, isclose, isfinite
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
WORLD_SOURCE = (ROOT / "SimpleGame/World.cpp").read_text(encoding="utf-8")
WORLD_HEADER = (ROOT / "SimpleGame/World.h").read_text(encoding="utf-8")
MAX_STEP = float(re.search(r"MAX_STEP_HEIGHT = ([\d.]+)f", WORLD_SOURCE)[1])
TREADS = int(re.search(r"STAIR_TREADS_PER_TILE = (\d+)", WORLD_HEADER)[1])
BODY_RADIUS = 0.28


def dimension(name):
	return float(re.search(rf"{name} = ([\d.]+)f", WORLD_HEADER)[1])


class Village:
	def __init__(self, path):
		lines = path.read_text(encoding="utf-8").splitlines()
		size = next(line for line in lines if line.startswith("SIZE ")).split()
		self.width, self.height = map(int, size[1:])
		first_row = lines.index("TILES") + 1
		self.tiles = [list(line) for line in lines[first_row:first_row + self.height]]
		assert len(self.tiles) == self.height
		assert all(len(row) == self.width for row in self.tiles), "Tile grid dimensions disagree with SIZE"
		self.surfaces = [[(0.0, 0.0, 0.0) for _ in range(self.width)] for _ in range(self.height)]
		self.props, self.npcs, self.regions, self.blockers = [], {}, [], []
		section = None
		covered = set()
		for line in lines[first_row + self.height:]:
			if not line or line.startswith("#"):
				continue
			fields = line.split()
			if fields[0] in ("ELEVATIONS", "PROPS", "NPCS"):
				section = fields[0]
			elif fields[0] == "PLAYER":
				self.player = tuple(map(float, fields[1:]))
			elif section == "ELEVATIONS":
				kind = fields[0]
				x, y, width, height = map(int, fields[1:5])
				start = float(fields[5])
				end = float(fields[6]) if kind == "stairs" else start
				axis = fields[7] if kind == "stairs" else None
				assert 0 <= x < x + width <= self.width and 0 <= y < y + height <= self.height
				assert isfinite(start) and isfinite(end) and 0 <= min(start, end) <= max(start, end) <= 12
				assert kind in ("stairs", "terrace") and axis in (None, "x", "y")
				sx = (end - start) / width if axis == "x" else 0
				sy = (end - start) / height if axis == "y" else 0
				assert abs(sx + sy) / TREADS <= MAX_STEP, "Stair risers exceed the climb limit"
				self.regions.append((kind, x, y, width, height, start, end, axis))
				for ty in range(y, y + height):
					for tx in range(x, x + width):
						assert (tx, ty) not in covered, "Elevation regions overlap"
						covered.add((tx, ty))
						self.surfaces[ty][tx] = (start + (tx - x + .5) * sx + (ty - y + .5) * sy, sx, sy)
						self.tiles[ty][tx] = "="
			elif section == "PROPS":
				kind, x, y, width, height, variant = fields
				x, y, width, height = map(float, (x, y, width, height))
				self.props.append((kind, x, y, width, height, int(variant)))
				assert 0 <= x < self.width and 0 <= y < self.height
				if kind in ("house", "ruin", "shrine", "keep", "tower", "wall"):
					self.blockers.append((x, y, x + width, y + height))
					for ty in range(max(0, floor(y) - 1), min(self.height, ceil(y + height) + 1)):
						for tx in range(max(0, floor(x) - 1), min(self.width, ceil(x + width) + 1)):
							if self.tiles[ty][tx] in "Tto":
								self.tiles[ty][tx] = ":"
				elif kind == "gate":
					pier = dimension("GATE_PIER_WIDTH")
					self.blockers.extend(((x, y, x + pier, y + height),
										  (x + width - pier, y, x + width, y + height)))
				elif kind == "torii":
					span = dimension("TORII_HALF_SPAN")
					rx, ry = dimension("TORII_BASE_HALF_WIDTH"), dimension("TORII_BASE_HALF_DEPTH")
					self.blockers.extend(((x - span - rx, y - ry, x - span + rx, y + ry),
										  (x + span - rx, y - ry, x + span + rx, y + ry)))
				elif kind in ("well", "cart", "stone"):
					self.blockers.append((x - .45, y - .45, x + .45, y + .45))
			elif section == "NPCS":
				assert fields[0] not in self.npcs, "Duplicate NPC id"
				self.npcs[fields[0]] = tuple(map(float, fields[1:]))

	def tile_height(self, x, y, lx=.5, ly=.5):
		if not (0 <= x < self.width and 0 <= y < self.height):
			return 0
		height, sx, sy = self.surfaces[y][x]
		return height + (lx - .5) * sx + (ly - .5) * sy

	def elevation(self, x, y):
		tx, ty = floor(x), floor(y)
		if not (0 <= tx < self.width and 0 <= ty < self.height):
			return 0
		_, sx, sy = self.surfaces[ty][tx]
		lx, ly = x - tx, y - ty
		if sx:
			lx = (floor(lx * TREADS) + .5) / TREADS
		if sy:
			ly = (floor(ly * TREADS) + .5) / TREADS
		return self.tile_height(tx, ty, lx, ly)

	def blocked(self, x, y, radius=BODY_RADIUS):
		rectangles = []
		for ty in range(floor(y - radius), floor(y + radius) + 1):
			for tx in range(floor(x - radius), floor(x + radius) + 1):
				if not (0 <= tx < self.width and 0 <= ty < self.height) or self.tiles[ty][tx] in "T~o":
					rectangles.append((tx, ty, tx + 1, ty + 1))
		for x0, y0, x1, y1 in rectangles + self.blockers:
			if hypot(x - min(max(x, x0), x1), y - min(max(y, y0), y1)) < radius:
				return True
		return False

	def can_move(self, a, b):
		steps = ceil(hypot(b[0] - a[0], b[1] - a[1]) / .2) + 1
		previous = self.elevation(*a)
		offsets = ((1, 0), (-1, 0), (0, 1), (0, -1),
				   (.7071, .7071), (-.7071, .7071), (.7071, -.7071), (-.7071, -.7071))
		for i in range(1, steps + 1):
			x = a[0] + (b[0] - a[0]) * i / steps
			y = a[1] + (b[1] - a[1]) * i / steps
			height = self.elevation(x, y)
			if self.blocked(x, y) or abs(height - previous) > MAX_STEP + 1e-6:
				return False
			if any(abs(self.elevation(x + dx * BODY_RADIUS, y + dy * BODY_RADIUS) - height) > MAX_STEP + 1e-6
				   for dx, dy in offsets):
				return False
			previous = height
		return True

	def reachable(self):
		root = tuple(floor(value) for value in self.player)
		center = lambda cell: (cell[0] + .5, cell[1] + .5)
		assert self.can_move(self.player, center(root)), "Player spawn cannot enter its navigation cell"
		found, queue = {root}, deque([root])
		while queue:
			cell = queue.popleft()
			for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
				nxt = cell[0] + dx, cell[1] + dy
				if nxt not in found and 0 <= nxt[0] < self.width and 0 <= nxt[1] < self.height:
					if self.can_move(center(cell), center(nxt)):
						found.add(nxt)
						queue.append(nxt)
		return found


def main():
	village = Village(ROOT / "SimpleGame/Data/village.map")
	reached = village.reachable()
	for name, position in village.npcs.items():
		cell = tuple(floor(value) for value in position)
		assert not village.blocked(*position), f"NPC {name} stands in a blocker"
		assert cell in reached and village.can_move(position, (cell[0] + .5, cell[1] + .5)), f"NPC {name} is unreachable"
	for kind, x, y, width, height, start, end, axis in village.regions:
		if kind == "stairs":
			a = (x - .5, y + height * .5) if axis == "x" else (x + width * .5, y - .5)
			b = (x + width + .5, y + height * .5) if axis == "x" else (x + width * .5, y + height + .5)
			assert village.can_move(a, b) and village.can_move(b, a), f"Stairs at {x},{y} do not connect both landings"
	for a, b in (((39.5, 18.5), (39.5, 17.5)), ((37.5, 41.5), (37.5, 42.5))):
		assert not village.blocked(*a) and not village.blocked(*b), "Cliff test must not rely on another blocker"
		assert not village.can_move(a, b) and not village.can_move(b, a), "A terrace cliff can be crossed without stairs"
	for kind, x, y, width, height, variant in village.props:
		if kind in ("keep", "tower", "wall", "house", "shrine"):
			heights = [village.elevation(px, py) for px in (x + .01, x + width - .01)
					   for py in (y + .01, y + height - .01)]
			assert all(isclose(value, heights[0], abs_tol=.01) for value in heights), f"{kind} at {x},{y} straddles a height seam"
	print(f"PASS: {village.width}x{village.height}; {len(village.props)} props; {len(village.npcs)} reachable NPCs; {len(reached)} reachable navigation cells")
	print("PASS: stair landings connect in both directions; cliffs block movement; structure foundations are level")


if __name__ == "__main__":
	main()
