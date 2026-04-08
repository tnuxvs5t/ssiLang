# ssiLang v3 Comprehensive Guide

这是一份**独立的、完整的、面向 v3 的语言教程与参考文档**。

它不依赖 `v2_2` 的历史背景，也不要求读者知道旧语法。
你可以把它当成一份单独的语言手册来读。

文档目标：

1. 从零介绍 ssiLang v3 的核心设计。
2. 系统解释语法、语义、运行模型与标准能力。
3. 详细覆盖 `@` 与 `$` 这两个 v3 的核心特性。
4. 提供足够多的例子，让后续 VM 重写时也能把它当行为规范参考。

---

## 29. v3 Frozen Clarifications

- map keys are string-only; `m?[1]` is still a runtime error
- list/string indices must be integer-valued numbers
- `arr[1.5]`, `arr["0"]`, and `"abc"["1"]` are runtime errors
- `arr[i] = v` and `&ref arr[i]` use the same integer-index rule
- safe access only suppresses null-base / missing / out-of-range cases
- safe access does not swallow type errors; `5?[0]` is still a runtime error

## 1. 语言定位

ssiLang 是一门：

- 动态类型
- 解释执行
- 语法偏轻量
- 适合脚本、算法、原型、小工具、轻量 IPC

的语言。

v3 的核心不是“加更多特性”，而是**统一语言的表达模型**。

v3 解决的核心问题有两个：

1. 闭包里访问/修改外层状态不直觉。
2. 高阶函数里到处写 `x -> ...`、`(a, b) -> ...` 太啰嗦。

v3 的答案是：

- `@` 只处理上下文
- `$` 只处理占位

这是 v3 最重要的设计原则。

---

## 2. 核心心智模型

如果你只记一件事，就记这一张表：

| 写法 | 含义 |
|---|---|
| `x` | 当前词法环境中的普通名字 |
| `@x` | 外层词法环境中的名字 |
| `@` | 当前匿名值上下文 |
| `$` | 第 1 个匿名参数 |
| `$2` | 第 2 个匿名参数 |

也可以换一种记法：

- `@` 回答“我在什么上下文里？”
- `$` 回答“这个位置的参数是谁？”

这就是 v3 的统一性来源。

---

## 3. Hello World

```sl
print("hello, world")
```

更接近 v3 风格的例子：

```sl
nums = [1, 2, 3, 4, 5]
result = nums |> filter($ > 2) |> map($ * 10)
print(result)
```

输出：

```text
[30, 40, 50]
```

---

## 4. 运行方式

### 4.1 运行脚本

Windows:

```powershell
.\sl.exe your_script.sl
```

### 4.2 REPL

如果不传脚本文件，进入 REPL：

```powershell
.\sl.exe
```

### 4.3 文件编码

建议使用 UTF-8。

当前实现同时接受：

- UTF-8
- UTF-8 with BOM

---

## 5. 基本语法

### 5.1 注释

只有单行注释：

```sl
# comment
x = 42
```

### 5.2 语句结束

默认使用换行结束语句，不需要分号。

```sl
x = 1
y = 2
print(x + y)
```

### 5.3 括号内自由换行

在 `()`, `[]`, `{}` 内可以换行：

```sl
arr = [
    1,
    2,
    3
]
```

---

## 6. 变量与作用域

### 6.1 普通变量绑定

没有 `let`、`var`、`const`。

```sl
x = 42
name = "Alice"
ok = true
```

`=` 的含义是：

- 如果当前环境里没有这个名字，就创建
- 如果当前环境里已经有这个名字，就重绑定

### 6.2 词法环境边界

当前 v3 实现里，真正创建新词法环境的结构只有两类：

1. 函数调用
2. 上下文块 / 上下文表达式

普通 `if` / `while` / `for` 的 block **只是语句分组，不自动创建新词法环境**。

所以：

```sl
sum = 0
for i in range(4) {
    sum = sum + i
}
print(sum)   # 6
```

这段代码中，`sum` 是在同一个环境里被更新的。

### 6.3 `@x`：访问外层词法绑定

如果你在闭包或上下文子环境中，想读/写外层变量，就用 `@x`。

```sl
fn make_counter(start) {
    total = start
    fn inc(step) {
        @total = @total + step
        return @total
    }
    return inc
}

c = make_counter(10)
print(c(1))   # 11
print(c(5))   # 16
```

