# sSTL: a small algorithm-oriented standard library for ssiLang v3

fn clone(v) => %deep v

fn _fresh(v) {
    t = type(v)
    if t == "list" => return %deep v
    if t == "map" => return %deep v
    return v
}

fn fill(n, value) {
    out = []
    for i in range(n) {
        out.push(_fresh(value))
    }
    return out
}

fn tabulate(n, make) {
    out = []
    for i in range(n) {
        out.push(make(i))
    }
    return out
}

fn grid(rows, cols, value) {
    out = []
    for i in range(rows) {
        out.push(fill(cols, value))
    }
    return out
}

fn grid_with(rows, cols, make) {
    out = []
    for i in range(rows) {
        row = []
        for j in range(cols) {
            row.push(make(i, j))
        }
        out.push(row)
    }
    return out
}

fn val(v) => {__sstl_kind: "val", value: v}
fn delay(make) => {__sstl_kind: "delay", make: make}
fn seq(n, item) => {__sstl_kind: "seq", size: n, item: item}
fn dict(keys, item) => {__sstl_kind: "dict", keys: keys, item: item}
fn fields(entries) => {__sstl_kind: "fields", entries: entries}

fn build(spec) {
    fn go(node, path) {
        if type(node) != "map" or node.__sstl_kind == null {
            return _fresh(node)
        }

        if node.__sstl_kind == "val" {
            return _fresh(node.value)
        }

        if node.__sstl_kind == "delay" {
            return node.make(path)
        }

        if node.__sstl_kind == "seq" {
            out = []
            for i in range(node.size) {
                next_path = %copy path
                next_path.push(i)
                out.push(go(node.item, next_path))
            }
            return out
        }

        if node.__sstl_kind == "dict" {
            out = {}
            for k in node.keys {
                next_path = %copy path
                next_path.push(k)
                out[k] = go(node.item, next_path)
            }
            return out
        }

        if node.__sstl_kind == "fields" {
            out = {}
            for k in node.entries.keys() {
                next_path = %copy path
                next_path.push(k)
                out[k] = go(node.entries[k], next_path)
            }
            return out
        }

        error("sstl.build: bad spec")
    }

    return go(spec, [])
}

fn key_of(v) => str(v)

fn set() {
    data = {}

    fn add(v) {
        @data[key_of(v)] = true
        return null
    }

    fn contains(v) => @data.has(key_of(v))

    fn erase(v) => @data.remove(key_of(v))

    fn size() => len(@data)

    fn empty() => len(@data) == 0

    fn members() => @data.keys()

    fn raw() => %copy @data

    return {
        add: add,
        contains: contains,
        erase: erase,
        size: size,
        empty: empty,
        members: members,
        raw: raw
    }
}

fn set_of(xs) {
    s = set()
    for x in xs {
        s.add(x)
    }
    return s
}

fn counter() {
    data = {}

    fn add(k, delta) {
        step = delta == null => 1 || delta
        kk = key_of(k)
        cur = @data?[kk]
        if cur == null {
            @data[kk] = step
        } else {
            @data[kk] = cur + step
        }
        return @data[kk]
    }

    fn get(k) {
        cur = @data?[key_of(k)]
        if cur == null => return 0
        return cur
    }

    fn contains(k) => @data.has(key_of(k))

    fn erase(k) => @data.remove(key_of(k))

    fn size() => len(@data)

    fn raw() => %copy @data

    return {
        add: add,
        get: get,
        contains: contains,
        erase: erase,
        size: size,
        raw: raw
    }
}

fn count_all(xs) {
    c = counter()
    for x in xs {
        c.add(x, 1)
    }
    return c
}

fn queue() {
    data = []
    head = 0

    fn compact() {
        if @head > 32 and @head * 2 >= len(@data) {
            @data = @data.slice(@head, len(@data))
            @head = 0
        }
        return null
    }

    fn push(v) {
        @data.push(v)
        return null
    }

    fn pop() {
        if @head >= len(@data) => return null
        v = @data[@head]
        @head = @head + 1
        compact()
        return v
    }

    fn peek() {
        if @head >= len(@data) => return null
        return @data[@head]
    }

    fn size() => len(@data) - @head

    fn empty() => @head >= len(@data)

    fn items() {
        out = []
        for i in range(@head, len(@data)) {
            out.push(@data[i])
        }
        return out
    }

    return {
        push: push,
        pop: pop,
        peek: peek,
        size: size,
        empty: empty,
        items: items
    }
}

fn heap_by(less) {
    data = []

    fn swap(i, j) {
        tmp = @data[i]
        @data[i] = @data[j]
        @data[j] = tmp
        return null
    }

    fn up(i) {
        cur = i
        while cur > 0 {
            parent = int((cur - 1) / 2)
            if not @less(@data[cur], @data[parent]) => break
            swap(cur, parent)
            cur = parent
        }
        return null
    }

    fn down(i) {
        n = len(@data)
        cur = i
        while true {
            left = cur * 2 + 1
            right = left + 1
            best = cur

            if left < n and @less(@data[left], @data[best]) {
                best = left
            }
            if right < n and @less(@data[right], @data[best]) {
                best = right
            }
            if best == cur => break

            swap(cur, best)
            cur = best
        }
        return null
    }

    fn push(v) {
        @data.push(v)
        up(len(@data) - 1)
        return null
    }

    fn top() {
        if len(@data) == 0 => return null
        return @data[0]
    }

    fn pop() {
        if len(@data) == 0 => return null
        root = @data[0]
        last = @data.pop()
        if len(@data) > 0 {
            @data[0] = last
            down(0)
        }
        return root
    }

    fn size() => len(@data)

    fn empty() => len(@data) == 0

    fn items() => %copy @data

    return {
        push: push,
        pop: pop,
        top: top,
        size: size,
        empty: empty,
        items: items
    }
}

fn min_heap() => heap_by((a, b) -> a < b)
fn max_heap() => heap_by((a, b) -> a > b)

fn graph(n, directed) {
    adj = fill(n, [])

    fn add_edge(u, v, w) {
        weight = w == null => 1 || w
        @adj[u].push([v, weight])
        if not @directed {
            @adj[v].push([u, weight])
        }
        return null
    }

    fn neighbors(u) => @adj[u]

    fn size() => len(@adj)

    fn data_list() => %deep @adj

    return {
        add_edge: add_edge,
        neighbors: neighbors,
        size: size,
        data: data_list,
        directed: directed
    }
}

fn digraph(n) => graph(n, true)
fn undir_graph(n) => graph(n, false)
