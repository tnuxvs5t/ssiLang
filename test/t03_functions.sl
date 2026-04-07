# Named functions
fn add(a, b) => a + b
print(add(3, 4))

fn gcd(a, b) {
    while b != 0 {
        t = a % b
        a = b
        b = t
    }
    return a
}
print(gcd(48, 18))

fn fib(n) {
    if n <= 1 => return n
    return fib(n - 1) + fib(n - 2)
}
print(fib(10))

# Lambda
double = x -> x * 2
print(double(21))

mul = (a, b) -> a * b
print(mul(6, 7))

# Higher order
fn apply(f, x) => f(x)
print(apply(x -> x * x, 9))
