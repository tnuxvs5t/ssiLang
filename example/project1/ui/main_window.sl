theme = import("./theme")

fn create(app) {
    colors = theme.palette()
    win = app.new_window("Project One", 980, 720, {resizable: true})

    header = win.label("Project One / Delivery Desk", 16, 12, 420, 24, null)

    tasks_group = win.groupbox("Task List", 16, 42, 330, 592, null)
    task_list = win.listbox(30, 72, 302, 430, [], {token: "task_list"})
    new_btn = win.button("New", 30, 520, 86, 28, {token: "new_task"})
    save_btn = win.button("Save", 128, 520, 86, 28, {token: "save_task"})
    delete_btn = win.button("Delete", 226, 520, 86, 28, {token: "delete_task"})

    details_group = win.groupbox("Details", 362, 42, 598, 340, null)
    title_label = win.label("Title", 380, 74, 100, 20, null)
    title_input = win.input("", 380, 96, 268, 24, {token: "title_input"})
    owner_label = win.label("Owner", 664, 74, 100, 20, null)
    owner_input = win.input("", 664, 96, 112, 24, {token: "owner_input"})
    points_label = win.label("Points", 792, 74, 100, 20, null)
    points_input = win.input("1", 792, 96, 64, 24, {token: "points_input"})
    done_box = win.checkbox("Done", 872, 94, 70, 24, {token: "done_box"})
    note_label = win.label("Notes", 380, 132, 100, 20, null)
    note_box = win.textarea("", 380, 154, 562, 204, {token: "note_input"})

    chart_group = win.groupbox("Delivery Snapshot", 362, 396, 598, 238, null)
    chart_canvas = win.canvas(380, 426, 562, 170, {token: "stats_canvas", color: colors.panel})

    status = win.label("Ready.", 16, 652, 944, 20, null)

    win.menu([
        {
            text: "File",
            children: [
                {text: "New Task", token: "menu_new"},
                {text: "Save Task", token: "menu_save"},
                {text: "Delete Task", token: "menu_delete"},
                {sep: true},
                {text: "Exit", token: "menu_exit"}
            ]
        },
        {
            text: "Help",
            children: [
                {text: "About Project One", token: "menu_about"}
            ]
        }
    ])

    return {
        window: win,
        colors: colors,
        header: header,
        task_list: task_list,
        new_btn: new_btn,
        save_btn: save_btn,
        delete_btn: delete_btn,
        title_input: title_input,
        owner_input: owner_input,
        points_input: points_input,
        done_box: done_box,
        note_box: note_box,
        chart_canvas: chart_canvas,
        status: status
    }
}
