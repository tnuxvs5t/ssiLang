# ====================================================
# Language feature stress test — closures, scoping,
# higher-order, derive, ref, expression composition
# ====================================================

# --- Closure counter factory ---
fn counter_factory(start) {
    state = [start]
    fn inc(n) {
        state[0] = state[0] + n
        return state[0]
    }
    fn get(dummy) {
        return state[0]
    }
    return {inc: inc, get: get}
}

c = counter_factory(0)
c.inc(1)
c.inc(5)
c.inc(3)
print(c.get(0))

# Two independent counters share nothing
c1 = counter_factory(100)
c2 = counter_factory(200)
c1.inc(10)
c2.inc(20)
print(c1.get(0))
print(c2.get(0))

# --- Currying via nested closures ---
fn adder(a) {
    fn inner(b) {
        return a + b
    }
    return inner
}

add5 = adder(5)
print(add5(3))
print(add5(10))
print(adder(100)(200))

# --- Pipeline with complex transforms ---
fn compose(f, g) {
    fn composed(x) {
        return g(f(x))
    }
    return composed
}

double = x -> x * 2
inc = x -> x + 1
square = x -> x * x

double_then_inc = compose(double, inc)
print(double_then_inc(5))

# chain of 3
fn compose3(f, g, h) {
    return compose(compose(f, g), h)
}

transform = compose3(inc, double, square)
print(transform(3))

# --- Map as poor man's object ---
fn make_stack() {
    data = []
    fn push(v) {
        data.push(v)
        return null
    }
    fn pop(dummy) {
        return data.pop()
    }
    fn peek(dummy) {
        return data[len(data) - 1]
    }
    fn size(dummy) {
        return len(data)
    }
    fn to_list(dummy) {
        return %copy data
    }
    return {push: push, pop: pop, peek: peek, size: size, to_list: to_list}
}

stk = make_stack()
stk.push(10)
stk.push(20)
stk.push(30)
print(stk.size(0))
print(stk.peek(0))
print(stk.to_list(0))
print(stk.pop(0))
print(stk.pop(0))
print(stk.size(0))

# --- Deep derive vs shallow derive ---
original = [[1, 2], [3, 4], [5, 6]]
shallow = %copy original
deep = %deep original

# mutate through shallow
shallow[0].push(99)
print(original[0])
print(deep[0])

# mutate deep — original untouched
deep[1].push(88)
print(original[1])

# --- Ref chaining ---
arr = [0, 0, 0, 0, 0]
refs = []
for i in range(5) {
    refs.push(&ref arr[i])
}
# write through refs
for i in range(5) {
    refs[i].set(i * i)
}
print(arr)

# read through refs
total = 0
for i in range(5) {
    total = total + refs[i].deref()
}
print(total)

# --- Complex split nesting ---
fn classify(score) {
    return score >= 90 => "A" || score >= 80 => "B" || score >= 70 => "C" || score >= 60 => "D" || "F"
}

for s in [95, 85, 75, 65, 55] {
    print(str(s) + " => " + classify(s))
}

# --- Recursive data structure (tree) ---
fn node(val, left, right) {
    return {val: val, left: left, right: right}
}

fn leaf(val) {
    return node(val, null, null)
}

fn tree_sum(n) {
    if n == null => return 0
    return n.val + tree_sum(n.left) + tree_sum(n.right)
}

fn tree_depth(n) {
    if n == null => return 0
    ld = tree_depth(n.left)
    rd = tree_depth(n.right)
    return 1 + (ld >= rd => ld || rd)
}

fn inorder(n) {
    if n == null => return []
    l = inorder(n.left)
    r = inorder(n.right)
    result = []
    for x in l { result.push(x) }
    result.push(n.val)
    for x in r { result.push(x) }
    return result
}

#       4
#      / \
#     2   6
#    / \ / \
#   1  3 5  7
tree = node(4,
    node(2, leaf(1), leaf(3)),
    node(6, leaf(5), leaf(7))
)

print(tree_sum(tree))
print(tree_depth(tree))
print(inorder(tree))

# --- Memoized fibonacci via map ---
fn make_memo_fib() {
    cache = {"0": 0, "1": 1}
    fn fib(n) {
        key = str(n)
        cached = cache?[key]
        if cached != null => return cached
        result = fib(n - 1) + fib(n - 2)
        cache[key] = result
        return result
    }
    return fib
}

memo_fib = make_memo_fib()
for i in range(20) {
    print(str(i) + ": " + str(memo_fib(i)))
}

# --- String processing pipeline ---
fn words(s) {
    return s.split(" ")
}

fn to_upper_first(s) {
    if len(s) == 0 => return s
    chars = s.chars()
    first = chars[0]
    # manual uppercase for ASCII lowercase
    code = 0
    if first == "a" { code = 65 }
    # Simplify: just return "X" + rest for testing
    return "*" + s
}

sentence = "the quick brown fox jumps over the lazy dog"
result = words(sentence) |> map(w -> "*" + w) |> join(" ")
print(result)

# word frequency
fn word_freq(s) {
    ws = s.split(" ")
    freq = {}
    for w in ws {
        c = freq?[w]
        if c == null {
            freq[w] = 1
        } else {
            freq[w] = c + 1
        }
    }
    return freq
}

print(word_freq("the cat sat on the mat the cat"))

# --- Higher-order: function composition array ---
fns = [
    x -> x + 1,
    x -> x * 2,
    x -> x - 3,
    x -> x * x
]

fn pipe_all(fns, val) {
    result = val
    for f in fns {
        result = f(result)
    }
    return result
}

print(pipe_all(fns, 5))

# --- Stress: nested for with closures ---
makers = []
for i in range(5) {
    # Capture i by creating a closure
    fn make(captured_i) {
        return x -> x + captured_i
    }
    makers.push(make(i))
}

results = []
for f in makers {
    results.push(f(100))
}
print(results)

# --- Iterator simulation ---
fn range_iter(start, end) {
    state = [start]
    fn next(dummy) {
        cur = state[0]
        if cur >= end => return null
        state[0] = cur + 1
        return cur
    }
    fn has_next(dummy) {
        return state[0] < end
    }
    return {next: next, has_next: has_next}
}

it = range_iter(0, 5)
collected = []
while it.has_next(0) {
    collected.push(it.next(0))
}
print(collected)

# --- net form round-trip ---
original_data = {
    users: [
        {name: "Alice", scores: [90, 85, 92]},
        {name: "Bob", scores: [78, 88, 95]}
    ],
    meta: {version: 1, active: true}
}
packet = %net original_data
print(type(packet))

print("=== LANG TESTS PASSED ===")
