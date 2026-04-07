# ssiLang 完全教程

> 面向精通 C++ 的程序员。读完本文，你可以用 ssiLang 写算法题、小工具和进程间通信程序。

---

## 0. 三分钟速览

```sl
# 变量绑定（无需声明类型、无需 let/var）
name = "ssiLang"
version = 2.2

# 函数
fn gcd(a, b) {
    while b != 0 {
        t = a % b
        a = b
        b = t
    }
    return a
}

# Lambda
double = x -> x * 2
ping = () -> "pong"

# 容器
arr = [1, 2, 3, 4, 5]
m = {name: "Alice", age: 30}

# 管道式调用
result = arr |> filter(x -> x > 2) |> map(x -> x * 10)
print(result)   # [30, 40, 50]

# 多文件
math = import("lib/math")
print(math.add(2, 3))   # 5
```

如果这段代码你已经大致能读懂，下面的内容会帮你把每个细节搞清楚。

---

## 1. 运行方式

```bash
# 运行脚本文件（扩展名 .sl）
./sl test.sl

# 交互式 REPL
./sl
sl> print("hello")
hello
```

源文件必须是 UTF-8 编码。当前实现同时接受带 BOM 和不带 BOM 的 UTF-8 文件。

如果脚本里调用 `import("...")`，路径会相对“当前脚本文件所在目录”解析，而不是相对启动 `sl.exe` 时的工作目录解析。

---

## 2. 基础语法

### 2.1 注释

只有单行注释，用 `#`：

```sl
# 这是注释
x = 42  # 行尾注释也可以
```

### 2.2 换行即语句结束

ssiLang **不需要分号**。换行默认终止语句。

例外：在 `()`、`[]`、`{}` 内部可以自由换行。

```sl
# 这是两条语句
x = 1
y = 2

# 这是一条语句（在 [] 内部换行）
arr = [
    1, 2, 3,
    4, 5, 6
]
```

### 2.3 变量绑定

```sl
x = 42
name = "Alice"
ok = true
```

**没有 `let`、`var`、`const` 关键字。** `=` 就是绑定。

语义规则：
- 当前作用域中 `x` 不存在 → 创建新绑定
- 当前作用域中 `x` 已存在 → 更新（rebind）
- 不会修改外层作用域的同名变量（内层会遮蔽外层）

```sl
x = 1
if true {
    x = 2       # 这里创建了内层的 x，外层的 x 不变
    print(x)    # 2
}
print(x)        # 1
```

> **C++ 对比：** 类似 Python 的作用域规则，但不需要 `global` 关键字。如果你需要修改外层的值，用 `&ref`（后面会讲）或者通过容器间接修改。

---

## 3. 数据类型

ssiLang 是纯动态类型语言。所有值在运行时携带类型标签。

| 类型 | 字面量示例 | `type()` 返回值 |
|------|-----------|----------------|
| 整数/浮点数 | `42`, `3.14`, `-7` | `"number"` |
| 字符串 | `"hello"` | `"string"` |
| 布尔 | `true`, `false` | `"bool"` |
| 空值 | `null` | `"null"` |
| 列表 | `[1, 2, 3]` | `"list"` |
| 映射 | `{a: 1, b: 2}` | `"map"` |
| 函数 | `fn f(x) => x`, `x -> x`, `() -> 42` | `"function"` |

> **注意：** 数值类型底层统一为 `double`（和 JavaScript 类似）。没有独立的 int 类型。`int()` 只是截断转换函数。

### 3.1 字符串

双引号括起，支持 `+` 拼接：

```sl
s = "hello" + " " + "world"
print(s)        # hello world
print(len(s))   # 11
print(s[0])     # h
print(s[4])     # o
```

字符串的常用方法：

```sl
"a,b,c".split(",")            # ["a", "b", "c"]
"  hello  ".trim()             # "hello"
"hello".chars()                # ["h", "e", "l", "l", "o"]
"hello".starts_with("he")     # true
"hello".ends_with("lo")       # true
"hello".contains("ell")       # true
"hello world".replace("world", "ssiLang")  # "hello ssiLang"
```

### 3.2 列表（List）

ssiLang 的列表就是 C++ 的 `std::vector`，动态数组：

```sl
arr = [1, 2, 3, 4, 5]
print(arr[0])       # 1（0-based 索引）
print(len(arr))     # 5

arr.push(6)         # 末尾追加，修改原列表
print(arr.pop())    # 6，移除并返回末尾元素
```

**列表方法一览：**

```sl
arr = [1, 2, 3, 4, 5]

# 切片（不修改原列表）
arr.slice(1, 3)                    # [2, 3]

# 函数式三件套
arr.map(x -> x * 2)               # [2, 4, 6, 8, 10]
arr.filter(x -> x % 2 == 0)       # [2, 4]
arr.reduce((acc, x) -> acc + x, 0) # 15

# 查找
arr.find(x -> x > 3)              # 4
arr.contains(3)                    # true

# 转换
arr.join("-")                      # "1-2-3-4-5"
arr.reverse()                      # [5, 4, 3, 2, 1]（新列表）
```

> **C++ 对比：** `push`/`pop` 对应 `push_back`/`pop_back`。`map`/`filter`/`reduce` 对应 `std::transform`/`std::copy_if`/`std::accumulate`，但写法极其简洁。

### 3.3 映射（Map）

键值对容器，键只能是字符串：

```sl
m = {name: "Alice", age: 30, active: true}

# 读取——两种写法等价
print(m.name)       # Alice
print(m["age"])     # 30

# 写入
m.city = "NYC"
m["score"] = 95

# 方法
m.keys()            # ["name", "age", "active", "city", "score"]
m.values()          # 对应的值列表
m.has("name")       # true
m.remove("age")     # 移除并返回 30
```

> **C++ 对比：** 类似 `std::unordered_map<std::string, Value>`，但加上了 JavaScript 风格的点号访问。

### 3.4 类型转换

```sl
int("123")     # 123
int(3.7)       # 3（截断）
float("3.14")  # 3.14
str(42)        # "42"
type(42)       # "number"
```

---

## 4. 运算符

### 4.1 算术

```sl
1 + 2       # 3
10 - 3      # 7
4 * 5       # 20
15 / 4      # 3.75（浮点除法，不是整除！）
17 % 5      # 2
-7          # 一元取负
```

