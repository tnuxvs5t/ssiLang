presenter = import("../services/presenter")

fn render_status(ui, state) {
    ui.status.set_text(presenter.status_line(state))
    ui.window.set_title(presenter.window_title(state))
    return null
}

fn render_chart(ui, state) {
    rows = presenter.chart_rows(state)
    colors = ui.colors
    chart = ui.chart_canvas

    chart.clear(colors.panel)
    chart.rect(0, 0, 560, 168, {
        fill: true,
        fill_color: colors.panel,
        line_color: colors.border
    })
    chart.draw_text("Delivery Snapshot", 16, 12, colors.accent)

    max_value = 1
    for row in rows {
        if row.value > max_value {
            max_value = row.value
        }
    }

    for i in range(len(rows)) {
        row = rows[i]
        y = 42 + i * 38

        bar_color = colors.bar_points
        if row.label == "Open" {
            bar_color = colors.bar_open
        }
        if row.label == "Done" {
            bar_color = colors.bar_done
        }

        chart.draw_text(row.label, 18, y, colors.ink)
        width = int(340 * row.value / max_value)
        chart.rect(112, y - 2, width + 1, 20, {
            fill: true,
            fill_color: bar_color,
            line_color: colors.border
        })
        chart.draw_text(str(row.value), 468, y, colors.ink)
    }

    return null
}

fn render_all(ui, state) {
    ui.task_list.set_items(presenter.list_items(state))

    if state.selected_index >= 0 {
        ui.task_list.set_selected(state.selected_index)
    } else {
        ui.task_list.set_selected(-1)
    }

    ui.title_input.set_text(state.draft.title)
    ui.owner_input.set_text(state.draft.owner)
    ui.points_input.set_text(str(state.draft.points))
    ui.done_box.set_checked(state.draft.done)
    ui.note_box.set_text(state.draft.note)

    render_chart(ui, state)
    render_status(ui, state)
    return null
}
