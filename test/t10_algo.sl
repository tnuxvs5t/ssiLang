# ====================================================
# Algorithmic stress test — sorting, graph, DP, math
# ====================================================

# --- Merge Sort ---
fn merge(left, right) {
    result = []
    i = 0
    j = 0
    while i < len(left) and j < len(right) {
        if left[i] <= right[j] {
            result.push(left[i])
            i = i + 1
        } else {
            result.push(right[j])
            j = j + 1
        }
    }
    while i < len(left) {
        result.push(left[i])
        i = i + 1
    }
    while j < len(right) {
        result.push(right[j])
        j = j + 1
    }
    return result
}

fn mergesort(arr) {
    n = len(arr)
    if n <= 1 => return arr
    mid = int(n / 2)
    left = mergesort(arr.slice(0, mid))
    right = mergesort(arr.slice(mid, n))
    return merge(left, right)
}

print(mergesort([38, 27, 43, 3, 9, 82, 10]))
print(mergesort([]))
print(mergesort([1]))
print(mergesort([5, 4, 3, 2, 1]))

# --- Sieve of Eratosthenes ---
fn sieve(limit) {
    is_prime = []
    for i in range(limit + 1) {
        is_prime.push(true)
    }
    is_prime[0] = false
    is_prime[1] = false
    p = 2
    while p * p <= limit {
        if is_prime[p] {
            mul = p * p
            while mul <= limit {
                is_prime[mul] = false
                mul = mul + p
            }
        }
        p = p + 1
    }
    primes = []
    for i in range(limit + 1) {
        if is_prime[i] => primes.push(i)
    }
    return primes
}

print(sieve(50))

# --- Fibonacci DP (bottom-up) ---
fn fib_dp(n) {
    if n <= 1 => return n
    dp = [0, 1]
    for i in range(2, n + 1) {
        dp.push(dp[i - 1] + dp[i - 2])
    }
    return dp[n]
}

for i in range(15) {
    print(str(i) + " -> " + str(fib_dp(i)))
}

# --- Matrix multiplication ---
fn mat_mul(a, b) {
    rows_a = len(a)
    cols_a = len(a[0])
    cols_b = len(b[0])
    result = []
    for i in range(rows_a) {
        row = []
        for j in range(cols_b) {
            s = 0
            for k in range(cols_a) {
                s = s + a[i][k] * b[k][j]
            }
            row.push(s)
        }
        result.push(row)
    }
    return result
}

a = [[1, 2, 3], [4, 5, 6]]
b = [[7, 8], [9, 10], [11, 12]]
print(mat_mul(a, b))

# identity check
id3 = [[1, 0, 0], [0, 1, 0], [0, 0, 1]]
m = [[2, 3, 4], [5, 6, 7], [8, 9, 10]]
print(mat_mul(id3, m))
print(mat_mul(m, id3))

# --- Dijkstra (adjacency list, no priority queue) ---
fn dijkstra(graph, src, n) {
    dist = []
    visited = []
    for i in range(n) {
        dist.push(999999)
        visited.push(false)
    }
    dist[src] = 0

    for step in range(n) {
        # pick min unvisited
        u = -1
        best = 999999
        for i in range(n) {
            if not visited[i] and dist[i] < best {
                best = dist[i]
                u = i
            }
        }
        if u == -1 => break
        visited[u] = true

        # relax neighbors
        edges = graph[u]
        for e in edges {
            v = e[0]
            w = e[1]
            if dist[u] + w < dist[v] {
                dist[v] = dist[u] + w
            }
        }
    }
    return dist
}

# Graph: 5 nodes
# 0 --1--> 1 --2--> 2
# |         |        |
# 4         1        1
# v         v        v
# 3 --3--> 4 <--5---+
graph = [
    [[1, 1], [3, 4]],
    [[2, 2], [4, 1]],
    [[4, 1]],
    [[4, 3]],
    []
]
print(dijkstra(graph, 0, 5))

# --- Quick Power (modular exponentiation) ---
fn mod_pow(base, exp, m) {
    result = 1
    base = base % m
    while exp > 0 {
        if exp % 2 == 1 {
            result = result * base % m
        }
        exp = int(exp / 2)
        base = base * base % m
    }
    return result
}

print(mod_pow(2, 10, 1000))
print(mod_pow(3, 13, 1000000007))

# --- Knapsack 0/1 ---
fn knapsack(weights, values, cap) {
    n = len(weights)
    dp = []
    for i in range(n + 1) {
        row = []
        for j in range(cap + 1) {
            row.push(0)
        }
        dp.push(row)
    }
    for i in range(1, n + 1) {
        for w in range(cap + 1) {
            dp[i][w] = dp[i - 1][w]
            if weights[i - 1] <= w {
                val = dp[i - 1][w - weights[i - 1]] + values[i - 1]
                if val > dp[i][w] {
                    dp[i][w] = val
                }
            }
        }
    }
    return dp[n][cap]
}

print(knapsack([2, 3, 4, 5], [3, 4, 5, 6], 8))

# --- Longest Common Subsequence ---
fn lcs(a, b) {
    m = len(a)
    n = len(b)
    dp = []
    for i in range(m + 1) {
        row = []
        for j in range(n + 1) {
            row.push(0)
        }
        dp.push(row)
    }
    for i in range(1, m + 1) {
        for j in range(1, n + 1) {
            if a[i - 1] == b[j - 1] {
                dp[i][j] = dp[i - 1][j - 1] + 1
            } else {
                dp[i][j] = dp[i - 1][j]
                if dp[i][j - 1] > dp[i][j] {
                    dp[i][j] = dp[i][j - 1]
                }
            }
        }
    }
    return dp[m][n]
}

print(lcs("ABCBDAB".chars(), "BDCAB".chars()))

# --- Flatten nested list ---
fn flatten(arr) {
    result = []
    for item in arr {
        if type(item) == "list" {
            sub = flatten(item)
            for x in sub {
                result.push(x)
            }
        } else {
            result.push(item)
        }
    }
    return result
}

nested = [1, [2, [3, 4]], [5, [6, [7, 8]]], 9]
print(flatten(nested))

# --- Group by with closures ---
fn group_by(arr, key_fn) {
    groups = {}
    for item in arr {
        k = key_fn(item)
        existing = groups?[k]
        if existing == null {
            groups[k] = [item]
        } else {
            existing.push(item)
        }
    }
    return groups
}

data = [
    {name: "Alice", dept: "eng"},
    {name: "Bob", dept: "sales"},
    {name: "Carol", dept: "eng"},
    {name: "Dave", dept: "sales"},
    {name: "Eve", dept: "eng"}
]

grouped = group_by(data, x -> x.dept)
print(grouped.keys())
print(len(grouped["eng"]))
print(len(grouped["sales"]))

print("=== ALGO TESTS PASSED ===")
