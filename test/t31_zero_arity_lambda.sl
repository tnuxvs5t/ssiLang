ping = () -> "pong"
print(ping())

make_counter = () -> {
    start: 41,
    next: () -> 42
}
box = make_counter()
print(box.start)
print(box.next())

safe = pcall(() -> 1 + 2)
print(safe.ok)
print(safe.value)

fail = pcall(() -> error("boom"))
print(fail.ok)
print(fail.error.contains("boom"))

print("=== ZERO ARITY LAMBDA TESTS PASSED ===")
