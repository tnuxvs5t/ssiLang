math = import("modules/math")
math_again = import("modules/math.sl")

counter = math.make_counter(10)

print(math == math_again)
print(math.add(2, 3))
print(math.tagged_sum(2, 5))
print(counter(5))
print(counter(2))
print(math.pi)
print(math._hidden)
print(type(math.make_counter))

cyc = pcall(() -> import("modules/cycle_a.sl"))
print(cyc.ok)
print(cyc.error.contains("Circular import"))

print("=== IMPORT TESTS PASSED ===")
