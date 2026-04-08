# Plan For VM Rewrite

这份文档是为**下一次从零开始**准备的。

目标不是在当前解释器上继续补洞，而是承认现状：

- 现有实现已经能跑
- 现有实现已经能验证 v3 语义
- 但实现层次混乱、耦合重、修补成本持续上升

因此，下一步应该是：

**以 v3 语义为契约，重新设计并实现一版真正可维护的 VM。**

这份文档的用途是：

1. 清空上下文后，下一轮可以直接按这里开工。
2. 明确“什么要保留，什么可以推翻重来”。
3. 把工程拆成可执行、可验收的阶段。

---

## 1. 为什么要重写 VM

当前源码的主要问题不是“有几个 bug”，而是结构性问题：

### 1.1 parser、语义糖、运行时耦合过深

当前 `@` / `$` 的实现里，存在大量“parse 期局部修正 + runtime 特判”的组合。

后果：

- 很难证明正确性
- 很难保证边界行为不回归
- 很难扩展新语法

### 1.2 AST 解释执行已经逼近维护极限

当前做法本质上是：

- 词法分析
- 直接生成 AST
- AST 上递归解释执行

这对早期原型合适，但对 v3 之后的复杂语义已经不够稳。

### 1.3 运行时错误模型不干净

不少错误目前仍依赖：

- 解释器层手写校验
- 宿主语言异常
- 即时 `die(...)`

需要更系统的：

- 运行时错误码 / 结构化异常
- 调试信息
- 源位置映射

### 1.4 值表示和对象语义不统一

当前 `Value` 承担了太多角色：

- 标量
- 容器
- 函数
- 引用
- 网络句柄
- 序列化形态

这使得：

- 生命周期管理难
- 类型分层不清晰
- VM 优化空间很差

### 1.5 当前代码已经不适合继续“边修边长”

这不是“再补一周 guard 就好”的问题。
继续修补的边际收益会越来越低。

---

## 2. 重写目标

下一版 VM 的目标不是“更炫”，而是“更稳、更清楚、更可扩展”。

必须满足：

1. 保持 v3 用户语义
2. 清晰分层
3. 可以系统性测试
4. 可以支持后续优化

更具体地说：

- 前端：词法、语法、降糖明确分层
- 中端：有规范化 IR 或字节码
- 后端：有显式 VM 执行模型
- 运行时：有统一 Value/Object 模型
- 错误：有稳定的 source span 与 stack trace 机制

---

## 3. 什么必须保留

### 3.1 必须保留的语言契约

以下内容是 v3 的用户语义契约，VM 重写不能破坏：

- `@x` 的外层词法查找语义
- bare `@` 的当前值上下文语义
- `@(expr) { ... }` 和 `@(expr) => expr`
- `$/$n` 的 placeholder 自动降糖语义
- `|>` 的数据流语义
- 当前已有的 list / string / map 方法语义
- `pcall` 的可捕获运行时错误行为
- `%copy/%deep/%snap/%net`
- `&ref`、`&tcp`、`&pipe`、`&shm`
- `import` 的相对路径与缓存语义

### 3.2 可以改变的内部实现

以下全部可以推翻重来：

- 当前 AST 结构
- 当前 parser 组织
- 当前 `Value` 内部实现
- 当前异常流实现
- 当前原生函数注册方式
- 当前序列化内部组织

---

## 4. 什么不应该在第一版 VM 里做

第一版 VM 不要贪多。

明确不做：

- JIT
- 类型推断优化
- 并行执行器
- 复杂 GC 优化
- 语言级类系统
- pattern matching
- 宏系统
- 协程 / async

第一版 VM 的目标是：

**把 v3 语义稳定、清晰、可维护地跑起来。**

---

## 5. 建议总体架构

推荐采用 5 层架构：

1. Lexer
2. Parser
3. Desugar / Normalize
4. Bytecode Compiler
5. VM Runtime

图示：

```text
source
  -> tokens
  -> raw AST
  -> normalized AST / HIR
  -> bytecode + constants + debug table
  -> VM
```

关键思想：

- 语法糖必须在进入 VM 前被消化
- VM 不应该理解“placeholder 是什么”
- VM 最好也不直接理解复杂的高层 sugar

---

## 6. 前端设计

### 6.1 Lexer

职责：

- token 化
- 记录精确 span
- 处理 UTF-8/BOM
- 统一 newline 规则

要求：

- token 必须带 `start/end/line/column`
- 所有错误都能定位到 span

### 6.2 Parser

职责：

- 构建 raw AST
- 只表达语法结构，不做 runtime 推断

建议 AST 节点分三层：

- stmt
- expr
- lvalue

不要再把“语法糖”“运行时概念”“宿主实现便利性”揉进一个节点里。

### 6.3 Desugar / Normalize

这是重写的关键层。

应该把这些东西集中处理：

- `$/$n` -> 显式 lambda
- `|>` -> 调用规范化
- `@(expr) => expr` -> 统一成 context frame + expr
- 其他语法糖

目标：

下游编译器面对的是“语义化后的普通结构”，而不是混杂语法糖。

---