规则：

1. `@x` 从“当前环境的父环境”开始查找。
2. 不查当前环境。
3. 找到最近的一层就停。
4. 找不到就报错。
5. `@x = ...` 只允许写已经存在的外层绑定，不会隐式创建。

---

## 7. 数据类型

当前语言内建的数据类型有：

| 类型 | 例子 | `type(...)` |
|---|---|---|
| number | `42`, `3.14`, `-7` | `"number"` |
| string | `"hello"` | `"string"` |
| bool | `true`, `false` | `"bool"` |
| null | `null` | `"null"` |
| list | `[1, 2, 3]` | `"list"` |
| map | `{name: "Alice"}` | `"map"` |
| function | `fn f(x) => x`, `$ + 1` | `"function"` |
| link-ref | `&ref x` | `"link-ref"` |
| link-tcp | `&tcp("127.0.0.1:9000")` | `"link-tcp"` |
| link-tcp-server | `tcp_listen("127.0.0.1:9000")` | `"link-tcp-server"` |
| link-pipe | `&pipe()` | `"link-pipe"` |
| link-shm | `&shm("slot")` | `"link-shm"` |
| net-form | `%net data` | `"net-form"` |

### 7.1 number

数值底层统一为一个 number 类型。

```sl
print(42)
print(3.14)
print(-7)
```

当前实现底层是 `double`，没有单独的 int 类型。

### 7.2 string

```sl
s = "hello"
print(s)
print(s[0])
```

### 7.3 bool

```sl
print(true)
print(false)
```

### 7.4 null

```sl
x = null
print(x)
```

### 7.5 list

```sl
arr = [1, 2, 3]
print(arr[0])
```

### 7.6 map

key 必须是字符串。
这意味着：
- `m["name"]` 合法
- `m[1]` 是运行时错误
- `m[1] = v` 是运行时错误
- `&ref m[1]` 是运行时错误

```sl
user = {
    name: "Alice",
    age: 30
}
print(user.name)
print(user["age"])
```

---

## 8. 真值规则

下列值是假：

- `false`
- `null`
- `0`
- `""`
- `[]`

其余值都是真。

例子：

```sl
if [] {
    print("won't run")
} else {
    print("empty list is false")
}
```

---

## 9. 运算符

### 9.1 算术

```sl
1 + 2
10 - 3
4 * 5
15 / 4
17 % 5
-7
```

注意：

- `/` 是浮点除法
- `%` 是浮点 `fmod`

### 9.2 比较

```sl
1 < 2
3 >= 3
1 == 1
1 != 2
```

number 与 string 支持有序比较。

### 9.3 逻辑

```sl
true and false
true or false
not true
```

### 9.4 管道

```sl
[1, 2, 3] |> map($ * 2)
```

`a |> f(b)` 等价于 `f(a, b)`。

### 9.5 flow 运算符

v3 只有 `|>` 一个 flow 运算符。

### 9.6 split 表达式

这是一种表达式式条件分流：

```sl
score = 85
grade = score >= 90 => "A" || score >= 80 => "B" || "C"
```

它相当于右结合的条件选择表达式。

---

## 10. 函数

### 10.1 命名函数

```sl
fn add(a, b) {
    return a + b
}
```

单表达式形式：

```sl
fn add(a, b) => a + b
```

### 10.2 lambda

```sl
inc = x -> x + 1
add = (a, b) -> a + b
ping = () -> "pong"
```

### 10.3 返回值

```sl
fn f() {
    return 42
}
```

如果没有显式 `return`，最终结果是 `null`。

### 10.4 闭包

函数会捕获定义时环境：

```sl
fn make_adder(base) {
    return x -> x + @base
}
```

---

## 11. v3 核心之一：`@`

`@` 全部只做“上下文”相关的事。

它有三种主要形态：

1. `@x`
2. bare `@`
3. `@(expr) ...`

### 11.1 `@x`

表示外层词法绑定。

```sl
fn make_total() {
    total = 0
    fn add(x) {
        @total = @total + x
        return @total
    }
    return add
}
```

### 11.2 bare `@`

表示当前匿名值上下文。

```sl
@(user) {
    print(@.name)
    print(@.age)
}
```

