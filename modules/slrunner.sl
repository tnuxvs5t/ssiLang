fn _copy_map(v, label) {
    if v == null => return {}
    if type(v) != "map" => error(label + " must be map")
    return %copy v
}

fn start(opts) {
    conf = _copy_map(opts, "slrunner.start opts")
    repo_root = __dir + "/.."
    helper_src = repo_root + "/tools/sl_runner_helper.cpp"
    helper_exe = repo_root + "/tools/sl_runner_helper.exe"
    pipe_name = conf.pipe_name == null => "sl_runner_pipe" || conf.pipe_name

    proc = null
    channel = null

    fn build_helper(force) {
        if not force and sys.exists(helper_exe) => return helper_exe
        wrap = pcall(() -> sys.spawn([
            "g++",
            "-std=c++17",
            "-O2",
            "-Wall",
            "-Wextra",
            "-I",
            repo_root + "/tools",
            "-o",
            helper_exe,
            helper_src,
            "-lws2_32"
        ], {
            cwd: repo_root,
            detached: false
        }))
        if not wrap.ok => error("slrunner build failed: " + str(wrap.error))
        code = wrap.value.wait()
        if code != 0 => error("slrunner build failed with exit code " + str(code))
        if not sys.exists(helper_exe) => error("slrunner build did not produce helper")
        return helper_exe
    }

    fn connect_channel() {
        cur_proc = @proc
        if cur_proc == null or not cur_proc.is_alive() {
            error("slrunner helper is not alive before pipe connect")
        }
        wrap = pcall(() -> &pipe(pipe_name))
        if not wrap.ok => error("slrunner could not open server pipe: " + str(wrap.error))
        @channel = wrap.value
        return @channel
    }

    fn ensure_open() {
        cur_channel = @channel
        cur_proc = @proc
        if cur_channel != null and cur_proc != null and cur_proc.is_alive() {
            return null
        }

        exe = build_helper(false)
        old_proc = @proc
        if old_proc != null and old_proc.is_alive() {
            old_proc.terminate()
        }
        @channel = null

        wrap = pcall(() -> sys.spawn([exe, pipe_name], {
            cwd: repo_root,
            detached: false
        }))
        if not wrap.ok => error("slrunner start failed: " + str(wrap.error))
        @proc = wrap.value
        sys.sleep(0.05)
        started_proc = @proc
        if not started_proc.is_alive() {
            code = started_proc.wait(0.2)
            error("slrunner helper exited before pipe connect, code " + str(code))
        }
        connect_channel()
        return null
    }

    fn request(payload) {
        ensure_open()
        cur_channel = @channel
        cur_channel.send(%net payload)
        return cur_channel.recv()
    }

    fn run(opts2) {
        req = _copy_map(opts2, "slrunner.run opts")
        req.cmd = "run"
        if req.exe == null {
            req.exe = repo_root + "/sl.exe"
        }
        if req.cwd == null {
            req.cwd = repo_root
        }
        if req.input == null {
            req.input = ""
        }
        resp = request(req)
        if resp.ok != true => error("slrunner run failed: " + str(resp.error))
        return resp
    }

    fn stop() {
        if @channel == null => return null
        wrap = pcall(() -> request({cmd: "stop"}))
        @channel = null
        cur_proc = @proc
        if cur_proc != null {
            code = cur_proc.wait(2)
            if code == null and cur_proc.is_alive() {
                cur_proc.terminate()
                cur_proc.wait(2)
            }
        }
        @proc = null
        return wrap.ok
    }

    fn alive() {
        cur_proc = @proc
        return cur_proc != null and cur_proc.is_alive()
    }
    fn helper_path() => helper_exe

    return {
        run: run,
        stop: stop,
        alive: alive,
        helper_path: helper_path
    }
}
