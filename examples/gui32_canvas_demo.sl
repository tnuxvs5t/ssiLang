gui32 = import("../modules/gui32")
app = gui32.connect(null)
white = gui32.rgb(255, 255, 255)
gray = gui32.rgb(128, 128, 128)
navy = gui32.rgb(0, 0, 128)
green = gui32.rgb(0, 128, 0)
dark_green = gui32.rgb(0, 64, 0)

win = app.new_window("ssiLang canvas demo", 520, 380, null)
status = win.label("Click inside the canvas.", 12, 12, 320, 24, null)
board = win.canvas(12, 44, 480, 260, {token: "board", color: white})
clear_btn = win.button("Clear", 12, 316, 90, 28, {token: "clear"})
about_btn = win.button("About", 116, 316, 90, 28, {token: "about"})

board.rect(0, 0, 479, 259, {fill: true, fill_color: white, line_color: gray})
board.draw_text("Pure Win32 canvas via gui32", 12, 12, navy)
win.show()

running = true
while running {
    ev = app.next_event()

    if ev.event == "mouse_move" and ev.token == "board" {
        status.set_text("move: " + str(ev.x) + ", " + str(ev.y))
    }

    if ev.event == "canvas_click" and ev.token == "board" {
        board.rect(ev.x - 3, ev.y - 3, 6, 6, {
            fill: true,
            fill_color: green,
            line_color: dark_green
        })
        status.set_text("point: " + str(ev.x) + ", " + str(ev.y))
    }

    if ev.event == "click" and ev.token == "clear" {
        board.clear(white)
        board.rect(0, 0, 479, 259, {fill: true, fill_color: white, line_color: gray})
        board.draw_text("Pure Win32 canvas via gui32", 12, 12, navy)
        status.set_text("Canvas cleared.")
    }

    if ev.event == "click" and ev.token == "about" {
        app.message("canvas + controls + events", "gui32", win.id)
    }

    if ev.event == "close" and ev.window == win.id {
        running = false
    }
}

app.disconnect()
