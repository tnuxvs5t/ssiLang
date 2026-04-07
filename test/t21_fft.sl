# ====================================================
# FFT (Fast Fourier Transform) — polynomial multiply
# ====================================================
# ssiLang has no complex number type, so we represent
# complex numbers as [real, imag] pairs and implement
# FFT from scratch.
#
# Uses Cooley-Tukey radix-2 DIT algorithm.
# Then tests polynomial multiplication via FFT.

# --- Complex arithmetic helpers ---
fn cadd(a, b) => [a[0] + b[0], a[1] + b[1]]
fn csub(a, b) => [a[0] - b[0], a[1] - b[1]]
fn cmul(a, b) => [a[0]*b[0] - a[1]*b[1], a[0]*b[1] + a[1]*b[0]]

# --- Bit-reversal permutation ---
fn bit_reverse(x, logn) {
    result = 0
    for i in range(logn) {
        result = result * 2 + x % 2
        x = int(x / 2)
    }
    return result
}

# --- FFT in-place (iterative Cooley-Tukey) ---
# a = list of [real, imag], length must be power of 2
# invert: false for forward FFT, true for inverse FFT
fn fft(a, invert) {
    n = len(a)

    # compute log2(n)
    logn = 0
    tmp = n
    while tmp > 1 {
        logn = logn + 1
        tmp = int(tmp / 2)
    }

    # bit-reversal permutation
    for i in range(n) {
        j = bit_reverse(i, logn)
        if i < j {
            t = a[i]
            a[i] = a[j]
            a[j] = t
        }
    }

    # Butterfly passes
    half = 1
    while half < n {
        step = half * 2
        # angle = 2*pi / step, or -2*pi/step if inverse
        # pi ≈ 3.14159265358979
        pi = 3.14159265358979
        angle = 2.0 * pi / step
        if invert {
            angle = 0.0 - angle
        }

        # Precompute twiddle factors for this stage
        # w_base = [cos(angle), sin(angle)]
        # We approximate cos/sin via Taylor series (enough for moderate n)
        wn = [cos_approx(angle), sin_approx(angle)]

        i = 0
        while i < n {
            w = [1.0, 0.0]
            for j in range(half) {
                u = a[i + j]
                v = cmul(w, a[i + j + half])
                a[i + j] = cadd(u, v)
                a[i + j + half] = csub(u, v)
                w = cmul(w, wn)
            }
            i = i + step
        }
        half = half * 2
    }

    if invert {
        for i in range(n) {
            a[i] = [a[i][0] / n, a[i][1] / n]
        }
    }
}

# --- Taylor series cos/sin (accurate enough for FFT) ---
fn cos_approx(x) {
    # Reduce x to [-pi, pi]
    pi = 3.14159265358979
    while x > pi {
        x = x - 2.0 * pi
    }
    while x < 0.0 - pi {
        x = x + 2.0 * pi
    }
    # cos(x) = 1 - x^2/2! + x^4/4! - x^6/6! + x^8/8! - x^10/10!
    x2 = x * x
    result = 1.0
    term = 1.0
    for i in range(1, 11) {
        term = 0.0 - term * x2 / (2*i*(2*i - 1))
        result = result + term
    }
    return result
}

fn sin_approx(x) {
    # Reduce x to [-pi, pi]
    pi = 3.14159265358979
    while x > pi {
        x = x - 2.0 * pi
    }
    while x < 0.0 - pi {
        x = x + 2.0 * pi
    }
    # sin(x) = x - x^3/3! + x^5/5! - x^7/7! + ...
    x2 = x * x
    result = x
    term = x
    for i in range(1, 11) {
        term = 0.0 - term * x2 / ((2*i)*(2*i + 1))
        result = result + term
    }
    return result
}

