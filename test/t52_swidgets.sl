print("--- SWIDGETS TESTS ---")

S = import("../modules/sWidgets/core.sl")

parent_hits = 0
button_hits = 0
line_submits = 0

fn parent_event(evt, ctx) {
if evt.type == "mouse_down" {
@parent_hits = @parent_hits + 1
}
return false
}

fn on_button() {
@button_hits = @button_hits + 1
return null
}

fn on_submit(text) {
@line_submits = @line_submits + 1
return null
}

line_state = S.line_edit_state("", {})
edit_state = S.editor_state("", {})

root = S.box(
S.column([
S.label("Title", {
font: S.default_theme().title_font
}),
S.divider({}),
S.button("Press", on_button, {}),
S.line_edit(line_state, {
placeholder: "name",
on_submit: on_submit
}),
S.editor(edit_state, {
height: 140
})
], {
spacing: 10,
padding: 12
}),
{
padding: 12,
bg: S.default_theme().panel_bg,
border: S.default_theme().panel_border,
on_event: parent_event
}
)

app = S.app({})
win = app.create_window({
title: "sWidgets smoke",
width: 420,
height: 420,
visible: false,
root: root
})

button_widget = root.children()[0].children()[2]
button_box = button_widget.bounds()

win.dispatch({type: "mouse_move", x: button_box.x + 5, y: button_box.y + 5})
win.dispatch({type: "mouse_down", button: "left", x: button_box.x + 5, y: button_box.y + 5})
win.dispatch({type: "mouse_up", button: "left", x: button_box.x + 5, y: button_box.y + 5})

print(button_hits == 1)
print(parent_hits >= 1)

line_widget = root.children()[0].children()[3]
line_box = line_widget.bounds()
win.dispatch({type: "mouse_down", button: "left", x: line_box.x + 8, y: line_box.y + 8})
print(win.focused() == line_widget)
win.dispatch({type: "char", text: "a"})
win.dispatch({type: "char", text: "b"})
win.dispatch({type: "key_down", key: 37})
win.dispatch({type: "char", text: "X"})
print(line_state.get() == "aXb")
win.dispatch({type: "key_down", key: 13})
print(line_submits == 1)

backspace_state = S.line_edit_state("xy", {})
backspace_state.set_caret(2)
backspace_widget = S.line_edit(backspace_state, {})
backspace_win = app.create_window({
title: "backspace path",
width: 240,
height: 90,
visible: false,
root: backspace_widget
})
backspace_box = backspace_widget.bounds()
backspace_win.dispatch({type: "mouse_down", button: "left", x: backspace_box.x + backspace_box.w - 4, y: backspace_box.y + 8})
backspace_state.set_caret(2)
backspace_win.dispatch({type: "key_down", key: 8})
backspace_win.dispatch({type: "char", text: "\b", code: 8})
print(backspace_state.get() == "x")

sel_state = S.line_edit_state("abcd", {})
sel_state.begin_selection(1)
sel_state.select_to(3)
sel_state.insert("X")
print(sel_state.get() == "aXd")

editor_widget = root.children()[0].children()[4]
editor_box = editor_widget.bounds()
win.dispatch({type: "mouse_down", button: "left", x: editor_box.x + 8, y: editor_box.y + 8})
print(win.focused() == editor_widget)
win.dispatch({type: "char", text: "h"})
win.dispatch({type: "char", text: "i"})
win.dispatch({type: "key_down", key: 13})
win.dispatch({type: "char", text: "!"})
print(edit_state.get() == "hi\n!")
win.dispatch({type: "key_down", key: 38})
win.dispatch({type: "key_down", key: 36})
win.dispatch({type: "char", text: ">"})
print(edit_state.get() == ">hi\n!")

flex_fill = S.editor(S.editor_state("body", {}), {})
flex_fill.flex = 1
flex_root = S.column([
S.spacer(20),
flex_fill,
S.spacer(30)
], {
spacing: 5,
padding: 10
})

