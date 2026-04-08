fn rgb(r, g, b) => r * 65536 + g * 256 + b

fn palette() {
    return {
        paper: rgb(212, 208, 200),
        panel: rgb(236, 233, 216),
        ink: rgb(0, 0, 0),
        accent: rgb(10, 36, 106),
        accent_soft: rgb(128, 128, 192),
        good: rgb(0, 128, 0),
        bad: rgb(128, 0, 0),
        border: rgb(128, 128, 128),
        white: rgb(255, 255, 255),
        bar_open: rgb(0, 0, 128),
        bar_done: rgb(0, 128, 0),
        bar_points: rgb(128, 64, 0)
    }
}