## 7. 词法环境与上下文设计

v3 的语义里有两套环境：

1. 词法绑定环境
2. 当前值上下文环境

下一版 VM 里不应该继续把它们用零散 if-else 拼起来。

建议：

- 编译期显式区分 local / outer / global / context lookup
- 运行期有统一 frame 结构

建议的 frame 信息：

```text
Frame {
  code pointer
  instruction pointer
  base pointer
  closure env / upvalues
  current context value (optional)
}
```

这样：

- `x` 可以编译成 local / upvalue / global access
- `@x` 可以编译成 explicit outer binding access
- bare `@` 可以编译成 current-context access

---

## 8. Closure 与 Upvalue 设计

这是整次重写的核心之一。

当前 v3 必须支持：

- 闭包捕获
- 外层更新
- `@x`
- `&ref @x`

建议实现：

### 8.1 编译期

给每个函数分析：

- locals
- captured locals
- outer captured names

### 8.2 运行期

用显式 upvalue 对象表示捕获变量：

```text
Upvalue {
  open/closed
  slot pointer or closed value
}
```

这样可以系统支持：

- 闭包读写
- 多闭包共享捕获状态
- `@x`
- ref 指向 upvalue

这比当前“运行时到处 find parent env”稳定得多。

---

## 9. Value / Object 模型

建议把值分成两层：

### 9.1 立即值

- null
- bool
- small number

### 9.2 堆对象

- string
- list
- map
- closure
- native fn
- ref
- tcp handle
- pipe handle
- shm handle
- net-form

建议至少统一出一个 `Obj` 基类：

```text
Obj {
  type tag
  gc mark / header
}
```

这样未来无论做：

- 引用计数
- mark-sweep
- arena + handles

都不会从零返工。

---

## 10. Bytecode 设计建议

第一版不需要追求极致 compact，先求清楚。

可以先做 stack-based VM。

建议的指令类别：

### 10.1 常量与字面量

- `LOAD_CONST`
- `LOAD_NULL`
- `LOAD_TRUE`
- `LOAD_FALSE`

### 10.2 变量访问

- `LOAD_LOCAL`
- `STORE_LOCAL`
- `LOAD_UPVALUE`
- `STORE_UPVALUE`
- `LOAD_GLOBAL`
- `STORE_GLOBAL`
- `LOAD_OUTER`
- `STORE_OUTER`
- `LOAD_CONTEXT`

### 10.3 容器

- `BUILD_LIST`
- `BUILD_MAP`
- `GET_INDEX`
- `SET_INDEX`
- `GET_MEMBER`
- `SET_MEMBER`
- `SAFE_GET_INDEX`
- `SAFE_GET_MEMBER`

### 10.4 算术与逻辑

- `ADD`
- `SUB`
- `MUL`
- `DIV`
- `MOD`
- `NEG`
- `NOT`
- `EQ`
- `NEQ`
- `LT`
- `LE`
- `GT`
- `GE`

### 10.5 控制流

- `JUMP`
- `JUMP_IF_FALSE`
- `JUMP_IF_TRUE`
- `RETURN`
- `ENTER_SCOPE` / `LEAVE_SCOPE` 如果需要

### 10.6 函数与调用

- `MAKE_CLOSURE`
- `CALL`
- `CALL_METHOD`
- `LOAD_NATIVE`

### 10.7 上下文

- `PUSH_CONTEXT`
- `POP_CONTEXT`

### 10.8 异常

- `THROW`
- `ENTER_TRY`
- `LEAVE_TRY`

是否真的做 try opcodes，可以后置。
第一版也可以先保持 C++ 异常桥接，但 debug 信息要正确。

---

## 11. Native API 设计

当前原生函数是“塞 lambda 到解释器里”，非常难维护。

下一版建议：

### 11.1 原生函数接口统一

例如：

```text
NativeResult native_fn(VM& vm, int argc, Value* args)
```

### 11.2 原生方法接口统一

例如：

```text
NativeResult native_method(VM& vm, Value receiver, int argc, Value* args)
```

### 11.3 参数校验工具统一

必须把这类工具做成 runtime 公共设施：

- `need_arity`
- `need_string`
- `need_number`
- `need_non_negative_int`
- `need_list`
- `need_map`
- `need_callable`

原因很直接：

这一轮“无限炸”已经证明，如果这个东西不统一，最终一定会漏出宿主崩溃。

---

## 12. 错误模型

下一版 VM 必须有系统错误模型。

建议：

### 12.1 RuntimeError 结构

至少包含：

- message
- source span
- stack trace
- error kind

### 12.2 错误类别

建议区分：

- ParseError
- CompileError
- RuntimeError
- NativeError
- IOError / SystemError

### 12.3 `pcall`

`pcall` 不能只包“部分错误”。
它应该稳定地把运行时错误转成语言值。

---

## 13. 模块系统设计

模块系统建议单独成一层，不要再散在 runtime 各处。

需要的能力：

- 路径解析
- `.sl` 自动补全
- 相对脚本目录解析
- module cache
- circular import detection
- export filtering

建议 API：

