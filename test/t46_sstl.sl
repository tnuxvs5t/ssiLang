S = import("../modules/sstl")

# fill / grid must create independent nested containers
rows = S.fill(3, [0, 0])
rows[0][0] = 7
print(rows)

board = S.grid(2, 3, 0)
board[1][2] = 9
print(board)

diag = S.grid_with(4, 4, (i, j) -> i == j => 1 || 0)
print(diag[2][2])
print(diag[2][1])

# nested builder
nested = S.build(
    S.fields({
        matrix: S.seq(3, S.seq(3, S.delay(path -> path[1] == path[2] => 1 || 0))),
        buckets: S.dict(["lo", "hi"], S.seq(2, 0))
    })
)
print(nested.matrix)
print(nested.buckets)

# queue + graph: BFS
g = S.undir_graph(6)
g.add_edge(0, 1, 1)
g.add_edge(0, 2, 1)
g.add_edge(1, 3, 1)
g.add_edge(2, 4, 1)
g.add_edge(4, 5, 1)

dist = S.fill(6, -1)
q = S.queue()
dist[0] = 0
q.push(0)

while not q.empty() {
    u = q.pop()
    for edge in g.neighbors(u) {
        v = edge[0]
        if dist[v] == -1 {
            dist[v] = dist[u] + 1
            q.push(v)
        }
    }
}
print(dist)
print(q.empty())

# heap
h = S.min_heap()
for x in [5, 1, 4, 2, 3] {
    h.push(x)
}
heap_sorted = []
while not h.empty() {
    heap_sorted.push(h.pop())
}
print(heap_sorted)

pair_heap = S.heap_by((a, b) -> a[1] < b[1])
pair_heap.push(["node-3", 30])
pair_heap.push(["node-1", 10])
pair_heap.push(["node-2", 20])
print(pair_heap.pop()[0])
print(pair_heap.pop()[0])
print(pair_heap.pop()[0])

# set / counter
seen = S.set_of(["b", "a", "b", "c"])
print(sort(seen.members()))
print(seen.contains("a"))
print(seen.contains("z"))

freq = S.count_all(["a", "b", "a", "c", "a", "b"])
print(freq.get("a"))
print(freq.get("b"))
print(freq.get("missing"))
print(freq.raw())

print("=== SSTL TESTS PASSED ===")
