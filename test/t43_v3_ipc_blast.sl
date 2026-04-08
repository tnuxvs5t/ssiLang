# Ad-hoc blast 4: tcp + pipe + shm + net + json on larger nested data

dataset = {
    users: [],
    projects: [],
    meta: {
        revision: 1,
        owner: "ops"
    }
}

for i in range(6) {
    scores = []
    for j in range(4) {
        scores.push(i * 10 + j)
    }
    dataset.users.push({
        id: i,
        name: "user_" + str(i),
        scores: scores
    })
}

for i in range(4) {
    hours = []
    for j in range(5) {
        hours.push(i + j + 1)
    }
    dataset.projects.push({
        key: "p" + str(i),
        hours: hours,
        owner: dataset.users[i].name
    })
}

fn sum_list(xs) {
    return xs |> reduce($1 + $2, 0)
}

fn summarize(d) {
    return @(d) => {
        users: len(@.users),
        projects: len(@.projects),
        total_scores: @.users |> map(sum_list($.scores)) |> reduce($1 + $2, 0),
        total_hours: @.projects |> map(sum_list($.hours)) |> reduce($1 + $2, 0),
        first_owner: @.projects[0].owner
    }
}

summary0 = summarize(dataset)
print(summary0.users)
print(summary0.projects)
print(summary0.total_scores)
print(summary0.total_hours)
print(summary0.first_owner)

srv = tcp_listen("127.0.0.1:19911")
cli = &tcp("127.0.0.1:19911")
peer = srv.accept()

cli.send(%net dataset)
received = peer.recv()
peer.send(%net summarize(received))
ack = cli.recv()
print(ack.users)
print(ack.projects)
print(ack.total_scores)
print(ack.total_hours)

cli.close()
peer.close()
srv.close()

p = &pipe()
for project in dataset.projects {
    p.send(%net project)
}
loaded_projects = []
for i in range(len(dataset.projects)) {
    loaded_projects.push(p.recv())
}
print(loaded_projects |> map($.key) |> join(","))
p.flush()
p.close()

slot = &shm("adhoc_v3_ipc_blast", 16384)
slot.store(%net dataset)
loaded = slot.load()
print(summarize(loaded).total_hours)
slot.clear()
print(slot.load())
slot.close()

json_text = json_encode(dataset)
round = json_parse(json_text)
print(summarize(round).total_scores)
print(round.meta.owner)

fn closed_pipe_fail() {
    closed = &pipe()
    closed.close()
    closed.recv()
}

safe = pcall(closed_pipe_fail)
print(safe.ok)
print(safe.error.contains("closed pipe"))

print("=== V3 IPC BLAST PASSED ===")
