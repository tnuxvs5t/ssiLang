fn make(id, title, owner, points, done, note) {
    return {
        id: id,
        title: title,
        owner: owner,
        points: points,
        done: done,
        note: note
    }
}

fn blank(id) => make(id, "", "", 1, false, "")

fn clone(task) => %deep task

fn line(task) {
    mark = task.done => "[x] " || "[ ] "
    return mark + task.title + " / " + task.owner + " / " + str(task.points) + "pt"
}