因为 bare `@` 是普通值，所以后面可以接：

- `@.field`
- `@[0]`
- `@?.field`
- `@?[0]`

### 11.3 `@(expr) { ... }`

上下文块。

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
2. 新建子词法环境
3. 把结果塞进这个子环境的当前值上下文
4. 执行 block

### 11.4 `@(expr) => expr`

上下文表达式。

```sl
label = @(user) => @.name + "#" + str(@.age)
```

它不是 lambda，而是立即执行。

### 11.5 上下文嵌套

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

这里：

- 里层 bare `@` 指向 `request.user`
- `@request_id` 指向外层词法绑定

### 11.6 `@` 与普通 block 的区别

`if/while/for` 的 block 不自动创建词法环境。

`@(expr) { ... }` 会创建新的子词法环境。

这两者在 v3 里是明确区分的。

---

## 12. v3 核心之二：`$`

`$` 全部只做“匿名参数占位”。

### 12.1 bare `$`

`$` 就是 `$1`。

```sl
inc = $ + 1
print(inc(41))   # 42
```

### 12.2 `$n`

```sl
add = $1 + $2
print(add(10, 20))   # 30
```

### 12.3 自动降糖

任何在当前表达式边界内包含 `$` 的表达式，都会整体降糖为 lambda。
显式 lambda / 函数体会形成新的降糖边界。

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

### 12.4 `$` 的作用范围

placeholder 的自动降糖是按表达式边界进行的。

这意味着：

```sl
map($ * 2)
reduce($1 + $2, 0)
find($.id == 42)
```

都能按你直觉工作。

### 12.5 `@` 与 `$` 一起使用

```sl
fn scale_by(factor) {
    return $ * @factor
}
```

这里：

- `$` 是匿名参数
- `@factor` 是外层词法绑定

这是 v3 最典型的组合写法之一。

---

## 13. 控制流

### 13.1 if / else

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

### 13.2 while

```sl
i = 0
while i < 5 {
    print(i)
    i = i + 1
}
```

### 13.3 for ... in

当前 `for` 遍历 list。

```sl
for x in [10, 20, 30] {
    print(x)
}
```

### 13.4 break / continue

```sl
for x in range(10) {
    if x == 3 => continue
    if x == 7 => break
    print(x)
}
```

---

## 14. string

### 14.1 基本操作

```sl
s = "hello" + " " + "world"
print(s)
print(s[0])
```

### 14.2 方法

#### `split(sep)`

```sl
print("a,b,c".split(","))
```

注意：

- 参数必须是 string
- 分隔符不能为空串

#### `trim()`

```sl
print("  hi  ".trim())
```

#### `chars()`

```sl
print("abc".chars())
```

#### `starts_with(prefix)`

```sl
print("hello".starts_with("he"))
```

#### `ends_with(suffix)`

```sl
print("hello".ends_with("lo"))
```

#### `contains(part)`

```sl
print("hello".contains("ell"))
```

#### `replace(old, new)`

```sl
print("hello world".replace("world", "ssiLang"))
```

注意：

- `old` 必须是非空字符串

---

## 15. list

### 15.1 基本操作

```sl
arr = [1, 2, 3]
print(arr[0])
arr.push(4)
print(arr)
```

### 15.2 方法

#### `push(v)`

```sl
arr.push(10)
```

#### `pop()`

```sl
last = arr.pop()
```

空 list 上 `pop()` 会报错。

#### `slice(start, end)`

```sl
print([1, 2, 3, 4, 5].slice(1, 3))
```

#### `map(fn)`

```sl
print([1, 2, 3] |> map($ * 2))
```

#### `filter(fn)`

```sl
print([1, 2, 3, 4] |> filter($ % 2 == 0))
```

#### `reduce(fn, init)`

```sl
print([1, 2, 3, 4] |> reduce($1 + $2, 0))
```

#### `find(fn)`

```sl
print([1, 2, 3, 4] |> find($ > 2))
```

#### `contains(v)`

```sl
print([1, 2, 3].contains(2))
```

#### `join(sep)`

```sl
print([1, 2, 3].join("-"))
```

#### `reverse()`

```sl
print([1, 2, 3].reverse())
```

---

## 16. map

### 16.1 创建

```sl
user = {
    name: "Alice",
    age: 30
}
```

