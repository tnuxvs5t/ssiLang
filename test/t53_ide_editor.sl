print("--- IDE EDITOR TESTS ---")

S = import("../modules/sWidgets/core.sl")

app = S.app({})
clipboard_text = ""

fn clipboard_get_stub() => @clipboard_text

fn clipboard_set_stub(text) {
@clipboard_text = text
return null
}

app.gui.clipboard_get = clipboard_get_stub
app.gui.clipboard_set = clipboard_set_stub

state = S.ide_editor_state("pri", {})
editor = S.ide_editor(state, {})

win = app.create_window({
title: "ide editor",
width: 520,
height: 320,
visible: false,
root: editor
})

box = editor.bounds()
win.dispatch({type: "mouse_down", button: "left", x: box.x + 80, y: box.y + 20})
state.set_caret(3)
print(state.refresh_completion(true))
print(state.completion_active())
print(state.current_completion().label == "print")
win.dispatch({type: "key_down", key: 9})
print(state.get() == "print")

state.set("sys")
state.set_caret(3)
win.dispatch({type: "char", text: ".", code: 46})
print(state.get() == "sys.")
print(state.completion_active())
print(state.current_completion().label == "push")
win.dispatch({type: "key_down", key: 40})
print(state.current_completion().label == "pop")

state.set("if true {")
state.set_caret(len(state.get()))
win.dispatch({type: "key_down", key: 13})
print(state.get() == "if true {\n    ")

state.set("hello\nworld")
state.set_caret(2)
win.dispatch({type: "key_down", key: 65, ctrl: true})
sel = state.selection()
print(sel.start == 0 and sel.end == len(state.get()))
win.dispatch({type: "key_down", key: 67, ctrl: true})
print(clipboard_text == "hello\nworld")
clipboard_text = "paste"
win.dispatch({type: "key_down", key: 86, ctrl: true})
print(state.get() == "paste")

scroll_lines = []
for i in range(24) {
scroll_lines.push("line_" + str(i))
}

scroll_state = S.ide_editor_state(scroll_lines.join("\n"), {})
scroll_editor = S.ide_editor(scroll_state, {})
scroll_win = app.create_window({
title: "ide wheel",
width: 520,
height: 220,
visible: false,
root: scroll_editor
})

scroll_box = scroll_editor.bounds()
scroll_x = scroll_box.x + 110
scroll_y = scroll_box.y + 18
scroll_win.dispatch({type: "mouse_move", x: scroll_x, y: scroll_y})
scroll_win.dispatch({type: "mouse_wheel", x: scroll_x, y: scroll_y, delta: -120})
scroll_win.dispatch({type: "timer", timer_id: 1})
scroll_win.dispatch({type: "mouse_down", button: "left", x: scroll_x, y: scroll_y})
print(scroll_state.line_col().line == 3)

scrollbar_x = scroll_box.x + scroll_box.w - 10
scrollbar_y = scroll_box.y + scroll_box.h - 20
scroll_win.dispatch({type: "mouse_down", button: "left", x: scrollbar_x, y: scrollbar_y})
scroll_win.dispatch({type: "timer", timer_id: 1})
scroll_win.dispatch({type: "mouse_down", button: "left", x: scroll_x, y: scroll_y})
print(scroll_state.line_col().line > 3)

scroll_win.close()
win.close()
print(app.stop() == 0)
print("=== IDE EDITOR TESTS PASSED ===")
