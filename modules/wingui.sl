_helper_root = __dir + "/wingui"
_helper_src = _helper_root + "/wingui_helper.cpp"
_helper_exe = _helper_root + "/wingui_helper.exe"
_repo_root = __dir + "/.."
_session_seq = 0

fn _copy_map(v, label) {
if v == null => return {}
if type(v) != "map" => error(label + " must be map")
return %copy v
}

fn _window_id(target) {
if type(target) == "map" and target.id != null => return target.id
return target
}

fn _strip_ok(resp) {
out = %copy resp
out.remove("ok")
return out
}

fn _next_pipe_name() {
@_session_seq = @_session_seq + 1
return "127.0.0.1:" + str(47000 + @_session_seq)
}

fn paths() {
return {
root: _helper_root,
src: _helper_src,
exe: _helper_exe,
repo: _repo_root
}
}

fn build_helper(force) {
force_build = force == true

if not sys.is_dir(_helper_root) {
sys.mkdir(_helper_root, true)
}

if not force_build and sys.exists(_helper_exe) => return _helper_exe

argv = [
"g++",
"-std=c++17",
"-O2",
"-Wall",
"-Wextra",
"-I",
_repo_root + "/tools",
"-o",
_helper_exe,
_helper_src,
"-lgdi32",
"-lws2_32"
]

proc = sys.spawn(argv, {cwd: _repo_root, detached: false})
code = proc.wait()

if code != 0 => error("wingui: helper build failed with exit code " + str(code))
if not sys.exists(_helper_exe) => error("wingui: helper build did not produce executable")
return _helper_exe
}

fn _make_window(session, id) {
fn present(ops) => @session.present(@id, ops)
fn show() => @session.show_window(@id, true)
fn hide() => @session.show_window(@id, false)
fn close() => @session.destroy_window(@id)
fn set_title(title) => @session.set_title(@id, title)
fn client_rect() => @session.client_rect(@id)
fn invalidate() => @session.invalidate(@id)

return {
id: id,
present: present,
show: show,
hide: hide,
close: close,
set_title: set_title,
client_rect: client_rect,
invalidate: invalidate
}
}

fn start(opts) {
conf = _copy_map(opts, "wingui.start opts")

exe = conf.exe
if exe == null {
if conf.rebuild == true {
exe = build_helper(true)
} else if not sys.exists(_helper_exe) {
exe = build_helper(false)
} else {
exe = _helper_exe
}
}

endpoint = conf.endpoint == null => _next_pipe_name() || conf.endpoint
spawn_opts = {cwd: _repo_root, detached: false}
if conf.cwd != null {
spawn_opts.cwd = conf.cwd
}
if conf.detached != null {
spawn_opts.detached = conf.detached
}

proc = sys.spawn([exe, endpoint], spawn_opts)
channel = null
last_error = null

for i in range(40) {
wrap = pcall(() -> &tcp(endpoint))
if wrap.ok {
channel = wrap.value
break
}
last_error = wrap.error
sys.sleep(0.05)
}

if channel == null {
if proc.is_alive() {
proc.terminate()
}
error("wingui: could not connect helper at " + endpoint + ": " + str(last_error))
}
closed = false
api = {}

fn ensure_open() {
if @closed => error("wingui: session is closed")
return null
}

fn request(payload) {
ensure_open()
@channel.send(%net payload)
resp = @channel.recv()
if type(resp) != "map" => error("wingui: helper returned non-map response")
if resp.ok == false => error("wingui helper error: " + str(resp.error))
if resp.ok != true => error("wingui: malformed helper response")
return resp
}

fn ping() => _strip_ok(request({cmd: "ping"}))

fn screen_size() => _strip_ok(request({cmd: "screen_size"}))

fn work_area() {
resp = request({cmd: "work_area"})
return resp.rect
}

fn measure_text(text, opts2) {
req = _copy_map(opts2, "wingui.measure_text opts")
req.cmd = "measure_text"
req.text = text
return _strip_ok(request(req))
}

fn poll_event(timeout_seconds) {
secs = timeout_seconds == null => 0 || timeout_seconds
resp = request({
cmd: "poll_event",
timeout_ms: int(secs * 1000)
})
return resp.event
}

fn create_window(opts2) {
req = _copy_map(opts2, "wingui.create_window opts")
req.cmd = "create_window"
resp = request(req)
return _make_window(@api, resp.id)
}

fn show_window(target, visible) {
request({
cmd: "show_window",
window_id: _window_id(target),
visible: visible == null => true || visible
})
return null
}

fn set_title(target, title) {
request({
cmd: "set_title",
window_id: _window_id(target),
title: title
})
return null
}

fn destroy_window(target) {
request({
cmd: "destroy_window",
window_id: _window_id(target)
})
return null
}

fn present(target, ops) {
if type(ops) != "list" => error("wingui.present ops must be list")
request({
cmd: "present",
window_id: _window_id(target),
ops: ops
})
return null
}

fn invalidate(target) {
request({
cmd: "invalidate",
window_id: _window_id(target)
})
return null
}

fn client_rect(target) {
resp = request({
cmd: "client_rect",
window_id: _window_id(target)
})
return resp.rect
}

fn wait(timeout_seconds) => @proc.wait(timeout_seconds)
fn is_alive() => @proc.is_alive()
fn pid() => @proc.pid()

fn stop() {
if not @closed {
pcall(() -> request({cmd: "stop"}))
@channel.close()
@closed = true
}

code = @proc.wait(5)
if code == null and @proc.is_alive() {
@proc.terminate()
code = @proc.wait(5)
}
return code
}

fn kill() {
if not @closed {
@channel.close()
@closed = true
}
if @proc.is_alive() {
@proc.kill()
}
return @proc.wait(5)
}

api.request = request
api.ping = ping
api.screen_size = screen_size
api.work_area = work_area
api.measure_text = measure_text
api.poll_event = poll_event
api.create_window = create_window
api.show_window = show_window
api.set_title = set_title
api.destroy_window = destroy_window
api.present = present
api.invalidate = invalidate
api.client_rect = client_rect
api.wait = wait
api.is_alive = is_alive
api.pid = pid
api.stop = stop
api.kill = kill

ping()
return api
}

fn rgb(r, g, b) => int(r) * 65536 + int(g) * 256 + int(b)

fn font(face, size, opts) {
out = _copy_map(opts, "wingui.font opts")
out.face = face
out.size = size
return out
}

fn clear(color) => {kind: "clear", color: color}

fn fill_rect(x, y, w, h, color) {
return {
kind: "fill_rect",
x: x,
y: y,
w: w,
h: h,
color: color
}
}

fn rect(x, y, w, h, color, thickness) {
return {
kind: "rect",
x: x,
y: y,
w: w,
h: h,
color: color,
thickness: thickness == null => 1 || thickness
}
}

fn line(x1, y1, x2, y2, color, thickness) {
return {
kind: "line",
x1: x1,
y1: y1,
x2: x2,
y2: y2,
color: color,
thickness: thickness == null => 1 || thickness
}
}

fn text(x, y, value, opts) {
out = _copy_map(opts, "wingui.text opts")
out.kind = "text"
out.x = x
out.y = y
out.text = value
return out
}
