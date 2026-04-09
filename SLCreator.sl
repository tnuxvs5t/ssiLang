W = import("modules/wingui.sl")
S = import("modules/sWidgets/core.sl")
runner_mod_wrap = pcall(() -> import("modules/slrunner.sl"))
if not runner_mod_wrap.ok {
    error("Failed to import modules/slrunner.sl: " + str(runner_mod_wrap.error))
}
Runner = runner_mod_wrap.value

fn make_theme() {
    return {
        window_bg: W.rgb(24, 26, 31),
        panel_bg: W.rgb(31, 34, 40),
        panel_border: W.rgb(58, 63, 74),
        text: W.rgb(227, 230, 236),
        muted_text: W.rgb(147, 155, 169),
        accent: W.rgb(22, 136, 255),
        accent_hover: W.rgb(48, 148, 255),
        accent_active: W.rgb(16, 116, 218),
        accent_border: W.rgb(16, 102, 188),
        selection_bg: W.rgb(44, 93, 153),
        gutter_bg: W.rgb(27, 29, 34),
        gutter_text: W.rgb(104, 111, 125),
        current_line_bg: W.rgb(39, 43, 50),
        popup_bg: W.rgb(36, 39, 46),
        popup_border: W.rgb(78, 84, 98),
        popup_active_bg: W.rgb(47, 78, 122),
        syntax_keyword: W.rgb(201, 132, 255),
        syntax_builtin: W.rgb(87, 197, 255),
        syntax_string: W.rgb(247, 197, 110),
        syntax_comment: W.rgb(110, 151, 96),
        syntax_number: W.rgb(111, 214, 170),
        syntax_operator: W.rgb(150, 161, 177),
        button_text: W.rgb(255, 255, 255),
        input_bg: W.rgb(22, 24, 29),
        input_border: W.rgb(63, 68, 80),
        input_focus_border: W.rgb(22, 136, 255),
        caret: W.rgb(255, 255, 255),
        font: W.font("Segoe UI", 16),
        title_font: W.font("Segoe UI", 22, {weight: 700}),
        button_font: W.font("Segoe UI", 15, {weight: 700}),
        mono_font: W.font("Consolas", 15)
    }
}

theme = make_theme()
app = S.app({})
runner = null

default_path = __dir + "/playground.sl"
window_ref = null
status_text = "Ready to learn ssiLang."
dirty = false
runner_visible = true
last_exit_code = null

path_state = S.line_edit_state(default_path, {})
runner_input_state = S.editor_state("ssiLang learner\n", {})
runner_output_state = S.editor_state(
    [
        "SLCreator runner is ready.",
        "",
        "- Put your script input in the left runner pane.",
        "- Click Run to save, execute, and inspect output below.",
        "- Demo loads an input() example for quick learning."
    ].join("\n"),
    {}
)

fn on_editor_change(text) {
    @dirty = true
    @status_text = "Editor updated."
    return null
}

editor_state = S.ide_editor_state(
    [
        "# ssiLang playground",
        "print(\"What is your name?\")",
        "name = input()",
        "print(\"Hello, \" + name)",
        "",
        "nums = [1, 2, 3, 4, 5]",
        "print(nums |> filter($ % 2 == 1) |> map($ * 10))"
    ].join("\n"),
    {
        on_change: on_editor_change,
        words: ["runner", "stdin", "stdout", "playground", "workspace"]
    }
)

fn redraw_now() {
    if @window_ref != null {
        win = @window_ref
        win.redraw()
    }
    return null
}

fn set_status(text) {
    @status_text = text
    redraw_now()
    return null
}

fn mark_clean() {
    @dirty = false
    return null
}

fn refresh_ui() {
    redraw_now()
    return null
}

fn current_path() => path_state.get()

fn basename(path) {
    xs = path.split("/")
    tail = xs[len(xs) - 1]
    ys = tail.split("\\")
    return ys[len(ys) - 1]
}

fn dirty_text() => dirty => "Modified" || "Saved"

fn append_ops(dst, src) {
    if src == null => return dst
    for op in src {
        dst.push(op)
    }
    return dst
}

