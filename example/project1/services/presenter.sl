task = import("../core/task")

fn list_items(state) {
    out = []
    for item in state.tasks {
        out.push(task.line(item))
    }
    return out
}

fn stats(state) {
    done = 0
    open = 0
    points = 0

    for item in state.tasks {
        points = points + item.points
        if item.done {
            done = done + 1
        } else {
            open = open + 1
        }
    }

    return {
        total: len(state.tasks),
        done: done,
        open: open,
        points: points
    }
}

fn status_line(state) {
    s = stats(state)
    selected_text = state.selected_index >= 0 => "selected #" + str(state.tasks[state.selected_index].id) || "new draft"
    return "Project One | " + selected_text + " | total " + str(s.total) + " | open " + str(s.open) + " | done " + str(s.done) + " | " + state.message
}

fn window_title(state) {
    if state.draft.title.trim() == "" => return "Project One"
    return "Project One - " + state.draft.title
}

fn chart_rows(state) {
    s = stats(state)
    return [
        {label: "Open", value: s.open},
        {label: "Done", value: s.done},
        {label: "Points", value: s.points}
    ]
}