> **C++ 对比：** `/` 始终是浮点除法。要整除，用 `int(a / b)`。

### 4.2 比较与逻辑

```sl
# 比较
1 < 2       # true
3 >= 3      # true
1 == 1      # true
1 != 2      # true

# 逻辑（关键字，不是符号）
true and false   # false
true or false    # true
not true         # false
```

`and`/`or` 短路求值，和 C++ 的 `&&`/`||` 行为一致。

### 4.3 优先级表（从高到低）

| 优先级 | 运算符 | 说明 |
|--------|--------|------|
| 1 | `.` `?.` `[]` `?[]` `()` | 成员/索引/调用 |
| 2 | `@@` `not` `-` `%mode` `&kind` | 一元前缀 |
| 3 | `*` `/` `%` | 乘除取模 |
| 4 | `+` `-` | 加减 |
| 5 | `<` `<=` `>` `>=` | 比较 |
| 6 | `==` `!=` | 相等 |
| 7 | `and` | 逻辑与 |
| 8 | `or` | 逻辑或 |
| 9 | `::` | Flow 运算符 |
| 10 | `\|>` | Pipe 运算符 |
| 11 | `=> \|\|` | Split 表达式（最低） |

---

## 5. 控制流

### 5.1 if / else

两种写法：

```sl
# 块语法（推荐在多行时使用）
if x > 5 {
    print("big")
} else {
    print("small")
}

# 箭头语法（适合单行）
if x > 0 => print("positive")
```

> **注意：** 条件不需要括号，但 `{` 必须和 `if` 在同一行（换行即语句结束）。

### 5.2 while 循环

```sl
i = 0
while i < 10 {
    print(i)
    i = i + 1
}
```

> **没有 `i++`。** 写 `i = i + 1`。

### 5.3 for-in 循环

ssiLang 只有 `for-in`，没有 C 风格的 `for(;;)`：

```sl
# 遍历列表
for x in [10, 20, 30] {
    print(x)
}

# 遍历 range
for i in range(5) {
    print(i)    # 0, 1, 2, 3, 4
}

for i in range(2, 7) {
    print(i)    # 2, 3, 4, 5, 6
}

for i in range(0, 10, 2) {
    print(i)    # 0, 2, 4, 6, 8
}

for i in range(5, 0, -1) {
    print(i)    # 5, 4, 3, 2, 1
}
```

### 5.4 break / continue

和 C++ 行为完全一致：

```sl
for x in range(10) {
    if x == 5 => break       # 跳出循环
    if x % 2 == 0 => continue # 跳过偶数
    print(x)                  # 1, 3
}
```

---

## 6. 函数

### 6.1 命名函数

两种写法：

```sl
# 块语法
fn gcd(a, b) {
    while b != 0 {
        t = a % b
        a = b
        b = t
    }
    return a
}

# 箭头语法（函数体只有一个表达式时）
fn add(a, b) => a + b

# 调用
print(gcd(48, 18))   # 6
print(add(3, 4))     # 7
```

**必须用括号调用。** `f x` 是非法的，必须写 `f(x)`。

### 6.2 Lambda

```sl
# 零参数
ping = () -> "pong"
print(ping())         # pong

# 单参数
double = x -> x * 2
print(double(5))     # 10

# 多参数
mul = (a, b) -> a * b
print(mul(6, 7))     # 42
```

Lambda 只有箭头语法，body 是单个表达式。支持三种参数形态：

- `() -> expr`：零参数
- `x -> expr`：单参数
- `(a, b, c) -> expr`：多参数

需要多行逻辑时用命名函数。

### 6.3 函数是一等公民

函数可以作为参数传递、作为返回值、存到列表和映射中：

```sl
# 高阶函数
fn apply(f, x) => f(x)
print(apply(x -> x * x, 9))   # 81

# 返回函数（闭包）
fn adder(a) {
    fn inner(b) {
        return a + b
    }
    return inner
}
add5 = adder(5)
print(add5(3))     # 8
print(add5(10))    # 15

# 函数组合
fn compose(f, g) {
    fn composed(x) {
        return g(f(x))
    }
    return composed
}
inc_then_double = compose(x -> x + 1, x -> x * 2)
print(inc_then_double(3))   # 8
```

### 6.4 闭包

ssiLang 的闭包通过词法作用域捕获外层变量。但注意：**闭包捕获的是变量名的绑定，不是值的拷贝。** 如果你需要通过闭包修改外层状态，常见做法是把状态放在列表里：

```sl
fn counter_factory(start) {
    state = [start]          # 用列表包装，这样闭包可以修改
    fn inc(n) {
        state[0] = state[0] + n
        return state[0]
    }
    fn get(dummy) {
        return state[0]
    }
    return {inc: inc, get: get}
}

c = counter_factory(0)
c.inc(1)
c.inc(5)
print(c.get(0))   # 6
```

> **为什么用 `state = [start]` 而不是 `state = start`？** 因为 `state = state + n` 在内层函数中会创建新的局部绑定而不是修改外层。用列表后 `state[0] = ...` 是对现有容器的元素赋值，不创建新绑定。这和 Python 的闭包限制类似。

### 6.5 循环中捕获变量

和 JavaScript 的经典坑一样，循环变量需要显式捕获：

```sl
makers = []
for i in range(5) {
    fn make(captured_i) {
        return x -> x + captured_i
    }
    makers.push(make(i))   # 通过参数传递来"捕获"当前值
}

for f in makers {
    print(f(100))   # 100, 101, 102, 103, 104
}
```

### 6.6 多文件与模块

ssiLang 现在支持通过内建函数 `import(path)` 进行多文件开发。

```sl
# app.sl
math = import("lib/math")
print(math.add(2, 3))
print(math.tagged_sum(2, 5))
```

```sl
# lib/math.sl
text = import("helpers/text.sl")

pi = 3.14

fn add(a, b) => a + b
fn tagged_sum(a, b) => text.wrap(str(add(a, b)))

_hidden = "private"
```

```sl
# lib/helpers/text.sl
fn wrap(s) => "<" + s + ">"
```

`import(path)` 的行为规则：

- 返回一个 `map`，内容是目标文件的顶层导出
- 路径相对当前文件所在目录解析
- 省略扩展名时会自动尝试补 `.sl`
- 同一个模块只执行一次，后续导入命中缓存
- 顶层名字以 `_` 开头时不会导出
- 循环导入会报错