fn save_file() {
    path = current_path()
    wrap = pcall(() -> sys.write_text(path, editor_state.get()))
    if not wrap.ok {
        set_status("Save failed: " + str(wrap.error))
        runner_output_state.set("Save failed.\n" + str(wrap.error))
        return false
    }
    mark_clean()
    set_status("Saved: " + path)
    return true
}

fn load_file() {
    path = current_path()
    wrap = pcall(() -> sys.read_text(path))
    if not wrap.ok {
        set_status("Load failed: " + str(wrap.error))
        runner_output_state.set("Load failed.\n" + str(wrap.error))
        return null
    }
    editor_state.set(wrap.value)
    mark_clean()
    set_status("Loaded: " + path)
    return null
}

fn new_file() {
    editor_state.set("")
    runner_input_state.set("")
    runner_output_state.set("New file created. Press Run after writing your script.")
    mark_clean()
    @last_exit_code = null
    set_status("New file.")
    return null
}

fn load_demo() {
    editor_state.set(
        [
            "# Demo: interactive runner",
            "print(\"Enter your favorite number:\")",
            "raw = input()",
            "n = int(raw)",
            "print(\"double = \" + str(n * 2))",
            "",
            "nums = [1, 2, 3, 4, 5]",
            "picked = nums |> filter($ % 2 == 1) |> map($ * 10)",
            "print(picked)"
        ].join("\n")
    )
    runner_input_state.set("21\n")
    runner_output_state.set("Demo loaded. Click Run to see input()/output() in the runner.")
    mark_clean()
    @runner_visible = true
    @last_exit_code = null
    set_status("Loaded interactive demo.")
    refresh_ui()
    return null
}

fn clear_runner_output() {
    runner_output_state.set("")
    @last_exit_code = null
    set_status("Runner output cleared.")
    return null
}

fn toggle_runner() {
    @runner_visible = not @runner_visible
    set_status(@runner_visible => "Runner opened." || "Runner collapsed.")
    refresh_ui()
    return null
}

fn ensure_runner_panel() {
    if not @runner_visible {
        @runner_visible = true
        refresh_ui()
    }
    return null
}

fn run_script() {
    if not save_file() => return null

    path = current_path()
    ensure_runner_panel()
    runner_output_state.set("Running " + basename(path) + " ...")
    set_status("Running: " + path)
    redraw_now()

    if @runner == null {
        start_wrap = pcall(() -> Runner.start({}))
        if not start_wrap.ok {
            runner_output_state.set("Runner init failed.\n" + str(start_wrap.error))
            set_status("Runner init failed.")
            return null
        }
        @runner = start_wrap.value
    }

    cur_runner = @runner
    wrap = pcall(() -> cur_runner.run({
        script: path,
        cwd: __dir,
        input: runner_input_state.get()
    }))
    if not wrap.ok {
        @last_exit_code = null
        runner_output_state.set("Runner failed.\n" + str(wrap.error))
        set_status("Run failed.")
        return null
    }

    resp = wrap.value
    @last_exit_code = resp.exit_code
    output = resp.output == "" => "[process finished with no stdout/stderr output]" || resp.output
    runner_output_state.set(output)
    set_status("Process exited with code " + str(resp.exit_code))
    return null
}
side_panel_width = 260
runner_panel_height = 250
shell_drag = null

fn shell_button(label_fn, action, style) {
    conf = {
        h: style.h == null => 34 || style.h,
        bg: style.bg == null => W.rgb(45, 49, 58) || style.bg,
        hover_bg: style.hover_bg == null => W.rgb(57, 62, 73) || style.hover_bg,
        active_bg: style.active_bg == null => W.rgb(68, 74, 86) || style.active_bg,
        border: style.border == null => theme.panel_border || style.border,
        text_color: style.text_color == null => theme.text || style.text_color,
        font: style.font == null => W.font("Segoe UI", 14, {weight: 600}) || style.font
    }
    self = null

    fn measure(ctx, avail_w) => {w: avail_w, h: conf.h}
    fn layout(ctx, box) {
        hot = ctx.is_hot(self)
        active = ctx.is_active(self)
        fill = active => conf.active_bg || hot => conf.hover_bg || conf.bg
        return [
            W.fill_rect(box.x, box.y, box.w, box.h, fill),
            W.rect(box.x, box.y, box.w, box.h, conf.border, 1),
            W.text(box.x + 12, box.y + 8, label_fn(), {
                color: conf.text_color,
                font: conf.font
            })
        ]
    }
    fn on_event(evt, ctx) {
        if evt.type == "mouse_up" and evt.button == "left" and ctx.is_active(self) and ctx.is_hot(self) {
            action()
            return true
        }
        if evt.type == "key_down" and (evt.key == 13 or evt.key == 32) {
            action()
            return true
        }
        return false
    }

    self = S.custom({
        measure: measure,
        layout: layout,
        on_event: on_event,
        interactive: true,
        focusable: true
    })
    return self
}

