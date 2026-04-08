store_mod = import("./core/store")
presenter = import("./services/presenter")

store = store_mod.create()
state0 = store.snapshot()
print(len(state0.tasks) >= 5)
print(state0.selected_index == 0)

store.begin_new()
store.set_field("title", "Ship Project One")
store.set_field("owner", "System")
store.set_points_text("13")
store.set_field("note", "Pure ssiLang software stack.")
print(store.save())

state1 = store.snapshot()
print(len(state1.tasks) == len(state0.tasks) + 1)
print(state1.tasks[state1.selected_index].title == "Ship Project One")

store.select(0)
store.set_done(true)
print(store.save())

state2 = store.snapshot()
stats = presenter.stats(state2)
print(stats.total == len(state2.tasks))
print(stats.done >= 2)
print(len(presenter.list_items(state2)) == len(state2.tasks))

print("=== PROJECT1 SMOKE PASSED ===")
