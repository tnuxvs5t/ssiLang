gui32 = import("../../../modules/gui32")
store_mod = import("../core/store")
main_window = import("../ui/main_window")
render = import("../ui/render")
controller_mod = import("./controller")

fn run() {
    app = gui32.connect(null)
    store = store_mod.create()
    ui = main_window.create(app)
    render.render_all(ui, store.snapshot())
    ui.window.show()

    handle = controller_mod.create(app, store, ui, render)

    running = true
    while running {
        running = handle(app.next_event())
    }

    app.disconnect()
}
