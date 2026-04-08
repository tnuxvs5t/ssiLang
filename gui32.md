# gui32

`gui32` is a pure Win32 GUI helper for ssiLang.

It stays outside the interpreter. ssiLang talks to the helper over the existing TCP `%net` channel, so the language core remains unchanged and still fits the original IPC-first design.

## Build

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\build_gui32_helper.ps1
```

## Run

Start the helper first:

```powershell
.\tools\gui32_helper.exe
```

Then run any ssiLang GUI script:

```powershell
.\sl.exe examples\gui32_demo.sl
.\sl.exe examples\gui32_canvas_demo.sl
```

## Smoke tests

```powershell
.\sl.exe test\t48_gui32_ping.sl
.\sl.exe test\t49_gui32_toolkit.sl
```

`t48` shuts the helper down, so if you run them manually, start the helper again before `t49`.

## Script model

Import the module:

```sl
gui32 = import("../modules/gui32")
app = gui32.connect(null)
```

Create a window with the object wrapper API:

```sl
win = app.new_window("demo", 480, 320, null)
name = win.input("ssiLang", 20, 20, 180, 24, null)
memo = win.textarea("", 20, 56, 220, 120, null)
ok = win.button("OK", 20, 188, 80, 28, {token: "ok"})
list = win.listbox(260, 20, 160, 120, ["a", "b"], {token: "list"})
canvas = win.canvas(260, 156, 160, 80, {token: "paint", color: gui32.rgb(255, 255, 255)})
win.show()
```

Event loop:

```sl
while true {
    ev = app.next_event()
    if ev.event == "click" and ev.token == "ok" {
        memo.set_text(name.text())
    }
    if ev.event == "close" and ev.window == win.id => break
}
```

## Available controls

- `label`
- `button`
- `input`
- `textarea`
- `checkbox`
- `listbox`
- `groupbox`
- `canvas`

## Available window features

- show / hide / close
- set/get title
- menu bar
- timers

## Available canvas features

- clear
- line
- rect
- draw_text
- mouse move / enter / leave
- mouse down / up
- click event
- focus / blur
- key down / key up / char

## Color helper

ssiLang has no hex numeric literal, so use:

```sl
red = gui32.rgb(255, 0, 0)
white = gui32.rgb(255, 255, 255)
```

## Event shapes

The helper intentionally does not enable Win32 double-click class style.
`gui32` reports primitive mouse events and leaves gesture composition to ssiLang.

Button / checkbox click:

```sl
{kind: "event", event: "click", window: 1, control: 2, token: "ok"}
```

Listbox selection:

```sl
{kind: "event", event: "select", window: 1, control: 3, token: "items", selected: 0}
```

Edit change:

```sl
{kind: "event", event: "change", window: 1, control: 4, token: "memo"}
```

Menu:

```sl
{kind: "event", event: "menu", window: 1, token: "file_exit"}
```

Timer:

```sl
{kind: "event", event: "timer", window: 1, timer: 1, token: "tick"}
```

Canvas click:

```sl
{kind: "event", event: "canvas_click", window: 1, control: 5, token: "paint", x: 42, y: 16}
```

Canvas move:

```sl
{kind: "event", event: "mouse_move", window: 1, control: 5, token: "paint", x: 42, y: 16}
```

Canvas button:

```sl
{kind: "event", event: "mouse_down", window: 1, control: 5, token: "paint", x: 42, y: 16, button: "left"}
```

Canvas focus:

```sl
{kind: "event", event: "focus", window: 1, control: 5, token: "paint"}
```

Window resize:

```sl
{kind: "event", event: "resize", window: 1, width: 640, height: 480}
```

Window key:

```sl
{kind: "event", event: "key_down", window: 1, key: 27, repeat: false}
```

Window close:

```sl
{kind: "event", event: "close", window: 1}
```
