# Demo: interactive runner
print("Enter your favorite number:")
raw = input()
n = int(raw)
print("double = " + str(n * 2))

nums = [1, 2, 3, 4, 5]
picked = nums |> filter($ % 2 == 1) |> map($ * 10)
print(picked)
print("double = " + str(n * 2))

nums = [1, 2, 3, 4, 5]
picked = nums |> filter($ % 2 == 1) |> map($ * 10)
print(picked)print("double = " + str(n * 2))

nums = [1, 2, 3, 4, 5]
picked = nums |> filter($ % 2 == 1) |> map($ * 10)
print(picked)