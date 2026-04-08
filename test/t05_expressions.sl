# Split expression
x = true => "yes" || "no"
print(x)
y = false => "yes" || "no"
print(y)

# Nested split
grade = 85
result = grade >= 90 => "A" || grade >= 80 => "B" || grade >= 70 => "C" || "F"
print(result)

# Safe access
m = {a: {b: 42}}
print(m?.a?.b)
print(m?.x?.y)
arr = [1, 2, 3]
print(arr?[0])
n = null
print(n?.foo)
print(n?[0])

# Pipe operator |>
fn double(x) => x * 2
fn inc(x) => x + 1
print(5 |> double)
print(5 |> double |> inc)

result = [1, 2, 3, 4, 5] |> filter($ > 2) |> map($ * 10)
print(result)

# Probe
debug(42)
debug("hello")

# Derive
data = [1, 2, 3]
clone = %copy data
deep = %deep data
snap = %snap data
clone.push(99)
print(data)
print(clone)

# Nested derive
nested = [[1, 2], [3, 4]]
shallow = %copy nested
deep2 = %deep nested
shallow[0].push(99)
print(nested)
print(deep2)

# Net form
packet = %net {kind: "test", val: 42}
print(type(packet))