path_widget = S.line_edit(path_state, {
    placeholder: "script path",
    font: theme.font,
    bg: theme.input_bg,
    border: theme.input_border,
    focus_border: theme.input_focus_border
})

editor_widget = S.ide_editor(editor_state, {
    bg: theme.input_bg,
    border: theme.input_border,
    focus_border: theme.input_focus_border,
    gutter_bg: theme.gutter_bg,
    current_line_bg: theme.current_line_bg,
    popup_bg: theme.popup_bg
})

runner_input_widget = S.editor(runner_input_state, {
    font: theme.mono_font,
    bg: theme.input_bg,
    border: theme.input_border,
    focus_border: theme.input_focus_border
})

runner_output_widget = S.editor(runner_output_state, {
    font: theme.mono_font,
    bg: theme.input_bg,
    border: theme.input_border,
    focus_border: theme.input_focus_border
})

btn_new = shell_button(() -> "New", new_file, {})
btn_load = shell_button(() -> "Load", load_file, {})
btn_save = shell_button(() -> "Save", save_file, {})
btn_demo = shell_button(() -> "Demo", load_demo, {})
btn_run = shell_button(() -> "Run", run_script, {
    bg: theme.accent,
    hover_bg: theme.accent_hover,
    active_bg: theme.accent_active,
    border: theme.accent_border,
    text_color: theme.button_text,
    font: W.font("Segoe UI", 14, {weight: 700})
})
btn_console = shell_button(() -> runner_visible => "Console -" || "Console +", toggle_runner, {})
btn_clear = shell_button(() -> "Clear Output", clear_runner_output, {})

fn clamp_side_width(total_w) {
    min_w = 200
    max_w = total_w - 480
    if max_w < min_w {
        max_w = min_w
    }
    @side_panel_width = side_panel_width < min_w => min_w || side_panel_width > max_w => max_w || side_panel_width
    return @side_panel_width
}

fn clamp_runner_height(total_h) {
    min_h = 150
    max_h = total_h - 220
    if max_h < min_h {
        max_h = min_h
    }
    @runner_panel_height = runner_panel_height < min_h => min_h || runner_panel_height > max_h => max_h || runner_panel_height
    return @runner_panel_height
}

fn panel_ops(rect, fill) {
    return [
        W.fill_rect(rect.x, rect.y, rect.w, rect.h, fill),
        W.rect(rect.x, rect.y, rect.w, rect.h, theme.panel_border, 1)
    ]
}

fn draw_lines(x, y, lines, color, font_cfg, line_h) {
    ops = []
    row = 0
    for line in lines {
        ops.push(W.text(x, y + row * line_h, line, {
            color: color,
            font: font_cfg
        }))
        row = row + 1
    }
    return ops
}

