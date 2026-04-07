# If/else
x = 10
if x > 5 {
    print("big")
} else {
    print("small")
}

# If with => body
if x > 0 => print("positive")

# While loop
i = 0
while i < 5 {
    print(i)
    i = i + 1
}

# For loop
for x in [10, 20, 30] {
    print(x)
}

# Break and continue
s = 0
for x in range(10) {
    if x == 5 => break
    s = s + x
}
print(s)

for x in range(10) {
    if x % 2 == 0 => continue
    print(x)
}
