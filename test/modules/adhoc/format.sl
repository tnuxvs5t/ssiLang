fn decorate(tag, n) => tag + ":" + str(n)

fn user_line(name, role) => name + "[" + role + "]"

fn pack_summary(xs) => xs |> join("|")
