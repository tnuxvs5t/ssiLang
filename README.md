# ssiLang v3

ssiLang 是一门面向脚本、算法题、小工具和轻量 IPC 的动态语言。

v3 的核心升级只有一件事：把语言里的“上下文”和“占位”彻底拆开。

- `@` 只管上下文
- `$` 只管占位

这让两类原本很别扭的代码同时变干净：

- 闭包里修改外层状态：用 `@x`
- 高阶函数里写匿名变换：用 `$`

```sl
fn make_counter(start) {
    total = start
    fn inc(n) {
        @total = @total + n
        return @total
    }
    return inc
}

result =
    [1, 2, 3, 4, 5]
    |> filter($ > 2)
    |> map($ * 10)
    |> join(", ")
```

---

## 快速开始

### 构建

Windows / MinGW:

```powershell
g++ -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter -Isrc -o sl.exe src/main.cpp -lws2_32
```

如果你的环境有 `make`，也可以直接使用仓库里的 `Makefile`。

### 运行

```powershell
.\sl.exe test\t12_v3_context.sl
```

REPL:

```powershell
.\sl.exe
```

源文件应为 UTF-8。当前实现同时接受带 BOM 和不带 BOM 的 UTF-8 文件。

---

## 一分钟上手

```sl
name = "ssiLang"
version = 3

fn greet(user) => "hello, " + user

nums = [1, 2, 3, 4, 5]
result = nums |> filter($ > 2) |> map($ * 10)
print(result)

fn make_adder(base) {
    return $ + @base
}

add10 = make_adder(10)
print(add10(7))

user = {name: "Alice", age: 30}
label = @(user) => @.name + "#" + str(@.age)
print(label)
```

---

## 设计核心

### `@` 是上下文

`@` 系列有两种用法：

1. `@x`
表示“外层词法上下文中的 `x`”。

2. `@`
表示“当前匿名值上下文”。

### `$` 是占位

`$` / `$1` / `$2` / ... 只表示匿名参数位置。
任何在当前表达式边界内包含 `$` 的表达式，都会整体降糖为 lambda。
显式 lambda / 函数体会形成新的降糖边界。

这条规则是 v3 的主轴。

---

## 词法与基本语法

### 注释

只有单行注释：

```sl
# this is a comment
x = 42
```

### 换行结束语句

ssiLang 默认用换行结束语句，不需要分号。

```sl
x = 1
y = 2
print(x + y)
```

在 `()`, `[]`, `{}` 内部可以自由换行。

### 变量绑定

没有 `let` / `var` / `const`。
`=` 就是绑定或重绑定。

```sl
x = 42
name = "Alice"
ok = true
```

### 当前作用域 vs 外层作用域

`x = ...` 永远只作用于当前作用域。

```sl
fn demo() {
    x = 1
    fn bump() {
        @x = @x + 1
    }
    bump()
    return x
}

print(demo())   # 2
```

如果你想访问或修改外层已有绑定，用 `@x`：

```sl
fn counter(start) {
    total = start
    fn inc(n) {
        @total = @total + n
        return @total
    }
    return inc
}
```

规则：

- `@x` 从当前环境的父环境开始查找
- 只查找外层，不查当前层
- 找不到会报错
- `@x = ...` 不会隐式创建新变量

### 词法作用域边界

当前实现里，真正会创建新词法环境的只有两类结构：

- 函数调用
- `@(expr) { ... }` / `@(expr) => expr`

普通的 `if` / `while` / `for` block 只是语句分组，不会自动新建词法作用域。

所以这种写法是成立的：

```sl
sum = 0
for i in range(4) {
    sum = sum + i
}
print(sum)      # 6
```

而 `@x` 的职责被收窄为：

- 闭包访问或修改外层函数状态
- 上下文块访问外层词法绑定

---

## 数据类型

ssiLang 是动态类型语言。

| 类型 | 例子 | `type(...)` 结果 |
|---|---|---|
| number | `42`, `3.14`, `-7` | `"number"` |
| string | `"hello"` | `"string"` |
| bool | `true`, `false` | `"bool"` |
| null | `null` | `"null"` |
| list | `[1, 2, 3]` | `"list"` |
| map | `{name: "Alice", age: 30}` | `"map"` |
| function | `fn f(x) => x`, `$ + 1` | `"function"` |
| link-ref | `&ref x` | `"link-ref"` |
| link-tcp | `&tcp("127.0.0.1:9000")` | `"link-tcp"` |
| link-tcp-server | `tcp_listen("127.0.0.1:9000")` | `"link-tcp-server"` |
| link-pipe | `&pipe()` | `"link-pipe"` |
| link-shm | `&shm("slot")` | `"link-shm"` |
| net-form | `%net data` | `"net-form"` |

