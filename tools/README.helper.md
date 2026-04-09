# SLCreator Runner Helper

这个 helper 是给 `SLCreator.sl` 的运行器面板用的本地桥接程序。

## 目标

让 `SLCreator` 具备这条学习闭环：

- 在编辑器里写 `ssiLang` 脚本
- 在 Runner 面板里填写 `stdin`
- 点击 `Run`
- 在面板里直接看到 `stdout / stderr`

## 为什么需要 helper

当前 `ssiLang` 的 `sys.spawn(argv, opts)` 只能管理进程生命周期：

- 启动
- 等待退出
- 判断是否还活着
- 终止

但它不能直接把子进程的标准输入输出挂到 GUI 面板里。

所以这里增加了一个很小的本地 helper，专门负责：

- 启动 `sl.exe`
- 写入 `stdin`
- 捕获 `stdout / stderr`
- 把结果发回 `SLCreator`

## 相关文件

- `tools/sl_runner_helper.cpp`
  - C++ helper
  - 负责真正执行 `sl.exe`
- `modules/slrunner.sl`
  - ssiLang 侧封装
  - 负责构建 helper、启动 helper、通过 Pipe 通信
- `SLCreator.sl`
  - IDE 外壳
  - 负责展示编辑器与 Runner 面板

## 通信方式

底层通信使用仓库现有的 `ssinet` framed `%net` 协议。

传输层使用 **命名 Pipe**。

## 这次为什么 Pipe 会更稳

这次专门避开了之前容易反复出问题的竞态：

- `slrunner.sl` 固定负责创建 **Pipe Server**
- `sl_runner_helper.cpp` 固定负责连接 **Pipe Client**
- helper 会等待 server 出现，再去连接

也就是说：

- 不再让两边都调用“既能建 server 又能当 client”的对称打开逻辑
- 不再让双方去“抢谁先建管道”

这就是这次的标杆点：**职责单向化，彻底消掉竞态源头**

## 工作流

1. `SLCreator.sl` 调用 `modules/slrunner.sl`
2. `slrunner.sl` 确保 `tools/sl_runner_helper.exe` 已存在
3. `slrunner.sl` 启动 helper
4. helper 作为 Pipe client 等待连接
5. `slrunner.sl` 创建 Pipe server 并完成连接
6. GUI 发送 `run` 请求
7. helper 执行目标脚本并抓取输出
8. helper 返回：
   - `ok`
   - `output`
   - `exit_code`

## 协议

### `run`

请求：

```text
{
  cmd: "run",
  exe: ".../sl.exe",
  script: ".../playground.sl",
  cwd: "...",
  input: "user stdin here\n"
}
```

响应：

```text
{
  ok: true,
  output: "...captured stdout/stderr...",
  exit_code: 0
}
```

失败时：

```text
{
  ok: false,
  error: "..."
}
```

### `stop`

请求：

```text
{cmd: "stop"}
```

响应：

```text
{ok: true}
```

## 当前取舍

当前实现是：

- 一次运行
- 一次返回完整输出
- 不是流式终端

这样做的优点：

- 实现短
- 行为稳定
- 很适合学习语言时跑小脚本

当前限制：

- 长时间运行脚本不会实时滚动输出
- 如果脚本等待更多 `input()`，而 Runner 没提供足够输入，脚本会阻塞

## 后续可扩展方向

- 升级成流式输出
- 增加“停止运行”按钮
- 区分 `stdout` / `stderr`
- 记录 Runner 历史
- 支持多个脚本页签
