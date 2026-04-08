# sSTL Tutorial

`sSTL` is a small algorithm-oriented library for ssiLang v3.

It is not trying to be a full STL clone.
It solves the practical gaps that show up first when writing algorithm exercises:

- repeated container initialization is verbose
- nested list/map construction is easy to get wrong
- queue / heap / graph scaffolding is repetitive
- map-only string keys make "set" / "counter" patterns annoying to rewrite each time

The library lives at:

- [modules/sstl.sl](C:/Users/ssilZ/Desktop/ssilang_v_2_2/ssiLang/modules/sstl.sl)

## Import

If your script is at repo root:

```sl
S = import("modules/sstl")
```

If your script is inside `test/`:

```sl
S = import("../modules/sstl")
```

## Design Goal

The library follows three rules:

1. Return plain ssiLang values whenever possible.
2. Prefer a few composable primitives over many ad-hoc helpers.
3. Make nested container initialization safe by default.

That third point matters.
`sSTL.fill` / `sSTL.grid` / `sSTL.build` all create independent nested containers.
They do not reuse the same inner list/map by accident.

One more practical rule shaped this library:
library objects avoid method names like `has`, `values`, and `remove`,
because plain objects are maps in current v3, and map builtins take precedence over same-named stored functions.

## 1. Basic Constructors

### `fill(n, value)`

Build a list of length `n`.
Each slot gets a fresh deep copy of `value`.

```sl
S = import("modules/sstl")

rows = S.fill(3, [0, 0])
rows[0][0] = 7
print(rows)
```

Result:

```sl
[[7, 0], [0, 0], [0, 0]]
```

This is the first helper you want for DP / visited / distance arrays.

### `tabulate(n, make)`

Build a list by calling `make(i)` for each index.

```sl
sq = S.tabulate(5, i -> i * i)
print(sq)
```

### `grid(rows, cols, value)`

Build a 2D list filled with independent copies of `value`.

```sl
dp = S.grid(3, 4, 0)
```

This directly fixes the old "2D array must be hand-pushed row by row" pain for the common case.

### `grid_with(rows, cols, make)`

Build a 2D list by calling `make(i, j)`.

```sl
diag = S.grid_with(4, 4, (i, j) -> i == j => 1 || 0)
print(diag)
```

This is ideal for:

- identity matrices
- distance tables
- coordinate-based preprocessing

## 2. Arbitrary Nested Initialization

For more complex container shapes, use the spec builder.

The spec constructors are:

- `val(v)`
- `delay(make)`
- `seq(n, item_spec)`
- `dict(keys, item_spec)`
- `fields(entries)`
- `build(spec)`

### `val(v)`

Wrap a literal value.

```sl
S.val(0)
S.val([])
S.val({ok: true})
```

### `delay(make)`

Call `make(path)` at build time.
`path` is a list describing where you are in the nested structure.

This is how you express position-dependent initialization.

### `seq(n, item)`

Build a list of `n` elements from the same nested item spec.

### `dict(keys, item)`

Build a map with the given keys.
Every key gets a value built from `item`.

Remember: ssiLang maps only support string keys.

### `fields(entries)`

Build an object-like map where each field may have a different nested spec.

### `build(spec)`

Recursively execute the spec and return plain list/map/scalar values.

### Example: matrix + named buckets

```sl
S = import("modules/sstl")

nested = S.build(
    S.fields({
        matrix: S.seq(3, S.seq(3, S.delay(path -> path[1] == path[2] => 1 || 0))),
        buckets: S.dict(["lo", "hi"], S.seq(2, 0))
    })
)

print(nested.matrix)
print(nested.buckets)
```

This pattern is the answer to deeply nested initialization in current v3.
You can express things like:

- `list<list<int>>`
- `map<list<int>>`
- `list<map<list<int>>>`
- object-like maps with mixed nested fields

without writing bespoke loops for every shape.

## 3. Set and Counter

ssiLang has no native `set`.
`sSTL` provides a practical set/counter layer on top of maps.

Internally, keys are canonicalized with `str(...)`.

That means:

- good for algorithm use
- especially good for strings / numbers
- not a true strongly-typed hash set

### `set()`

```sl
s = S.set()
s.add("a")
s.add("b")
print(s.contains("a"))
print(s.members())
```

