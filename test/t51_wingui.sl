print("--- WINGUI TESTS ---")

W = import("../modules/wingui.sl")

helper = W.build_helper(true)
print(sys.exists(helper))

gui = W.start()

meta = gui.ping()
print(meta.helper == "wingui")
print(meta.protocol == "wingui/v1")
print(meta.pid > 0)

screen = gui.screen_size()
print(screen.width > 0)
print(screen.height > 0)

work = gui.work_area()
print(work.width > 0)
print(work.height > 0)

metrics = gui.measure_text("wingui", {
    font: W.font("Segoe UI", 16)
})
print(metrics.width > 0)
print(metrics.height > 0)
print(metrics.line_height >= metrics.height)

win = gui.create_window({
    title: "wingui-smoke",
    width: 320,
    height: 200,
    visible: false
})

client = win.client_rect()
print(client.width == 320)
print(client.height == 200)

scene = [
    W.clear(W.rgb(24, 28, 36)),
    W.fill_rect(16, 16, 96, 48, W.rgb(60, 110, 200)),
    W.rect(12, 12, 140, 88, W.rgb(230, 230, 230), 2),
    W.line(0, 0, 319, 199, W.rgb(255, 80, 80), 1),
    W.text(24, 32, "wingui", {
        color: W.rgb(255, 255, 255),
        font: W.font("Segoe UI", 16)
    })
]

win.present(scene)
evt = gui.poll_event(0.05)
print(evt == null or type(evt) == "map")

win.set_timer(7, 20)
timer_ok = false
for i in range(8) {
    evt2 = gui.poll_event(0.05)
    if evt2 != null and evt2.type == "timer" and evt2.timer_id == 7 {
        timer_ok = true
        break
    }
}
print(timer_ok)
win.kill_timer(7)

win.close()
code = gui.stop()
print(code == 0)

print("=== WINGUI TESTS PASSED ===")
