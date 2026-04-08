# Ad-hoc blast 5: arity guards should raise language errors, not crash the VM

fn bad_list_push() {
    xs = []
    xs.push()
}

fn bad_split() {
    return "abc".split()
}

fn bad_ref_set() {
    r = &ref [1][0]
    r.set()
}

fn bad_tcp_send_raw() {
    srv = tcp_listen("127.0.0.1:19921")
    cli = &tcp("127.0.0.1:19921")
    peer = srv.accept()
    wrap = pcall(() -> cli.send_raw())
    peer.close()
    cli.close()
    srv.close()
    return wrap
}

r1 = pcall(bad_list_push)
print(r1.ok)
print(r1.error.contains("list.push"))

r2 = pcall(bad_split)
print(r2.ok)
print(r2.error.contains("string.split"))

r3 = pcall(bad_ref_set)
print(r3.ok)
print(r3.error.contains("ref.set"))

r4 = bad_tcp_send_raw()
print(r4.ok)
print(r4.error.contains("tcp.send_raw"))

fn bad_split_empty() {
    return "abc".split("")
}

fn bad_split_type() {
    return "abc".split(1)
}

fn bad_replace_empty() {
    return "abc".replace("", "x")
}

fn bad_map_key_type() {
    return {a: 1}.has(1)
}

fn bad_recv_raw_negative() {
    srv = tcp_listen("127.0.0.1:19921")
    cli = &tcp("127.0.0.1:19921")
    peer = srv.accept()
    wrap = pcall(() -> cli.recv_raw(-1))
    peer.close()
    cli.close()
    srv.close()
    return wrap
}

fn bad_shm_size() {
    return &shm("bad_guard_shm", -1)
}

r5 = pcall(bad_split_empty)
print(r5.ok)
print(r5.error.contains("separator cannot be empty"))

r6 = pcall(bad_split_type)
print(r6.ok)
print(r6.error.contains("string.split"))

r7 = pcall(bad_replace_empty)
print(r7.ok)
print(r7.error.contains("old value cannot be empty"))

r8 = pcall(bad_map_key_type)
print(r8.ok)
print(r8.error.contains("map.has"))

r9 = bad_recv_raw_negative()
print(r9.ok)
print(r9.error.contains("tcp.recv_raw"))

r10 = pcall(bad_shm_size)
print(r10.ok)
print(r10.error.contains("shm size"))

print("=== V3 GUARD BLAST PASSED ===")
