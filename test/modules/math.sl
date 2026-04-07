text = import("helpers/text.sl")

pi = 3.14

fn add(a, b) => a + b

fn tagged_sum(a, b) => text.wrap(str(add(a, b)))

fn make_counter(start) {
    state = [start]
    fn inc(step) {
        state[0] = state[0] + step
        return state[0]
    }
    return inc
}

_hidden = "secret"