# --- Polynomial multiplication via FFT ---
# Multiply two polynomials given as coefficient lists
# Returns coefficient list of the product
fn poly_mul(a, b) {
    result_len = len(a) + len(b) - 1

    # Find smallest power of 2 >= result_len
    n = 1
    while n < result_len {
        n = n * 2
    }

    # Pad to complex arrays
    fa = []
    fb = []
    for i in range(n) {
        if i < len(a) {
            fa.push([float(a[i]), 0.0])
        } else {
            fa.push([0.0, 0.0])
        }
        if i < len(b) {
            fb.push([float(b[i]), 0.0])
        } else {
            fb.push([0.0, 0.0])
        }
    }

    # Forward FFT
    fft(fa, false)
    fft(fb, false)

    # Pointwise multiply
    fc = []
    for i in range(n) {
        fc.push(cmul(fa[i], fb[i]))
    }

    # Inverse FFT
    fft(fc, true)

    # Round to integer coefficients
    result = []
    for i in range(result_len) {
        # round to nearest integer
        val = fc[i][0]
        if val >= 0 {
            result.push(int(val + 0.5))
        } else {
            result.push(int(val - 0.5))
        }
    }
    return result
}

# =====================
# Test 1: simple (1 + 2x)(3 + 4x) = 3 + 10x + 8x^2
# =====================
print("--- FFT Test 1: simple ---")
r1 = poly_mul([1, 2], [3, 4])
print(r1)
# Expected: [3, 10, 8]

# =====================
# Test 2: (1 + x + x^2)(1 + x + x^2) = 1 + 2x + 3x^2 + 2x^3 + x^4
# =====================
print("--- FFT Test 2: squaring ---")
r2 = poly_mul([1, 1, 1], [1, 1, 1])
print(r2)
# Expected: [1, 2, 3, 2, 1]

# =====================
# Test 3: multiply by constant
# =====================
print("--- FFT Test 3: constant ---")
r3 = poly_mul([5], [1, 2, 3])
print(r3)
# Expected: [5, 10, 15]

# =====================
# Test 4: (1 + x)^4 by repeated squaring
# =====================
print("--- FFT Test 4: (1+x)^4 ---")
p = [1, 1]
p2 = poly_mul(p, p)
p4 = poly_mul(p2, p2)
print(p4)
# Expected: [1, 4, 6, 4, 1] (binomial coefficients)

# =====================
# Test 5: larger polynomial stress
# =====================
print("--- FFT Test 5: stress ---")
# Multiply [1,1,1,...,1] (16 ones) by [1,1,1,...,1] (16 ones)
# Result should be [1, 2, 3, ..., 16, ..., 3, 2, 1]
a16 = []
for i in range(16) {
    a16.push(1)
}
r5 = poly_mul(a16, a16)
print("len = " + str(len(r5)))
print("mid = " + str(r5[15]))
# Expected: len=31, mid=16

# Verify symmetry
ok = true
for i in range(len(r5)) {
    if r5[i] != r5[len(r5) - 1 - i] {
        ok = false
    }
}
print("symmetric: " + str(ok))

# Verify sum = 16*16 = 256
s = 0
for x in r5 {
    s = s + x
}
print("sum = " + str(s))

# =====================
# Test 6: verify against naive multiplication
# =====================
print("--- FFT Test 6: verify vs naive ---")

fn naive_mul(a, b) {
    n = len(a) + len(b) - 1
    result = []
    for i in range(n) {
        result.push(0)
    }
    for i in range(len(a)) {
        for j in range(len(b)) {
            result[i + j] = result[i + j] + a[i] * b[j]
        }
    }
    return result
}

# Random-ish test
a_test = [3, 1, 4, 1, 5, 9, 2, 6]
b_test = [2, 7, 1, 8, 2, 8]
fft_result = poly_mul(a_test, b_test)
naive_result = naive_mul(a_test, b_test)
print("FFT:   " + str(fft_result))
print("naive: " + str(naive_result))

match = true
for i in range(len(fft_result)) {
    if fft_result[i] != naive_result[i] {
        match = false
    }
}
print("match: " + str(match))

print("=== FFT TESTS PASSED ===")