因此推荐的项目结构是：

```text
project/
  app.sl
  lib/
    math.sl
    helpers/
      text.sl
```

当前版本还没有 `export` / `from ... import ...` 语法，所以模块边界约定主要依赖“顶层名字 + `_` 私有前缀”。

---

## 7. 特色表达式

这是 ssiLang 和多数语言差别最大的地方。

### 7.1 Split 表达式 `cond => a || b`

三元运算符，但不是 `?:` 而是 `=> ||`：

```sl
x = true => "yes" || "no"
print(x)   # yes

y = false => "yes" || "no"
print(y)   # no
```

| C++ | ssiLang |
|-----|---------|
| `cond ? a : b` | `cond => a \|\| b` |

**右结合，可以链式使用：**

```sl
fn classify(score) {
    return score >= 90 => "A" ||
           score >= 80 => "B" ||
           score >= 70 => "C" ||
           score >= 60 => "D" || "F"
}
print(classify(85))   # B
```

在有歧义的位置（如函数参数、lambda body 中），需要加括号：

```sl
x = (ok => a || b)
result = f((flag => x || y))
```

### 7.2 Safe Access `?.` 和 `?[]`

当左侧为 `null` 时短路返回 `null`，不报错：

```sl
m = {a: {b: 42}}
print(m?.a?.b)      # 42
print(m?.x?.y)      # null（不会报错）

arr = [1, 2, 3]
print(arr?[0])      # 1

n = null
print(n?.foo)       # null
print(n?[0])        # null
```

> **C++ 对比：** 类似 C++ 提案中的 `std::optional` 链式操作，或 Kotlin/Swift 的 `?.`。

在算法竞赛中，这对处理可能为空的 map 查询特别有用：

```sl
fn freq(arr) {
    m = {}
    for x in arr {
        c = m?[x]            # 不存在时返回 null 而不是报错
        if c == null {
            m[x] = 1
        } else {
            m[x] = c + 1
        }
    }
    return m
}
```

### 7.3 Flow 运算符 `::`

`a::f` 等价于 `f(a)`，`a::f(b)` 等价于 `f(a, b)`：

```sl
fn normalize(x) => x * 10
fn encode(x) => x + 1

# 这三种写法等价
print(encode(normalize(5)))  # 51
print(5::normalize::encode)  # 51

result = 5::normalize::encode
```

适合表达"数据流经一系列变换"的场景。

### 7.4 Pipe 运算符 `|>`

和 `::` 语义相同（`a |> f` = `f(a)`），但优先级更低，适合长链：

```sl
result = [1, 2, 3, 4, 5]
    |> filter(x -> x > 2)
    |> map(x -> x * 10)
    |> join(", ")
print(result)   # "30, 40, 50"
```

> **`::` vs `|>`：** 功能相同，`::` 更紧凑适合短链，`|>` 更适合多步数据处理管道。

### 7.5 Probe `@@`

调试用的一元前缀运算符，打印表达式值并返回它：

```sl
x = @@42           # 打印调试信息，x 仍然是 42
@@"checkpoint"     # 快速打印检查点
```

---

## 8. 内建函数速查

### 全局函数

| 函数 | 说明 | 示例 |
|------|------|------|
| `print(v...)` | 输出到标准输出，末尾换行 | `print("hi", 42)` |
| `input()` | 从 stdin 读一行 | `s = input()` |
| `len(v)` | list/string/map 的长度 | `len([1,2,3])` → 3 |
| `type(v)` | 返回类型名字符串 | `type(42)` → `"number"` |
| `int(v)` | 转整数 | `int("123")` → 123 |
| `float(v)` | 转浮点数 | `float("3.14")` → 3.14 |
| `str(v)` | 转字符串 | `str(42)` → `"42"` |
| `range(...)` | 生成整数列表 | `range(5)` → [0,1,2,3,4] |
| `abs(n)` | 绝对值 | `abs(-5)` → 5 |
| `min(a, b)` | 较小值 | `min(3, 7)` → 3 |
| `max(a, b)` | 较大值 | `max(3, 7)` → 7 |
| `sort(list)` | 升序排序（返回新列表） | `sort([3,1,2])` → [1,2,3] |
| `error(msg)` | 触发运行时错误 | `error("bad input")` |
| `import(path)` | 加载模块，返回导出 map | `import("lib/math")` |
| `pcall(fn)` | 保护调用，捕获错误 | `pcall(my_fn)` → `{ok: true, value: 42}` |
| `tcp_listen(addr)` | 创建 TCP 服务器 | `tcp_listen("0.0.0.0:9000")` |
| `json_encode(v)` | 值转 JSON 字符串 | `json_encode({a: 1})` → `'{"a":1}'` |
| `json_parse(s)` | JSON 字符串转值 | `json_parse("[1,2]")` → `[1, 2]` |

---

## 9. 引用系统 `&ref`

ssiLang 的 `&ref` 类似 C++ 的引用/指针，但是安全的运行时抽象：

```sl
# 引用列表元素
arr = [10, 20, 30]
r = &ref arr[1]
print(r.deref())     # 20（读取）
r.set(99)            # 写入
print(arr)           # [10, 99, 30]

# 引用变量
x = 42
rv = &ref x
print(rv.deref())    # 42
rv.set(100)
print(x)             # 100

# 引用 map 成员
m = {a: 1, b: 2}
mr = &ref m.a
mr.set(999)
print(m)             # {a: 999, b: 2}
```

引用只有两个方法：`deref()` 读取，`set(v)` 写入。

**批量操作示例：**

```sl
arr = [0, 0, 0, 0, 0]
refs = []
for i in range(5) {
    refs.push(&ref arr[i])
}
for i in range(5) {
    refs[i].set(i * i)
}
print(arr)   # [0, 1, 4, 9, 16]
```

---

## 10. Derive 系统 `%`

`%` 前缀用于从现有值派生出新形态：

| 运算 | 语义 | C++ 近似 |
|------|------|---------|
| `%copy x` | 浅拷贝 | 顶层容器新建，内部元素共享 |
| `%deep x` | 深拷贝 | 递归完全独立 |
| `%snap x` | 快照 | COW 语义（当前实现退化为深拷贝） |
| `%net x` | 网络序列化形式 | 序列化为二进制传输格式 |

