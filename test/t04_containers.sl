# Lists
arr = [1, 2, 3, 4, 5]
print(arr)
print(len(arr))
print(arr[0])
print(arr[4])

arr.push(6)
print(arr)
print(arr.pop())
print(arr)

# Slice
print(arr.slice(1, 3))

# Map/filter/reduce
doubled = arr.map(x -> x * 2)
print(doubled)

evens = arr.filter(x -> x % 2 == 0)
print(evens)

total = arr.reduce((acc, x) -> acc + x, 0)
print(total)

# Find, contains, join, reverse
print(arr.find(x -> x > 3))
print(arr.contains(3))
print(arr.join("-"))
print(arr.reverse())

# Maps
m = {name: "Alice", age: 30}
print(m)
print(m.name)
print(m["age"])

m.city = "NYC"
print(m)
print(m.keys())
print(m.values())
print(m.has("name"))
m.remove("age")
print(m)

# String methods
s = "  hello world  "
print(s.trim())
print("a,b,c".split(","))
print("hello".chars())
print("hello".starts_with("he"))
print("hello".ends_with("lo"))
print("hello".contains("ell"))
print("hello world".replace("world", "ssiLang"))