Methods:

- `add(v)`
- `contains(v)`
- `erase(v)`
- `size()`
- `empty()`
- `members()`
- `raw()`

### `set_of(xs)`

```sl
seen = S.set_of(["b", "a", "b", "c"])
print(sort(seen.members()))
```

### `counter()`

```sl
c = S.counter()
c.add("a", 1)
c.add("a", 1)
c.add("b", 3)
print(c.get("a"))
print(c.raw())
```

Methods:

- `add(k, delta?)`
- `get(k)`
- `contains(k)`
- `erase(k)`
- `size()`
- `raw()`

If `delta` is omitted, it defaults to `1`.

### `count_all(xs)`

```sl
freq = S.count_all(["a", "b", "a", "c", "a"])
print(freq.get("a"))
```

This is the common "frequency map" shortcut.

## 4. Queue

### `queue()`

The queue is implemented as a list plus a moving head index.
It periodically compacts itself, so repeated BFS-style `pop()` does not degrade into repeated shifting.

```sl
q = S.queue()
q.push(10)
q.push(20)
print(q.peek())
print(q.pop())
print(q.pop())
print(q.empty())
```

Methods:

- `push(v)`
- `pop()`
- `peek()`
- `size()`
- `empty()`
- `items()`

Empty `pop()` / `peek()` returns `null`.

This is the queue you want for:

- BFS
- topological traversal
- flood fill
- simple simulation problems

## 5. Heap

### `heap_by(less)`

Build a binary heap with a custom priority rule.

`less(a, b)` should return true when `a` has higher priority than `b`.

```sl
h = S.heap_by((a, b) -> a[1] < b[1])
h.push(["node-3", 30])
h.push(["node-1", 10])
h.push(["node-2", 20])
print(h.pop()[0])
```

Methods:

- `push(v)`
- `pop()`
- `top()`
- `size()`
- `empty()`
- `items()`

Empty `pop()` / `top()` returns `null`.

### `min_heap()`

```sl
h = S.min_heap()
for x in [5, 1, 4, 2, 3] {
    h.push(x)
}
```

### `max_heap()`

```sl
h = S.max_heap()
```

This is enough for:

- priority queues
- greedy problems
- Dijkstra / A* style frontiers
- online min/max maintenance

## 6. Graph Helper

### `graph(n, directed)`

Build an adjacency list graph of `n` nodes.

### `digraph(n)`

Shortcut for directed graphs.

### `undir_graph(n)`

Shortcut for undirected graphs.

Example:

```sl
g = S.undir_graph(5)
g.add_edge(0, 1, 1)
g.add_edge(0, 2, 1)
g.add_edge(2, 4, 1)
```

Methods:

- `add_edge(u, v, w?)`
- `neighbors(u)`
- `size()`
- `data()`
- `directed`

If `w` is omitted, it defaults to `1`.

## 7. Example: BFS

```sl
S = import("modules/sstl")

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
```

This is already enough to write a lot of basic graph problems cleanly.

## 8. Example: Dijkstra Frontier

```sl
S = import("modules/sstl")

pq = S.heap_by((a, b) -> a[0] < b[0])
pq.push([0, 0])   # [dist, node]
```

You still write the algorithm yourself, but the painful storage layer is gone.

## 9. Practical Limits

`sSTL` is intentionally small.

Current limits:

- no native set type; set/counter keys are canonicalized through `str(...)`
- no generic iterator abstraction
- no balanced tree / ordered map
- no union-find / segment tree / Fenwick tree yet
- the nested builder reserves the internal field name `__sstl_kind` inside build specs

So this library is not the final standard library.
It is the first pass that makes basic algorithm problems much less annoying in current v3.

## 10. Recommended Usage

Use this decision rule:

- repeated fixed-size list init: `fill`
- index-based construction: `tabulate`
- 2D table: `grid` / `grid_with`
- deep mixed nested structure: `build`
- visited / frequency / dedup: `set` / `counter`
- BFS / topo: `queue`
- priority queue: `heap_by` / `min_heap`
- graph scaffold: `graph` / `undir_graph` / `digraph`

Reference test:

- [test/t46_sstl.sl](C:/Users/ssilZ/Desktop/ssilang_v_2_2/ssiLang/test/t46_sstl.sl)
