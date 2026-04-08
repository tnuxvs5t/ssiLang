task = import("./task")

fn load() {
    return [
        task.make(1, "Landing page QA pass", "Mina", 3, true, "Marketing review closed. Keep screenshots for release notes."),
        task.make(2, "Win32 gui32 hardening", "Kai", 8, false, "Focus on event semantics, canvas, and menu stability."),
        task.make(3, "Installer draft", "Rin", 5, false, "Package helper binary, app scripts, and launch shortcuts."),
        task.make(4, "Regression matrix", "Zed", 2, true, "Smoke list is green on ping, toolkit, and canvas demo."),
        task.make(5, "Project One polish", "Nora", 5, false, "Make the sample app feel like a real internal tool.")
    ]
}
