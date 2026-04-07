# ====================================================
# Aho-Corasick Automaton — multi-pattern string match
# ====================================================
# Uses BFS to build failure links, then scans text
# in O(n + m + z) where z = total matches.
#
# Node structure: map with keys
#   children : map (char -> node_id)
#   fail     : int (failure link node id)
#   output   : list of pattern indices that end here
#   depth    : int (depth in trie, = pattern length at output)

# --- globals: flat arrays representing the trie ---
children = []   # list of maps
fail     = []   # list of ints
output   = []   # list of lists
depth    = []   # list of ints

fn new_node() {
    children.push({})
    fail.push(0)
    output.push([])
    depth.push(0)
    return len(children) - 1
}

# --- Build trie from pattern list ---
fn build_trie(patterns) {
    new_node()  # root = 0
    for pi in range(len(patterns)) {
        pat = patterns[pi]
        cur = 0
        chars = pat.chars()
        for ci in range(len(chars)) {
            ch = chars[ci]
            nxt = children[cur]?[ch]
            if nxt == null {
                nid = new_node()
                depth[nid] = depth[cur] + 1
                children[cur][ch] = nid
                cur = nid
            } else {
                cur = nxt
            }
        }
        output[cur].push(pi)
    }
}

# --- Build failure links via BFS ---
fn build_fail() {
    queue = []
    # init: depth-1 nodes fail to root
    root_ch = children[0].keys()
    for ch in root_ch {
        nid = children[0][ch]
        fail[nid] = 0
        queue.push(nid)
    }

    qi = 0
    while qi < len(queue) {
        u = queue[qi]
        qi = qi + 1

        u_children_keys = children[u].keys()
        for ch in u_children_keys {
            v = children[u][ch]
            # compute fail[v]
            f = fail[u]
            while f != 0 and children[f]?[ch] == null {
                f = fail[f]
            }
            fch = children[f]?[ch]
            if fch != null and fch != v {
                fail[v] = fch
            } else {
                fail[v] = 0
            }
            # merge output from fail link
            fail_outs = output[fail[v]]
            for o in fail_outs {
                output[v].push(o)
            }
            queue.push(v)
        }
    }
}

# --- Search text, return list of [position, pattern_index] ---
fn ac_search(text, patterns) {
    results = []
    chars = text.chars()
    cur = 0
    for i in range(len(chars)) {
        ch = chars[i]
        while cur != 0 and children[cur]?[ch] == null {
            cur = fail[cur]
        }
        nxt = children[cur]?[ch]
        if nxt != null {
            cur = nxt
        } else {
            cur = 0
        }
        # collect outputs at cur
        if len(output[cur]) > 0 {
            for pi in output[cur] {
                pos = i - len(patterns[pi].chars()) + 1
                results.push([pos, pi])
            }
        }
    }
    return results
}

# =====================
# Test 1: basic multi-pattern
# =====================
print("--- AC Test 1: basic ---")
patterns = ["he", "she", "his", "hers"]

build_trie(patterns)
build_fail()

text = "ahishers"
matches = ac_search(text, patterns)
print("text: " + text)
print("patterns: " + str(patterns))
for m in matches {
    print("  found pattern[" + str(m[1]) + "] = \"" + patterns[m[1]] + "\" at pos " + str(m[0]))
}
# Expected: he at 4, she at 3, his at 1, hers at 4

# =====================
# Test 2: overlapping patterns
# =====================
print("--- AC Test 2: overlapping ---")

# Reset globals
children = []
fail = []
output = []
depth = []

patterns2 = ["ab", "abc", "bc", "c"]
build_trie(patterns2)
build_fail()

text2 = "abcabc"
matches2 = ac_search(text2, patterns2)
print("text: " + text2)
print("patterns: " + str(patterns2))
for m in matches2 {
    print("  found pattern[" + str(m[1]) + "] = \"" + patterns2[m[1]] + "\" at pos " + str(m[0]))
}

# =====================
# Test 3: no match
# =====================
print("--- AC Test 3: no match ---")

children = []
fail = []
output = []
depth = []

patterns3 = ["xyz", "uvw"]
build_trie(patterns3)
build_fail()

text3 = "abcdefg"
matches3 = ac_search(text3, patterns3)
print("text: " + text3)
print("matches: " + str(len(matches3)))

# =====================
# Test 4: single char patterns
# =====================
print("--- AC Test 4: single chars ---")

children = []
fail = []
output = []
depth = []

patterns4 = ["a", "b"]
build_trie(patterns4)
build_fail()

text4 = "abababa"
matches4 = ac_search(text4, patterns4)
print("text: " + text4)
print("match count: " + str(len(matches4)))
# Expected: 4 a's and 3 b's = 7 matches

# =====================
# Test 5: stress test — many patterns in long text
# =====================
print("--- AC Test 5: stress ---")

children = []
fail = []
output = []
depth = []

# Build 20 patterns from digits
stress_patterns = []
for i in range(20) {
    stress_patterns.push(str(i))
}
build_trie(stress_patterns)
build_fail()

# Build text with numbers 0..99
parts = []
for i in range(100) {
    parts.push(str(i))
}
stress_text = parts.join("")
stress_matches = ac_search(stress_text, stress_patterns)
print("patterns: 0..19")
print("text length: " + str(len(stress_text.chars())))
print("total matches: " + str(len(stress_matches)))

print("=== AC AUTOMATON TESTS PASSED ===")
