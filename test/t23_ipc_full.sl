# ====================================================
# IPC Full Test Suite
# Tests: pcall, tcp server/client, pipe, shm, JSON,
#        serialization V1, RAII
# ====================================================

# =====================
# Test 1: pcall — success
# =====================
print("--- Test 1: pcall success ---")
fn ok_fn() { return 42 }
r = pcall(ok_fn)
print("ok: " + str(r.ok))
print("value: " + str(r.value))

# =====================
# Test 2: pcall — error capture
# =====================
print("--- Test 2: pcall error ---")
fn err_fn() { error("boom") }
r2 = pcall(err_fn)
print("ok: " + str(r2.ok))
print("has error: " + str(r2.error.contains("boom")))

# =====================
# Test 3: pcall — div by zero
# =====================
print("--- Test 3: pcall div zero ---")
fn div0_fn() {
    x = 10 / 0
}
r3 = pcall(div0_fn)
print("ok: " + str(r3.ok))
print("has div0: " + str(r3.error.contains("Division")))

# =====================
# Test 4: TCP Server + Client — single-process
# =====================
print("--- Test 4: TCP server+client ---")
srv = tcp_listen("127.0.0.1:19876")
print("server type: " + type(srv))

cli = &tcp("127.0.0.1:19876")
peer = srv.accept()
print("peer type: " + type(peer))

# send from client, recv from server side
cli.send(%net {msg: "hello", n: 42})
got = peer.recv()
print("server got msg: " + str(got.msg))
print("server got n: " + str(got.n))

# send from server side back to client
peer.send(%net "pong")
reply = cli.recv()
print("client got: " + str(reply))

# =====================
# Test 5: TCP send_raw / recv_raw
# =====================
print("--- Test 5: send_raw/recv_raw ---")
cli.send_raw("HELLO")
raw = peer.recv_raw(5)
print("raw recv: " + raw)

peer.send_raw("BYE!")
raw2 = cli.recv_raw(4)
print("raw recv2: " + raw2)

# =====================
# Test 6: TCP peer_addr / set_timeout
# =====================
print("--- Test 6: peer_addr/set_timeout ---")
addr = peer.peer_addr()
print("peer_addr is string: " + str(type(addr) == "string"))
print("peer_addr has 127: " + str(addr.contains("127")))

# set_timeout should not error
cli.set_timeout(5)
print("set_timeout: OK")

cli.close()
peer.close()
srv.close()

# =====================
# Test 7: TCP server addr method
# =====================
print("--- Test 7: server addr ---")
srv2 = tcp_listen("127.0.0.1:19877")
sa = srv2.addr()
print("server addr has 19877: " + str(sa.contains("19877")))
srv2.close()

# =====================
# Test 8: Pipe — anonymous send/recv
# =====================
print("--- Test 8: Pipe anonymous ---")
p = &pipe()
p.send(%net [1, 2, 3])
got_p = p.recv()
print("pipe recv: " + str(got_p))

p.send(%net {key: "val"})
got_p2 = p.recv()
print("pipe map: " + str(got_p2.key))

# =====================
# Test 9: Pipe methods
# =====================
print("--- Test 9: Pipe methods ---")
print("is_named: " + str(p.is_named()))
print("name: " + str(p.name()))
p.flush()
print("flush: OK")
p.close()

# Test 10: Named pipe
# Single-script symmetric testing is easy to deadlock across
# platforms, so the named-pipe constructor path is covered in
# other tests and by manual multi-process validation.
print("--- Test 10: Named pipe (constructor path documented) ---")

# =====================
# Test 11: SHM — configurable size
# =====================
print("--- Test 11: SHM config size ---")
slot = &shm("ipc_test_sz", 8192)
print("size: " + str(slot.size()))
print("name: " + str(slot.name()))

