fn create(app, store, ui, render) {
    suppressed = {
        title_input: 0,
        owner_input: 0,
        points_input: 0,
        note_input: 0
    }

    fn ignore_change(token) {
        left = suppressed?[token]
        if left == null or left <= 0 => return false
        suppressed[token] = left - 1
        return true
    }

    fn rerender() {
        suppressed.title_input = 1
        suppressed.owner_input = 1
        suppressed.points_input = 1
        suppressed.note_input = 1
        render.render_all(ui, store.snapshot())
        return null
    }

    fn refresh_status() {
        render.render_status(ui, store.snapshot())
        return null
    }

    fn handle_menu(token) {
        if token == "menu_new" {
            store.begin_new()
            rerender()
            return null
        }
        if token == "menu_save" {
            store.save()
            rerender()
            return null
        }
        if token == "menu_delete" {
            store.remove_selected()
            rerender()
            return null
        }
        if token == "menu_about" {
            app.message("Project One is a pure ssiLang desktop application built from layered modules.", "About Project One", ui.window.id)
            return null
        }
        if token == "menu_exit" {
            ui.window.close()
            return null
        }
        return null
    }

    fn handle(ev) {
        if ev.event == "close" and ev.window == ui.window.id => return false

        if ev.event == "menu" {
            handle_menu(ev.token)
            return true
        }

        if ev.event == "click" and ev.token == "new_task" {
            store.begin_new()
            rerender()
            return true
        }

        if ev.event == "click" and ev.token == "save_task" {
            store.save()
            rerender()
            return true
        }

        if ev.event == "click" and ev.token == "delete_task" {
            store.remove_selected()
            rerender()
            return true
        }

        if ev.event == "click" and ev.token == "done_box" {
            store.set_done(ev.checked)
            rerender()
            return true
        }

        if ev.event == "select" and ev.token == "task_list" {
            store.select(ev.selected)
            rerender()
            return true
        }

        if ev.event == "change" and ev.token == "title_input" {
            if ignore_change(ev.token) => return true
            store.set_field("title", ui.title_input.text())
            refresh_status()
            return true
        }

        if ev.event == "change" and ev.token == "owner_input" {
            if ignore_change(ev.token) => return true
            store.set_field("owner", ui.owner_input.text())
            refresh_status()
            return true
        }

        if ev.event == "change" and ev.token == "points_input" {
            if ignore_change(ev.token) => return true
            store.set_points_text(ui.points_input.text())
            refresh_status()
            return true
        }

        if ev.event == "change" and ev.token == "note_input" {
            if ignore_change(ev.token) => return true
            store.set_field("note", ui.note_box.text())
            refresh_status()
            return true
        }

        if ev.event == "canvas_click" and ev.token == "stats_canvas" {
            store.note_canvas(ev.x, ev.y)
            refresh_status()
            return true
        }

        if ev.event == "resize" and ev.window == ui.window.id {
            store.note_resize(ev.width, ev.height)
            refresh_status()
            return true
        }

        if ev.event == "key_down" and ev.key == 27 {
            ui.window.close()
            return true
        }

        return true
    }

    return handle
}