```sl
original = [[1, 2], [3, 4]]

shallow = %copy original
deep = %deep original

# 浅拷贝：顶层独立，内层共享
shallow.push([5, 6])
print(len(original))   # 2（顶层不受影响）
shallow[0].push(99)
print(original[0])     # [1, 2, 99]（内层被共享修改了！）

# 深拷贝：完全独立
deep[1].push(88)
print(original[1])     # [3, 4]（不受影响）
```

---

## 11. 面向对象模式

ssiLang 没有 `class` 关键字，但用闭包 + map 可以模拟对象：

```sl
fn make_stack() {
    data = []

    fn push(v) {
        data.push(v)
        return null
    }
    fn pop(dummy) {
        return data.pop()
    }
    fn peek(dummy) {
        return data[len(data) - 1]
    }
    fn size(dummy) {
        return len(data)
    }

    return {push: push, pop: pop, peek: peek, size: size}
}

stk = make_stack()
stk.push(10)
stk.push(20)
stk.push(30)
print(stk.size(0))    # 3
print(stk.peek(0))    # 30
print(stk.pop(0))     # 30
```

**关键模式：** 工厂函数返回一个 map，map 的值是闭包，闭包共享同一个 `data`。调用 `stk.push(10)` 时，runtime 在 map 中找到 key `"push"` 对应的函数并调用。

> **`dummy` 参数的问题：** 当前实现中，通过 map 调用函数时，方法名后面的括号内容会作为参数传递。对于只读方法，需要传一个占位参数。这是当前语言版本的限制。

---

## 11.5 错误处理 `pcall()`

ssiLang 没有 try/catch 语法，但提供 `pcall`（protected call）内建函数来捕获运行时错误：

```sl
fn risky() {
    x = 10 / 0
}

result = pcall(() -> risky())
if result.ok {
    print("成功: " + str(result.value))
} else {
    print("错误: " + result.error)
}
# 输出: 错误: [ln 2] Division by zero
```

`pcall(fn)` 接受一个无参函数，返回一个 map。现在最方便的写法就是直接传零参数 lambda：
- 成功时：`{ok: true, value: <返回值>}`
- 出错时：`{ok: false, error: "<错误信息>"}`

适用于 IPC 操作等可能失败的场景：

```sl
r = pcall(() -> &tcp("127.0.0.1:9999"))
if not r.ok {
    print("连接失败: " + r.error)
}
```

---

## 11.6 JSON 支持

`json_encode` 和 `json_parse` 提供 JSON 序列化/反序列化：

```sl
data = {name: "Alice", scores: [90, 85], active: true}

# 编码为 JSON 字符串
s = json_encode(data)
print(s)   # {"name":"Alice","scores":[90,85],"active":true}

# 解析 JSON 字符串
parsed = json_parse(s)
print(parsed.name)     # Alice
print(parsed.scores)   # [90, 85]
```

支持所有基础类型：number, string, bool, null, list, map。

但要分清楚：

- `json_encode` / `json_parse` 的目标是“人类可读”和“跨语言通用”
- `%net` 的目标是“ssiLang 运行时值的高效传输”

两者不是一回事。

| 场景 | 推荐 |
|------|------|
| ssiLang 和 ssiLang 之间传值 | `%net` + `send/recv` / `store/load` |
| 和 Python/Go/Node 临时调试，想直接看懂内容 | `send_raw(json_encode(...))` |
| 面向外部系统、公共 API、抓包分析 | JSON |
| 追求更低解析成本、保留 ssiLang 类型结构 | `%net` |

如果双方都是你控制的程序，不要因为“怕规范不清楚”就默认退回 `&tcp + JSON`。`%net` 的规范其实很明确，下面会把它讲透。

---

## 12. 网络与进程间通信

ssiLang 内建了最小但真实的 IPC 能力。最重要的是先建立统一心智模型：

```text
ssiLang 值
  -> %net          # 变成 net-form
  -> send/store    # 通过 tcp/pipe/shm 传输
  -> recv/load     # 还原回普通 ssiLang 值
```

所以请把下面几层分开看：

- `%net`：值编码格式
- `tcp.send/recv`、`pipe.send/recv`：带消息边界的传输
- `send_raw/recv_raw`：纯字节流，不做编码
- `shm.store/load`：共享内存单槽读写

### 12.1 什么时候选什么

| 需求 | 推荐 |
|------|------|
| 两个 ssiLang 进程走网络 RPC | `&tcp` + `%net` |
| 两个本机进程低延迟共享“最新状态” | `&shm` + `%net` |
| 两个本机进程做本地双向消息传递 | `&pipe` + `%net` |
| 要和外部语言快速联调，但对方没实现 `%net` | `send_raw` + JSON |
| 要精确控制协议或接入现有二进制协议 | `send_raw` / `recv_raw` |

一句话：

- 要“值”就用 `%net`
- 要“字节”就用 `send_raw`
- 要“共享最新值”就用 `&shm`
- 要“本地消息通道”就用 `&pipe`
- 要“跨机器连接”就用 `&tcp`

### 12.2 `%net` — 值编码，不是 JSON

任何值都可以用 `%net` 转为传输形式：

```sl
packet = %net {kind: "ping", data: [1, 2, 3]}
print(type(packet))   # "net-form"
```

`%net` 生成的是 `net-form`，它不是字符串，不是 map，也不是 JSON 文本。它的设计目标是“把 ssiLang 的普通值变成可以传输的格式”。

#### 12.2.1 `%net` 能编码什么

支持：

- `number`
- `string`
- `bool`
- `null`
- `list`
- `map`

不支持：

- `function`
- `link-ref`
- `link-tcp`
- `link-tcp-server`
- `link-pipe`
- `link-shm`
- 其他运行时句柄

也就是说，`%net` 适合传“数据”，不适合传“行为”和“资源句柄”。

#### 12.2.2 `%net` 的底层格式

当前实现的 payload 以版本头开头：

```text
V1\n
```

后面紧跟一个递归值。值的编码规则是：

```text
N<number>\n                 number
S<len>:<raw-bytes>          string
B1\n / B0\n                 bool
Z\n                         null
L<count>\n <item>...        list
M<count>\n <key><value>...  map
```

注意点：

- 字符串是“长度前缀 + 原始字节”，不是以 `\0` 结尾
- map 的 key 在编码时本质上也是字符串值
- list/map 递归编码
- 版本号现在是 `V1`

