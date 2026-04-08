# Ad-hoc blast 1: context + placeholder + closure mesh

fn make_board(name) {
    next_id = 1
    tasks = []
    done_count = 0
    audit = []

    fn add_task(title, points, tags) {
        task = {
            id: @next_id,
            title: title,
            points: points,
            tags: tags,
            status: "todo",
            notes: []
        }
        @next_id = @next_id + 1
        @tasks.push(task)
        @audit.push("add:" + title)
        return task
    }

    fn get_task(id) {
        return @tasks |> find($.id == id)
    }

    fn note(id, text) {
        task = get_task(id)
        @(task) {
            if @ == null => error("task missing")
            @.notes.push(text)
            @audit.push("note:" + @.title)
        }
        return null
    }

    fn start(id) {
        task = get_task(id)
        @(task) {
            if @ == null => error("task missing")
            if @.status == "todo" {
                @.status = "doing"
                @audit.push("start:" + @.title)
            }
            return @.status
        }
    }

    fn finish(id) {
        task = get_task(id)
        @(task) {
            if @ == null => error("task missing")
            if @.status != "done" {
                @.status = "done"
                @done_count = @done_count + 1
                @audit.push("done:" + @.title)
            }
            return @.status
        }
    }

    fn retitle(id, prefix) {
        task = get_task(id)
        @(task) {
            if @ == null => error("task missing")
            @.title = prefix + @.title
            return @.title
        }
    }

    fn summary(dummy) {
        total_points = @tasks |> map($.points) |> reduce($1 + $2, 0)
        open_titles = @tasks |> filter($.status != "done") |> map($.title)
        done_titles = @tasks |> filter($.status == "done") |> map($.title)
        total_notes = @tasks |> map(len($.notes)) |> reduce($1 + $2, 0)
        return {
            board: @name,
            total: len(@tasks),
            done: @done_count,
            total_points: total_points,
            open_titles: open_titles,
            done_titles: done_titles,
            total_notes: total_notes,
            audit: %copy @audit
        }
    }

    fn done_ref(dummy) => &ref @done_count

    return {
        add_task: add_task,
        get_task: get_task,
        note: note,
        start: start,
        finish: finish,
        retitle: retitle,
        summary: summary,
        done_ref: done_ref
    }
}

board = make_board("alpha")
board.add_task("lexer", 3, ["core", "parse"])
board.add_task("parser", 5, ["core", "parse"])
board.add_task("runtime", 8, ["core", "eval"])
board.note(1, "token stream")
board.note(1, "unicode later")
board.note(3, "context stack")
print(board.start(1))
print(board.finish(1))
print(board.start(2))
print(board.retitle(2, "v3-"))

report = board.summary(0)
print(report.board)
print(report.total)
print(report.done)
print(report.total_points)
print(report.open_titles)
print(report.done_titles)
print(report.total_notes)
print(report.audit)

r = board.done_ref(0)
print(r.deref())
r.set(7)
print(board.summary(0).done)

__ph1 = 100
collision_safe = $ + __ph1
print(collision_safe(23))

velocity = 0
for item in report.done_titles {
    velocity = velocity + len(item)
}
if velocity > 0 {
    velocity = velocity + len(report.open_titles)
}
print(velocity)

print("=== V3 CONTEXT BLAST PASSED ===")
