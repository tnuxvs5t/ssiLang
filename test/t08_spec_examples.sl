# Spec §12 examples

# 12.1 Euclidean GCD
fn gcd(a, b) {
    while b != 0 {
        t = a % b
        a = b
        b = t
    }
    return a
}
print(gcd(48, 18))

# 12.2 Sum of array
fn sum(arr) {
    s = 0
    for x in arr {
        s = s + x
    }
    return s
}
print(sum([1, 2, 3, 4, 5]))

# 12.3 Maximum scan
fn max_of(arr) {
    best = arr[0]
    for x in arr {
        if x > best {
            best = x
        }
    }
    return best
}
print(max_of([3, 7, 2, 9, 4]))

# 12.4 Counting frequencies
fn freq(arr) {
    m = {}
    for x in arr {
        c = m?[x]
        if c == null {
            m[x] = 1
        } else {
            m[x] = c + 1
        }
    }
    return m
}
print(freq(["a", "b", "a", "c", "b", "a"]))

# 12.5 Rich expression composition
double = x -> x * 2
print(double(21))

ok = true
pick = ok => "left" || "right"
print(pick)

user = {profile: {name: "Alice"}}
name = user?.profile?.name
print(name)

data = [1, 2, 3]
value = data?[1]
print(value)

fn normalize(x) => x * 10
fn encode(x) => x + 1
out = 5::normalize::encode
print(out)

result = [1, 2, 3, 4, 5] |> map(x -> x * 2) |> filter(x -> x > 3)
print(result)

# 12.6 Explicit derive
data2 = [1, 2, 3]
clone = %copy data2
backup = %snap data2
deep = %deep data2
packet = %net data2
print(clone)
print(backup)
print(deep)

# 12.7 Local link
arr2 = [1, 2, 3]
ref = &ref arr2[0]
print(ref.deref())

# 12.11 Parentheses for clarity
x = (ok => "a" || "b")
print(x)
