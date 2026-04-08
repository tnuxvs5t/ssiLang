fmt = import("format.sl")

version = "adhoc-v3"
_secret = "hidden"

fn make_counter(tag, start) {
    value = start

    fn inc(step) {
        @value = @value + step
        return fmt.decorate(tag, @value)
    }

    fn raw(dummy) => @value
    fn ref(dummy) => &ref @value

    return {
        inc: inc,
        raw: raw,
        ref: ref
    }
}

fn make_scaler(factor) {
    return $ * @factor
}

fn describe(user) {
    return @(user) => fmt.user_line(@.name, @.role)
}

fn summarize_names(xs) {
    return fmt.pack_summary(xs |> map($.name))
}
