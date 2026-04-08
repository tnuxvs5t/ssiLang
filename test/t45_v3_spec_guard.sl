# v3 spec guard: invalid index/key usage must fail deterministically

arr = [10, 20, 30]
m = {name: "Alice"}

fn bad_list_key() => arr["x"]
fn bad_list_float() => arr[1.5]
fn bad_str_key() => "abc"["x"]
fn bad_safe_scalar() => 5?[0]
fn bad_map_num_key() => m?[1]

fn bad_list_assign() {
    arr["x"] = 99
}

fn bad_list_ref() => &ref arr["x"]

print(arr?[99])
print(m?["missing"])

r1 = pcall(bad_list_key)
print(r1.ok)
print(r1.error.contains("list index requires integer index"))

r2 = pcall(bad_list_float)
print(r2.ok)
print(r2.error.contains("list index requires integer index"))

r3 = pcall(bad_str_key)
print(r3.ok)
print(r3.error.contains("string index requires integer index"))

r4 = pcall(bad_safe_scalar)
print(r4.ok)
print(r4.error.contains("Cannot index number"))

r5 = pcall(bad_map_num_key)
print(r5.ok)
print(r5.error.contains("map index requires string key"))

r6 = pcall(bad_list_assign)
print(r6.ok)
print(r6.error.contains("list assignment requires integer index"))

r7 = pcall(bad_list_ref)
print(r7.ok)
print(r7.error.contains("list ref requires integer index"))

print("=== V3 SPEC GUARD PASSED ===")