说明：

- 数值底层统一为 `double`
- 没有单独的 int 类型
- `int(...)` 是截断转换

---

## 字符串、列表、映射

### 字符串

```sl
s = "hello" + " " + "world"
print(s)
print(s[0])
```

字符串方法：

- `split(sep)`
- `trim()`
- `chars()`
- `starts_with(prefix)`
- `ends_with(suffix)`
- `contains(part)`
- `replace(old, new)`

### 列表

```sl
arr = [1, 2, 3]
arr.push(4)
print(arr[0])
print(arr.pop())
```

列表方法：

- `push(v)`
- `pop()`
- `slice(start, end)`
- `map(fn)`
- `filter(fn)`
- `reduce(fn, init)`
- `find(fn)`
- `contains(v)`
- `join(sep)`
- `reverse()`

### 映射

键只能是字符串。
这意味着：
- `m["name"]` 合法
- `m[1]` 是运行时错误
- `m[1] = v` 是运行时错误
- `&ref m[1]` 是运行时错误

```sl
m = {name: "Alice", age: 30}
print(m.name)
print(m["age"])

m.city = "NYC"
m["score"] = 95
```

映射方法：

- `keys()`
- `values()`
- `has(key)`
- `remove(key)`

---

## 表达式

### 算术

```sl
1 + 2
10 - 3
4 * 5
15 / 4
17 % 5
-7
```

### 比较与逻辑

```sl
1 < 2
3 >= 3
1 == 1
1 != 2

true and false
true or false
not true
```

### 条件分流表达式

ssiLang 保留了 split 表达式：

```sl
grade = 85
result = grade >= 90 => "A" || grade >= 80 => "B" || "C"
```

### 安全访问

```sl
m = {a: {b: 42}}
print(m?.a?.b)
print(m?.x?.y)

arr = [1, 2, 3]
print(arr?[0])
```

支持：

- `obj?.field`
- `obj?[index]`

---

## 函数

### 命名函数

```sl
fn add(a, b) {
    return a + b
}

fn mul(a, b) => a * b
```

### Lambda

```sl
inc = x -> x + 1
pair_sum = (a, b) -> a + b
ping = () -> "pong"
```

### 闭包

函数是闭包，自动捕获定义时环境。

```sl
fn make_adder(base) {
    fn add(x) {
        return x + @base
    }
    return add
}
```

---

## v3 上下文语法 `@`

### `@x`：外层词法绑定

```sl
fn make_counter(start) {
    total = start
    fn inc(step) {
        @total = @total + step
        return @total
    }
    return inc
}
```

### bare `@`：当前匿名值上下文

bare `@` 是一个普通值，所以可以继续做成员访问、索引、安全访问。

```sl
@(user) {
    print(@.name)
    print(@.age)
}
```

### `@(expr) { ... }`：上下文块

```sl
config = {
    server: {
        host: "127.0.0.1",
        port: 8080
    }
}

@(config.server) {
    print(@.host)
    print(@.port)
}
```

语义：

1. 先计算 `expr`
2. 建一个子环境
3. 把它设为当前值上下文
4. 在子环境里执行 block

### `@(expr) => expr`：上下文表达式

```sl
label = @(user) => @.name + "#" + str(@.age)
```

这不是 lambda，而是“立刻执行的上下文表达式”。

### 上下文嵌套

```sl
request_id = "outer-ctx"
request = {user: {name: "Eve"}}

@(request) {
    @( @.user ) {
        print(@.name)
        print(@request_id)
    }
}
```

---

## v3 占位语法 `$`

### `$` 和 `$n`

- `$` 等于 `$1`
- `$1`, `$2`, `$3` 表示匿名参数位置

```sl
inc = $ + 1
add = $1 + $2
pick_name = $.name
```

### 自动降糖为 lambda

任何包含 `$` 的表达式都会自动变成函数。

```sl
$ + 1
```

等价于：

```sl
x -> x + 1
```

```sl
$1 + $2
```

等价于：

```sl
(a, b) -> a + b
```

### 高阶函数写法

