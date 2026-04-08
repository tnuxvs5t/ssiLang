print("--- SYS TESTS ---")

tmp = "__sys_tmp"
if sys.exists(tmp) {
    sys.remove(tmp, true)
}

sys.mkdir(tmp, true)
print(sys.is_dir(tmp))

file = tmp + "/note.txt"
sys.write_text(file, "alpha")
sys.append_text(file, "\nbeta")
text = sys.read_text(file)
print(text.contains("alpha"))
print(text.contains("beta"))
print(sys.listdir(tmp).contains("note.txt"))

moved = tmp + "/moved.txt"
sys.rename(file, moved)
print(sys.exists(file))
print(sys.exists(moved))
print(sys.is_file(moved))

pipe_name = "sl_sys_helper_pipe"
p = sys.spawn(["python", "tools/sys_helper.py", pipe_name])
print(p.pid() > 0)
print(p.is_alive())

ch = &pipe(pipe_name)
ch.send(%net {cmd: "echo", text: "hello-helper"})
resp = ch.recv()
print(resp.ok)
print(resp.text)

ch.send(%net {cmd: "sum", xs: [1, 2, 3, 4, 5]})
resp2 = ch.recv()
print(resp2.ok)
print(resp2.total)

ch.send(%net {cmd: "stop"})
resp3 = ch.recv()
print(resp3.ok)
print(p.wait(5))
print(p.is_alive())
ch.close()

sys.remove(tmp, true)
print(sys.exists(tmp))

print("=== SYS TESTS PASSED ===")