例如：

```sl
v = {a: 1, b: true, c: ["x", "y"]}
packet = %net v
```

概念上会变成类似：

```text
V1
M3
S1:aN1
S1:bB1
S1:cL2
S1:xS1:y
```

这里示意的是结构，不是为了让你手写字节流。重点是你现在知道：

- `%net` 不是黑盒
- 它是有版本头、有类型标签、有长度信息的递归格式
- 它比 JSON 更接近运行时内部值

#### 12.2.3 `%net` 什么时候优于 JSON

`%net` 的优点：

- 不需要在发送前先 `json_encode`
- 接收后直接得到普通 ssiLang 值
- 能明确保留 `null/list/map/bool/number` 的结构
- 本地进程间、ssiLang 对 ssiLang 协作时更直接

JSON 的优点：

- 外部生态支持广
- 人类好读
- 更适合公共协议和日志

如果你控制通信双方，而且双方都愿意实现 V1 解码器，`%net` 完全可以作为正式协作格式，而不需要回退到 JSON。

### 12.3 `&tcp` — 跨机器或跨进程网络连接

```sl
conn = &tcp("127.0.0.1:9000")
conn.send(%net {kind: "ping", n: 1})
reply = conn.recv()
conn.close()
```

| 方法 | 说明 |
|------|------|
| `conn.send(net_form)` | 发送序列化数据 |
| `conn.recv()` | 阻塞接收并反序列化 |
| `conn.send_raw(string)` | 发送原始字节（无长度前缀） |
| `conn.recv_raw(n)` | 接收 n 字节返回 string |
| `conn.peer_addr()` | 返回对端地址字符串 |
| `conn.set_timeout(secs)` | 设置收发超时 |
| `conn.close()` | 关闭连接 |

> `&tcp` 支持可选第二参数设置连接超时秒数：`&tcp("host:port", 5)`

#### 12.3.1 地址格式

当前实现接受的是：

```text
"IPv4:port"
```

例如：

- `"127.0.0.1:9000"`
- `"192.168.1.10:8080"`

不要把它当成这些别的东西：

- 不是 URL：`"http://127.0.0.1:9000"` 错
- 不是 host/path：`"127.0.0.1/9000"` 错
- 当前实现也不该假定支持 DNS 名称如 `"localhost:9000"`

如果你要最稳，直接用 IPv4 字面量。

#### 12.3.2 `send/recv` 的消息边界

`conn.send(%net value)` 不是裸写 payload。实现会在外层再包一层消息 framing：

```text
[4-byte length][payload bytes]
```

其中：

- `length` 是 payload 长度
- `payload` 就是 `%net` 生成的 `V1...` 数据

这意味着 `recv()` 每次读到的是“一整条消息”，不是半包，不是流式片段。

> 重要：当前实现已经把这 4 字节长度字段固定为 little-endian `uint32`。  
> 也就是说，跨语言互通时可以明确按“小端 uint32 长度前缀 + V1 payload”实现，而不是猜宿主机字节序。

#### 12.3.3 `send_raw/recv_raw`

这是纯字节流接口：

```sl
conn.send_raw("HELLO")
raw = conn.recv_raw(5)
print(raw)   # HELLO
```

它和 `send/recv` 完全不是一套协议：

- `send/recv`：传“编码后的值”
- `send_raw/recv_raw`：传“原始字节”

不要混用：

- `send_raw(...)` 后面接 `recv()`：错
- `send(%net ...)` 后面接 `recv_raw(n)`：通常也错，除非你自己知道 framing 在干什么

#### 12.3.4 ssiLang 对 ssiLang：推荐写法

客户端：

```sl
conn = &tcp("127.0.0.1:9000")
conn.send(%net {kind: "sum", a: 20, b: 22})
reply = conn.recv()
print(reply.ok)
print(reply.value)
conn.close()
```

服务端：

```sl
srv = tcp_listen("127.0.0.1:9000")
peer = srv.accept()
req = peer.recv()

if req.kind == "sum" {
    peer.send(%net {ok: true, value: req.a + req.b})
} else {
    peer.send(%net {ok: false, error: "unknown kind"})
}

peer.close()
srv.close()
```

#### TCP 服务端

```sl
srv = tcp_listen("0.0.0.0:9000")
peer = srv.accept()            # 阻塞等待客户端连接
peer.send(%net {status: "ok"})
msg = peer.recv()
peer.close()
srv.close()
```

| 方法 | 说明 |
|------|------|
| `srv.accept()` | 阻塞等待连接，返回新 TCP handle |
| `srv.addr()` | 返回绑定地址字符串 |
| `srv.close()` | 关闭监听 socket |

> `tcp_listen` 支持可选第二参数设置 backlog：`tcp_listen("0.0.0.0:9000", 32)`

#### 12.3.5 和其他语言协作：Python 最小互通示例

如果你不想退回 JSON，而是想直接和 `%net` 协作，外部语言至少要做两件事：

1. 读写 4 字节长度前缀
2. 解析 / 生成 `V1` payload

下面是 Python 侧最小 framing：

```python
import socket
import struct

def recv_exact(sock, n):
    buf = bytearray()
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            raise RuntimeError("socket closed")
        buf.extend(chunk)
    return bytes(buf)

def recv_frame(sock):
    # ssiLang v2.2 固定使用 little-endian uint32 长度前缀
    length = struct.unpack("<I", recv_exact(sock, 4))[0]
    return recv_exact(sock, length)

def send_frame(sock, payload: bytes):
    sock.sendall(struct.pack("<I", len(payload)) + payload)
```

再加上一个 V1 解析器就可以直接互通。V1 的规则就是上一节列出的标签格式。

如果外部语言暂时还没实现 V1 parser，但你又想先把链路打通，那么退一步用：

```sl
conn.send_raw(json_encode(data))
```

这是“过渡方案”，不是 `%net` 不可用。

### 12.4 `&pipe` — 本机管道，不走网络栈

```sl
# 匿名管道（单进程内可测试）
p = &pipe()
p.send(%net 42)
got = p.recv()
print(got)       # 42
p.close()

# 命名管道（跨进程通信）
p = &pipe("my_channel")
p.send(%net {task: "compute", args: [1, 2, 3]})
result = p.recv()
p.close()
```

`&pipe` 有两种形态：

- `&pipe()`：匿名管道
- `&pipe("name")`：命名管道