```text
ModuleLoader {
  resolve(spec, importer)
  load(path)
  cache
  loading set
}
```

模块与 VM 的接口应该是：

- 编译脚本
- 生成 module closure / module chunk
- 执行
- 收集 exports

---

## 14. IPC / Handle 设计

这部分不要和 core VM 混在一起。

建议：

### 14.1 runtime core 只认识 handle object

VM 核心只知道：

- 它是对象
- 它有方法表

### 14.2 平台差异封装在 platform 层

分成：

- tcp
- pipe
- shm
- serialization

这样未来：

- Windows
- POSIX

才不会把核心执行层污染得更重。

---

## 15. 测试策略

下一轮从零重写，不能靠“写完再补测试”。

测试要分层：

### 15.1 Lexer tests

- token 序列
- span
- BOM
- newline

### 15.2 Parser tests

- 语法树结构
- 优先级
- `@`
- `$`
- pipe
- split

### 15.3 Desugar tests

这里非常关键。

必须单测：

- `$` -> lambda
- `$/$1/$2`
- 嵌套 HOF 参数位
- `|>` 规范化
- `@(expr)` 规范化

### 15.4 Runtime tests

- 闭包
- upvalue 共享
- `@x`
- bare `@`
- context nesting
- ref

### 15.5 Integration tests

沿用并扩展现有 `test/*.sl`。

### 15.6 Adversarial tests

保留这一轮已经证明有价值的测试思想：

- 错 arity
- 错类型
- 负数边界
- 空串边界
- 嵌套 placeholder
- 多行 pipe
- IPC 极限

---

## 16. 开发阶段建议

推荐按下面的顺序推进。

### Phase 0: 冻结行为契约

输入：

- `comprehensive_v3.md`
- `v3-concrete.md`
- 当前通过的测试集

输出：

- 一个清晰的“语言契约”

验收：

- 先不写 VM，只确认语言边界

### Phase 1: 新前端骨架

做：

- 新 lexer
- 新 parser
- 基础 AST

不做：

- VM

验收：

- 能稳定 parse 所有现有测试脚本

### Phase 2: 降糖与规范化层

做：

- `$` 自动降糖
- pipe 规范化
- context 规范化

验收：

- 所有“语法糖层”的单测通过

### Phase 3: 字节码编译器

做：

- 常量池
- 局部变量表
- upvalue 分析
- control flow lowering

验收：

- 能生成可调试的 bytecode dump

### Phase 4: 最小可用 VM

先支持：

- number/string/bool/null
- list/map
- function/call
- basic control flow

验收：

- `t01`~`t12` 这类基础语义测试通过

### Phase 5: Closure / Context / Ref

这是 v3 的核心阶段。

必须支持：

- closure
- `@x`
- bare `@`
- `@(expr)`
- `&ref`

验收：

- v3 context blast 全过

### Phase 6: Module / JSON / `%net`

验收：

- import tests
- json tests
- network serialization tests

### Phase 7: TCP / Pipe / SHM

验收：

- IPC full tests
- ad-hoc IPC blast

### Phase 8: Cleanup

做：

- debug dump
- stack trace
- better diagnostics
- performance cleanup

---

## 17. 第一版 VM 的验收标准

如果满足下面这些条件，可以认为“新的 VM 已经可以替代当前解释器”：

1. `comprehensive_v3.md` 中定义的核心语义不变
2. 所有现有 `test/*.sl` 全部通过
3. 所有 blast / guard 测试通过
4. 不再出现宿主崩溃级错误
5. 错误能给出稳定的位置与调用栈
6. parser / desugar / VM 分层清晰

---

## 18. 建议文件布局

建议新项目布局：

```text
src/
  frontend/
    token.h
    lexer.h
    parser.h
    ast.h
    desugar.h

  compiler/
    symbol_table.h
    bytecode.h
    compiler.h

  vm/
    value.h
    object.h
    vm.h
    frame.h
    upvalue.h
    builtins.h

  runtime/
    module_loader.h
    json.h
    serialize.h
    ref.h
    net/
      tcp.h
      pipe.h
      shm.h

  support/
    error.h
    span.h
    arena.h
```

这不是唯一方案，但比现在“几乎所有东西都塞在几个头文件里”强得多。

---

## 19. 下一轮真正开工时的规则

如果下一轮真的按这份计划从零开始，建议遵守以下规则：

1. 不要复用当前 parser/interpreter 的实现细节。
2. 只复用“行为契约”和测试。
3. 不要边修旧代码边写新 VM。
4. 先做前端与降糖，再做 VM。
5. 任何语义 bug 都先补测试，再修实现。
6. 任何宿主崩溃都必须视为 P0。

---

## 20. 最终建议

现在最合理的决策不是继续在当前源码上叠补丁，而是：

1. 把 v3 语言行为文档定住
2. 把现有测试定住
3. 清空上下文
4. 按本计划从零实现 VM

当前仓库已经足够告诉我们：

- v3 语言本身是可以成立的
- 但当前实现不值得继续作为长期基础

所以下一步的正确方向不是“再修”，而是“重建”。

这份文档就是下一轮重建时的起点。
