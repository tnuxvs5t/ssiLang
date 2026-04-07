math = import("modules/math")

fn make_const(v) => () -> v

closures = []
for i in range(200) {
    closures.push(make_const(i))
}

sum = 0
for f in closures {
    sum = sum + f()
}
print(sum)

mods = []
for i in range(50) {
    mods.push(import("modules/math"))
}

same = true
for m in mods {
    if m != math {
        same = false
    }
}
print(same)

counter = math.make_counter(0)
for i in range(100) {
    counter(1)
}
print(counter(0))

nums = range(300)
mapped = nums |> map(x -> x + 1) |> filter(x -> x % 3 == 0)
print(len(mapped))

total = 0
for x in mapped {
    total = total + x
}
print(total)

ok_cycle = true
for i in range(30) {
    r = pcall(() -> import("modules/cycle_a.sl"))
    if r.ok or not r.error.contains("Circular import") {
        ok_cycle = false
    }
}
print(ok_cycle)

bag = {}
for i in range(200) {
    bag[str(i)] = i * i
}
print(bag["10"])
print(bag["199"])

factory = () -> {
    run: () -> "ok",
    value: 7
}
obj = factory()
print(obj.run())
print(obj.value)

print("=== STRESS TESTS PASSED ===")
