task = import("./task")
sample = import("./sample_data")

fn _parse_points(text) {
    cleaned = text.trim()
    if cleaned == "" => return {ok: false, value: 0, error: "Points cannot be empty."}

    fn parse_it() {
        return int(cleaned)
    }

    r = pcall(parse_it)
    if r.ok == false => return {ok: false, value: 0, error: "Points must be an integer."}
    if r.value < 1 => return {ok: false, value: 0, error: "Points must be >= 1."}
    return {ok: true, value: r.value, error: ""}
}

fn create() {
    tasks = sample.load()
    selected_index = 0
    draft = task.clone(tasks[0])
    next_id = len(tasks) + 1
    message = "Ready."

    fn _has_selection() => @selected_index >= 0 and @selected_index < len(@tasks)

    fn _sync_draft() {
        if _has_selection() {
            @draft = task.clone(@tasks[@selected_index])
        } else {
            @draft = task.blank(@next_id)
        }
        return null
    }

    fn snapshot() {
        return {
            tasks: %deep @tasks,
            selected_index: @selected_index,
            draft: %deep @draft,
            next_id: @next_id,
            message: @message
        }
    }

    fn select(index) {
        if index < 0 or index >= len(@tasks) {
            @selected_index = -1
            @draft = task.blank(@next_id)
            @message = "No task selected."
            return null
        }

        @selected_index = index
        _sync_draft()
        @message = "Loaded task #" + str(@draft.id) + "."
        return null
    }

    fn begin_new() {
        @selected_index = -1
        @draft = task.blank(@next_id)
        @message = "Composing a new task."
        return null
    }

    fn set_field(key, value) {
        @draft[key] = value
        @message = "Editing " + key + "."
        return null
    }

    fn set_done(done) {
        @draft.done = done
        @message = done => "Marked draft as done." || "Marked draft as open."
        return null
    }

    fn set_points_text(text) {
        parsed = _parse_points(text)
        if parsed.ok {
            @draft.points = parsed.value
            @message = "Points updated."
            return true
        }
        @message = parsed.error
        return false
    }

    fn save() {
        title = @draft.title.trim()
        owner = @draft.owner.trim()

        if title == "" {
            @message = "Title cannot be empty."
            return false
        }
        if owner == "" {
            @message = "Owner cannot be empty."
            return false
        }

        if _has_selection() {
            current_id = @tasks[@selected_index].id
            saved = task.make(current_id, title, owner, @draft.points, @draft.done, @draft.note)
            @tasks[@selected_index] = saved
            @draft = task.clone(saved)
            @message = "Updated task #" + str(current_id) + "."
            return true
        }

        saved = task.make(@next_id, title, owner, @draft.points, @draft.done, @draft.note)
        @tasks.push(saved)
        @selected_index = len(@tasks) - 1
        @draft = task.clone(saved)
        @message = "Created task #" + str(@next_id) + "."
        @next_id = @next_id + 1
        return true
    }

    fn remove_selected() {
        if not _has_selection() {
            @message = "Nothing to delete."
            return false
        }

        removed = @tasks[@selected_index]
        kept = []
        for i in range(len(@tasks)) {
            if i != @selected_index {
                kept.push(@tasks[i])
            }
        }

        @tasks = kept
        if len(@tasks) == 0 {
            @selected_index = -1
            @draft = task.blank(@next_id)
        } else {
            if @selected_index >= len(@tasks) {
                @selected_index = len(@tasks) - 1
            }
            _sync_draft()
        }

        @message = "Deleted task #" + str(removed.id) + "."
        return true
    }

    fn note_canvas(x, y) {
        @message = "Canvas probe at (" + str(x) + ", " + str(y) + ")."
        return null
    }

    fn note_resize(width, height) {
        @message = "Window size " + str(width) + " x " + str(height) + "."
        return null
    }

    return {
        snapshot: snapshot,
        select: select,
        begin_new: begin_new,
        set_field: set_field,
        set_done: set_done,
        set_points_text: set_points_text,
        save: save,
        remove_selected: remove_selected,
        note_canvas: note_canvas,
        note_resize: note_resize
    }
}