flex_win = app.create_window({
title: "flex layout",
width: 320,
height: 280,
visible: false,
root: flex_root
})

expected_flex_h = flex_root.bounds().h - 20 - 30 - 10 - 10 - 5 - 5
print(flex_fill.bounds().h == expected_flex_h)

drag_state = S.editor_state("abcd\nefgh", {})
drag_editor = S.editor(drag_state, {})
drag_root = S.column([
drag_editor,
S.button("Below", null, {})
], {
spacing: 10,
padding: 10
})

drag_win = app.create_window({
title: "drag selection",
width: 420,
height: 260,
visible: false,
root: drag_root
})

mono_font = S.default_theme().mono_font
editor_text_x = drag_editor.bounds().x + 12
editor_text_y = drag_editor.bounds().y + 10
drag_x = editor_text_x + app.gui.measure_text("efg", {font: mono_font}).width
drag_y = drag_root.children()[1].bounds().y + 5

drag_win.dispatch({type: "mouse_down", button: "left", x: editor_text_x + 1, y: editor_text_y + 1})
drag_win.dispatch({type: "mouse_move", x: drag_x, y: drag_y})
drag_win.dispatch({type: "mouse_up", button: "left", x: drag_x, y: drag_y})
drag_win.dispatch({type: "char", text: "Z"})
print(drag_state.get() == "Zh")

wheel_hits = 0
timer_hits = 0

fn wheel_event(evt, ctx) {
if evt.type == "mouse_wheel" {
@wheel_hits = @wheel_hits + evt.delta
return true
}
if evt.type == "timer" and evt.timer_id == 99 {
@timer_hits = @timer_hits + 1
return true
}
if evt.type == "mouse_move" {
return true
}
return false
}

fn wheel_measure(ctx, avail_w) => {w: avail_w, h: 120}
fn wheel_layout(ctx, box) => []

wheel_widget = S.custom({
measure: wheel_measure,
layout: wheel_layout,
on_event: wheel_event,
interactive: true,
focusable: true
})

wheel_app = S.app({
defer_redraw: true,
redraw_timer_id: 77,
redraw_interval_ms: 16
})

wheel_win = wheel_app.create_window({
title: "wheel + deferred redraw",
width: 300,
height: 180,
visible: false,
root: wheel_widget
})

wheel_present_hits = 0
wheel_timer_set_hits = 0
wheel_timer_kill_hits = 0

fn wheel_present_stub(ops) {
@wheel_present_hits = @wheel_present_hits + 1
return null
}

fn wheel_set_timer_stub(timer_id, interval_ms) {
@wheel_timer_set_hits = @wheel_timer_set_hits + 1
return {timer_id: timer_id, interval_ms: interval_ms}
}

fn wheel_kill_timer_stub(timer_id) {
@wheel_timer_kill_hits = @wheel_timer_kill_hits + 1
return {timer_id: timer_id}
}

wheel_win.native.present = wheel_present_stub
wheel_win.native.set_timer = wheel_set_timer_stub
wheel_win.native.kill_timer = wheel_kill_timer_stub

wheel_box = wheel_widget.bounds()
wheel_x = wheel_box.x + 8
wheel_y = wheel_box.y + 8
wheel_win.dispatch({type: "mouse_move", x: wheel_x, y: wheel_y})
print(wheel_present_hits == 0)
print(wheel_timer_set_hits == 1)
wheel_win.dispatch({type: "timer", timer_id: 77})
print(wheel_present_hits == 1)
print(wheel_timer_kill_hits == 1)
wheel_win.dispatch({type: "timer", timer_id: 99})
print(timer_hits == 1)
wheel_win.dispatch({type: "mouse_wheel", x: wheel_x, y: wheel_y, delta: -120})
print(wheel_hits == -120)

wheel_win.close()
print(wheel_app.stop() == 0)

drag_win.close()
flex_win.close()
backspace_win.close()
win.close()
print(app.stop() == 0)

print("=== SWIDGETS TESTS PASSED ===")
