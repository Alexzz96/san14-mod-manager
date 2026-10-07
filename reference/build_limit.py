"""Offline reference rule; this module does not attach to or modify the game.

Confirmed scope: same-owner earth/stone walls form one component, at most five.
One wall occupies one hex in this model. The game adapter must verify that.
An explicit continuation operation preserves the original game's decision.
"""

from dataclasses import dataclass
from typing import Mapping

WALL_KINDS = frozenset(("earth_wall", "stone_wall"))


@dataclass(frozen=True)
class Building:
    kind: str
    owner: int


@dataclass(frozen=True)
class Decision:
    allowed: bool
    reason: str
    component_count: int | None = None
    count_is_exact: bool = False
    neighbor_reads: int = 0


def neighbors(tile: int, width: int = 220, height: int = 220):
    if width <= 0 or height <= 0 or not 0 <= tile < width * height:
        raise ValueError("Invalid map dimensions or hex ID")
    x, y = tile % width, tile // width
    deltas = ((-1, -1), (0, -1), (1, -1), (-1, 0), (0, 1), (1, 0)) if x & 1 else (
        (-1, 0), (0, -1), (1, 0), (-1, 1), (0, 1), (1, 1))
    for dx, dy in deltas:
        nx, ny = x + dx, y + dy
        if 0 <= nx < width and 0 <= ny < height:
            yield ny * width + nx


def evaluate(cells: Mapping[int, Building], tile: int, proposed: Building,
             *, original_allowed: bool, continuation: bool = False,
             width: int = 220, height: int = 220) -> Decision:
    if width <= 0 or height <= 0 or not 0 <= tile < width * height:
        raise ValueError("Invalid map dimensions or hex ID")
    if not original_allowed:
        return Decision(False, "original_rule_rejected")
    if proposed.kind not in WALL_KINDS:
        return Decision(True, "outside_wall_scope")
    existing = cells.get(tile)
    if continuation:
        if existing != proposed:
            raise ValueError("Continuation requires the existing wall of the same type and owner")
        return Decision(True, "existing_wall_continuation")
    if existing is not None and existing.kind in WALL_KINDS:
        return Decision(False, "occupied_hex")

    # Count the proposed wall once. Stop at six, including when joining groups.
    # At most five nodes are expanded, so there are at most 30 neighbor reads.
    seen, pending, count, reads = {tile}, [tile], 1, 0
    while pending:
        for adjacent in neighbors(pending.pop(), width, height):
            if adjacent in seen:
                continue
            seen.add(adjacent)
            reads += 1
            wall = cells.get(adjacent)
            if wall is None or wall.owner != proposed.owner or wall.kind not in WALL_KINDS:
                continue
            count += 1
            if count > 5:
                return Decision(False, "connected_wall_limit", count, False, reads)
            pending.append(adjacent)
    return Decision(True, "within_limit", count, True, reads)