### 16.2 读取

```sl
print(user.name)
print(user["age"])
```

### 16.3 写入

```sl
user.city = "NYC"
user["score"] = 95
```

### 16.4 方法

#### `keys()`

```sl
print(user.keys())
```

#### `values()`

```sl
print(user.values())
```

#### `has(key)`

```sl
print(user.has("name"))
```

参数必须是 string。

#### `remove(key)`

```sl
print(user.remove("age"))
```

参数必须是 string。

### 16.5 map 里的函数

如果 map 某个字段本身是 function，也可以像方法一样调：

```sl
obj = {
    hello: name -> "hi " + name
}
print(obj.hello("ssi"))
```

---

## 17. 安全访问

### 17.1 安全成员访问

```sl
m = {a: {b: 42}}
print(m?.a?.b)
print(m?.x?.y)
```

### 17.2 安全索引

```sl
arr = [1, 2, 3]
print(arr?[0])
print(arr?[99])
```

如果中间值是 `null` 或索引越界，返回 `null`。

---

## 18. 复制与派生

### 18.1 `%copy`

浅拷贝 list 或 map：

```sl
clone = %copy data
```

### 18.2 `%deep`

深拷贝 list 或 map：

```sl
deep = %deep data
```

### 18.3 `%snap`

`%snap` 的语言语义固定为“独立快照”。
当前实现使用 eager deep copy 达到这个语义；以后即使内部优化为写时复制，外部可观察行为也必须与深拷贝一致。

```sl
snap = %snap data
```

### 18.4 `%net`

把值编码成 `net-form`，用于 tcp / pipe / shm。

```sl
packet = %net {kind: "ping", id: 42}
print(type(packet))
```

可编码值：

- number
- string
- bool
- null
- list
- map

---

## 19. 引用系统

### 19.1 `&ref`

`&ref` 生成一等引用值。

```sl
arr = [10, 20, 30]
r = &ref arr[1]
print(r.deref())
r.set(99)
print(arr)
```

### 19.2 可引用目标

当前支持：

- `&ref x`
- `&ref @x`
- `&ref arr[i]`
- `&ref m.key`

### 19.3 ref 方法

- `deref()`
- `set(v)`

---

## 20. 模块系统

### 20.1 导入

```sl
math = import("modules/math")
print(math.add(2, 3))
```

### 20.2 路径规则

相对路径相对于“当前脚本所在目录”解析，而不是相对于进程工作目录。

### 20.3 扩展名

如果没有扩展名，会尝试补 `.sl`。

### 20.4 缓存

同一路径模块会缓存：

```sl
m1 = import("modules/math")
m2 = import("modules/math.sl")
print(m1 == m2)
```

### 20.5 导出规则

只有**不以下划线开头**的绑定会被导出。

---

## 21. 错误处理

### 21.1 主动抛错

```sl
error("boom")
```

### 21.2 `pcall`

```sl
fn risky() {
    error("boom")
}

r = pcall(risky)
print(r.ok)
print(r.error)
```

返回值结构：

成功：

```sl
{ok: true, value: ...}
```

失败：

```sl
{ok: false, error: "..."}
```

---

## 22. 内建函数总表

### 22.1 基础

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

### 22.2 数值与通用工具

- `range(end)`
- `range(start, end)`
- `range(start, end, step)`
- `abs(x)`
- `min(a, b)`
- `max(a, b)`
- `sort(list)`

### 22.3 管道友好的全局包装

- `map(list, fn)`
- `filter(list, fn)`
- `reduce(list, fn, init)`
- `find(list, fn)`
- `join(list, sep)`
- `reverse(list)`
- `push(list, value)`

### 22.4 模块与序列化

- `import(path)`
- `json_encode(value)`
- `json_parse(text)`

### 22.5 网络

- `tcp_listen(addr, backlog?)`

---

## 23. IPC 与系统能力

### 23.1 TCP client: `&tcp`

```sl
cli = &tcp("127.0.0.1:19876")
```

方法：

- `send(net_form)`
- `recv()`
- `send_raw(text)`
- `recv_raw(n)`
- `peer_addr()`
- `set_timeout(seconds)`
- `close()`

### 23.2 TCP server: `tcp_listen`

