# Ad-hoc blast 3: import cache + exported closures + placeholder/context inside modules

mod1 = import("modules/adhoc/stateful")
mod2 = import("modules/adhoc/stateful.sl")

print(mod1 == mod2)
print(mod1.version)
print(mod1._secret)

counter = mod1.make_counter("jobs", 10)
print(counter.inc(5))
print(counter.inc(2))
print(counter.raw(0))

r = counter.ref(0)
print(r.deref())
r.set(99)
print(counter.raw(0))

times6 = mod1.make_scaler(6)
print(times6(7))

user = {name: "Alice", role: "admin"}
print(mod1.describe(user))

users = [
    {name: "A", role: "ops"},
    {name: "B", role: "dev"},
    {name: "C", role: "qa"}
]
print(mod1.summarize_names(users))

wrap = pcall(() -> import("modules/cycle_a.sl"))
print(wrap.ok)
print(wrap.error.contains("Circular import"))

print("=== V3 MODULE BLAST PASSED ===")
