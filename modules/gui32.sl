_default_addr = "127.0.0.1:32123"

fn rgb(r, g, b) => r * 65536 + g * 256 + b

fn _copy_map(src) {
    out = {}
    if src == null => return out
    for k in src.keys() {
        out[k] = src[k]
    }
    return out
}

fn _opt_map(src) {
    if src == null => return {}
    return _copy_map(src)
}

fn _opt_get(src, key, fallback) {
    if src == null => return fallback
    v = src?[key]
    if v == null => return fallback
    return v
}

fn _first(xs) {
    if len(xs) == 0 => return null
    return xs[0]
}

fn _rest(xs) {
    if len(xs) <= 1 => return []
    return xs.slice(1, len(xs))
}

fn connect(addr) {
    endpoint = addr == null => _default_addr || addr
    sock = &tcp(endpoint)
    next_req = 1
    queued_events = []

    fn request(op, payload) {
        req_id = @next_req
        @next_req = @next_req + 1

        body = _copy_map(payload)
        body.kind = "req"
        body.req_id = req_id
        body.op = op

        @sock.send(%net body)

        while true {
            msg = @sock.recv()
            if msg.kind == "event" {
                @queued_events.push(msg)
            } else if msg.kind == "resp" and msg.req_id == req_id {
                if msg.ok == false => error("gui32 helper: " + msg.error)
                return msg
            }
        }
    }

    fn next_event() {
        cached = _first(@queued_events)
        if cached != null {
            @queued_events = _rest(@queued_events)
            return cached
        }

        while true {
            msg = @sock.recv()
            if msg.kind == "event" => return msg
        }
    }

    fn ping() => request("ping", {}).value

    fn window(title, width, height, opts) {
        cfg = _opt_map(opts)
        cfg.title = title
        cfg.width = width
        cfg.height = height
        return request("create_window", cfg).window
    }

    fn show(window_id) {
        request("show", {window: window_id})
        return null
    }

    fn hide(window_id) {
        request("hide", {window: window_id})
        return null
    }

    fn close(window_id) {
        request("close", {window: window_id})
        return null
    }

    fn set_title(window_id, text) {
        request("set_title", {
            window: window_id,
            text: text
        })
        return null
    }

    fn set_bounds(window_id, x, y, width, height) {
        request("set_bounds", {
            window: window_id,
            x: x,
            y: y,
            width: width,
            height: height
        })
        return null
    }

    fn get_title(window_id) {
        return request("get_title", {
            window: window_id
        }).text
    }

    fn label(window_id, text, x, y, width, height, opts) {
        cfg = _opt_map(opts)
        cfg.window = window_id
        cfg.text = text
        cfg.x = x
        cfg.y = y
        cfg.width = width
        cfg.height = height
        return request("label", cfg).control
    }

    fn button(window_id, text, x, y, width, height, opts) {
        cfg = _opt_map(opts)
        cfg.window = window_id
        cfg.text = text
        cfg.x = x
        cfg.y = y
        cfg.width = width
        cfg.height = height
        return request("button", cfg).control
    }

    fn input(window_id, text, x, y, width, height, opts) {
        cfg = _opt_map(opts)
        cfg.window = window_id
        cfg.text = text
        cfg.x = x
        cfg.y = y
        cfg.width = width
        cfg.height = height
        return request("input", cfg).control
    }

    fn textarea(window_id, text, x, y, width, height, opts) {
        cfg = _opt_map(opts)
        cfg.window = window_id
        cfg.text = text
        cfg.x = x
        cfg.y = y
        cfg.width = width
        cfg.height = height
        return request("textarea", cfg).control
    }

    fn checkbox(window_id, text, x, y, width, height, opts) {
        cfg = _opt_map(opts)
        cfg.window = window_id
        cfg.text = text
        cfg.x = x
        cfg.y = y
        cfg.width = width
        cfg.height = height
        return request("checkbox", cfg).control
    }

    fn listbox(window_id, x, y, width, height, items, opts) {
        cfg = _opt_map(opts)
        cfg.window = window_id
        cfg.text = ""
        cfg.x = x
        cfg.y = y
        cfg.width = width
        cfg.height = height
        cfg.items = items
        return request("listbox", cfg).control
    }

    fn groupbox(window_id, text, x, y, width, height, opts) {
        cfg = _opt_map(opts)
        cfg.window = window_id
        cfg.text = text
        cfg.x = x
        cfg.y = y
        cfg.width = width
        cfg.height = height
        return request("groupbox", cfg).control
    }

    fn canvas(window_id, x, y, width, height, opts) {
        cfg = _opt_map(opts)
        cfg.window = window_id
        cfg.text = ""
        cfg.x = x
        cfg.y = y
        cfg.width = width
        cfg.height = height
        return request("canvas", cfg).control
    }

    fn set_text(control_id, text) {
        request("set_text", {
            control: control_id,
            text: text
        })
        return null
    }

    fn get_text(control_id) {
        return request("get_text", {
            control: control_id
        }).text
    }

    fn set_enabled(control_id, enabled) {
        request("set_enabled", {
            control: control_id,
            enabled: enabled
        })
        return null
    }

    fn focus(control_id) {
        request("focus", {
            control: control_id
        })
        return null
    }

    fn set_checked(control_id, checked) {
        request("set_checked", {
            control: control_id,
            checked: checked
        })
        return null
    }

    fn get_checked(control_id) {
        return request("get_checked", {
            control: control_id
        }).checked
    }

    fn list_set_items(control_id, items) {
        request("list_set_items", {
            control: control_id,
            items: items
        })
        return null
    }

    fn list_get_items(control_id) {
        return request("list_get_items", {
            control: control_id
        }).items
    }

    fn list_add_item(control_id, text) {
        request("list_add_item", {
            control: control_id,
            text: text
        })
        return null
    }

    fn list_set_selected(control_id, selected) {
        request("list_set_selected", {
            control: control_id,
            selected: selected
        })
        return null
    }

    fn list_get_selected(control_id) {
        return request("list_get_selected", {
            control: control_id
        }).selected
    }

    fn set_menu(window_id, items) {
        request("set_menu", {
            window: window_id,
            items: items
        })
        return null
    }

    fn set_timer(window_id, ms, token) {
        return request("set_timer", {
            window: window_id,
            ms: ms,
            token: token
        }).timer
    }

    fn kill_timer(timer_id) {
        request("kill_timer", {
            timer: timer_id
        })
        return null
    }

    fn canvas_clear(control_id, color) {
        request("canvas_clear", {
            control: control_id,
            color: color
        })
        return null
    }

    fn canvas_line(control_id, x1, y1, x2, y2, color, line_width) {
        request("canvas_line", {
            control: control_id,
            x1: x1,
            y1: y1,
            x2: x2,
            y2: y2,
            color: color,
            line_width: line_width
        })
        return null
    }

    fn canvas_rect(control_id, x, y, width, height, opts) {
        cfg = _opt_map(opts)
        cfg.control = control_id
        cfg.x = x
        cfg.y = y
        cfg.width = width
        cfg.height = height
        request("canvas_rect", cfg)
        return null
    }

    fn canvas_text(control_id, text, x, y, color) {
        request("canvas_text", {
            control: control_id,
            text: text,
            x: x,
            y: y,
            color: color
        })
        return null
    }

    fn message(text, title, window_id) {
        request("message", {
            text: text,
            title: title,
            window: window_id
        })
        return null
    }

    fn wrap_control(id, kind, token) {
        fn set_text_m(text) {
            set_text(@id, text)
            return null
        }

        fn text_m() => get_text(@id)

        fn enable_m(enabled) {
            set_enabled(@id, enabled)
            return null
        }

        fn focus_m() {
            focus(@id)
            return null
        }

        base = {
            id: id,
            kind: kind,
            token: token,
            set_text: set_text_m,
            text: text_m,
            enable: enable_m,
            focus: focus_m
        }

        if kind == "checkbox" {
            fn checked_m() => get_checked(@id)
            fn set_checked_m(checked) {
                set_checked(@id, checked)
                return null
            }
            base.checked = checked_m
            base.set_checked = set_checked_m
        }

        if kind == "listbox" {
            fn items_m() => list_get_items(@id)
            fn set_items_m(items) {
                list_set_items(@id, items)
                return null
            }
            fn add_item_m(text) {
                list_add_item(@id, text)
                return null
            }
            fn selected_m() => list_get_selected(@id)
            fn set_selected_m(selected) {
                list_set_selected(@id, selected)
                return null
            }
            base.items = items_m
            base.set_items = set_items_m
            base.add_item = add_item_m
            base.selected = selected_m
            base.set_selected = set_selected_m
        }

        if kind == "canvas" {
            fn clear_m(color) {
                canvas_clear(@id, color)
                return null
            }
            fn line_m(x1, y1, x2, y2, color, line_width) {
                canvas_line(@id, x1, y1, x2, y2, color, line_width)
                return null
            }
            fn rect_m(x, y, width, height, opts) {
                canvas_rect(@id, x, y, width, height, opts)
                return null
            }
            fn draw_text_m(text, x, y, color) {
                canvas_text(@id, text, x, y, color)
                return null
            }
            base.clear = clear_m
            base.line = line_m
            base.rect = rect_m
            base.draw_text = draw_text_m
        }

        return base
    }

    fn new_window(title, width, height, opts) {
        cfg = _opt_map(opts)
        win_id = window(title, width, height, cfg)

        fn show_m() {
            show(@win_id)
            return null
        }

        fn hide_m() {
            hide(@win_id)
            return null
        }

        fn close_m() {
            close(@win_id)
            return null
        }

        fn set_title_m(text) {
            set_title(@win_id, text)
            return null
        }

        fn title_m() => get_title(@win_id)

        fn set_bounds_m(x, y, width, height) {
            set_bounds(@win_id, x, y, width, height)
            return null
        }

        fn label_m(text, x, y, width, height, opts) {
            cfg = _opt_map(opts)
            token = _opt_get(cfg, "token", "")
            return wrap_control(label(@win_id, text, x, y, width, height, cfg), "label", token)
        }

        fn button_m(text, x, y, width, height, opts) {
            cfg = _opt_map(opts)
            token = _opt_get(cfg, "token", "")
            return wrap_control(button(@win_id, text, x, y, width, height, cfg), "button", token)
        }

        fn input_m(text, x, y, width, height, opts) {
            cfg = _opt_map(opts)
            token = _opt_get(cfg, "token", "")
            return wrap_control(input(@win_id, text, x, y, width, height, cfg), "input", token)
        }

        fn textarea_m(text, x, y, width, height, opts) {
            cfg = _opt_map(opts)
            token = _opt_get(cfg, "token", "")
            return wrap_control(textarea(@win_id, text, x, y, width, height, cfg), "textarea", token)
        }

        fn checkbox_m(text, x, y, width, height, opts) {
            cfg = _opt_map(opts)
            token = _opt_get(cfg, "token", "")
            return wrap_control(checkbox(@win_id, text, x, y, width, height, cfg), "checkbox", token)
        }

        fn listbox_m(x, y, width, height, items, opts) {
            cfg = _opt_map(opts)
            token = _opt_get(cfg, "token", "")
            return wrap_control(listbox(@win_id, x, y, width, height, items, cfg), "listbox", token)
        }

        fn groupbox_m(text, x, y, width, height, opts) {
            cfg = _opt_map(opts)
            token = _opt_get(cfg, "token", "")
            return wrap_control(groupbox(@win_id, text, x, y, width, height, cfg), "groupbox", token)
        }

        fn canvas_m(x, y, width, height, opts) {
            cfg = _opt_map(opts)
            token = _opt_get(cfg, "token", "")
            return wrap_control(canvas(@win_id, x, y, width, height, cfg), "canvas", token)
        }

        fn menu_m(items) {
            set_menu(@win_id, items)
            return null
        }

        fn timer_m(ms, token) => set_timer(@win_id, ms, token)

        return {
            id: win_id,
            show: show_m,
            hide: hide_m,
            close: close_m,
            set_title: set_title_m,
            set_bounds: set_bounds_m,
            title: title_m,
            label: label_m,
            button: button_m,
            input: input_m,
            textarea: textarea_m,
            checkbox: checkbox_m,
            listbox: listbox_m,
            groupbox: groupbox_m,
            canvas: canvas_m,
            menu: menu_m,
            timer: timer_m
        }
    }

    fn shutdown() {
        request("shutdown", {})
        @sock.close()
        return null
    }

    fn disconnect() {
        @sock.close()
        return null
    }

    return {
        addr: endpoint,
        ping: ping,
        next_event: next_event,
        window: window,
        show: show,
        hide: hide,
        close: close,
        set_title: set_title,
        set_bounds: set_bounds,
        get_title: get_title,
        label: label,
        button: button,
        input: input,
        textarea: textarea,
        checkbox: checkbox,
        listbox: listbox,
        groupbox: groupbox,
        canvas: canvas,
        set_text: set_text,
        get_text: get_text,
        set_enabled: set_enabled,
        focus: focus,
        set_checked: set_checked,
        get_checked: get_checked,
        list_set_items: list_set_items,
        list_get_items: list_get_items,
        list_add_item: list_add_item,
        list_set_selected: list_set_selected,
        list_get_selected: list_get_selected,
        set_menu: set_menu,
        set_timer: set_timer,
        kill_timer: kill_timer,
        canvas_clear: canvas_clear,
        canvas_line: canvas_line,
        canvas_rect: canvas_rect,
        canvas_text: canvas_text,
        message: message,
        new_window: new_window,
        shutdown: shutdown,
        disconnect: disconnect
    }
}

fn helper_addr() => _default_addr
