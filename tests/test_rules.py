"""Independent geometry oracle and portable rule regression tests."""
from pathlib import Path
import random
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'reference'))
from build_limit import Building, evaluate, neighbors
EARTH = Building('earth_wall', 1)
STONE = Building('stone_wall', 1)
ENEMY = Building('earth_wall', 2)
OTHER = Building('tower', 1)

def h(x, y):
    return y * 220 + x

def decide(cells, tile=h(100, 100), wall=EARTH, **kwargs):
    return evaluate(cells, tile, wall, original_allowed=True, **kwargs)

def cube(tile, width):
    x, y = tile % width, tile // width
    z = y - (x + (x & 1)) // 2
    return x, -x - z, z

def oracle(cells, tile, owner, width, height):
    # Independent adjacency by cube distance, full component traversal.
    cubes = {i: cube(i, width) for i in range(width * height)}
    pending, seen = [tile], {tile}
    while pending:
        a = pending.pop()
        for b, wall in cells.items():
            if b in seen or wall.kind not in ("earth_wall", "stone_wall") or wall.owner != owner:
                continue
            if max(abs(u - v) for u, v in zip(cubes[a], cubes[b])) == 1:
                seen.add(b)
                pending.append(b)
    return len(seen)

class Rules(unittest.TestCase):
    def test_isolated_and_fifth(self):
        self.assertEqual(decide({}).component_count, 1)
        board = {h(100, y): EARTH for y in range(96, 100)}
        result = decide(board)
        self.assertTrue(result.allowed)
        self.assertEqual(result.component_count, 5)

    def test_sixth_and_existing_long_chain(self):
        for end in (95, 0):
            result = decide({h(100, y): EARTH for y in range(end, 100)})
            self.assertFalse(result.allowed)
            self.assertEqual(result.component_count, 6)
            self.assertFalse(result.count_is_exact)
            self.assertLessEqual(result.neighbor_reads, 30)

    def test_mixed_walls_and_bend(self):
        board = {h(100, 99): EARTH, h(100, 98): STONE, h(101, 98): STONE,
                 h(102, 98): EARTH, h(103, 98): STONE}
        self.assertEqual(oracle(board, h(100, 100), 1, 220, 220), 6)
        self.assertFalse(decide(board, wall=STONE).allowed)

    def test_branch(self):
        ring = list(neighbors(h(100, 100)))
        self.assertTrue(decide({i: EARTH for i in ring[:4]}).allowed)
        self.assertFalse(decide({i: EARTH for i in ring[:5]}).allowed)

    def test_join_two_groups(self):
        board = {h(100, y): EARTH for y in (98, 99, 101, 102)}
        self.assertEqual(decide(board).component_count, 5)
        board[h(100, 103)] = STONE
        self.assertFalse(decide(board).allowed)

    def test_cycle_is_not_counted_twice(self):
        ring = list(neighbors(h(100, 100)))
        board = {i: EARTH for i in ring[:4]}
        self.assertEqual(decide(board).component_count, 5)

    def test_enemy_and_other_buildings_do_not_bridge(self):
        for barrier in (ENEMY, OTHER):
            board = {h(100, y): EARTH for y in range(90, 99)}
            board[h(100, 99)] = barrier
            self.assertEqual(decide(board).component_count, 1)

    def test_other_facilities_are_unrestricted_by_additional_rule(self):
        board = {h(100, y): EARTH for y in range(90, 100)}
        self.assertTrue(decide(board, wall=OTHER).allowed)

    def test_original_rejection_is_preserved(self):
        for wall in (EARTH, OTHER):
            self.assertFalse(evaluate({}, h(100, 100), wall, original_allowed=False).allowed)

    def test_nonwall_replacement_preserves_original_permission(self):
        board={h(100,100):OTHER,h(100,99):EARTH}
        self.assertEqual(decide(board).component_count,2)
        self.assertFalse(evaluate(board,h(100,100),EARTH,original_allowed=False).allowed)

    def test_existing_wall_continuation_is_explicit(self):
        board = {h(100, y): EARTH for y in range(90, 101)}
        self.assertTrue(decide(board, continuation=True).allowed)
        self.assertFalse(decide(board).allowed)
        self.assertFalse(evaluate(board, h(100, 100), EARTH,
                                  original_allowed=False, continuation=True).allowed)
        with self.assertRaises(ValueError):
            decide(board, wall=STONE, continuation=True)

    def test_invalid_coordinates(self):
        for tile in (-1, 48400):
            with self.assertRaises(ValueError):
                decide({}, tile=tile)

    def test_entire_map_geometry(self):
        for tile in range(48400):
            near = tuple(neighbors(tile))
            self.assertLessEqual(len(near), 6)
            self.assertEqual(len(near), len(set(near)))
            for adjacent in near:
                self.assertIn(tile, neighbors(adjacent))
                self.assertEqual(max(abs(a - b) for a, b in zip(cube(tile, 220), cube(adjacent, 220))), 1)
        self.assertNotIn(220, neighbors(219))
        self.assertEqual(set(neighbors(0)), {1, 220, 221})
        self.assertEqual(set(neighbors(219)), {218, 439})

    def test_randomized_against_independent_oracle(self):
        rng = random.Random(20261007)
        for _ in range(1000):
            tile = rng.randrange(64)
            cells = {i: rng.choice((EARTH, STONE, ENEMY, OTHER)) for i in range(64)
                     if i != tile and rng.random() < 0.75}
            owner = rng.choice((1, 2))
            result = evaluate(cells, tile, Building("stone_wall", owner),
                              original_allowed=True, width=8, height=8)
            exact = oracle(cells, tile, owner, 8, 8)
            self.assertEqual(result.allowed, exact <= 5)
            self.assertEqual(result.component_count, min(exact, 6))
            self.assertLessEqual(result.neighbor_reads, 30)

    def test_dense_map_work_bound(self):
        board = {i: EARTH for i in range(48400) if i != h(100, 100)}
        result = decide(board)
        self.assertFalse(result.allowed)
        self.assertLessEqual(result.neighbor_reads, 30)

if __name__ == "__main__":
    unittest.main(verbosity=2)
