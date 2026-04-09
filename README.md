# ssiLang 完整教程

(更新：我们打造了一个勉强不能用的IDE: SLCreator，以后还会迭代优化）

这份 `README.md` 是当前仓库唯一保留的主教程。

目标不是“简单列一下语法”，而是把这门语言从零讲到能稳定上手，尽量把以下问题一次讲透：

- 这门语言到底想解决什么问题
- `@`、`$`、`|>`、`%net`、`&ref` 分别是什么，为什么这样设计
- 普通作用域、闭包作用域、上下文作用域到底怎么区分
- 从 C++ 视角如何迁移，不再把旧习惯带错
- 三种通信能力怎么选，怎么用，什么情况下会踩坑
- `sSTL` 这个算法库到底能解决什么痛点

本文档内容以当前仓库中的源码和测试为准，而不是以历史文档为准。

## 1. 项目定位

`ssiLang` 是一门小型、动态、解释执行的脚本语言，适合这几类事情：

- 写小工具
- 写算法题脚本
- 做轻量数据处理
- 做模块化脚本组织
- 做轻量 IPC / 本地通信实验

它的风格不是“大而全”，而是“把一批高频动作做得非常顺手”。

当前版本里最重要的设计有两条：

- `@` 只管上下文和外层词法绑定
- `$` 只管匿名参数占位

这条分工是整门语言的核心心智模型。

## 2. 三十秒先看结果

先看一个最能体现语言风格的例子：

```sl
fn make_counter(start) {
    total = start
    fn inc(step) {
        @total = @total + step
        return @total
    }
    return inc
}

counter = make_counter(10)
print(counter(1))
print(counter(5))

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

你需要立刻看懂三件事：

- `@total` 表示“改外层作用域里的 `total`”
- `$.age` 表示“当前匿名参数的 `age` 字段”
- `|>` 表示“把左边结果喂给右边调用”

如果这三件事已经直觉成立，你会发现 ssiLang 的大部分代码都很顺。

## 3. 从 C++ 迁移时，先把脑子切换过来

很多误解都来自“拿 C++ 的默认心智模型硬套”。

下面这张表很重要。

| C++ 习惯 | ssiLang 对应理解 |
| --- | --- |
| 变量声明要写类型 | 没有类型声明，直接 `x = ...` |
| `{}` 一般意味着一个 block scope | 不是。普通 `if/while/for` 的 `{}` 不新建词法环境 |
| lambda capture 要显式写 | 不需要，闭包自动捕获外层环境 |
| 改闭包外层变量通常要靠引用或捕获列表 | 用 `@x` 直接访问或修改外层绑定 |
| 成员不存在通常是编译错误或异常 | map 上不存在的成员读取会得到 `null` |
| `std::vector` / `std::map` 是强类型容器 | list / map 是动态容器 |
| 异常通过 `try/catch` | 用 `pcall(fn)` 把运行时错误包成返回值 |
| 套接字、管道、共享内存是系统 API | 这里做成了一等对象和方法调用 |

如果你是 C++ 使用者，最需要记住的不是“ssiLang 像 Python 还是像 Lua”，而是下面这 7 条。

## 4. 先背这 7 条，不然后面会乱

### 4.1 `=` 永远只写当前词法环境

```sl
x = 1
```

这行代码的含义永远是：

- 如果当前环境没有 `x`，就创建它
- 如果当前环境已经有 `x`，就重绑它

它不会自动“向外层找一个同名变量顺手改掉”。

### 4.2 普通 `if/while/for` 的 block 不新建词法环境

这是当前实现最容易被误判的一点。

```sl
sum = 0
for i in range(4) {
    sum = sum + i
}
print(sum)   # 6
```

这里 `sum` 不是在新的 block 里被影子覆盖，而是在同一个环境里被更新。

所以：

- 普通控制流 block 是“语句分组”
- 不是“新的局部变量作用域”

### 4.3 `@x` 只查外层，不查当前层

```sl
fn outer() {
    x = 10
    fn inner() {
        @x = @x + 1
    }
}
```

`@x` 的意思不是“随便找个 x”。

它的规则非常明确：

- 从当前环境的父环境开始查
- 只查外层
- 找到最近的那个就停
- 找不到就报错

### 4.4 bare `@` 不是变量名，它是“当前上下文值”

```sl
@(user) {
    print(@.name)
    print(@.age)
}
```

这里的 bare `@` 不是“某个特殊标识符”，而是“当前上下文对象”。

### 4.5 `$` / `$1` / `$2` 不是运行时变量，而是语法糖

```sl
inc = $ + 1
add = $1 + $2
```

这两行会在解析阶段被降糖成普通 lambda。

所以 `$` 是“写函数的一种更短方式”，不是一个真的全局变量。

### 4.6 map 的 key 只能是字符串

合法：

```sl
m = {name: "Alice"}
print(m["name"])
```

非法：

```sl
m[1]
m[1] = 3
&ref m[1]
```

这些都是运行时错误。

### 4.7 安全访问不会吞掉类型错误

合法：

```sl
arr?[99]
m?["missing"]
```

不合法：

```sl
arr?["x"]
5?[0]
m?[1]
```

也就是说，安全访问只是“对本来就允许的那类访问，遇到空值 / 缺失 / 越界时返回 `null`”。
它不是“万能护身符”。

## 5. 快速开始

### 5.1 目录里有什么

当前仓库里最关键的目录和文件是：

```text
src/
  main.cpp
  lexer.h
  parser.h
  interp.h
  types.h

modules/
  sstl.sl

test/
  t01_basics.sl
  ...
  t12_v3_context.sl
  t23_ipc_full.sl
  t46_sstl.sl
  t47_sstl_algo.sl
```

### 5.2 直接运行

仓库根目录已经带了 `sl.exe`，你可以直接运行脚本：

```powershell
.\sl.exe test\t12_v3_context.sl
```

### 5.3 进入 REPL

```powershell
.\sl.exe
```

注意：

- 当前 REPL 是按“单行输入”工作的
- 适合试表达式、试变量、试短函数
- 不适合写复杂多行脚本

### 5.4 从源码构建

Windows / MinGW:

```powershell
g++ -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter -Isrc -o sl.exe src/main.cpp -lws2_32
```

如果你有 `make`，可以使用仓库里的 `Makefile`。

### 5.5 文件编码

脚本建议使用 UTF-8。

当前实现接受：

- UTF-8
- UTF-8 with BOM

## 6. 第一份脚本

新建一个 `hello.sl`：

```sl
name = "ssiLang"
nums = [1, 2, 3, 4, 5]

fn greet(user) => "hello, " + user

print(greet(name))
print(nums |> filter($ > 2) |> map($ * 10))
```

运行：

```powershell
.\sl.exe hello.sl
```

## 7. 基础语法

### 7.1 注释

只有单行注释：

```sl
# this is a comment
x = 42
```

### 7.2 语句结束

默认用换行结束语句，不需要分号：

```sl
x = 1
y = 2
print(x + y)
```

### 7.3 括号内可以换行

`()`, `[]`, `{}` 内部可以自由换行：

```sl
arr = [
    1,
    2,
    3
]
```

## 8. 数据类型总览

当前内建类型如下：

| 类型 | 例子 | `type(...)` 结果 |
| --- | --- | --- |
| number | `42`, `3.14` | `"number"` |
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

补充说明：

- `number` 底层统一是浮点数
- 没有单独的 `int` 类型
- `int(...)` 是转换，不是类型声明

## 9. 真值规则

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

## 10. 变量、绑定与作用域

### 10.1 普通绑定

没有 `let` / `var` / `const`：

```sl
x = 42
name = "Alice"
ok = true
```

### 10.2 什么时候会产生新的词法环境

当前实现里，真正新建词法环境的结构只有：

- 函数调用
- `@(expr) { ... }`
- `@(expr) => expr`

普通 `if` / `while` / `for` 的 block 不会新建词法环境。

### 10.3 给 C++ 用户的准确对照

在 C++ 里，你会自然把：

```cpp
if (...) {
    int x = 1;
}
```

理解成一个新的作用域。

在 ssiLang 里不要这样理解。

这里更接近“控制流块里的语句在当前环境执行”，而不是“自动引入新局部层级”。

## 11. 表达式与运算

### 11.1 算术

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
- `%` 也是数值运算

### 11.2 比较

```sl
1 < 2
3 >= 3
1 == 1
1 != 2
```

当前实现支持：

- number 的有序比较
- string 的有序比较

### 11.3 逻辑

```sl
true and false
true or false
not true
```

### 11.4 split 条件表达式

这是 ssiLang 比较有特色的表达式形式：

```sl
grade = 85
result = grade >= 90 => "A" || grade >= 80 => "B" || "C"
```

可以理解成“表达式版 if / else if / else”。

### 11.5 管道 `|>`

```sl
result =
    [1, 2, 3, 4, 5]
    |> filter($ > 2)
    |> map($ * 10)
    |> join(", ")
```

它的思路很简单：

- 左边先算出一个值
- 再把这个值当作右边调用的第一个参数

例如：

```sl
5 |> double
```

等价于：

```sl
double(5)
```

再例如：

```sl
xs |> map($ * 2)
```

等价于：

```sl
map(xs, $ * 2)
```

## 12. 安全访问

### 12.1 安全成员访问

```sl
m = {a: {b: 42}}
print(m?.a?.b)
print(m?.x?.y)
```

### 12.2 安全索引

```sl
arr = [1, 2, 3]
print(arr?[0])
print(arr?[99])
```

### 12.3 一定要知道的边界

安全访问只会处理这些情况：

- 基值是 `null`
- map 没有这个字段
- list / string 索引越界

它不会吞掉这些错误：

- list 索引类型错误
- string 索引类型错误
- map key 类型错误
- 对 number 做索引

所以：

```sl
arr?[99]      # null
m?["missing"] # null
```

但：

```sl
arr?["x"]   # 运行时错误
5?[0]       # 运行时错误
m?[1]       # 运行时错误
```

## 13. 函数

### 13.1 命名函数

```sl
fn add(a, b) => a + b

fn gcd(a, b) {
    while b != 0 {
        t = a % b
        a = b
        b = t
    }
    return a
}
```

### 13.2 lambda

```sl
double = x -> x * 2
mul = (a, b) -> a * b
ping = () -> "pong"
```

### 13.3 多语句逻辑请用 `fn`

当前 lambda 主体是“表达式体”。
如果你需要多条语句、循环、提前返回，推荐直接写命名函数。

### 13.4 闭包

函数会自动捕获定义时环境：

```sl
fn make_adder(base) {
    return x -> x + @base
}
```

## 14. v3 的核心一：`@`

这一章最重要。

### 14.1 `@x`：访问外层词法绑定

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

这里：

- `total` 是外层函数里的局部变量
- `inc` 是闭包
- `@total` 明确表示“去外层拿那个 `total`”

### 14.2 bare `@`：当前上下文值

```sl
@(user) {
    print(@.name)
    print(@.age)
}
```

bare `@` 是普通值，所以它后面可以继续接：

- `@.field`
- `@[index]`
- `@?.field`
- `@?[index]`

### 14.3 `@(expr) { ... }`：上下文块

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

它的语义是：

1. 先计算 `expr`
2. 新建一个子词法环境
3. 把 `expr` 的结果挂成这个子环境的当前上下文
4. 在这个子环境执行 block

这里一定要把两个概念分开：

- bare `@` 访问当前上下文值
- `@x` 访问外层词法绑定

### 14.4 `@(expr) => expr`：上下文表达式

```sl
label = @(user) => @.name + "#" + str(@.age)
```

它会立刻执行，不是 lambda。

这是一个非常容易误判的点。

错误理解：

- “看起来有个 `=>`，是不是就是函数？”

正确理解：

- 这是“把 `user` 设为上下文后，马上算一个表达式”

### 14.5 上下文嵌套

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

- 内层 bare `@` 指向 `request.user`
- `@request_id` 指向外层词法绑定

### 14.6 `@(expr)` 不能单独裸写

当前语法只允许：

- `@(expr) { ... }`
- `@(expr) => expr`

不允许单独写一个裸的 `@(expr)`。

## 15. v3 的核心二：`$`

### 15.1 bare `$`

`$` 就是 `$1`。

```sl
inc = $ + 1
```

等价理解：

```sl
inc = x -> x + 1
```

### 15.2 `$n`

```sl
add = $1 + $2
```

等价理解：

```sl
add = (a, b) -> a + b
```

### 15.3 自动降糖发生在“表达式边界”

这是最关键的理解点。

```sl
len($.notes)
```

它不是：

```sl
len(x -> x.notes)
```

而是：

```sl
x -> len(x.notes)
```

也就是说，包含 `$` 的整个当前表达式会一起降糖成 lambda。

### 15.4 最常见的用途

```sl
nums |> map($ * 2)
nums |> filter($ > 0)
nums |> reduce($1 + $2, 0)
users |> map($.name)
users |> find($.id == 42)
```

### 15.5 `@` 和 `$` 可以同时出现

```sl
fn scale_by(factor) {
    return $ * @factor
}
```

这里：

- `$` 是匿名参数
- `@factor` 是外层词法绑定

两者完全不冲突。

## 16. 容器：list / map / string

### 16.1 list

创建：

```sl
arr = [1, 2, 3]
```

常见操作：

```sl
print(arr[0])
arr.push(4)
print(arr.pop())
print(arr.slice(1, 3))
print(arr.map(x -> x * 2))
print(arr.filter(x -> x % 2 == 0))
print(arr.reduce((acc, x) -> acc + x, 0))
print(arr.find(x -> x > 2))
print(arr.contains(2))
print(arr.join("-"))
print(arr.reverse())
```

方法一览：

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

注意点：

- `pop()` 对空 list 会报错
- `slice(start, end)` 是截取区间，不是 C++ 的迭代器语义
- list 索引必须是整数值 number

### 16.2 map

创建：

```sl
m = {name: "Alice", age: 30}
```

读：

```sl
print(m.name)
print(m["age"])
```

写：

```sl
m.city = "NYC"
m["score"] = 95
```

方法一览：

- `keys()`
- `values()`
- `has(key)`
- `remove(key)`

注意点：

- key 只能是字符串
- `m.name` 和 `m["name"]` 都可以
- `m[1]` 是运行时错误
- 读取不存在字段时返回 `null`

这个“缺字段返回 `null`”非常重要，因为它和很多静态语言不同。

例如：

```sl
m = {a: 1}
print(m.missing)   # null
```

### 16.3 map 中存函数

如果 map 某个字段本身是函数，可以按方法风格调用：

```sl
obj = {
    hello: name -> "hi " + name
}

print(obj.hello("ssi"))
```

### 16.4 string

```sl
s = "hello world"
print(s[0])
print(s.trim())
print(s.split(" "))
print(s.chars())
print(s.starts_with("he"))
print(s.ends_with("ld"))
print(s.contains("lo"))
print(s.replace("world", "ssiLang"))
```

方法一览：

- `split(sep)`
- `trim()`
- `chars()`
- `starts_with(prefix)`
- `ends_with(suffix)`
- `contains(part)`
- `replace(old, new)`

注意点：

- string 索引必须是整数值 number
- `split("")` 会报错
- `replace("", "x")` 会报错

## 17. 控制流

### 17.1 `if / else`

```sl
if x > 0 {
    print("positive")
} else {
    print("non-positive")
}
```

也支持 `=>` 的短体：

```sl
if x > 0 => print("positive")
```

### 17.2 `while`

```sl
i = 0
while i < 5 {
    print(i)
    i = i + 1
}
```

### 17.3 `for ... in`

```sl
for x in [10, 20, 30] {
    print(x)
}
```

实践里通常遍历：

- list
- `range(...)` 的结果

### 17.4 `break / continue / return`

```sl
for x in range(10) {
    if x == 3 => continue
    if x == 7 => break
    print(x)
}
```

## 18. 内建函数

### 18.1 基础

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

说明：

- `debug(...)` / `probe(...)` 会打印带类型的内容
- 它们还会返回第一个参数，适合调试流水线

### 18.2 数值 / 通用工具

- `range(end)`
- `range(start, end)`
- `range(start, end, step)`
- `abs(x)`
- `min(a, b)`
- `max(a, b)`
- `sort(list)`

### 18.3 为管道准备的全局包装

- `map(list, fn)`
- `filter(list, fn)`
- `reduce(list, fn, init)`
- `find(list, fn)`
- `join(list, sep)`
- `reverse(list)`
- `push(list, value)`

这些存在的主要意义就是配合 `|>` 使用。

### 18.4 JSON / 模块

- `import(path)`
- `json_encode(value)`
- `json_parse(text)`

### 18.5 网络

- `tcp_listen(addr, backlog?)`

## 19. 错误处理：`error` 与 `pcall`

### 19.1 主动报错

```sl
error("boom")
```

### 19.2 捕获运行时错误

```sl
fn risky() {
    error("boom")
}

r = pcall(risky)
print(r.ok)
print(r.error)
```

成功时：

```sl
{ok: true, value: ...}
```

失败时：

```sl
{ok: false, error: "..."}
```

### 19.3 C++ 用户最容易写错的一种姿势

如果你要捕获“调用某函数时”的错误，请传函数，不要传结果。

正确：

```sl
r = pcall(() -> dangerous(x))
```

错误：

```sl
r = pcall(dangerous(x))
```

第二种写法会先执行 `dangerous(x)`，根本来不及保护。

## 20. `%copy` / `%deep` / `%snap` / `%net`

这是另一组非常有用的前缀语法。

### 20.1 `%copy`

浅拷贝 list / map：

```sl
clone = %copy data
```

如果内部还有嵌套容器，里层是共享的。

### 20.2 `%deep`

深拷贝 list / map：

```sl
deep = %deep data
```

### 20.3 `%snap`

语义上表示“独立快照”：

```sl
snap = %snap data
```

当前实现里它等价于 eager deep copy，但你在使用时应该把它理解成：

- 我需要一份后续修改互不影响的快照

### 20.4 `%net`

把普通值编码成 `net-form`，供通信对象传输：

```sl
packet = %net {kind: "ping", id: 42}
print(type(packet))
```

可序列化值包括：

- number
- string
- bool
- null
- list
- map

如果你试图对函数、引用、句柄做 `%net`，会失败。

## 21. 引用系统：`&ref`

在 v3 里，`@x` 已经解决了“闭包里改外层变量”的需求，所以 `&ref` 的定位变成了：

- 真正的一等引用值
- 可以传递、存储、调用方法

### 21.1 基础例子

```sl
arr = [10, 20, 30]
r = &ref arr[1]
print(r.deref())
r.set(99)
print(arr)
```

### 21.2 引用外层绑定

```sl
fn make_box(v) {
    value = v
    fn expose(dummy) => &ref @value
    return {expose: expose}
}
```

### 21.3 支持的引用目标

- `&ref x`
- `&ref @x`
- `&ref arr[i]`
- `&ref m.key`

### 21.4 ref 方法

- `deref()`
- `set(v)`

## 22. 模块系统：`import`

### 22.1 基本导入

```sl
math = import("modules/math")
print(math.add(2, 3))
```

### 22.2 路径规则

相对路径是相对于“当前脚本文件所在目录”解析的，不是相对于你启动进程时的工作目录。

这一点非常重要。

也就是说：

- 在仓库根目录执行 `test\t30_import.sl`
- 里面 `import("modules/math")`
- 实际会从 `test/` 目录相对解析

### 22.3 自动补 `.sl`

```sl
import("modules/math")
import("modules/math.sl")
```

这两种写法都可以。

### 22.4 模块缓存

同一路径的模块只加载一次，后续会走缓存。

### 22.5 循环导入

会报错，可以用 `pcall` 捕获：

```sl
wrap = pcall(() -> import("modules/cycle_a.sl"))
print(wrap.ok)
```

### 22.6 导出规则

模块环境里以下划线开头的绑定不会导出。

例如模块里有：

```sl
_hidden = "secret"
```

导入方：

```sl
math = import("modules/math")
print(math._hidden)
```

输出会是 `null`，原因不是“成功导出了 `_hidden`”，而是：

1. `_hidden` 没被导出
2. 导入结果本身是一个 map
3. 读取 map 上不存在字段时返回 `null`

这个细节非常容易误判。

### 22.7 每个脚本 / 模块自带的环境变量

当前实现会注入：

- `__file`
- `__dir`

它们分别表示当前脚本的绝对路径和所在目录。

## 23. JSON

### 23.1 编码

```sl
text = json_encode({
    name: "Alice",
    scores: [90, 85]
})
print(text)
```

### 23.2 解码

```sl
obj = json_parse(text)
print(obj.name)
print(obj.scores[0])
```

这一块在通信和测试里经常一起使用。

## 24. 三种通信 / 共享能力

你要求“完整介绍三种通信”，这里按实际能力拆成：

1. TCP
2. Pipe
3. Shared Memory

另外请记住：

- 真正传数据时通常要先转成 `%net`
- 这三种能力定位不同，不是“功能重复的三套 API”

### 24.1 什么时候选哪一种

先给一个决策表：

| 场景 | 建议 |
| --- | --- |
| 需要跨进程、跨机器、客户端/服务端模型 | TCP |
| 本机轻量消息流，尤其匿名通道 | Pipe |
| 本机共享一份可反复覆盖读取的状态槽 | SHM |

### 24.2 TCP

客户端通过 `&tcp(...)` 创建，服务端通过 `tcp_listen(...)` 创建。

#### 基础例子

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

#### TCP client 方法

- `send(net_form)`
- `recv()`
- `send_raw(text)`
- `recv_raw(n)`
- `peer_addr()`
- `set_timeout(seconds)`
- `close()`

#### TCP server 方法

- `accept()`
- `addr()`
- `close()`

#### 使用建议

- 传结构化数据优先用 `send(%net ...)` / `recv()`
- 只在你明确需要原始字节文本时用 `send_raw` / `recv_raw`
- `recv_raw(n)` 里的 `n` 必须是非负 number

### 24.3 Pipe

Pipe 适合本机消息流。

#### 匿名 pipe

```sl
p = &pipe()
p.send(%net [1, 2, 3])
print(p.recv())
```

#### 命名 pipe

```sl
p = &pipe("channel_name")
```

这个构造路径存在，但完整双端协作通常需要多进程配合。

#### Pipe 方法

- `send(net_form)`
- `recv()`
- `is_named()`
- `name()`
- `flush()`
- `close()`

#### 什么时候更适合 pipe

- 只在本机
- 想用消息流而不是共享状态
- 不想引入 TCP 地址和监听管理

### 24.4 Shared Memory：`&shm`

SHM 适合“共享一个状态槽位”。

#### 基础例子

```sl
slot = &shm("sl_test_basic")
slot.store(%net {x: 10, y: 20})
print(slot.load())
slot.close()
```

#### 自定义大小

```sl
slot = &shm("ipc_test_sz", 8192)
```

#### SHM 方法

- `store(net_form)`
- `load()`
- `size()`
- `name()`
- `clear()`
- `close()`

#### SHM 的正确心智模型

把它理解成“一个命名的共享数据槽”会最准确：

- `store(...)` 覆盖写入
- `load()` 读取当前内容
- `clear()` 清空

它不是队列，不会帮你保留消息历史。

## 25. 系统能力：`sys`

当前实现已经提供了一个全局 `sys` 对象，用来做最小可用的系统交互。

它的定位不是“把所有 OS API 全塞进语言”，而是解决两类高频问题：

- 文件和目录管理
- 主动启动 helper 进程，再通过现有 `%net + tcp/pipe/shm` 通信

### 25.1 `sys` 里现在有什么

文件和目录：

- `sys.cwd()`
- `sys.chdir(path)`
- `sys.exists(path)`
- `sys.is_file(path)`
- `sys.is_dir(path)`
- `sys.mkdir(path, recursive?)`
- `sys.listdir(path?)`
- `sys.read_text(path)`
- `sys.write_text(path, text)`
- `sys.append_text(path, text)`
- `sys.remove(path, recursive?)`
- `sys.rename(src, dst)`

进程和时序：

- `sys.spawn(argv, opts?)`
- `sys.sleep(seconds)`

`sys.spawn(...)` 返回 `link-proc` 进程句柄，支持方法：

- `pid()`
- `wait(timeout_seconds?)`
- `is_alive()`
- `kill()`
- `terminate()`

### 25.2 最常见的 helper 模式

这是推荐工作流：

1. 用 `sys.spawn(...)` 启动 Python / C++ helper
2. 把 pipe 名或 TCP 地址作为参数传给 helper
3. ssiLang 侧用 `&pipe(...)` 或 `&tcp(...)` 建链
4. 用 `%net` 传结构化数据

例子：

```sl
pipe_name = "sl_sys_helper_pipe"
p = sys.spawn(["python", "tools/sys_helper.py", pipe_name])

ch = &pipe(pipe_name)
ch.send(%net {cmd: "echo", text: "hello-helper"})
resp = ch.recv()
print(resp.ok)
print(resp.text)

ch.send(%net {cmd: "stop"})
print(ch.recv().ok)
print(p.wait(5))
```

### 25.3 `spawn` 参数格式

第一参数必须是字符串列表：

```sl
p = sys.spawn(["python", "tools/sys_helper.py", "sl_sys_helper_pipe"])
```

可选第二参数目前支持：

- `cwd: "path"`
- `detached: true/false`

例如：

```sl
p = sys.spawn(
    ["python", "tools/sys_helper.py", "sl_sys_helper_pipe"],
    {cwd: ".", detached: false}
)
```

### 25.4 `wait(timeout?)` 语义

- 不传参数：一直等到进程退出，返回 exit code
- 传正数秒：超时返回 `null`
- 进程已退出后再次 `wait()`：返回缓存的 exit code

### 25.5 回归脚本

和 `sys` 相关的完整链路测试在：

```text
test/t50_sys.sl
tools/sys_helper.py
```

直接运行：

```powershell
.\sl.exe test\t50_sys.sl
```

## 26. C++ / Python 对接文件

为了让外部进程直接和 `ssiLang` 做基于 `%net` 的通信，仓库里现在提供了两套对接实现。

### 25.1 C++ 版

仓库里新增了一组 C++ 头文件：

```text
tools/
  ssinet.hpp
  ssinet_selftest.cpp
  ssinet/
    value.hpp
    codec.hpp
    transport.hpp
    shm.hpp
```

推荐入口：

```cpp
#include "tools/ssinet.hpp"
```

这组头文件严格对齐当前解释器实现，协议规则如下：

- `%net` 负载格式是文本协议，开头固定为 `V1\n`
- 可传输类型只有 `number / string / bool / null / list / map`
- TCP 和 pipe 传输时，外层使用 4 字节小端长度前缀
- SHM 布局是 `4 字节锁 + 4 字节长度 + payload`
- map 保持插入顺序，编码时按当前顺序输出

头文件提供的核心能力：

- `Value`：C++ 侧的 `%net` 值对象
- `encode_payload` / `decode_payload`：纯编解码
- `TcpSocket` / `TcpServer`：按 ssiLang 协议收发 framed `%net`
- `PipeChannel`：匿名 pipe / 命名 pipe 的 framed `%net`
- `SharedMemory`：按 ssiLang SHM 布局读写 `%net`

自测源码：

```text
tools/ssinet_selftest.cpp
```

Windows / MinGW 编译示例：

```powershell
g++ -std=c++17 -O2 -Wall -Wextra -Itools -o tools\ssinet_selftest.exe tools\ssinet_selftest.cpp -lws2_32
.\tools\ssinet_selftest.exe
```

这份自测会覆盖：

- `%net` 编解码回环
- TCP 回环
- 匿名 pipe 回环
- SHM 存取与清空

### 25.2 Python 版

同时也提供了 Python 版本，目录如下：

```text
tools/
  ssinet_py_selftest.py
  __init__.py
  ssinet_py/
    __init__.py
    value.py
    codec.py
    transport.py
    shm.py
```

推荐入口：

```python
from tools.ssinet_py import (
    encode_payload,
    decode_payload,
    TcpSocket,
    TcpServer,
    PipeChannel,
    SharedMemory,
)
```

如果你只是临时把 `tools/` 加到 `PYTHONPATH`，也可以：

```python
from ssinet_py import TcpSocket, SharedMemory
```

Python 版和 C++ 版能力对称：

- 原生 Python 值 `dict / list / str / int / float / bool / None` 直接映射 `%net`
- `encode_payload` / `decode_payload` 负责 `%net` 编解码
- `TcpSocket` / `TcpServer` 负责 framed `%net` over TCP
- `PipeChannel` 负责 framed `%net` over pipe
- `SharedMemory` 负责按 `ssiLang` 的 SHM 布局读写 `%net`

协议层完全对齐当前解释器：

- `%net` payload 头部是 `V1\n`
- TCP / pipe 外层是 4 字节小端长度前缀
- SHM 布局是 `4 字节锁 + 4 字节长度 + payload`

Python 自测入口：

```text
tools/ssinet_py_selftest.py
```

运行方式示例：

```powershell
python tools\ssinet_py_selftest.py
```

注意：

- 我当前这个环境里没有可用的 Python 解释器，所以 Python 版没有现场实跑
- 代码是按当前 `ssiLang` 实现反推协议后写的
- C++ 版已经本地编译并跑通

## 27. `sSTL`：算法导向的小标准库

`sSTL` 位于：

```text
modules/sstl.sl
```

它不是想模仿完整 STL，而是想解决算法脚本里最烦的几类重复劳动：

- 重复初始化数组 / 二维数组
- 嵌套 list / map 初始化容易写错共享引用
- 队列 / 堆 / 图的样板代码太多
- 由于 map key 只能是字符串，手写 set / counter 很烦

### 26.1 导入

如果你的脚本在仓库根目录：

```sl
S = import("modules/sstl")
```

如果你的脚本在 `test/` 目录：

```sl
S = import("../modules/sstl")
```

### 26.2 最重要的设计目标

它尽量遵守三条原则：

1. 尽量返回普通 ssiLang 值
2. 少量高频组件优先，而不是做一堆“看起来全但不好用”的杂项 API
3. 嵌套容器默认给你独立副本，避免二维数组共享同一行这种坑

### 26.3 初始化工具

#### `fill(n, value)`

构造长度为 `n` 的 list，每个元素都是 `value` 的独立副本。

```sl
rows = S.fill(3, [0, 0])
rows[0][0] = 7
print(rows)
```

#### `tabulate(n, make)`

按索引生成：

```sl
sq = S.tabulate(5, i -> i * i)
```

#### `grid(rows, cols, value)`

创建二维表：

```sl
dp = S.grid(3, 4, 0)
```

#### `grid_with(rows, cols, make)`

按坐标生成二维表：

```sl
diag = S.grid_with(4, 4, (i, j) -> i == j => 1 || 0)
```

### 26.4 复杂嵌套结构构造器

这是 `sSTL` 的重头戏之一。

支持的规格构造器：

- `val(v)`
- `delay(make)`
- `seq(n, item_spec)`
- `dict(keys, item_spec)`
- `fields(entries)`
- `build(spec)`

#### 一个典型例子

```sl
nested = S.build(
    S.fields({
        matrix: S.seq(3, S.seq(3, S.delay(path -> path[1] == path[2] => 1 || 0))),
        buckets: S.dict(["lo", "hi"], S.seq(2, 0))
    })
)
```

这非常适合初始化：

- `list<list<int>>`
- `map<list<int>>`
- `list<map<list<int>>>`
- 混合字段对象

### 26.5 set / counter

`ssiLang` 没有原生 set，所以 `sSTL` 在 map 之上做了轻量封装。

#### `set()`

```sl
s = S.set()
s.add("a")
s.add("b")
print(s.contains("a"))
print(s.members())
```

方法：

- `add(v)`
- `contains(v)`
- `erase(v)`
- `size()`
- `empty()`
- `members()`
- `raw()`

#### `set_of(xs)`

```sl
seen = S.set_of(["b", "a", "b", "c"])
```

#### `counter()`

```sl
c = S.counter()
c.add("a", 1)
c.add("a", 1)
c.add("b", 3)
print(c.get("a"))
```

方法：

- `add(k, delta?)`
- `get(k)`
- `contains(k)`
- `erase(k)`
- `size()`
- `raw()`

#### `count_all(xs)`

```sl
freq = S.count_all(["a", "b", "a", "c", "a"])
```

### 26.6 queue

```sl
q = S.queue()
q.push(10)
q.push(20)
print(q.peek())
print(q.pop())
print(q.empty())
```

方法：

- `push(v)`
- `pop()`
- `peek()`
- `size()`
- `empty()`
- `items()`

它内部是“数组 + head 指针 + 必要时压缩”的实现，适合 BFS。

### 26.7 heap

#### 自定义优先级

```sl
h = S.heap_by((a, b) -> a[1] < b[1])
```

`less(a, b)` 返回 `true` 表示 `a` 优先级更高。

方法：

- `push(v)`
- `pop()`
- `top()`
- `size()`
- `empty()`
- `items()`

#### 便捷版本

- `min_heap()`
- `max_heap()`

### 26.8 graph

#### 通用图

```sl
g = S.graph(5, true)
```

#### 便捷版本

- `digraph(n)`
- `undir_graph(n)`

#### 方法

- `add_edge(u, v, w?)`
- `neighbors(u)`
- `size()`
- `data()`
- `directed`

### 26.9 `sSTL` 实战例子

#### BFS

```sl
g = S.undir_graph(6)
g.add_edge(0, 1, 1)
g.add_edge(0, 2, 1)
g.add_edge(1, 3, 1)
g.add_edge(2, 4, 1)
g.add_edge(4, 5, 1)

dist = S.fill(6, -1)
q = S.queue()
dist[0] = 0
q.push(0)

while not q.empty() {
    u = q.pop()
    for edge in g.neighbors(u) {
        v = edge[0]
        if dist[v] == -1 {
            dist[v] = dist[u] + 1
            q.push(v)
        }
    }
}
```

#### Dijkstra 前沿

```sl
pq = S.heap_by((a, b) -> a[0] < b[0])
pq.push([0, 0])   # [dist, node]
```

## 28. 给 C++ 用户的推荐学习路线

如果你本来主要写 C++，建议按这个顺序学，不要一上来就看 IPC。

### 第一步：先把语言最小核心跑熟

先读并运行：

- `test\t01_basics.sl`
- `test\t02_control.sl`
- `test\t03_functions.sl`
- `test\t04_containers.sl`
- `test\t05_expressions.sl`
- `test\t07_builtins.sl`

你要达到的目标：

- 能自然写 list / map / string
- 知道换行就是语句结束
- 知道 split 表达式和管道

### 第二步：把 `@` / `$` 真正吃透

重点看：

- `test\t12_v3_context.sl`
- `test\t40_v3_context_blast.sl`
- `test\t41_v3_graph_blast.sl`

你要达到的目标：

- 看见 `@x` 不会再把它误当“引用语法”
- 看见 `$` 不会再把它误当“变量”

### 第三步：再学模块和错误处理

重点看：

- `test\t30_import.sl`
- `test\t31_zero_arity_lambda.sl`

你要达到的目标：

- 知道 `import` 相对谁解析
- 知道 `pcall` 怎么包住风险调用

### 第四步：最后再进通信和算法库

重点看：

- `test\t23_ipc_full.sl`
- `test\t46_sstl.sl`
- `test\t47_sstl_algo.sl`

你要达到的目标：

- 能判断什么时候用 TCP / Pipe / SHM
- 能用 `sSTL` 省掉算法题里的样板代码

## 29. 最常见的易错点

这一章建议直接收藏。

### 27.1 误以为普通 block 会新建作用域

不会。

### 27.2 误以为 `@x` 能访问当前层

不能。
`@x` 从父环境开始找。

### 27.3 误以为 `@(expr) => expr` 是 lambda

不是。
它是立即求值的上下文表达式。

### 27.4 误以为 `@(expr)` 可以单独写

不能。

### 27.5 误以为 `$` 是运行时变量

不是。
它是 placeholder 语法糖。

### 27.6 误以为安全访问能吞掉所有错误

不能。
它不会吞掉类型错误。

### 27.7 误以为 map 可以用数字 key

不能。
map key 必须是字符串。

### 27.8 误以为 `mod._hidden` 是真的导出了私有字段

不是。
那只是“map 上缺字段返回 `null`”。

### 27.9 误以为 `%copy` 是深拷贝

不是。
它是浅拷贝。

### 27.10 误以为 SHM 是消息队列

不是。
它更像共享槽位。

### 27.11 误以为 REPL 能像完整脚本那样舒服写多行程序

不行。
当前 REPL 主要适合单行试验。

### 27.12 误以为 `pcall(expr)` 能保护 `expr`

不行。
`pcall` 需要的是函数值。

正确写法：

```sl
pcall(() -> risky())
```

### 27.13 误以为 split 条件表达式可以随意断行

不建议这样写：
```sl
color =
    hover => "red" ||
    "blue"
```

当前实现里这种写法很容易在换行处触发 `Unexpected '\n'`。
更稳妥的写法是：
```sl
color = hover => "red" || "blue"
```

### 27.14 误以为顶层函数里也能随便用 `@x` 改全局

`@x` 的语义一直是“从父词法环境开始向外找”，不是“全局变量快捷写法”。
如果当前层本来就没有更外层绑定，`@x` 就不是你想象中的“顺手改全局”。

实战建议：
- 顶层同一层里的普通读写，直接写 `x = ...`
- 只有在闭包里明确要改外层绑定时，再用 `@x`

### 27.15 误以为模块名和同名目录可以无歧义共存

如果同时存在：
```text
modules/wingui.sl
modules/wingui/
```

那就不要偷懒写：
```sl
import("modules/wingui")
```

更稳妥的写法是显式写文件名：
```sl
import("modules/wingui.sl")
```

### 27.16 误以为 Windows 上 C++ helper 只要 `-lgdi32` 就够了

如果 helper 通过仓库里的 `tools/ssinet.hpp` 走 TCP / socket 通信，
Windows 下链接时还要带上：
```powershell
-lws2_32
```

否则会出现 `WSAGetLastError` 之类的链接错误。

### 27.17 误以为 `sys.spawn(...)` 启动 helper 之后，通信端一定已经立刻就绪

不是。
helper 进程启动成功，只说明进程起来了，不说明你的 pipe / TCP 监听端已经完成初始化。

实战建议：
- helper 侧先把通信端建好
- ssiLang 侧连接时做好短时间重试
- 不要把“spawn 成功”和“通信链路已就绪”混为一谈

### 27.18 误以为 `WM_PAINT` 应该承担实际绘制

在当前 `wingui` / `sWidgets` 这套链路里，更稳的约定是：

- `WM_PAINT` 只做冷处理：`BeginPaint(...)` / `EndPaint(...)`，把系统的无效区确认掉
- 真正的场景绘制只放在“程序自己决定要刷新”的那条路径里
- 当前实现里，这条路径落在 helper 内部的渲染 `WM_TIMER`

也就是说：

- 脚本侧状态变化时，先 `present(...)`
- helper 侧只把最新 scene 暂存，并挂一次内部渲染 timer
- timer 到点后才真正 `render_scene(...)`
- `paint` 事件本身只保留为通知语义，不要在里面递归触发新的提交

这样做的好处是：

- 避免 `present(...) -> InvalidateRect(...) -> WM_PAINT -> 再次提交` 这种环路
- 避免拖拽、resize、遮挡恢复时把系统 `paint` 洪峰直接变成真实绘制洪峰
- 让“何时画”只由程序自己的重绘策略决定，而不是被 `WM_PAINT` 牵着走

### 27.19 误以为模块文件最外层也可以直接 `return`

当前模块系统的工作方式是：

1. `import(path)` 先执行整个模块文件
2. 然后把模块环境里的顶层绑定导出成 map

所以模块文件最外层并不是一个会自动承接 `return` 的函数体。

不要这样写：

```sl
fn start() {
    return {ok: true}
}

return {
    start: start
}
```

这类“文件级 `return`”在某些调用路径下会把解释器直接从模块执行流程里打出去。

更稳妥的写法是：

```sl
fn start() {
    return {ok: true}
}
```

然后让 `import(...)` 自动导出顶层绑定。

也就是说：

- 函数体里的 `return` 正常使用
- 模块文件最外层当前不要写 `return`

如果你只是想诊断某个模块会不会导入失败，可以临时写成：

```sl
wrap = pcall(() -> import("modules/demo.sl"))
print(wrap.ok)
```

但这只是排错手段，不是推荐的建库方式。

## 30. 推荐测试命令

下面这些脚本最适合作为回归入口：

```powershell
.\sl.exe test\t12_v3_context.sl
.\sl.exe test\t30_import.sl
.\sl.exe test\t23_ipc_full.sl
.\sl.exe test\t46_sstl.sl
.\sl.exe test\t47_sstl_algo.sl
```

如果你想从小到大逐步跑，也可以顺着 `test/` 目录编号走。

## 31. 这份 README 覆盖了什么

如果你认真读完并亲手跑过上面的例子和测试，你应该已经能稳定完成这些事情：

- 写普通脚本
- 写带 list / map / string 处理的程序
- 正确使用 `@`、`$`、`|>`
- 正确理解作用域和上下文
- 写模块并组织工程
- 用 `pcall` 做错误隔离
- 用 `%net` + TCP / Pipe / SHM 传数据
- 用 `sSTL` 搭出 BFS / 堆 / 图 / 频率表 / 二维 DP

这就已经覆盖了当前项目真正完成的主体能力。

## 32. 最后一句话：怎么用这门语言才最顺

把 ssiLang 想成下面这个组合最准确：

- 用 `fn` 写清楚主要逻辑
- 用 `$` 缩短局部高阶变换
- 用 `@x` 明确表达“我要碰外层状态”
- 用 bare `@` 处理当前上下文对象
- 用 `|>` 让数据流更顺
- 用 `%net` 把可传输数据和普通值分开
- 用 `sSTL` 解决算法脚本里的重复劳动

只要这套心智模型立住了，这门语言会非常顺手。