# =====================
# Test 12: SHM — spinlock store/load
# =====================
print("--- Test 12: SHM spinlock ---")
for i in range(20) {
    slot.store(%net i)
}
val = slot.load()
print("after 20 writes: " + str(val))

# =====================
# Test 13: SHM — clear
# =====================
print("--- Test 13: SHM clear ---")
slot.store(%net "data")
print("before clear: " + str(slot.load()))
slot.clear()
print("after clear: " + str(slot.load()))
slot.close()

# =====================
# Test 14: SHM — owner logic (second handle survives first close)
# =====================
print("--- Test 14: SHM owner ---")
s1 = &shm("ipc_owner_test")
s1.store(%net "shared")
s2 = &shm("ipc_owner_test")
val2 = s2.load()
print("s2 read: " + str(val2))
s1.close()
# s2 should still work
val3 = s2.load()
print("s2 after s1 close: " + str(val3))
s2.close()

# =====================
# Test 15: JSON encode — all types
# =====================
print("--- Test 15: json_encode ---")
print(json_encode(42))
print(json_encode("hello"))
print(json_encode(true))
print(json_encode(false))
print(json_encode(null))
print(json_encode([1, 2, 3]))
print(json_encode({a: 1, b: "two"}))

# =====================
# Test 16: JSON parse
# =====================
print("--- Test 16: json_parse ---")
v1 = json_parse("42")
print("num: " + str(v1))

v2 = json_parse("\"hello\"")
print("str: " + str(v2))

v3 = json_parse("true")
print("bool: " + str(v3))

v4 = json_parse("null")
print("null: " + str(v4))

v5 = json_parse("[1,2,3]")
print("list: " + str(v5))

v6 = json_parse("{\"a\":1,\"b\":\"two\"}")
print("map a: " + str(v6.a))
print("map b: " + str(v6.b))

# =====================
# Test 17: JSON round-trip
# =====================
print("--- Test 17: JSON round-trip ---")
original = {
    name: "Alice",
    scores: [90, 85, 100],
    active: true,
    meta: {level: 3, tag: "pro"},
    nothing: null
}
encoded = json_encode(original)
decoded = json_parse(encoded)
re_encoded = json_encode(decoded)
print("round-trip match: " + str(encoded == re_encoded))

# =====================
# Test 18: JSON special chars
# =====================
print("--- Test 18: JSON special chars ---")
s = json_encode("line1\nline2\ttab\\slash\"quote")
print("escaped: " + s)
back = json_parse(s)
print("has newline: " + str(back.contains("\n")))
print("has tab: " + str(back.contains("\t")))

# =====================
# Test 19: Serialization V1 round-trip
# =====================
print("--- Test 19: V1 serialization ---")
data = {x: [1, "two", true, null], y: 3.14}
slot_v = &shm("ipc_v1_test")
slot_v.store(%net data)
loaded = slot_v.load()
print("x[0]: " + str(loaded.x[0]))
print("x[1]: " + str(loaded.x[1]))
print("x[2]: " + str(loaded.x[2]))
print("x[3]: " + str(loaded.x[3]))
print("y: " + str(loaded.y))
slot_v.close()

# =====================
# Test 20: RAII — handle in function, no close
# =====================
print("--- Test 20: RAII ---")
fn make_pipe() {
    p = &pipe()
    p.send(%net "raii_test")
    return p.recv()
}
raii_val = make_pipe()
print("raii result: " + str(raii_val))

fn make_shm() {
    s = &shm("ipc_raii_shm")
    s.store(%net 99)
    return s.load()
}
raii_shm = make_shm()
print("raii shm: " + str(raii_shm))

# =====================
# Test 21: pcall with IPC errors
# =====================
print("--- Test 21: pcall + IPC ---")
fn bad_tcp() {
    c = &tcp("127.0.0.1:1")
}
r_tcp = pcall(bad_tcp)
print("tcp error caught: " + str(r_tcp.ok == false))

print("=== IPC FULL TESTS PASSED ===")