```sl
nums = [1, 2, 3, 4, 5]

print(nums |> map($ * 2))
print(nums |> filter($ > 2))
print(nums |> reduce($1 + $2, 0))

users = [
    {name: "Alice", age: 30},
    {name: "Bob", age: 17}
]

print(users |> map($.name))
print(users |> find($.age >= 18))
```

### `@` 和 `$` 可以同时出现

```sl
fn scale_by(factor) {
    return $ * @factor
}
```

这里：

- `$` 是传入参数
- `@factor` 是外层词法绑定

---

## 数据流

### `|>` 管道

`a |> f` 等价于 `f(a)`。

```sl
result =
    [1, 2, 3, 4, 5]
    |> filter($ > 2)
    |> map($ * 10)
    |> join(", ")
```

v3 里只有 `|>` 一个 flow 运算符。

---

## 控制流

### `if`

```sl
if x > 0 {
    print("positive")
} else {
    print("non-positive")
}
```

也支持箭头体：

```sl
if x > 0 => return "positive"
```

### `while`

```sl
i = 0
while i < 5 {
    print(i)
    i = i + 1
}
```

### `for ... in`

`for` 目前遍历 list。

```sl
for x in [10, 20, 30] {
    print(x)
}
```

### `break` / `continue` / `return`

```sl
for x in range(10) {
    if x == 3 => continue
    if x == 7 => break
    print(x)
}
```

---

## 复制、派生与网络表单

前缀 `%mode` 仍然保留。

### `%copy`

浅拷贝 list / map。

```sl
clone = %copy data
```

### `%deep`

深拷贝 list / map。

```sl
deep = %deep data
```

### `%snap`

`%snap` 的语言语义固定为“独立快照”。
当前实现使用 eager `deep copy` 达到这个语义；以后即使内部优化为写时复制，外部可观察行为也必须与深拷贝一致。

```sl
snap = %snap data
```

### `%net`

把值序列化为 `net-form`，可用于 tcp / pipe / shm。

```sl
packet = %net {kind: "ping", id: 42}
print(type(packet))
```

支持序列化的值类型：

- number
- string
- bool
- null
- list
- map

---

## 引用系统 `&ref`

v3 里 `@x` 已经能解决闭包改外层变量的问题，所以 `&ref` 的定位变成“一等引用值”。

```sl
arr = [10, 20, 30]
r = &ref arr[1]
print(r.deref())
r.set(99)
print(arr)

x = 42
rv = &ref x
rv.set(100)
print(x)
```

也可以直接引用外层词法绑定：

```sl
fn make_box(v) {
    value = v
    fn expose(dummy) => &ref @value
    return expose
}
```

可引用目标：

- `&ref x`
- `&ref @x`
- `&ref arr[i]`
- `&ref m.key`

ref 方法：

- `deref()`
- `set(v)`

---

## 模块系统 `import`

```sl
math = import("modules/math")
print(math.add(2, 3))
```

规则：

- 相对路径相对于当前脚本文件所在目录解析
- 如果没有扩展名，会尝试自动补 `.sl`
- 模块会缓存
- 循环导入会报错
- 只有不以下划线开头的绑定会被导出

例子：

```sl
math = import("modules/math")
math_again = import("modules/math.sl")
print(math == math_again)
```

---

## 内建函数

### 基础

- `print(...)`
- `debug(...)`
- `probe(...)`
- `input()`
- `len(x)`
- `type(x)`
- `int(x)`
- `float(x)`
- `str(x)`
- `error(msg)`
- `pcall(fn)`

`pcall(fn)` 返回：

```sl
{ok: true, value: ...}
{ok: false, error: "..."}
```

### 数值 / 序列

- `range(end)`
- `range(start, end)`
- `range(start, end, step)`
- `abs(x)`
- `min(a, b)`
- `max(a, b)`
- `sort(list)`

### 管道友好的全局包装

这些也都存在于全局函数层，便于 `|>`：

- `map(list, fn)`
- `filter(list, fn)`
- `reduce(list, fn, init)`
- `find(list, fn)`
- `join(list, sep)`
- `reverse(list)`
- `push(list, value)`

### JSON

- `json_encode(value)`
- `json_parse(text)`

### 网络

- `tcp_listen(addr, backlog?)`

---

## 对象方法总览

### list

- `push(v)`
- `pop()`
- `slice(start, end)`
- `map(fn)`
- `filter(fn)`
- `reduce(fn, init)`
- `find(fn)`
- `contains(v)`
- `join(sep)`
- `reverse()`