#### 12.4.1 匿名管道

匿名管道更适合：

- 单进程测试
- 父子流程式协作
- 不需要全局名字的临时通道

它本质上是一个本地双端通道，ssiLang 把读端和写端封装进一个 handle，所以你可以直接：

```sl
p = &pipe()
p.send(%net 42)
print(p.recv())   # 42
```

#### 12.4.2 命名管道

命名管道更适合两个独立进程在同一台机器上通信。

平台上的实际名字是：

- Windows：`\\.\pipe\<name>`
- POSIX：`/tmp/sl_pipe_<name>`

所以：

```sl
p = &pipe("jobs")
```

对应的是：

- Windows：`\\.\pipe\jobs`
- Linux/macOS(POSIX)：`/tmp/sl_pipe_jobs`

#### 12.4.3 `pipe.send/recv` 的协议

它和 TCP 一样，也是：

```text
[4-byte length][V1 payload]
```

所以跨语言协作时，读取逻辑和 TCP 基本一样，只是底层句柄从 socket 换成了 pipe/fifo。

#### 12.4.4 什么时候选 `&pipe`

适合：

- 同机进程通信
- 不需要 TCP 网络配置
- 想要 `%net` 的值语义，但不想经过网络栈

不适合：

- 跨机器
- 广域网服务
- 想共享一块最新状态内存

#### 12.4.5 常用方法

| 方法 | 说明 |
|------|------|
| `p.send(%net v)` | 发送一条完整消息 |
| `p.recv()` | 接收一条完整消息 |
| `p.is_named()` | 是否为命名管道 |
| `p.name()` | 命名管道名称，匿名管道返回 `null` |
| `p.flush()` | 刷新底层管道 |
| `p.close()` | 关闭句柄 |

### 12.5 `&shm` — 共享内存单槽，不是消息队列

单值槽模型，适合跨进程共享一个值。内置 spinlock 自动同步，支持可配置大小：

```sl
slot = &shm("my_counter")            # 默认 4096 字节
slot = &shm("big_data", 65536)       # 自定义大小

# 写入（自动加锁）
slot.store(%net 42)

# 读取（自动加锁）
val = slot.load()
print(val)          # 42

# 覆写
slot.store(%net {config: "fast", limit: 100})
print(slot.load())  # {config: "fast", limit: 100}

slot.close()
```

| 方法 | 说明 |
|------|------|
| `slot.store(net_form)` | 写入共享内存（自动加锁） |
| `slot.load()` | 读取当前值，空槽返回 `null`（自动加锁） |
| `slot.clear()` | 清空数据（加锁后写 length=0） |
| `slot.size()` | 返回分配的字节数 |
| `slot.name()` | 返回共享内存名称 |
| `slot.close()` | 释放共享内存访问 |

> **Owner 语义：** 第一个创建共享内存段的进程是 owner。只有 owner 关闭时才会删除底层资源（POSIX `shm_unlink`），其他 handle 关闭仅解除映射。

#### 12.5.1 正确心智模型

把 `&shm` 想成：

```text
一个带锁的“最新值寄存器”
```

而不是：

- 队列
- 数据库
- 多槽共享数组
- 自动 merge 的共享 map

每次 `store(%net v)` 都会直接覆盖之前的内容。

所以它特别适合：

- 发布当前状态
- 发布最新配置
- 发布最新计算结果
- 一个生产者不断刷新、多个消费者不断读取

不适合：

- 需要保留历史消息
- 多个写者做复杂事务
- 想用它模拟 append log

#### 12.5.2 共享内存的底层布局

当前实现里，一段共享内存的布局是：

```text
offset 0..3   : 4-byte spinlock
offset 4..7   : 4-byte little-endian payload length
offset 8..end : payload bytes（即 %net 的 V1 数据）
```

语义上：

- lock = `0`：空闲
- lock = `1`：持有锁
- `length = 0`：槽为空，`load()` 返回 `null`
- `length > 0`：后面跟着一份完整 `%net` payload

这也解释了为什么容量并不是全部可用给业务数据：真正能存的 payload 大约是：

```text
size - 8
```

#### 12.5.3 名称与跨语言协作

底层名字是：

- Windows：`Local\sl_shm_<name>`
- POSIX：`/sl_shm_<name>`

如果你想让 Python/C++/Rust 直接访问同一块共享内存，外部程序需要：

1. 打开同名共享内存对象
2. 按同样的布局解释前 8 个字节
3. 遵守相同的锁协议
4. 把 payload 当作 `%net V1` 数据解析

如果外部程序不实现同一套锁协议，读写就可能互相踩坏。

#### 12.5.4 什么时候选 `&shm`

当你关心的是：

- 同机
- 低开销
- 最新值
- 快速轮询读取

就选 `&shm`。

如果你关心的是：

- 每条消息都不能丢
- 有先后顺序
- 一问一答

那应该选 `&pipe` 或 `&tcp`。

### 12.6 典型错误：杀一儆百

下面这些错法最容易让人把 ssiLang 和别的语言模型混淆。

#### 错误 1：把 `%net` 当成 JSON

错：

```sl
packet = %net {a: 1}
print(packet)   # 以为会打印 {"a":1}
```

对：

```sl
packet = %net {a: 1}
conn.send(packet)

json = json_encode({a: 1})
conn.send_raw(json)
```

`%net` 是运行时编码，不是给人看的 JSON。

#### 错误 2：忘了 `send` / `store` 只吃 net-form

错：

```sl
conn.send({a: 1})
slot.store([1, 2, 3])
```

对：

```sl
conn.send(%net {a: 1})
slot.store(%net [1, 2, 3])
```

#### 错误 3：把 `send_raw` 和 `recv` 混用

错：

```sl
conn.send_raw("hello")
v = conn.recv()
```

对：

```sl
conn.send_raw("hello")
s = conn.recv_raw(5)
```

或者：

```sl
conn.send(%net "hello")
v = conn.recv()
```

#### 错误 4：把 `&shm` 当消息队列

错：

```sl
slot.store(%net "first")
slot.store(%net "second")
print(slot.load())   # 以为能取到 first 再取 second
```

实际只会读到最后一次写入的 `"second"`。

#### 错误 5：把 `&tcp` 地址写成 URL

错：

```sl
conn = &tcp("http://127.0.0.1:9000")
```

对：

