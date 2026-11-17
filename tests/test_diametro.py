"""Pruebas de caja negra; Python solo verifica, nunca genera la entrega C++."""
import collections
import json
import os
from pathlib import Path
import random
import subprocess
import sys
import tempfile
import unittest

EXECUTABLE = Path(sys.argv.pop(1)).resolve()
ROOT = Path(__file__).resolve().parents[1]


def edge_id(u, v):
    return f"{min(u, v)}-{max(u, v)}"


def reference(n, edges, start):
    """Oraculo independiente con contenedores Python y comparacion de toda la traza."""
    adj = {u: [] for u in range(1, n + 1)}
    for u, v in edges:
        adj[u].append(v)
        adj[v].append(u)
    events = []

    def emit(phase, kind, **fields):
        events.append(dict(step=len(events), phase=phase, type=kind, **fields))

    def bfs(source, phase):
        dist, parents = {source: 0}, {source: None}
        emit(phase, "phase_start", start=source)
        emit(phase, "node_discovered", node=source, distance=0, parent=None)
        queue = collections.deque([source])
        emit(phase, "node_enqueued", node=source)
        while queue:
            u = queue.popleft()
            emit(phase, "node_dequeued", node=u)
            for v in adj[u]:
                fresh = v not in dist
                emit(phase, "edge_examined", **{"from": u, "to": v,
                     "edge_id": edge_id(u, v), "outcome": "new" if fresh else "already_seen"})
                if fresh:
                    dist[v], parents[v] = dist[u] + 1, u
                    emit(phase, "node_discovered", node=v, distance=dist[v], parent=u)
                    queue.append(v)
                    emit(phase, "node_enqueued", node=v)
            emit(phase, "node_finished", node=u)
        farthest = min(dist, key=lambda u: (-dist[u], u))
        emit(phase, "phase_end", start=source, farthest_node=farthest, max_distance=dist[farthest])
        return dict(id=phase, start=source, farthest_node=farthest,
                    max_distance=dist[farthest],
                    nodes=[dict(node=u, distance=dist[u], parent=parents[u]) for u in range(1, n + 1)])

    first = bfs(start, "bfs_1")
    second = bfs(first["farthest_node"], "bfs_2")
    a, b = first["farthest_node"], second["farthest_node"]
    path, current = [], b
    while current is not None:
        path.append(current)
        current = second["nodes"][current - 1]["parent"]
    path.reverse()
    path_edges = [edge_id(u, v) for u, v in zip(path, path[1:])]
    diameter = dict(length=len(path) - 1, endpoints=[a, b], path_nodes=path, path_edges=path_edges)
    emit("result", "diameter_path", nodes=path, edges=path_edges)
    emit("result", "diameter_result", endpoints=[a, b], length=len(path) - 1)
    return dict(schema_version="1.0", algorithm="tree_diameter_two_bfs",
                graph=dict(type="undirected_unweighted_tree", node_count=n,
                           nodes=list(range(1, n + 1)),
                           edges=[dict(id=edge_id(u, v), source=u, target=v) for u, v in edges]),
                config=dict(initial_node=start, neighbor_order="input_order",
                            farthest_tie_breaker="smallest_node_id", distance_unit="edges"),
                runs=[first, second], diameter=diameter, events=events,
                stats=dict(bfs_runs=2, events_count=len(events)))


class DiameterTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="diametro-")
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.input = self.directory / "entrada.txt"
        self.output = self.directory / "salida.json"

    def execute(self, args=None, output=None):
        return subprocess.run([str(EXECUTABLE), "--input", str(self.input), "--output",
                               str(output or self.output), *(args or [])],
                              capture_output=True, text=True, timeout=30)

    def valid(self, n, edges, start=1, brute=True):
        self.input.write_text(str(n) + "\n" + "".join(f"{u} {v}\n" for u, v in edges), encoding="utf-8")
        completed = self.execute(["--start", str(start)])
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertFalse(completed.stderr)
        data = json.loads(self.output.read_text(encoding="utf-8"))
        self.assertEqual(data, reference(n, edges, start))
        self.assertEqual(len(data["events"]), 12 * n + 2)
        self.assertFalse(list(self.directory.glob("*.tmp.*")))
        # Independientemente de los dos BFS, comprobar todas las parejas.
        if brute:
            adj = [[] for _ in range(n + 1)]
            for u, v in edges:
                adj[u].append(v)
                adj[v].append(u)
            longest = 0
            for origin in range(1, n + 1):
                seen, queue = {origin}, collections.deque([(origin, 0)])
                while queue:
                    u, distance = queue.popleft()
                    longest = max(longest, distance)
                    for v in adj[u]:
                        if v not in seen:
                            seen.add(v)
                            queue.append((v, distance + 1))
            self.assertEqual(data["diameter"]["length"], longest)
        return data

    def invalid(self, text, args=None):
        self.input.write_text(text, encoding="utf-8")
        self.output.write_bytes(b"resultado anterior intacto")
        completed = self.execute(args)
        self.assertNotEqual(completed.returncode, 0)
        self.assertIn("Error:", completed.stderr)
        self.assertFalse(completed.stdout)
        self.assertEqual(self.output.read_bytes(), b"resultado anterior intacto")
        self.assertFalse(list(self.directory.glob("*.tmp.*")))
        self.output.unlink()
        self.assertNotEqual(self.execute(args).returncode, 0)
        self.assertFalse(self.output.exists())

    def test_t1_demo_and_generated_fixture(self):
        data = self.valid(7, [(1, 2), (2, 3), (3, 4), (2, 5), (5, 6), (6, 7)])
        self.assertEqual(data["diameter"]["path_nodes"], [7, 6, 5, 2, 3, 4])
        self.assertEqual(data["stats"]["events_count"], 86)
        self.assertEqual(self.output.read_bytes(), (ROOT / "outputs/animacion_diametro.json").read_bytes())

    def test_t2_one_node_and_generated_fixture(self):
        data = self.valid(1, [])
        self.assertEqual(data["diameter"], dict(length=0, endpoints=[1, 1], path_nodes=[1], path_edges=[]))
        self.assertEqual(self.output.read_bytes(), (ROOT / "outputs/animacion_un_nodo.json").read_bytes())

    def test_t3_two_nodes(self):
        self.valid(2, [(1, 2)])

    def test_t4_chain(self):
        self.valid(5, [(1, 2), (2, 3), (3, 4), (4, 5)])

    def test_t5_star(self):
        self.assertEqual(self.valid(4, [(1, 2), (1, 3), (1, 4)])["diameter"]["endpoints"], [2, 3])

    def test_t6_ties_reversed_order(self):
        self.assertEqual(self.valid(4, [(4, 1), (3, 1), (2, 1)])["diameter"]["endpoints"], [2, 3])

    def test_t7_duplicate(self):
        for text in ["3\n1 2\n2 1\n", "3\n1 2\n1 2\n"]:
            with self.subTest(text=text): self.invalid(text)

    def test_t8_loop(self):
        self.invalid("3\n1 2\n2 2\n")

    def test_t9_disconnected_with_cycle(self):
        self.invalid("4\n1 2\n2 3\n3 1\n")

    def test_t10_invalid_node(self):
        for text in ["2\n0 1\n", "2\n1 3\n", "2\n-1 1\n"]:
            with self.subTest(text=text): self.invalid(text)

    def test_t11_empty_and_incomplete(self):
        for text in ["", "\n \n", "0\n", "-1\n", "3\n1 2\n", "2\n1\n"]:
            with self.subTest(text=text): self.invalid(text)

    def test_t12_output_unavailable(self):
        self.input.write_text("1\n", encoding="utf-8")
        for destination in [self.directory / "missing" / "out.json", self.directory]:
            with self.subTest(destination=str(destination)):
                completed = self.execute(output=destination)
                self.assertNotEqual(completed.returncode, 0)
                self.assertIn("Error:", completed.stderr)
                self.assertFalse(completed.stdout)
                self.assertFalse(list(self.directory.glob("*.tmp.*")))

    def test_malformed_and_extra_data(self):
        for text in ["1 2\n", "1\n2 3\n", "2\n1 2 3\n", "2\n1 x\n", "2\n1.0 2\n",
                     "2\n1 2\nextra\n", "2147483648\n", "2\n+ 2\n", "2\n1\x002\n",
                     "2\n1\n2\n", "1 # comentario\n"]:
            with self.subTest(text=text): self.invalid(text)

    def test_cli_invalid(self):
        for args in [[], ["--input"], ["--unknown", "x"], ["--help", "x"]]:
            with self.subTest(args=args):
                completed = subprocess.run([str(EXECUTABLE), *args], capture_output=True, text=True)
                self.assertNotEqual(completed.returncode, 0)
                self.assertIn("Error:", completed.stderr)
        for args in [["--start", "0"], ["--start", "2"], ["--start", "-1"],
                     ["--start", "1x"], ["--start", "9999999999999999999999999"],
                     ["--start", "1", "--start", "1"], ["--input", "again"],
                     ["--output", "again"], ["--start"]]:
            with self.subTest(args=args): self.invalid("1\n", args)

    def test_help(self):
        completed = subprocess.run([str(EXECUTABLE), "--help"], capture_output=True, text=True)
        self.assertEqual(completed.returncode, 0)
        self.assertIn("Uso:", completed.stdout)

    def test_missing_input(self):
        self.assertNotEqual(self.execute().returncode, 0)
        self.assertFalse(self.output.exists())

    def test_blank_lines_and_crlf(self):
        self.input.write_bytes(b"\r\n  2\r\n\t\r\n1\t2\r\n\r\n")
        completed = self.execute()
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(json.loads(self.output.read_text()), reference(2, [(1, 2)], 1))

    def test_input_never_overwritten(self):
        self.input.write_text("1\n", encoding="utf-8")
        completed = self.execute(output=self.directory / "." / "entrada.txt")
        self.assertNotEqual(completed.returncode, 0)
        self.assertEqual(self.input.read_text(), "1\n")

    def test_repeat_replace_and_temp_collision(self):
        self.valid(5, [(1, 2), (2, 3), (3, 4), (4, 5)])
        original = self.output.read_bytes()
        collision = Path(str(self.output) + ".tmp.0")
        collision.write_bytes(b"archivo ajeno")
        completed = self.execute()
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(self.output.read_bytes(), original)
        self.assertEqual(collision.read_bytes(), b"archivo ajeno")
        collision.unlink()
        self.assertFalse(list(self.directory.glob("*.tmp.*")))

    def test_destination_read_only(self):
        # En POSIX, renombrar depende del permiso del directorio, no del archivo.
        if os.name != "nt":
            self.skipTest("Prueba especifica del reemplazo de archivo de solo lectura en Windows")
        self.input.write_text("1\n", encoding="utf-8")
        self.output.write_bytes(b"anterior")
        self.output.chmod(0o444)
        try:
            completed = self.execute()
            self.assertNotEqual(completed.returncode, 0)
            self.assertFalse(completed.stdout)
            self.assertEqual(self.output.read_bytes(), b"anterior")
            self.assertFalse(list(self.directory.glob("*.tmp.*")))
        finally:
            self.output.chmod(0o666)

    def test_random_trees_all_pairs(self):
        rng = random.Random(20261116)
        for case in range(75):
            n = rng.randint(1, 35)
            labels = list(range(1, n + 1))
            rng.shuffle(labels)
            edges = [(labels[i], labels[rng.randrange(i)]) for i in range(1, n)]
            edges = [(u, v) if rng.randrange(2) else (v, u) for u, v in edges]
            rng.shuffle(edges)
            with self.subTest(case=case): self.valid(n, edges, rng.randint(1, n))

    def test_large_chain_no_recursion(self):
        n = 10000
        data = self.valid(n, [(u, u + 1) for u in range(1, n)], brute=False)
        self.assertEqual(data["diameter"]["length"], n - 1)

    def test_large_star_no_quadratic_duplicates(self):
        n = 10000
        data = self.valid(n, [(1, u) for u in range(n, 1, -1)], brute=False)
        self.assertEqual(data["diameter"]["endpoints"], [2, 3])


if __name__ == "__main__":
    unittest.main(verbosity=2)