### string

- `split(sep)`
- `trim()`
- `chars()`
- `starts_with(prefix)`
- `ends_with(suffix)`
- `contains(part)`
- `replace(old, new)`

### map

- `keys()`
- `values()`
- `has(key)`
- `remove(key)`

如果 map 某个字段是函数，也可以通过方法风格调用它：

```sl
obj = {
    hello: name -> "hi " + name
}
print(obj.hello("ssi"))
```

### ref

- `deref()`
- `set(v)`

### tcp client

通过 `&tcp("host:port")` 创建。

方法：

- `send(net_form)`
- `recv()`
- `send_raw(text)`
- `recv_raw(n)`
- `peer_addr()`
- `set_timeout(seconds)`
- `close()`

### tcp server

通过 `tcp_listen("127.0.0.1:19876")` 创建。

方法：

- `accept()`
- `addr()`
- `close()`

### pipe

通过 `&pipe()` 或 `&pipe("name")` 创建。

方法：

- `send(net_form)`
- `recv()`
- `is_named()`
- `name()`
- `flush()`
- `close()`

### shared memory

通过 `&shm("name", size?)` 创建。

方法：

- `store(net_form)`
- `load()`
- `size()`
- `name()`
- `clear()`
- `close()`

---

## IPC / 网络例子

### TCP

```sl
srv = tcp_listen("127.0.0.1:19876")
cli = &tcp("127.0.0.1:19876")
peer = srv.accept()

cli.send(%net {msg: "hello", n: 42})
got = peer.recv()
print(got.msg)
print(got.n)

peer.send(%net "pong")
print(cli.recv())
```

### Pipe

```sl
p = &pipe()
p.send(%net [1, 2, 3])
print(p.recv())
```

### SHM

```sl
slot = &shm("sl_test_basic")
slot.store(%net {x: 10, y: 20})
print(slot.load())
slot.close()
```

---

## JSON

```sl
text = json_encode({name: "Alice", scores: [90, 85]})
print(text)

obj = json_parse(text)
print(obj.name)
print(obj.scores[0])
```

---

## 当前实现的语义注意点

### 1. 词法环境边界要分清

只有函数调用和上下文块会引入新的词法环境。

所以：

- 在普通 `if` / `for` / `while` block 里，`x = ...` 会更新当前环境中的 `x`
- 在闭包或上下文子环境里，如果你想改外层，必须显式写 `@x`

### 2. `@(expr)` 不是普通表达式

当前语法只允许：

- `@(expr) { ... }`
- `@(expr) => expr`

不允许单独写一个裸的 `@(expr)`。

### 3. `$` 只在表达式里有意义

它会降糖成 lambda，不是运行时变量。

### 4. `%snap` 的语义已经冻结

它表示“与原值后续修改隔离的独立快照”。
当前实现方式是 eager deep copy。

实现保留它用于迁移，README 只把它当兼容语法。

---

## 测试

仓库中的脚本测试覆盖：

- 基础语法与表达式
- 容器和字符串方法
- 闭包、递归、高阶函数
- v3 的 `@` / `$`
- 模块导入
- `%net` / pipe / shared memory / tcp
- JSON
- 大一点的算法样例

推荐直接运行：

```powershell
.\sl.exe test\t12_v3_context.sl
.\sl.exe test\t23_ipc_full.sl
.\sl.exe test\t11_lang.sl
```

---

## 仓库结构

```text
src/
  main.cpp
  types.h
  lexer.h
  parser.h
  interp.h

test/
  t01_basics.sl
  ...
  t12_v3_context.sl
  t23_ipc_full.sl

v3-concrete.md
README.md
```

---

## 相关文档

- `README.md`
  面向使用者的 v3 手册
- `v3-concrete.md`
  v3 concrete design / source of truth

---

## v3 Frozen Clarifications

- map keys are string-only; `m?[1]` is still a runtime error
- list/string indices must be integer-valued numbers
- `arr[1.5]`, `arr["0"]`, and `"abc"["1"]` are runtime errors
- `arr[i] = v` and `&ref arr[i]` use the same integer-index rule
- safe access only suppresses null-base / missing / out-of-range cases
- safe access does not swallow type errors; `5?[0]` is still a runtime error
  面向实现的 v3 精确定义

如果两者有分歧，以 `v3-concrete.md` 为准。
