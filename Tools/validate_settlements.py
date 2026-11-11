"""Static asset checks only; does not build or run the C++ game."""

from collections import Counter, deque, defaultdict
from functools import lru_cache
from math import floor, hypot
from pathlib import Path
import re
import validate_village

ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / "SimpleGame/Data"


def rows(name):
	return [line.split() for line in (DATA / name).read_text(encoding="utf-8").splitlines()
		if line and not line.startswith("#")]


def geometry():
	village = validate_village.Village(DATA / "village.map")
	# Index the same rectangles to make a larger source-data audit practical.
	buckets = defaultdict(list)
	for box in village.blockers:
		for y in range(floor((box[1] - 1) / 8), floor((box[3] + 1) / 8) + 1):
			for x in range(floor((box[0] - 1) / 8), floor((box[2] + 1) / 8) + 1):
				buckets[x, y].append(box)

	@lru_cache(maxsize=300000)
	def blocked(x, y, radius=.28):
		boxes = list(buckets[floor(x / 8), floor(y / 8)])
		for yy in range(floor(y - radius), floor(y + radius) + 1):
			for xx in range(floor(x - radius), floor(x + radius) + 1):
				if not (0 <= xx < village.width and 0 <= yy < village.height) or village.tiles[yy][xx] in "T~o":
					boxes.append((xx, yy, xx + 1, yy + 1))
		return any(hypot(x - min(max(x, a), c), y - min(max(y, b), d)) < radius for a, b, c, d in boxes)

	village.blocked = blocked
	village.elevation = lru_cache(maxsize=300000)(village.elevation)
	return village


def main():
	v = geometry()
	root = tuple(floor(x) for x in v.player)
	reached = {root}
	queue = deque([root])
	edges = {}
	center = lambda p: (p[0] + .5, p[1] + .5)
	while queue:
		a = queue.popleft()
		for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1)):
			b = a[0] + dx, a[1] + dy
			if b in reached or not (0 <= b[0] < v.width and 0 <= b[1] < v.height) or v.blocked(*center(b)):
				continue
			key = tuple(sorted((a, b)))
			if key not in edges:
				edges[key] = v.can_move(center(a), center(b)) and v.can_move(center(b), center(a))
			if edges[key]:
				reached.add(b)
				queue.append(b)
	profiles = rows("village_npcs.tsv")
	towns = rows("settlements.tsv")
	links = rows("village_routes.tsv")
	roster = rows("village_roster.tsv")
	assert all(len(row) == 2 for row in roster)
	assert len({row[0] for row in roster}) == len(roster), "Duplicate roster NPC"
	assert len(towns) == 5 and len(links) == 4
	assert len(profiles) == len(v.npcs) == len(roster)
	assert set(v.npcs) == {row[0] for row in profiles}
	assert {row[0]: row[7] for row in profiles} == dict(roster)
	counts = Counter(row[7] for row in profiles)
	assert counts == {"sanctuary": 12, "market": 7, "shrine": 7, "lakeside": 7, "frontier": 7}
	assert Counter(row[7] for row in profiles if row[8] == "1") == {row[0]: 2 for row in towns}
	assert {"elder", "smith", "fisher", "child", "hunter"} <= set(v.npcs)
	assert {"forge", "fish", "garden", "torch", "sentry", "hunt", "pray", "read", "carry"} <= {row[4] for row in profiles}
	for town in towns:
		assert {"guard", "healer"} <= {row[1] for row in profiles if row[7] == town[0]}, town[0]
	for name, point in v.npcs.items():
		cell = tuple(floor(x) for x in point)
		assert cell in reached and v.can_move(point, center(cell)), name
	centers = {row[0]: tuple(map(float, row[2:4])) for row in towns}
	for town in towns:
		plaza = centers[town[0]]
		assert any(row[7] == town[0] and row[8] == "0"
			and hypot(v.npcs[row[0]][0] - plaza[0], v.npcs[row[0]][1] - plaza[1]) <= 3.5
			and v.can_move(v.npcs[row[0]], plaza) for row in profiles), f"{town[0]} lacks a local rumor contact"
	for a, b in links:
		pa, pb = centers[a], centers[b]
		assert v.can_move(pa, pb), f"Road {a}-{b} must be directly walkable"
		seconds = hypot(pb[0] - pa[0], pb[1] - pa[1]) / 3.1
		assert 19.5 <= seconds <= 20.5
		candidates = []
		for cell in reached:
			x, y = center(cell)
			if any(hypot(x - float(t[2]), y - float(t[3])) <= float(t[4]) for t in towns):
				continue
			dx, dy = pb[0] - pa[0], pb[1] - pa[1]
			t = ((x - pa[0]) * dx + (y - pa[1]) * dy) / (dx * dx + dy * dy)
			if 0 < t < 1 and hypot(x - pa[0] - dx * t, y - pa[1] - dy * t) <= 12:
				candidates.append((x, y))
		assert len(candidates) >= 100, f"Insufficient relocation candidates on {a}-{b}"
		print(f"Road {a}-{b}: {seconds:.2f}s walk, {len(candidates)} reachable spawn candidates")
	dialogue = (DATA / "dialogue.txt").read_text(encoding="utf-8-sig")
	keys = re.findall(r"^@([^\r\n]+)", dialogue, re.M)
	assert len(keys) == len(set(keys))
	assert all(row[0] in keys for row in profiles if row[0] not in ("elder", "smith", "fisher", "child"))
	assert len(rows("rumors.tsv")) == 5
	assert all(row[1] in keys for row in rows("rumors.tsv"))
	print(f"PASS: {v.width}x{v.height}, {len(profiles)} home residents {dict(counts)}, 10 physical couriers, {len(reached)} connected navigation cells")
	print("PASS: complete profiles/dialogue, 4 direct 20-second links; no C++ build or gameplay run")


if __name__ == "__main__":
	main()
