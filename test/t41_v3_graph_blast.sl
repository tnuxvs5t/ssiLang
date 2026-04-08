# Ad-hoc blast 2: recursion + graph + placeholder arity + map/list pressure

graph = {
    build: ["parse", "scan", "check"],
    parse: ["tokens"],
    scan: ["tokens"],
    check: ["parse", "scan"],
    tokens: [],
    test: ["build"],
    package: ["build", "test"],
    ship: ["package"]
}

fn topo(g) {
    visiting = {}
    visited = {}
    out = []

    fn walk(node) {
        if visiting?[node] == true => error("cycle:" + node)
        if visited?[node] == true => return null
        visiting[node] = true
        deps = g?[node]
        if deps == null {
            deps = []
        }
        for dep in deps {
            walk(dep)
        }
        visiting.remove(node)
        visited[node] = true
        out.push(node)
        return null
    }

    for node in g.keys() {
        walk(node)
    }
    return out
}

order = topo(graph)
print(order)
print(order.contains("ship"))
print(order.contains("tokens"))

name_lengths = order |> map(len($))
print(name_lengths)
print(name_lengths |> reduce($1 + $2, 0))

tri = $3 + $1 + $2
print(tri(10, 20, 30))

fn make_weighted_sum(base) {
    return $1 * @base + $2
}

score = make_weighted_sum(4)
print(score(7, 3))

pipeline = order
    |> filter(len($) >= 5)
    |> map($ + ":" + str(len($)))
print(pipeline)

pkg = {
    name: "ship",
    deps: graph.ship,
    meta: {
        owner: "release",
        size: 3
    }
}

desc = @(pkg) => @.name + "|" + @.meta.owner + "|" + str(len(@.deps))
print(desc)

fn all_edges(g) {
    edges = []
    for key in g.keys() {
        deps = g[key]
        for dep in deps {
            edges.push(key + "->" + dep)
        }
    }
    return edges
}

edges = all_edges(graph)
print(len(edges))
print(edges |> find($.contains("ship->package")))

print("=== V3 GRAPH BLAST PASSED ===")
