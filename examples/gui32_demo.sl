gui32 = import("../modules/gui32")
app = gui32.connect(null)

win = app.new_window("ssiLang gui32 demo", 420, 220, null)
name_box = win.input("ssiLang", 20, 20, 220, 28, null)
status = win.label("Click the button.", 20, 60, 320, 24, null)
ok_btn = win.button("Update", 20, 100, 100, 30, {token: "update"})
quit_btn = win.button("Close", 140, 100, 100, 30, {token: "close"})

win.show()

running = true
while running {
    ev = app.next_event()

    if ev.event == "click" and ev.token == "update" {
        status.set_text("Hello, " + name_box.text())
    }

    if ev.event == "click" and ev.token == "close" {
        win.close()
    }

    if ev.event == "close" and ev.window == win.id {
        running = false
    }
}

app.disconnect()
