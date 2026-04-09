W = import("../modules/wingui.sl")

fn inside(rect, x, y) {
    return x >= rect.x and x < rect.x + rect.w and y >= rect.y and y < rect.y + rect.h
}

fn center_text_x(gui, text, font_cfg, box) {
    m = gui.measure_text(text, {font: font_cfg})
    return int(box.x + (box.w - m.width) / 2)
}

fn center_text_y(gui, text, font_cfg, box) {
    m = gui.measure_text(text, {font: font_cfg})
    return int(box.y + (box.h - m.height) / 2)
}

gui = W.start()

title_font = W.font("Segoe UI", 26, {weight: 700})
button_font = W.font("Segoe UI", 18, {weight: 700})
popup_font = W.font("Microsoft YaHei UI", 24, {weight: 700})
popup_button_font = W.font("Segoe UI", 16, {weight: 700})

main_win = gui.create_window({
    title: "ssiLang GUI Test",
    width: 520,
    height: 320,
    visible: true
})

popup_win = gui.create_window({
    title: "提示",
    width: 280,
    height: 160,
    visible: false,
    resizable: false
})

button_rect = {x: 150, y: 120, w: 220, h: 64}
popup_ok_rect = {x: 95, y: 92, w: 90, h: 36}

hover_main = false
down_main = false
show_popup = false
hover_popup_ok = false
down_popup_ok = false
running = true

fn draw_main() {
    bg = W.rgb(245, 247, 252)
    text_color = W.rgb(35, 41, 52)
    border = W.rgb(61, 107, 255)
    fill = @down_main => W.rgb(61, 107, 255) || @hover_main => W.rgb(97, 134, 255) || W.rgb(80, 121, 255)

    ops = [
        W.clear(bg),
        W.text(36, 34, "Hello World Demo", {
            color: text_color,
            font: title_font
        }),
        W.text(36, 74, "Click the custom-drawn button below.", {
            color: W.rgb(90, 98, 114),
            font: W.font("Segoe UI", 14)
        }),
        W.fill_rect(button_rect.x, button_rect.y, button_rect.w, button_rect.h, fill),
        W.rect(button_rect.x, button_rect.y, button_rect.w, button_rect.h, border, 2),
        W.text(
            center_text_x(gui, "Hello World", button_font, button_rect),
            center_text_y(gui, "Hello World", button_font, button_rect),
            "Hello World",
            {
                color: W.rgb(255, 255, 255),
                font: button_font
            }
        )
    ]

    main_win.present(ops)
}

fn draw_popup() {
    fill = @down_popup_ok => W.rgb(54, 95, 228) || @hover_popup_ok => W.rgb(78, 117, 242) || W.rgb(66, 103, 235)

    ops = [
        W.clear(W.rgb(255, 255, 255)),
        W.text(102, 28, "你好！", {
            color: W.rgb(34, 34, 34),
            font: popup_font
        }),
        W.fill_rect(popup_ok_rect.x, popup_ok_rect.y, popup_ok_rect.w, popup_ok_rect.h, fill),
        W.rect(popup_ok_rect.x, popup_ok_rect.y, popup_ok_rect.w, popup_ok_rect.h, W.rgb(49, 83, 201), 1),
        W.text(
            center_text_x(gui, "OK", popup_button_font, popup_ok_rect),
            center_text_y(gui, "OK", popup_button_font, popup_ok_rect),
            "OK",
            {
                color: W.rgb(255, 255, 255),
                font: popup_button_font
            }
        )
    ]

    popup_win.present(ops)
}

fn show_popup_now() {
    @show_popup = true
    popup_win.show()
    popup_win.set_title("提示")
    draw_popup()
}

fn hide_popup_now() {
    @show_popup = false
    @hover_popup_ok = false
    @down_popup_ok = false
    popup_win.hide()
}

draw_main()

while running {
    evt = gui.poll_event(0.05)
    if evt == null => continue

    if evt.window_id == main_win.id {
        if evt.type == "paint" => continue

        if evt.type == "resize" {
            draw_main()
            continue
        }

        if evt.type == "mouse_move" {
            next_hover = inside(button_rect, evt.x, evt.y)
            if next_hover != hover_main {
                hover_main = next_hover
                draw_main()
            }
            continue
        }

        if evt.type == "mouse_leave" {
            if hover_main or down_main {
                hover_main = false
                down_main = false
                draw_main()
            }
            continue
        }

        if evt.type == "mouse_down" and evt.button == "left" {
            if inside(button_rect, evt.x, evt.y) {
                down_main = true
                draw_main()
            }
            continue
        }

        if evt.type == "mouse_up" and evt.button == "left" {
            fire = down_main and inside(button_rect, evt.x, evt.y)
            if down_main {
                down_main = false
                draw_main()
            }
            if fire {
                show_popup_now()
            }
            continue
        }

        if evt.type == "close" {
            running = false
            continue
        }
    }

    if evt.window_id == popup_win.id {
        if evt.type == "paint" => continue

        if evt.type == "resize" {
            if show_popup {
                draw_popup()
            }
            continue
        }

        if evt.type == "mouse_move" {
            next_hover = inside(popup_ok_rect, evt.x, evt.y)
            if next_hover != hover_popup_ok {
                hover_popup_ok = next_hover
                draw_popup()
            }
            continue
        }

        if evt.type == "mouse_leave" {
            if hover_popup_ok or down_popup_ok {
                hover_popup_ok = false
                down_popup_ok = false
                draw_popup()
            }
            continue
        }

        if evt.type == "mouse_down" and evt.button == "left" {
            if inside(popup_ok_rect, evt.x, evt.y) {
                down_popup_ok = true
                draw_popup()
            }
            continue
        }

        if evt.type == "mouse_up" and evt.button == "left" {
            fire = down_popup_ok and inside(popup_ok_rect, evt.x, evt.y)
            if down_popup_ok {
                down_popup_ok = false
                draw_popup()
            }
            if fire {
                hide_popup_now()
            }
            continue
        }

        if evt.type == "close" {
            hide_popup_now()
            continue
        }
    }
}

pcall(() -> popup_win.close())
pcall(() -> main_win.close())
gui.stop()