```sl
srv = tcp_listen("127.0.0.1:19876")
peer = srv.accept()
```

方法：

- `accept()`
- `addr()`
- `close()`

### 23.3 Pipe: `&pipe`

```sl
p = &pipe()
```

或：

```sl
p = &pipe("name")
```

方法：

- `send(net_form)`
- `recv()`
- `is_named()`
- `name()`
- `flush()`
- `close()`

### 23.4 Shared Memory: `&shm`

```sl
slot = &shm("demo", 8192)
```

方法：

- `store(net_form)`
- `load()`
- `size()`
- `name()`
- `clear()`
- `close()`

---

## 24. JSON

### 24.1 编码

```sl
text = json_encode({
    name: "Alice",
    scores: [90, 85]
})
print(text)
```

### 24.2 解码

```sl
obj = json_parse(text)
print(obj.name)
print(obj.scores[0])
```

---

## 25. 综合例子

### 25.1 闭包 + `@x`

```sl
fn make_counter(start) {
    total = start
    fn inc(step) {
        @total = @total + step
        return @total
    }
    return inc
}

c = make_counter(10)
print(c(1))
print(c(5))
```

### 25.2 上下文块 + bare `@`

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

### 25.3 `$` 管道

```sl
users = [
    {name: "Alice", age: 30},
    {name: "Bob", age: 17},
    {name: "Carol", age: 24}
]

adult_names =
    users
    |> filter($.age >= 18)
    |> map($.name)
    |> join(", ")

print(adult_names)
```

### 25.4 IPC

```sl
srv = tcp_listen("127.0.0.1:19876")
cli = &tcp("127.0.0.1:19876")
peer = srv.accept()

cli.send(%net {msg: "hello", n: 42})
packet = peer.recv()
print(packet.msg)
print(packet.n)

peer.send(%net "pong")
print(cli.recv())

peer.close()
cli.close()
srv.close()
```

---

## 26. 当前实现边界

这是语言与当前实现之间最值得记住的几件事。

### 26.1 普通控制流 block 不新建词法环境

只有：

- 函数调用
- `@(expr) { ... }`
- `@(expr) => expr`

会新建词法环境。

### 26.2 `@(expr)` 不能单独裸用

当前只支持：

- `@(expr) { ... }`
- `@(expr) => expr`

不支持单独写一个裸的 `@(expr)` 表达式。

### 26.3 `%snap` 的语义已经冻结

它表示“与原值后续修改隔离的独立快照”。
当前实现方式是 eager deep copy。

### 26.4 block-bodied lambda 还不是语法一等公民

当前 lambda 主要支持表达式 body：

```sl
x -> x + 1
```

如果你需要多语句逻辑，当前更稳的写法仍然是命名函数。

---

## 27. 推荐编程风格

### 27.1 把 `@` 留给“上下文”

不要把 `@` 当成别的语法糖去理解。

- `@x` 是外层词法
- `@` 是当前值上下文

### 27.2 把 `$` 留给“局部高阶表达式”

短高阶变换很适合 `$`：

```sl
filter($ > 0)
map($.name)
reduce($1 + $2, 0)
```

复杂逻辑就老老实实写命名函数。

### 27.3 `@x` 不要滥用

只有在“明确要碰外层状态”的时候才用它。

### 27.4 复杂上下文建议显式命名数据结构

如果 `@(expr)` 的嵌套太深，读者会失去方向。
一旦出现这种情况，宁可拆成几个中间变量。

---

## 28. 读完这份文档后，你应该知道什么

如果你已经理解这份文档，你应该已经能稳定写出：

1. 普通脚本
2. 列表/映射处理
3. 闭包与外层状态更新
4. 上下文块与占位符组合
5. 基础模块
6. 基础 TCP / Pipe / SHM 通信
7. `pcall` 风格的错误隔离

这也是 v3 这门语言当前真正的完成面。

---

## 29. 对后续 VM 重写的意义

这份文档不仅是“怎么用 v3”的教程，也应该被视为后续 VM 重写时的**行为契约**。

后续如果要从零重写：

- parser 可以重写
- runtime 可以重写
- object model 可以重写
- bytecode / VM 可以重写

但只要 v3 还叫 v3，这份文档里描述的用户可观察语义就不应该随便变化。

如果要变，就应该是 v4 的事。
