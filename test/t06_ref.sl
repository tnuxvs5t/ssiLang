# Ref to list element
arr = [10, 20, 30]
r = &ref arr[1]
print(r.deref())
r.set(99)
print(arr)

# Ref to variable
x = 42
rv = &ref x
print(rv.deref())
rv.set(100)
print(x)

# Ref to map entry
m = {a: 1, b: 2}
mr = &ref m.a
print(mr.deref())
mr.set(999)
print(m)
