S = import("../modules/sstl")

fn dijkstra(g, src) {
    n = g.size()
    inf = 1000000000
    dist = S.fill(n, inf)
    pq = S.heap_by((a, b) -> a[0] < b[0])

    dist[src] = 0
    pq.push([0, src])

    while not pq.empty() {
        cur = pq.pop()
        d = cur[0]
        u = cur[1]
        if d != dist[u] => continue

        for edge in g.neighbors(u) {
            v = edge[0]
            w = edge[1]
            nd = d + w
            if nd < dist[v] {
                dist[v] = nd
                pq.push([nd, v])
            }
        }
    }

    return dist
}

fn lcs_len(a, b) {
    m = len(a)
    n = len(b)
    dp = S.grid(m + 1, n + 1, 0)

    for i in range(1, m + 1) {
        for j in range(1, n + 1) {
            if a[i - 1] == b[j - 1] {
                dp[i][j] = dp[i - 1][j - 1] + 1
            } else {
                dp[i][j] = max(dp[i - 1][j], dp[i][j - 1])
            }
        }
    }

    return dp[m][n]
}

g = S.digraph(5)
g.add_edge(0, 1, 2)
g.add_edge(0, 2, 5)
g.add_edge(1, 2, 1)
g.add_edge(1, 3, 2)
g.add_edge(2, 3, 1)
g.add_edge(3, 4, 3)
g.add_edge(2, 4, 10)
print(dijkstra(g, 0))

print(lcs_len("ABAZDC", "BACBAD"))

state = S.build(
    S.fields({
        dist: S.seq(4, 999),
        used: S.seq(4, false),
        buckets: S.dict(["front", "back"], S.seq(2, 0))
    })
)
print(state)

print("=== SSTL ALGO TESTS PASSED ===")