```sl
conn = &tcp("127.0.0.1:9000")
```

#### 错误 6：以为函数和句柄也能 `%net`

错：

```sl
f = x -> x + 1
packet = %net f
```

错：

```sl
conn = &tcp("127.0.0.1:9000")
packet = %net conn
```

`%net` 传的是普通数据，不传函数和资源句柄。

#### 错误 7：以为 `recv_raw(n)` 按“消息条数”读

错：

```sl
msg = conn.recv_raw(1)   # 以为是读一条消息
```

对：

```sl
msg = conn.recv_raw(1)   # 这是只读 1 个字节
```

`recv_raw(n)` 的参数单位是“字节数”。

### 12.7 选型建议：别再下意识退回 `&tcp + JSON`

最后给一个实用结论：

- 双方都是 ssiLang：直接 `%net`
- 你控制外部语言实现：优先 `%net V1`
- 公共 API / 人类可读日志：JSON
- 同机最新状态同步：`&shm + %net`
- 同机消息通信：`&pipe + %net`
- 网络请求应答：`&tcp + %net`

JSON 不是默认答案。它只是“最通用”的答案，不是“最合适”的答案。

---

## 13. 算法竞赛实战

下面是几个常见算法的完整 ssiLang 实现，帮你建立"在这门语言里该怎么写"的直觉。

### 13.1 二维 DP 模板（01 背包）

```sl
fn knapsack(weights, values, cap) {
    n = len(weights)
    # 初始化 (n+1) x (cap+1) 的二维数组
    dp = []
    for i in range(n + 1) {
        row = []
        for j in range(cap + 1) {
            row.push(0)
        }
        dp.push(row)
    }
    # 填表
    for i in range(1, n + 1) {
        for w in range(cap + 1) {
            dp[i][w] = dp[i - 1][w]
            if weights[i - 1] <= w {
                val = dp[i - 1][w - weights[i - 1]] + values[i - 1]
                if val > dp[i][w] {
                    dp[i][w] = val
                }
            }
        }
    }
    return dp[n][cap]
}

print(knapsack([2, 3, 4, 5], [3, 4, 5, 6], 8))   # 10
```

**要点：** ssiLang 没有 `vector<vector<int>>` 的一步初始化，需要循环 push 构建二维数组。

### 13.2 图论模板（Dijkstra）

```sl
fn dijkstra(graph, src, n) {
    dist = []
    visited = []
    for i in range(n) {
        dist.push(999999)
        visited.push(false)
    }
    dist[src] = 0

    for step in range(n) {
        # 找最小未访问节点
        u = -1
        best = 999999
        for i in range(n) {
            if not visited[i] and dist[i] < best {
                best = dist[i]
                u = i
            }
        }
        if u == -1 => break
        visited[u] = true

        # 松弛邻居
        for e in graph[u] {
            v = e[0]
            w = e[1]
            if dist[u] + w < dist[v] {
                dist[v] = dist[u] + w
            }
        }
    }
    return dist
}

# 邻接表：graph[u] = [[v, w], ...]
graph = [
    [[1, 1], [3, 4]],
    [[2, 2], [4, 1]],
    [[4, 1]],
    [[4, 3]],
    []
]
print(dijkstra(graph, 0, 5))   # [0, 1, 3, 4, 2]
```

### 13.3 递归树结构

```sl
fn node(val, left, right) {
    return {val: val, left: left, right: right}
}

fn leaf(val) => node(val, null, null)

fn tree_sum(n) {
    if n == null => return 0
    return n.val + tree_sum(n.left) + tree_sum(n.right)
}

fn inorder(n) {
    if n == null => return []
    l = inorder(n.left)
    r = inorder(n.right)
    result = []
    for x in l { result.push(x) }
    result.push(n.val)
    for x in r { result.push(x) }
    return result
}

tree = node(4,
    node(2, leaf(1), leaf(3)),
    node(6, leaf(5), leaf(7))
)
print(tree_sum(tree))    # 28
print(inorder(tree))     # [1, 2, 3, 4, 5, 6, 7]
```

### 13.4 快速幂

```sl
fn mod_pow(base, exp, m) {
    result = 1
    base = base % m
    while exp > 0 {
        if exp % 2 == 1 {
            result = result * base % m
        }
        exp = int(exp / 2)
        base = base * base % m
    }
    return result
}

print(mod_pow(2, 10, 1000))          # 24
print(mod_pow(3, 13, 1000000007))    # 1594323
```

### 13.5 归并排序

```sl
fn merge(left, right) {
    result = []
    i = 0
    j = 0
    while i < len(left) and j < len(right) {
        if left[i] <= right[j] {
            result.push(left[i])
            i = i + 1
        } else {
            result.push(right[j])
            j = j + 1
        }
    }
    while i < len(left) {
        result.push(left[i])
        i = i + 1
    }
    while j < len(right) {
        result.push(right[j])
        j = j + 1
    }
    return result
}

fn mergesort(arr) {
    n = len(arr)
    if n <= 1 => return arr
    mid = int(n / 2)
    left = mergesort(arr.slice(0, mid))
    right = mergesort(arr.slice(mid, n))
    return merge(left, right)
}

print(mergesort([38, 27, 43, 3, 9, 82, 10]))
# [3, 9, 10, 27, 38, 43, 82]
```

### 13.6 带记忆化的递归

```sl
fn make_memo_fib() {
    cache = {"0": 0, "1": 1}
    fn fib(n) {
        key = str(n)
        cached = cache?[key]
        if cached != null => return cached
        result = fib(n - 1) + fib(n - 2)
        cache[key] = result
        return result
    }
    return fib
}

memo_fib = make_memo_fib()
print(memo_fib(30))   # 832040
```

> **注意：** map 的 key 必须是 string，所以数字要先 `str()` 转换。`cache?[key]` 用 safe access 避免 key 不存在时报错。

---

## 14. C++ 程序员速查对照表

