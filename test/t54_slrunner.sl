print("--- SLRUNNER TESTS ---")

mod_wrap = pcall(() -> import("../modules/slrunner.sl"))
print(mod_wrap.ok)
if not mod_wrap.ok {
    print(str(mod_wrap.error))
    error("import failed")
}
R = mod_wrap.value

start_wrap = pcall(() -> R.start({pipe_name: "sl_runner_test_pipe"}))
print(start_wrap.ok)
if not start_wrap.ok {
    print(str(start_wrap.error))
    error("start failed")
}
runner = start_wrap.value
tmp = "__runner_tmp.sl"

sys.write_text(tmp, [
    "print(\"Enter your name:\")",
    "name = input()",
    "print(\"Hello, \" + name)"
].join("\n"))

run_wrap = pcall(() -> runner.run({
    script: tmp,
    cwd: ".",
    input: "runner\n"
}))
print(run_wrap.ok)
if not run_wrap.ok {
    print(str(run_wrap.error))
    error("run failed")
}
resp = run_wrap.value

print(resp.ok)
print(resp.exit_code == 0)
print(resp.output.contains("Enter your name:"))
print(resp.output.contains("Hello, runner"))

stop_wrap = pcall(() -> runner.stop())
print(stop_wrap.ok)
print(stop_wrap.ok and stop_wrap.value == true)
sys.remove(tmp)

print("=== SLRUNNER TESTS PASSED ===")