fn shell_rects(box) {
    title_h = 62
    status_h = 30
    outer_pad = 10
    gap = 10
    side_split = 6
    runner_split = 6
    tool_h = 44
    toolbar_gap = 10
    runner_h = runner_visible => clamp_runner_height(box.h - title_h - status_h - outer_pad * 2 - gap * 2) || 0

    body_y = box.y + title_h + outer_pad
    body_h = box.h - title_h - status_h - outer_pad * 2
    side_w = clamp_side_width(box.w - outer_pad * 2)
    side_rect = {x: box.x + outer_pad, y: body_y, w: side_w, h: body_h}
    side_split_rect = {x: side_rect.x + side_rect.w + 4, y: body_y, w: side_split, h: body_h}

    work_x = side_split_rect.x + side_split_rect.w + gap
    work_w = box.x + box.w - outer_pad - work_x
    toolbar_rect = {x: work_x, y: body_y, w: work_w, h: tool_h}

    editor_y = toolbar_rect.y + toolbar_rect.h + toolbar_gap
    editor_h = body_y + body_h - editor_y
    runner_split_rect = null
    runner_rect = null
    if runner_visible {
        editor_h = editor_h - runner_h - runner_split - gap
        runner_split_rect = {x: work_x, y: editor_y + editor_h + gap / 2, w: work_w, h: runner_split}
        runner_rect = {x: work_x, y: runner_split_rect.y + runner_split_rect.h + gap / 2, w: work_w, h: runner_h}
    }
    editor_rect = {x: work_x, y: editor_y, w: work_w, h: editor_h}
    status_rect = {x: box.x, y: box.y + box.h - status_h, w: box.w, h: status_h}

    return {
        title: {x: box.x, y: box.y, w: box.w, h: title_h},
        side: side_rect,
        side_split: side_split_rect,
        toolbar: toolbar_rect,
        editor: editor_rect,
        runner_split: runner_split_rect,
        runner: runner_rect,
        status: status_rect
    }
}

fn root_children() {
    xs = [
        path_widget,
        btn_new,
        btn_load,
        btn_save,
        btn_demo,
        btn_run,
        btn_console,
        btn_clear,
        editor_widget
    ]
    if runner_visible {
        xs.push(runner_input_widget)
        xs.push(runner_output_widget)
    }
    return xs
}