| C++ | ssiLang | 备注 |
|-----|---------|------|
| `int x = 42;` | `x = 42` | 无类型声明，无分号 |
| `std::string s = "hi";` | `s = "hi"` | |
| `std::vector<int> v = {1,2,3};` | `v = [1, 2, 3]` | |
| `std::map<string,int> m;` | `m = {}` | key 只能是 string |
| `v.push_back(x);` | `v.push(x)` | |
| `v.pop_back();` | `v.pop()` | 且返回弹出的值 |
| `v.size()` | `len(v)` | 全局函数 |
| `v[i]` | `v[i]` | 相同 |
| `m["key"]` | `m["key"]` 或 `m.key` | 支持点号语法 |
| `m.count("k")` | `m.has("k")` | |
| `m.erase("k")` | `m.remove("k")` | 返回被移除的值 |
| `for(int i=0;i<n;i++)` | `for i in range(n)` | |
| `for(auto& x : vec)` | `for x in vec` | |
| `cond ? a : b` | `cond => a \|\| b` | |
| `x?.value()` | `x?.value` | Safe access |
| `std::sort(v.begin(),v.end())` | `sort(v)` | 返回新列表 |
| `auto f = [](int x){ return x*2; };` | `f = x -> x * 2` | |
| `int& ref = arr[i];` | `ref = &ref arr[i]` | |
| `*ref = 42;` | `ref.set(42)` | |
| `std::cout << x;` | `print(x)` | |
| `std::cin >> s;` | `s = input()` | |
| `// comment` | `# comment` | |
| `nullptr` | `null` | |
| `dynamic_cast` / `typeid` | `type(v)` | 返回字符串 |

---

## 15. 关键字完整列表

```
fn  if  else  while  for  in  return  break  continue
true  false  null  and  or  not
```

一共 15 个，比 C++ 的 90+ 个少很多。

---

## 16. 常见坑与技巧

### 16.1 除法是浮点除法

```sl
print(7 / 2)       # 3.5，不是 3
print(int(7 / 2))  # 3（需要手动截断）
```

### 16.2 没有 `+=`、`++`

```sl
# 错误
# x += 1
# x++

# 正确
x = x + 1
```

### 16.3 `{}` 在语句位置是 block，在表达式位置是 map

```sl
if true {          # 这里的 {} 是 block
    print("yes")
}

m = {a: 1}        # 这里的 {} 是 map
```

### 16.4 map key 必须是 string

```sl
m = {}
m[0] = "zero"     # key 实际上是数字...
m["0"] = "zero"   # 推荐显式用字符串

# 做记忆化时：
cache[str(n)] = result   # 数字 key 先转 string
cached = cache?[str(n)]  # 查询也用 string
```

### 16.5 闭包修改外层变量

```sl
# 这样不行——x = x + 1 在内层创建了新绑定
x = 0
fn bad_inc() {
    x = x + 1   # 这里的 x 是新的局部变量！
}

# 这样可以——通过列表间接修改
state = [0]
fn good_inc() {
    state[0] = state[0] + 1
}
good_inc()
print(state[0])   # 1
```

### 16.6 数值精度

底层是 `double`（64 位浮点），大整数会丢失精度：

```sl
print(9007199254740992 + 1)   # 可能不准确
# 对于竞赛中的大数取模题，在 10^15 以内是安全的
```

---

## 17. 完整程序示例：词频统计

一个展示多种语言特性的完整程序：

```sl
fn word_freq(text) {
    words = text.split(" ")
    freq = {}
    for w in words {
        w = w.trim()
        if len(w) == 0 => continue
        c = freq?[w]
        freq[w] = (c == null => 1 || c + 1)
    }
    return freq
}

fn top_words(freq, n) {
    keys = freq.keys()
    # 简单选择排序找 top n
    result = []
    for round in range(n) {
        best_key = ""
        best_count = -1
        for k in keys {
            c = freq[k]
            if c > best_count {
                best_count = c
                best_key = k
            }
        }
        if best_key == "" => break
        result.push({word: best_key, count: best_count})
        freq[best_key] = -1   # 标记已选
    }
    return result
}

text = "the cat sat on the mat the cat ate the rat"
freq = word_freq(text)
print("All frequencies:")
for k in freq.keys() {
    print("  " + k + ": " + str(freq[k]))
}

top = top_words(%deep freq, 3)
print("Top 3:")
for item in top {
    print("  " + item.word + " -> " + str(item.count))
}
```

---

## 附录 A：ssiLang 方法速查

### List 方法

| 方法 | 修改原列表 | 返回值 |
|------|-----------|--------|
| `.push(v)` | 是 | `null` |
| `.pop()` | 是 | 弹出的元素 |
| `.slice(start, end)` | 否 | 新列表 |
| `.map(fn)` | 否 | 新列表 |
| `.filter(fn)` | 否 | 新列表 |
| `.reduce(fn, init)` | 否 | 累积值 |
| `.find(fn)` | 否 | 元素或 `null` |
| `.contains(v)` | 否 | `bool` |
| `.join(sep)` | 否 | `string` |
| `.reverse()` | 否 | 新列表 |

### String 方法

| 方法 | 返回值 |
|------|--------|
| `.split(sep)` | `list` |
| `.trim()` | `string` |
| `.chars()` | `list` |
| `.starts_with(s)` | `bool` |
| `.ends_with(s)` | `bool` |
| `.contains(s)` | `bool` |
| `.replace(old, new)` | `string` |

### Map 方法

| 方法 | 返回值 |
|------|--------|
| `.keys()` | `list` |
| `.values()` | `list` |
| `.has(key)` | `bool` |
| `.remove(key)` | 被移除的值或 `null` |

### TCP Client 方法

| 方法 | 返回值 |
|------|--------|
| `.send(net_form)` | `null` |
| `.recv()` | 反序列化后的值 |
| `.send_raw(string)` | `null` |
| `.recv_raw(n)` | `string` |
| `.peer_addr()` | `string` |
| `.set_timeout(secs)` | `null` |
| `.close()` | `null` |

### TCP Server 方法

| 方法 | 返回值 |
|------|--------|
| `.accept()` | 新 TCP client handle |
| `.addr()` | `string` |
| `.close()` | `null` |

### Pipe 方法

| 方法 | 返回值 |
|------|--------|
| `.send(net_form)` | `null` |
| `.recv()` | 反序列化后的值 |
| `.is_named()` | `bool` |
| `.name()` | `string` 或 `null` |
| `.flush()` | `null` |
| `.close()` | `null` |

### SHM 方法

| 方法 | 返回值 |
|------|--------|
| `.store(net_form)` | `null` |
| `.load()` | 值或 `null` |
| `.clear()` | `null` |
| `.size()` | `number` |
| `.name()` | `string` |
| `.close()` | `null` |
