gui32 = import("../modules/gui32")
app = gui32.connect(null)
white = gui32.rgb(255, 255, 255)
red = gui32.rgb(255, 0, 0)
green = gui32.rgb(0, 255, 0)
blue = gui32.rgb(0, 0, 255)
black = gui32.rgb(0, 0, 0)

print(app.ping())

win = app.new_window("toolkit", 360, 260, null)
print(win.title() == "toolkit")
win.set_title("toolkit2")
print(win.title() == "toolkit2")
win.show()
win.set_bounds(40, 40, 400, 300)

resized = null
while resized == null {
    ev = app.next_event()
    if ev.event == "resize" and ev.window == win.id {
        resized = ev
    }
}
print(resized.width > 0)
print(resized.height > 0)

box = win.checkbox("check", 10, 10, 100, 24, {checked: true, token: "check"})
print(box.checked())
box.set_checked(false)
print(box.checked() == false)

name_box = win.input("abc", 10, 40, 120, 24, null)
print(name_box.text() == "abc")
name_box.set_text("xyz")
print(name_box.text() == "xyz")

memo = win.textarea("hello", 10, 72, 120, 60, null)
memo.set_text("memo-text")
print(memo.text() == "memo-text")

lst = win.listbox(150, 10, 120, 90, ["a", "b", "c"], {selected: 1, token: "list"})
print(len(lst.items()) == 3)
print(lst.selected() == 1)
lst.add_item("d")
lst.set_selected(3)
print(len(lst.items()) == 4)
print(lst.selected() == 3)

cv = win.canvas(10, 150, 120, 60, {color: white, token: "cv"})
cv.clear(white)
cv.line(0, 0, 40, 20, red, 1)
cv.rect(10, 10, 20, 12, {fill: true, fill_color: green, line_color: blue})
cv.draw_text("ok", 6, 36, black)

win.menu([
    {
        text: "File",
        children: [
            {text: "Exit", token: "exit"}
        ]
    }
])

timer_id = win.timer(20, "tick")
tick = null
while tick == null {
    ev = app.next_event()
    if ev.event == "timer" and ev.token == "tick" {
        tick = ev
    }
}
print(tick.event == "timer")
print(tick.token == "tick")

app.kill_timer(timer_id)
app.shutdown()

print("=== GUI32 TOOLKIT PASSED ===")