fn shell_root() {
    self = null

    fn measure(ctx, avail_w) => {w: avail_w, h: 0}

    fn layout(ctx, box) {
        rs = shell_rects(box)
        title_font = theme.title_font
        sub_font = W.font("Segoe UI", 13)
        small_font = W.font("Segoe UI", 12)
        status_font = W.font("Segoe UI", 13)
        ops = [W.fill_rect(box.x, box.y, box.w, box.h, theme.window_bg)]

        append_ops(ops, panel_ops(rs.side, theme.panel_bg))
        append_ops(ops, panel_ops(rs.toolbar, W.rgb(26, 28, 34)))
        append_ops(ops, panel_ops(rs.editor, theme.panel_bg))
        if rs.runner != null {
            append_ops(ops, panel_ops(rs.runner, theme.panel_bg))
            ops.push(W.fill_rect(rs.runner_split.x, rs.runner_split.y, rs.runner_split.w, rs.runner_split.h, theme.panel_border))
        }

        ops.push(W.fill_rect(rs.title.x, rs.title.y, rs.title.w, rs.title.h, W.rgb(20, 22, 27)))
        ops.push(W.fill_rect(rs.title.x, rs.title.y + rs.title.h - 1, rs.title.w, 1, theme.panel_border))
        ops.push(W.fill_rect(rs.status.x, rs.status.y, rs.status.w, rs.status.h, W.rgb(20, 22, 27)))
        ops.push(W.fill_rect(rs.status.x, rs.status.y, rs.status.w, 1, theme.panel_border))
        ops.push(W.fill_rect(rs.side_split.x, rs.side_split.y, rs.side_split.w, rs.side_split.h, shell_drag != null and shell_drag.kind == "side" => theme.accent || theme.panel_border))

        ops.push(W.text(rs.title.x + 18, rs.title.y + 12, "SLCreator", {
            color: theme.text,
            font: title_font
        }))
        ops.push(W.text(rs.title.x + 18, rs.title.y + 38, "Fixed-shell IDE with built-in runner", {
            color: theme.muted_text,
            font: sub_font
        }))
        ops.push(W.text(rs.title.x + 420, rs.title.y + 22, basename(current_path()) + "  |  " + dirty_text() + "  |  " + (runner_visible => "Runner visible" || "Runner hidden"), {
            color: theme.muted_text,
            font: status_font
        }))

        side_x = rs.side.x + 14
        side_y = rs.side.y + 16
        ops.push(W.text(side_x, side_y, "Workspace", {
            color: theme.text,
            font: W.font("Segoe UI", 15, {weight: 700})
        }))
        append_ops(ops, draw_lines(side_x, side_y + 28, [
            "File: " + basename(current_path()),
            "State: " + dirty_text(),
            "Runner: " + (runner_visible => "open" || "collapsed"),
            "Last exit: " + (last_exit_code == null => "--" || str(last_exit_code))
        ], theme.muted_text, small_font, 18))
        ops.push(W.text(side_x, side_y + 120, "Learning Loop", {
            color: theme.text,
            font: W.font("Segoe UI", 14, {weight: 700})
        }))
        append_ops(ops, draw_lines(side_x, side_y + 148, [
            "1. Edit code in the main editor",
            "2. Feed input in Runner stdin",
            "3. Run and inspect stdout / stderr",
            "4. Repeat until it clicks"
        ], theme.muted_text, small_font, 18))
        ops.push(W.text(side_x, side_y + 248, "ssiLang Tips", {
            color: theme.text,
            font: W.font("Segoe UI", 14, {weight: 700})
        }))
        append_ops(ops, draw_lines(side_x, side_y + 276, [
            "Ctrl+Space / '.' => completion",
            "print(...), input(), pcall(...)",
            "list |> map(...) |> filter(...)",
            "@outer and $ placeholders"
        ], theme.muted_text, small_font, 18))

        ops.push(W.text(rs.toolbar.x + 14, rs.toolbar.y + 8, "Command Bar", {
            color: theme.text,
            font: W.font("Segoe UI", 14, {weight: 700})
        }))

        path_rect = {x: rs.toolbar.x + 120, y: rs.toolbar.y + 5, w: rs.toolbar.w - 120 - 504, h: 34}
        if path_rect.w < 180 {
            path_rect.w = 180
        }
        btn_y = rs.toolbar.y + 5
        btn_w = 76
        gap = 8
        bx = path_rect.x + path_rect.w + gap
        append_ops(ops, path_widget.layout(ctx, path_rect))
        append_ops(ops, btn_new.layout(ctx, {x: bx + 0 * (btn_w + gap), y: btn_y, w: btn_w, h: 34}))
        append_ops(ops, btn_load.layout(ctx, {x: bx + 1 * (btn_w + gap), y: btn_y, w: btn_w, h: 34}))
        append_ops(ops, btn_save.layout(ctx, {x: bx + 2 * (btn_w + gap), y: btn_y, w: btn_w, h: 34}))
        append_ops(ops, btn_demo.layout(ctx, {x: bx + 3 * (btn_w + gap), y: btn_y, w: btn_w, h: 34}))
        append_ops(ops, btn_run.layout(ctx, {x: bx + 4 * (btn_w + gap), y: btn_y, w: btn_w, h: 34}))
        append_ops(ops, btn_console.layout(ctx, {x: bx + 5 * (btn_w + gap), y: btn_y, w: 96, h: 34}))

        ops.push(W.text(rs.editor.x + 14, rs.editor.y + 10, "Editor", {
            color: theme.text,
            font: W.font("Segoe UI", 14, {weight: 700})
        }))
        ops.push(W.text(rs.editor.x + 92, rs.editor.y + 11, "Syntax highlight + completion", {
            color: theme.muted_text,
            font: small_font
        }))
        append_ops(ops, editor_widget.layout(ctx, {x: rs.editor.x + 10, y: rs.editor.y + 36, w: rs.editor.w - 20, h: rs.editor.h - 46}))

        if rs.runner != null {
            ops.push(W.text(rs.runner.x + 14, rs.runner.y + 10, "Runner", {
                color: theme.text,
                font: W.font("Segoe UI", 14, {weight: 700})
            }))
            ops.push(W.text(rs.runner.x + 82, rs.runner.y + 11, "stdin / stdout / stderr", {
                color: theme.muted_text,
                font: small_font
            }))
            append_ops(ops, btn_clear.layout(ctx, {x: rs.runner.x + rs.runner.w - 120, y: rs.runner.y + 6, w: 106, h: 34}))
            runner_gap = 10
            input_w = 300
            input_rect = {x: rs.runner.x + 10, y: rs.runner.y + 46, w: input_w, h: rs.runner.h - 56}
            output_rect = {x: input_rect.x + input_rect.w + runner_gap, y: rs.runner.y + 46, w: rs.runner.w - input_w - runner_gap - 20, h: rs.runner.h - 56}
            ops.push(W.text(input_rect.x, rs.runner.y + 28, "stdin", {
                color: theme.muted_text,
                font: small_font
            }))
            ops.push(W.text(output_rect.x, rs.runner.y + 28, "stdout / stderr", {
                color: theme.muted_text,
                font: small_font
            }))
            append_ops(ops, runner_input_widget.layout(ctx, input_rect))
            append_ops(ops, runner_output_widget.layout(ctx, output_rect))
        }

        line_col = editor_state.line_col()
        stats_wrap = pcall(() -> app.stats())
        wingui_text = "WQ --"
        script_text = "SQ --/--"
        fps_text = "WFPS --"
        if stats_wrap.ok {
            wingui_text = stats_wrap.value.wingui_queue_len == null => wingui_text || "WQ " + str(int(stats_wrap.value.wingui_queue_len))
            high = stats_wrap.value.script_queue_high_watermark == null => 0 || stats_wrap.value.script_queue_high_watermark
            script_text = "SQ " + str(int(stats_wrap.value.script_queue_len)) + "/" + str(int(high))
            fps_text = stats_wrap.value.wingui_fps == null => fps_text || "WFPS " + str(int(stats_wrap.value.wingui_fps + 0.5))
        }
        ops.push(W.text(rs.status.x + 12, rs.status.y + 7, status_text, {
            color: theme.text,
            font: status_font
        }))
        ops.push(W.text(rs.status.x + 520, rs.status.y + 7, wingui_text + " | " + script_text + " | " + fps_text + " | Ln " + str(line_col.line + 1) + ", Col " + str(line_col.col + 1) + " | " + (last_exit_code == null => "not run yet" || "exit " + str(last_exit_code)), {
            color: theme.muted_text,
            font: status_font
        }))

        return ops
    }

    fn on_event(evt, ctx) {
        rs = shell_rects(self.bounds())
        if evt.type == "mouse_down" and evt.button == "left" {
            if rs.side_split != null and evt.x >= rs.side_split.x and evt.x < rs.side_split.x + rs.side_split.w and evt.y >= rs.side_split.y and evt.y < rs.side_split.y + rs.side_split.h {
                @shell_drag = {kind: "side", start_x: evt.x, start_width: side_panel_width}
                return true
            }
            if rs.runner_split != null and evt.x >= rs.runner_split.x and evt.x < rs.runner_split.x + rs.runner_split.w and evt.y >= rs.runner_split.y and evt.y < rs.runner_split.y + rs.runner_split.h {
                @shell_drag = {kind: "runner", start_y: evt.y, start_height: runner_panel_height}
                return true
            }
        }
        if evt.type == "mouse_move" and ctx.is_active(self) and shell_drag != null {
            if shell_drag.kind == "side" {
                @side_panel_width = shell_drag.start_width + (evt.x - shell_drag.start_x)
                clamp_side_width(self.bounds().w - 20)
                return true
            }
            if shell_drag.kind == "runner" {
                @runner_panel_height = shell_drag.start_height - (evt.y - shell_drag.start_y)
                clamp_runner_height(self.bounds().h - 120)
                return true
            }
        }
        if evt.type == "mouse_up" and evt.button == "left" and shell_drag != null {
            @shell_drag = null
            return true
        }
        return false
    }

    self = S.custom({
        measure: measure,
        layout: layout,
        on_event: on_event,
        children: root_children,
        interactive: true
    })

    for child in root_children() {
        child.set_parent(self)
    }
    return self
}

root = shell_root()

window_ref = app.create_window({
    title: "SLCreator IDE",
    width: 1360,
    height: 900,
    visible: true,
    theme: theme,
    background: theme.window_bg,
    root: root
})

app.run()
if runner != null {
    runner.stop()
}
