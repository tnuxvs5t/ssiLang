# ====================================================
# Network module tests — %net, &shm, &pipe
# ====================================================
# Tests %net serialization and &shm (shared memory).
# Pipe support is implemented on both Windows and POSIX.
# This file focuses on %net / shm behavior; full tcp/pipe
# round-trip coverage lives in t23_ipc_full.sl.

# =====================
# Test 1: %net form — serialize various types
# =====================
print("--- Net Test 1: %net form types ---")

n1 = %net 42
print(type(n1))

n2 = %net "hello"
print(type(n2))

n3 = %net [1, 2, 3]
print(type(n3))

n4 = %net {a: 1, b: "two", c: true}
print(type(n4))

n5 = %net null
print(type(n5))

n6 = %net true
print(type(n6))

# Nested structure
n7 = %net {
    users: [
        {name: "Alice", scores: [90, 85]},
        {name: "Bob", scores: [78, 92]}
    ],
    count: 2,
    active: true
}
print(type(n7))
print("all 7 net-forms: OK")

# =====================
# Test 2: %net preserves type info
# =====================
print("--- Net Test 2: %net is distinct type ---")

original = [1, 2, 3]
packet = %net original
print("original type: " + type(original))
print("net-form type: " + type(packet))
print("types differ: " + str(type(original) != type(packet)))

# =====================
# Test 3: &shm — basic store/load round-trip
# =====================
print("--- Net Test 3: &shm basic ---")

slot = &shm("sl_test_basic")

# Number
slot.store(%net 42)
v = slot.load()
print("number: " + str(v))

# String
slot.store(%net "hello world")
v2 = slot.load()
print("string: " + str(v2))

# List
slot.store(%net [1, 2, 3, 4, 5])
v3 = slot.load()
print("list: " + str(v3))

# Map
slot.store(%net {x: 10, y: 20})
v4 = slot.load()
print("map: " + str(v4))

# Boolean
slot.store(%net true)
print("true: " + str(slot.load()))

slot.store(%net false)
print("false: " + str(slot.load()))

# Null
slot.store(%net null)
print("null: " + str(slot.load()))

slot.close()
print("shm basic: OK")

# =====================
# Test 4: &shm — nested structure round-trip
# =====================
print("--- Net Test 4: &shm nested ---")

slot2 = &shm("sl_test_nested")

nested = {
    data: [1, [2, 3]],
    flag: true,
    tag: "ok",
    inner: {a: [10, 20], b: "deep"}
}

slot2.store(%net nested)
loaded = slot2.load()

print("data: " + str(loaded.data))
print("flag: " + str(loaded.flag))
print("tag: " + str(loaded.tag))
print("inner.a: " + str(loaded.inner.a))
print("inner.b: " + str(loaded.inner.b))

slot2.close()

# =====================
# Test 5: &shm — overwrite semantics
# =====================
print("--- Net Test 5: &shm overwrite ---")

slot3 = &shm("sl_test_overwrite")

# Write 10 times, only last should persist
for i in range(10) {
    slot3.store(%net i)
}
final_val = slot3.load()
print("after 10 writes: " + str(final_val))

# Overwrite with different type
slot3.store(%net "now a string")
print("after type change: " + str(slot3.load()))

slot3.store(%net [99, 98, 97])
print("after list write: " + str(slot3.load()))

slot3.close()

# =====================
# Test 6: &shm — use as data exchange between functions
# =====================
print("--- Net Test 6: &shm data exchange ---")

# Use a single shm handle — producer stores, consumer reads
# (On Windows, closing the handle destroys the mapping,
#  so we must keep it alive between store and load.)
fn compute_primes(limit) {
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

exchange = &shm("sl_prime_exchange")
computed = compute_primes(50)
exchange.store(%net computed)
primes = exchange.load()
exchange.close()
print("primes via shm: " + str(primes))

expected = [2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47]
ok = true
if len(primes) != len(expected) {
    ok = false
} else {
    for i in range(len(expected)) {
        if primes[i] != expected[i] {
            ok = false
        }
    }
}
print("primes match: " + str(ok))

# =====================
# Test 7: &shm — stress with larger data
# =====================
print("--- Net Test 7: &shm stress ---")

slot4 = &shm("sl_test_stress")

# Store a list of 100 elements
big = []
for i in range(100) {
    big.push(i * i)
}
slot4.store(%net big)
loaded_big = slot4.load()

print("stored 100 elements, loaded: " + str(len(loaded_big)))

ok2 = true
for i in range(100) {
    if loaded_big[i] != i * i {
        ok2 = false
    }
}
print("data integrity: " + str(ok2))

slot4.close()

# =====================
# Test 8: &shm — multiple independent slots
# =====================
print("--- Net Test 8: &shm multiple slots ---")

s1 = &shm("sl_slot_a")
s2 = &shm("sl_slot_b")
s3 = &shm("sl_slot_c")

s1.store(%net "alpha")
s2.store(%net "beta")
s3.store(%net "gamma")

# Read them back — each slot independent
print("slot a: " + str(s1.load()))
print("slot b: " + str(s2.load()))
print("slot c: " + str(s3.load()))

# Overwrite one, others unchanged
s2.store(%net "BETA_V2")
print("slot a: " + str(s1.load()))
print("slot b: " + str(s2.load()))
print("slot c: " + str(s3.load()))

s1.close()
s2.close()
s3.close()

# =====================
# Test 9: &shm — map with many keys
# =====================
print("--- Net Test 9: &shm map with many keys ---")

slot5 = &shm("sl_test_mapkeys")

big_map = {}
for i in range(20) {
    big_map[str(i)] = i * 10
}

slot5.store(%net big_map)
loaded_map = slot5.load()

# Verify some keys
print("key 0: " + str(loaded_map["0"]))
print("key 10: " + str(loaded_map["10"]))
print("key 19: " + str(loaded_map["19"]))
print("total keys: " + str(len(loaded_map.keys())))

slot5.close()

# =====================
# Test 10: %derive family comparison
# =====================
print("--- Net Test 10: derive family ---")

data = [[1, 2], [3, 4]]

# %copy — shallow copy
c = %copy data
c.push([5, 6])
print("copy: original len=" + str(len(data)) + " copy len=" + str(len(c)))
# shallow: inner lists still shared
c[0].push(99)
print("shallow share: original[0]=" + str(data[0]))

# %deep — deep copy
d = %deep data
d[0].push(77)
print("deep: original[0]=" + str(data[0]) + " deep[0]=" + str(d[0]))

# %snap — snapshot (falls back to deep in this impl)
s = %snap data
s[1].push(88)
print("snap: original[1]=" + str(data[1]) + " snap[1]=" + str(s[1]))

# %net — serialization form
n = %net data
print("net type: " + type(n))

print("=== NETWORK TESTS PASSED ===")
