# v3 context + placeholder coverage

fn counter(start) {
    total = start
    fn inc(n) {
        @total = @total + n
        return @total
    }
    return inc
}

c = counter(10)
print(c(1))
print(c(5))

nums = [1, 2, 3, 4, 5]
print(nums |> filter($ > 2) |> map($ * 10))
print(nums |> reduce($1 + $2, 0))

config = {
    server: {
        host: "127.0.0.1",
        port: 8080
    }
}
request_id = "req-7"

@(config.server) {
    print(@.host)
    print(@.port)
    @.port = @.port + 1
    print(@request_id)
}
print(config.server.port)

user = {name: "Alice", age: 30, scores: [7, 8, 9]}
label = @(user) => @.name + "#" + str(@.age)
print(label)

fn scale_by(factor) {
    return $ * @factor
}

times3 = scale_by(3)
print(times3(9))

settings = {suffix: "!"}
labeler = @(settings) => $.name + @.suffix
print(labeler({name: "Bob"}))

__ph1 = 100
collision_safe = $ + __ph1
print(collision_safe(1))

fn make_box(v) {
    value = v
    fn expose(dummy) => &ref @value
    return {expose: expose}
}

box = make_box(5)
r = box.expose(0)
print(r.deref())
r.set(9)
print(box.expose(0).deref())

sum = 0
for i in range(4) {
    sum = sum + i
}
print(sum)

flag = 1
if true {
    flag = flag + 1
}
print(flag)

request_id = "outer-ctx"
request = {user: {name: "Eve"}}
@(request) {
    @( @.user ) {
        print(@.name)
        print(@request_id)
    }
}

print("=== V3 CONTEXT TESTS PASSED ===")
