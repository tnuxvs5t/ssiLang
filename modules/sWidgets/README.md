# sWidgets

`modules/sWidgets/core.sl` 是基于 `wingui` 的纯 ssiLang GUI 组件层。

定位很明确：

- 纯绘制、纯消息驱动
- 不封装 Win32 原生控件
- widget 本质是普通 map，对外暴露一组协议方法
- 布局和事件都在脚本侧完成
- 可以继续在其上实现更高层的纯 sL 组件系统

本 README 只以当前仓库中的实际代码为准，目标是把现在已经存在的 API 和行为写清楚，不写“计划中功能”。

---

## 1. 依赖与入口

典型导入：

```sl
S = import("modules/sWidgets/core.sl")
W = import("modules/wingui.sl")
```

其中：

- `sWidgets` 负责 widget、状态、布局、事件、app runtime
- `wingui` 负责窗口、文本测量、绘制命令、底层事件

`sWidgets` 内部已经 `import("../wingui.sl")`，所以普通使用者通常只需要导入 `sWidgets`。

---

## 2. 公开导出总览

当前 `sWidgets` 的公开导出如下：

- `merge(base, patch)`
- `default_theme()`
- `text_state(initial, opts)`
- `line_edit_state(initial, opts)`
- `editor_state(initial, opts)`
- `ide_editor_state(initial, opts)`
- `label(text, opts)`
- `spacer(height)`
- `divider(opts)`
- `box(child, opts)`
- `column(children, opts)`
- `row(children, opts)`
- `stack(children, opts)`
- `align(child, opts)`
- `button(text, on_click, opts)`
- `line_edit(state, opts)`
- `editor(state, opts)`
- `ide_editor(state, opts)`
- `custom(spec)`
- `app(opts)`

说明：

- 以 `_` 开头的函数是内部实现细节，不属于稳定公开 API。
- 模块系统不会导出以下划线开头的绑定，所以 `_widget_base` 等内部函数不对外可见。

---

## 3. 主题 API

### 3.1 `default_theme()`

返回默认主题 map。

```sl
theme = S.default_theme()
```

当前字段如下：

- `window_bg`
- `panel_bg`
- `panel_border`
- `text`
- `muted_text`
- `accent`
- `accent_hover`
- `accent_active`
- `accent_border`
- `selection_bg`
- `gutter_bg`
- `gutter_text`
- `current_line_bg`
- `popup_bg`
- `popup_border`
- `popup_active_bg`
- `syntax_keyword`
- `syntax_builtin`
- `syntax_string`
- `syntax_comment`
- `syntax_number`
- `syntax_operator`
- `button_text`
- `input_bg`
- `input_border`
- `input_focus_border`
- `caret`
- `font`
- `title_font`
- `button_font`
- `mono_font`

颜色值和字体值都来自 `wingui`：

```sl
theme = S.default_theme()
theme.accent = W.rgb(30, 120, 210)
theme.mono_font = W.font("Consolas", 15)
```

---

## 4. 文本状态 API

这一层只管“文本 + 光标 + 选择区”，不关心窗口。

---

### 4.1 `text_state(initial, opts)`

创建通用文本状态。

```sl
st = S.text_state("hello", {
    multiline: true,
    on_change: x -> print(x)
})
```

参数：

- `initial`
  - 初始文本，`null` 会按空字符串处理
- `opts.multiline`
  - `true` 表示多行文本状态
  - 只影响 `multiline` 标记本身，不自动增加 UI 行为
- `opts.on_change`
  - 文本变化后的回调
  - 调用形式：`on_change(new_text)`

返回值是一个状态对象，公开方法如下：

- `get()`
  - 取当前文本
- `set(value)`
  - 设置整段文本
- `caret()`
  - 取当前 caret 的字符索引
- `selection()`
  - 取当前选择区
  - 无选区时返回 `null`
  - 有选区时返回 `{start: ..., end: ...}`
- `clear_selection()`
  - 清除选择区
- `begin_selection(index)`
  - 把 caret 放到 `index` 并开始拖选锚点
- `select_to(index)`
  - 把选择区延展到 `index`
- `set_caret(index)`
  - 设置 caret，同时清空选择区
- `insert(piece)`
  - 在 caret 处插入文本
  - 如果有选择区，会先替换选区
- `backspace()`
  - 退格
  - 有选区时先删选区
  - 返回 `true/false`
- `delete_forward()`
  - 向前删除
  - 有选区时先删选区
  - 返回 `true/false`
- `move_left()`
- `move_right()`
- `move_home()`
- `move_end()`
- `move_vertical(delta)`
- `line_col()`
  - 返回 `{line: ..., col: ...}`
- `set_line_col(line, col)`
- `multiline`
  - 布尔字段，不是函数

行为细节：

- 索引按字符索引，不是字节索引。
- `move_left()` / `move_right()` 在存在选区时会先折叠选区。
- `move_home()` / `move_end()` / `move_vertical()` 会清空选择区。
- `set()`、`insert()`、`backspace()`、`delete_forward()` 会触发 `on_change`。

---

### 4.2 `line_edit_state(initial, opts)`

等价于：

```sl
S.text_state(initial, opts)
```

也就是说，它本质上就是单行场景常用的 `text_state` 别名。

---

### 4.3 `editor_state(initial, opts)`

创建多行文本状态。

```sl
st = S.editor_state("a\nb\nc", {
    on_change: x -> print("changed")
})
```

等价行为：

- 内部使用 `text_state`
- 自动设置 `multiline: true`
- 目前只额外接受 `opts.on_change`

返回对象 API 与 `text_state` 相同。

---

### 4.4 `ide_editor_state(initial, opts)`

创建带补全状态的 IDE 文本状态。

```sl
st = S.ide_editor_state("pri", {
    on_change: x -> print(x),
    words: ["project_root", "workspace", "build"]
})
```

参数：

- `initial`
- `opts.on_change`
- `opts.words`
  - 可选字符串列表
  - 会被加入自动补全候选

返回对象包含 `editor_state` 的全部 API，并额外增加：

- `hide_completion()`
  - 关闭补全弹层
- `refresh_completion(force)`
  - 刷新补全候选
  - 返回 `true/false`
- `completion_active()`
  - 当前是否有打开的候选列表
- `completion_items()`
  - 返回当前候选列表副本
  - 每项形如 `{label: "...", kind: "..."}`
- `completion_index()`
  - 当前选中项索引
- `current_completion()`
  - 当前选中候选，或 `null`
- `move_completion(delta)`
  - 上下移动候选索引
- `set_completion_index(index)`
  - 直接设置候选索引
- `accept_completion()`
  - 接受当前候选
  - 返回插入的 label，或 `null`
- `completion_summary()`
  - 返回形如 `"print [builtin]"` 的摘要，或 `null`

当前补全来源：

- ssiLang 关键字
- 一组内建函数名
- 一组常见成员方法名
- 当前缓冲区里扫描出的标识符
- `opts.words` 中额外提供的字符串

当前补全特征：

- 词法级，不做类型推断
- `.` 后补的是通用成员词表，不是对象真实成员
- 当前匹配逻辑是大小写敏感匹配

---

## 5. Widget 协议

所有内建 widget 本质都是 map，并遵循同一套协议。

当前一个 widget 至少会暴露这些字段/方法：

- `kind`
- `parent()`
- `set_parent(next)`
- `bounds()`
- `set_bounds(next)`
- `contains(x, y)`
- `children()`
- `focusable()`
- `measure(ctx, avail_w)`
- `layout(ctx, next_box)`
- `on_event(evt, ctx)`
- `on_focus(ctx)`
- `on_blur(ctx)`
- `hit(x, y)`

说明：

- `bounds()` 返回当前布局后的 `{x, y, w, h}`。
- `layout(...)` 会更新内部 bounds。
- `children()` 返回子 widget 列表。
- `hit(...)` 会做从后往前的命中测试，所以后画的子 widget 会优先命中。

---

### 5.1 布局阶段上下文 `ctx`

传给 `measure` / `layout` 的 `ctx` 字段如下：

- `gui`
  - `wingui` session
- `theme`
  - 当前窗口主题
- `is_hot(widget)`
- `is_active(widget)`
- `has_focus(widget)`

---

### 5.2 事件阶段上下文 `ctx`

传给 `on_event` / `on_focus` / `on_blur` 的 `ctx` 字段如下：

- `app`
  - 当前 `sWidgets.app(...)` 返回的 app 对象
- `gui`
  - `wingui` session
- `window`
  - 当前窗口对象
- `theme`
  - 当前窗口主题
- `is_hot(widget)`
- `is_active(widget)`
- `has_focus(widget)`
- `request_focus(widget)`
  - 让某个 widget 或其可聚焦祖先获得焦点

焦点规则：

- 实际焦点对象是“从目标 widget 往上找的最近 `focusable() == true` 的对象”
- 也就是点中不可聚焦子节点时，焦点可能落到其可聚焦父节点

---

### 5.3 通用可挂字段

很多容器并不是通过构造参数，而是通过“给 widget map 动态挂字段”的方式参与布局。

当前布局层实际会识别这些字段：

- `widget.width`
  - `row(...)` 中的固定宽度
- `widget.flex`
  - `row(...)` 中的横向弹性份额
  - `column(...)` 中的纵向弹性份额
- `widget.height`
  - `column(...)` 中的固定高度

这是当前 `sWidgets` 的真实风格，不是类继承模型。

示例：

```sl
path = S.line_edit(path_state, {})
path.flex = 1

run_btn = S.button("Run", do_run, {})
run_btn.width = 72
```

---

## 6. 基础组件 API

---

### 6.1 `label(text, opts)`

静态文本。

参数：

- `text`
- `opts.color`
- `opts.font`
- `opts.padding`
  - `number` 或 `{left, top, right, bottom}`

特点：

- 宽度测量始终返回传入的 `avail_w`
- 高度按字体高度 + padding 计算

---

### 6.2 `spacer(height)`

占位空白。

参数：

- `height`

行为：

- 测量结果为 `{w: avail_w, h: height}`

---

### 6.3 `divider(opts)`

分隔线。

参数：

- `opts.color`
- `opts.thickness`
  - 默认 `1`
- `opts.padding`

---

### 6.4 `box(child, opts)`

单子节点容器。

参数：

- `child`
- `opts.padding`
- `opts.bg`
- `opts.border`
- `opts.border_width`
  - 默认 `1`
- `opts.on_event`
- `opts.focusable`
- `opts.height`
  - 代码支持，虽然不在默认 merge 表里

行为：

- `measure(...)` 默认取子节点自然高度 + padding
- 如果传了 `opts.height`，固定用该高度
- 支持背景与边框绘制
- 如果传了 `on_event`，自身会成为 interactive 命中目标

---

### 6.5 `column(children, opts)`

纵向布局容器。

参数：

- `children`
- `opts.spacing`
- `opts.padding`
- `opts.bg`
- `opts.border`
- `opts.border_width`
- `opts.on_event`
- `opts.focusable`

额外布局规则：

- 子项 `child.height`
  - 固定高度
- 子项 `child.flex`
  - 剩余高度分配

说明：

- `measure(...)` 只按自然高度求和，不会把 `flex` 膨胀进测量值
- `layout(...)` 阶段才会把剩余高度分给 `flex` 子项

---

### 6.6 `row(children, opts)`

横向布局容器。

参数：

- `children`
- `opts.spacing`
- `opts.padding`
- `opts.bg`
- `opts.border`
- `opts.border_width`
- `opts.align_y`
  - `"start"` / `"center"` / `"end"` / `"stretch"`
- `opts.on_event`
- `opts.focusable`

额外布局规则：

- 子项 `child.width`
  - 固定宽度
- 子项 `child.flex`
  - 剩余宽度分配

说明：

- `measure(...)` 的返回宽度始终写成 `avail_w`
- 高度取子项最大自然高度

---

### 6.7 `stack(children, opts)`

叠放容器。

参数：

- `children`
- `opts.padding`
- `opts.bg`
- `opts.border`
- `opts.border_width`
- `opts.on_event`
- `opts.focusable`

行为：

- 所有子节点都会拿到同一个 inner box
- 常用于覆盖层、浮层或前后景叠放

---

### 6.8 `align(child, opts)`

单子节点对齐容器。

参数：

- `child`
- `opts.x`
  - `"start"` / `"center"` / `"end"` / `"stretch"`
- `opts.y`
  - `"start"` / `"center"` / `"end"` / `"stretch"`
- `opts.height`
  - 代码支持
- `opts.child_width`
  - 代码支持
- `opts.child_height`
  - 代码支持

说明：

- `opts.height`、`opts.child_width`、`opts.child_height` 都是当前代码真实支持的字段，即使它们不在默认 merge 表里。

---

### 6.9 `button(text, on_click, opts)`

按钮。

参数：

- `text`
- `on_click`
- `opts.padding`
- `opts.font`
- `opts.bg`
- `opts.hover_bg`
- `opts.active_bg`
- `opts.border`
- `opts.text_color`
- `opts.focusable`
  - 默认 `true`

交互：

- 鼠标左键按下并在同一按钮上抬起时触发 `on_click`
- `Enter` / `Space` 也会触发 `on_click`

---

### 6.10 `line_edit(state, opts)`

单行编辑器。

参数：

- `state`
  - 推荐传 `line_edit_state(...)`
- `opts.padding`
- `opts.font`
- `opts.color`
- `opts.bg`
- `opts.border`
- `opts.focus_border`
- `opts.caret_color`
- `opts.selection_bg`
- `opts.placeholder`
- `opts.placeholder_color`
- `opts.on_submit`
  - 代码支持，虽然不在默认 merge 表里

交互：

- 鼠标左键定位 caret，并可拖动选择
- `Left` / `Right`
- `Home` / `End`
- `Backspace`
- `Delete`
- `Enter`
  - 如果提供 `on_submit`，调用 `on_submit(state.get())`
- `char` 输入普通字符

当前限制：

- 没有横向滚动
- 文本超长会直接继续向右画
- 不支持 `Shift + 方向键` 扩选；当前扩选主要靠鼠标拖拽和状态 API

---

### 6.11 `editor(state, opts)`

基础多行编辑器。

参数：

- `state`
  - 推荐传 `editor_state(...)`
- `opts.padding`
- `opts.font`
- `opts.color`
- `opts.bg`
- `opts.border`
- `opts.focus_border`
- `opts.caret_color`
- `opts.selection_bg`

交互：

- 鼠标左键定位 caret，并可拖动选择
- `Left` / `Right`
- `Up` / `Down`
- `Home` / `End`
- `Backspace`
- `Delete`
- `Enter`
  - 直接插入 `"\n"`
- `char` 输入普通字符

当前限制：

- 没有滚动
- 没有语法高亮
- 没有自动缩进
- 没有补全

---

### 6.12 `ide_editor(state, opts)`

IDE 版编辑器。

参数：

- `state`
  - 必须是 `ide_editor_state(...)`
  - 如果没有补全 API，会直接报错
- `opts.padding`
  - 默认 `{left: 8, top: 8, right: 8, bottom: 8}`
- `opts.font`
- `opts.line_no_font`
  - 默认 `Consolas 13`
- `opts.color`
- `opts.bg`
- `opts.border`
- `opts.focus_border`
- `opts.caret_color`
- `opts.selection_bg`
- `opts.gutter_bg`
- `opts.gutter_text`
- `opts.current_line_bg`
- `opts.popup_bg`
- `opts.popup_border`
- `opts.popup_active_bg`
- `opts.scrollbar_bg`
- `opts.scrollbar_thumb`
- `opts.scrollbar_thumb_hover`
- `opts.scrollbar_thumb_active`
- `opts.tab_text`
  - 默认四个空格
- `opts.popup_max_items`
  - 默认 `8`
- `opts.gutter_min_width`
  - 默认 `48`
- `opts.scrollbar_width`
  - 默认 `12`

内建能力：

- 行号 gutter
- 当前行高亮
- ssiLang 词法高亮
- 选择区绘制
- caret 自动保持在可见区内
- 垂直滚动
- 垂直滚动条
- 自动补全弹层
- `Enter` 自动缩进

交互：

- `Ctrl + Space`
  - 强制触发补全
- 输入 `.` 或标识符字符
  - 自动尝试刷新补全
- 鼠标滚轮
  - 垂直滚动 viewport
- 点击滚动条 track
  - 跳转到对应位置
- 拖拽滚动条 thumb
  - 连续滚动 viewport
- 补全弹层打开时：
  - `Up` / `Down` 选项切换
  - `Tab` / `Enter` 接受补全
  - `Escape` 关闭补全
- 常规编辑：
  - `Left` / `Right`
  - `Up` / `Down`
  - `Home` / `End`
  - `Backspace`
  - `Delete`
  - `Tab`
    - 直接插入 `opts.tab_text`
  - `Enter`
    - 插入换行并继承当前行前导空白
    - 如果当前光标左侧去尾空白后以 `{`、`[`、`(` 结尾，再额外补一层 `tab_text`

当前真实限制：

- 补全是词法级，不是语义级
- `.` 后成员补全是通用词表，不做对象类型分析
- 当前匹配逻辑大小写敏感
- 当前滚动只有垂直方向，没有水平滚动

---

### 6.13 `custom(spec)`

创建自定义 widget。

```sl
wd = S.custom({
    focusable: false,
    interactive: false,
    measure: (ctx, avail_w) -> {w: avail_w, h: 40},
    layout: (ctx, box) -> [
        W.fill_rect(box.x, box.y, box.w, box.h, S.default_theme().panel_bg)
    ]
})
```

`spec` 可用字段与 `_widget_base` 一致：

- `children`
  - 函数，返回子 widget 列表
- `measure(ctx, avail_w)`
- `layout(ctx, box_rect)`
- `on_event(evt, ctx)`
- `on_focus(ctx)`
- `on_blur(ctx)`
- `interactive`
  - `true` 时会参与 hit test
- `focusable`
  - `true` 时可获得焦点

注意：

- `layout(...)` 应返回 `wingui` 绘制命令列表
- 自定义 widget 如果想被命中，必须 `interactive: true` 或 `focusable: true`

---

## 7. 运行时 API

---

### 7.1 `app(opts)`

创建 GUI app runtime。

```sl
app = S.app({})
```

`opts` 会直接传给 `wingui.start(opts)`。

除此之外，`sWidgets.app(opts)` 当前还识别这些 runtime 选项：

- `opts.defer_redraw`
  - 是否使用“标脏 + timer 触发”的异步重绘
  - 默认 `true`
- `opts.redraw_interval_ms`
  - 延迟重绘 timer 间隔
  - 默认 `16`
- `opts.redraw_timer_id`
  - 内部重绘 timer 的 id
  - 默认 `1`

返回对象字段：

- `gui`
  - `wingui` session
- `theme`
  - 默认主题
- `create_window(opts2)`
- `step(timeout_seconds)`
- `run()`
- `stop()`

---

### 7.2 `app.create_window(opts2)`

创建窗口。

参数：

- `opts2.root`
  - 必填
- `opts2.title`
  - 默认 `"sWidgets"`
- `opts2.width`
  - 默认 `800`
- `opts2.height`
  - 默认 `600`
- `opts2.visible`
  - 默认 `true`
- `opts2.resizable`
  - 默认 `true`
- `opts2.theme`
  - 默认 `app.theme`
- `opts2.background`
  - 默认 `app.theme.window_bg`

返回窗口对象，当前字段/方法如下：

- `id`
- `native`
  - 底层 `wingui` window
- `root`
- `hot`
- `active`
- `focus`
- `closed`
- `dirty`
- `redraw_timer_active`
- `redraw_interval_ms`
- `redraw_timer_id`
- `defer_redraw`
- `theme`
- `background`
- `redraw()`
- `close()`
- `dispatch(evt)`
  - 手动把事件打进窗口，测试里常用
- `set_root(root_widget)`
- `focused()`
  - 返回当前聚焦 widget

说明：

- `create_window(...)` 创建后会请求一次立即重绘
- `root_widget.set_parent(null)` 会在挂载前被调用一次
- `redraw()`
  - 强制立即重绘
- 高频输入、鼠标移动、滚轮等场景默认走“标脏 + timer 异步重绘”

---

### 7.3 `app.step(timeout_seconds)`

轮询一次底层 GUI 事件：

```sl
app.step(0.05)
```

行为：

- 调 `gui.poll_event(...)`
- 如果事件属于已知窗口，则自动走 `dispatch(...)`
- 返回事件 map 或 `null`

---

### 7.4 `app.run()`

主循环：

```sl
app.run()
```

行为：

- 持续 `step(0.05)`
- 没有窗口时退出

---

### 7.5 `app.stop()`

关闭全部窗口并停止底层 `wingui` session。

返回值：

- `gui.stop()` 的返回码

---

## 8. 事件模型

---

### 8.1 runtime 当前实际会处理的事件

`sWidgets.app.dispatch(...)` 当前显式处理这些事件：

- `resize`
- `paint`
- `timer`
- `mouse_leave`
- `mouse_move`
- `mouse_wheel`
- `mouse_down` 且 `button == "left"`
- `mouse_up` 且 `button == "left"`
- `key_down`
- `key_up`
- `char`
- `close`

其他事件当前不会自动分发给 widget。

这意味着：

- 右键、中键按下/抬起也不会走 widget 事件链

---

### 8.2 常见事件字段

#### 鼠标事件

常见字段：

- `type`
- `window_id`
- `x`
- `y`
- `left`
- `right`
- `middle`
- `shift`
- `ctrl`
- `alt`

其中 `mouse_down` / `mouse_up` 还会带：

- `button`

#### 键盘事件

`key_down` / `key_up` 常见字段：

- `type`
- `window_id`
- `key`
- `repeat`
- `shift`
- `ctrl`
- `alt`

#### 字符事件

`char` 常见字段：

- `type`
- `window_id`
- `text`
- `code`

---

### 8.3 `hot` / `active` / `focus`

当前 runtime 维护三类状态：

- `hot`
  - 鼠标当前悬停命中的 widget
- `active`
  - 左键按下时捕获的 widget
- `focus`
  - 当前有键盘焦点的 widget

当前规则：

- `mouse_move`
  - 更新 `hot`
  - 若有 `active`，事件优先打给 `active`
- `mouse_wheel`
  - 优先打给 `hot`
  - 如果没有 `hot`，退化到 `focus`
- `mouse_down(left)`
  - 更新 `hot`
  - `active = hot`
  - 尝试请求焦点
- `mouse_up(left)`
  - 事件先打给 `active` 或 `hot`
  - 然后清空 `active`
- `mouse_leave`
  - 只清空 `hot`
  - 不清空 `active`

这正是拖选能工作的原因。

另外：

- runtime 默认采用“标脏 + timer 异步重绘”
- 高频 `mouse_move` / `mouse_wheel` / 键盘输入不会每条消息都立即重建整棵 scene
- 真正重绘通常由内部 `timer` 事件触发
- 显式 `win.redraw()`、初次建窗和 `set_root(...)` 仍会立即重绘

---

## 9. 绘制模型

`sWidgets` 自己不直接画像素，而是返回 `wingui` 绘制命令列表。

内建 widget 内部主要使用这些 `wingui` 操作：

- `W.clear(color)`
- `W.fill_rect(x, y, w, h, color)`
- `W.rect(x, y, w, h, color, width)`
- `W.text(x, y, text, {color, font})`

如果你用 `custom(...)`，你也应该返回同一类命令。

---

## 10. 最小示例

```sl
S = import("modules/sWidgets/core.sl")

clicked = 0
app = S.app({})

fn on_click() {
    @clicked = @clicked + 1
    return null
}

root = S.box(
    S.column([
        S.label("Hello sWidgets", {
            font: S.default_theme().title_font
        }),
        S.divider({}),
        S.button("Click", on_click, {})
    ], {
        spacing: 10,
        padding: 12
    }),
    {
        padding: 12,
        bg: S.default_theme().panel_bg,
        border: S.default_theme().panel_border
    }
)

win = app.create_window({
    title: "Demo",
    width: 420,
    height: 240,
    visible: true,
    root: root
})

app.run()
```

---

## 11. 布局示例：`flex`

```sl
path = S.line_edit(path_state, {})
path.flex = 1

run_btn = S.button("Run", do_run, {})
run_btn.width = 72

toolbar = S.row([
    path,
    run_btn
], {
    spacing: 8
})

editor = S.ide_editor(editor_state, {})
editor.flex = 1

root = S.column([
    toolbar,
    editor
], {
    spacing: 12,
    padding: 12
})
```

规则总结：

- `row` 看 `width` 和 `flex`
- `column` 看 `height` 和 `flex`

---

## 12. 当前限制与已知边界

这些不是猜测，是当前代码真实现状：

- `line_edit` 没有水平滚动
- `editor` 没有滚动
- `ide_editor` 没有水平滚动
- `ide_editor` 补全是词法级，不做语义分析
- `ide_editor` 的成员补全不是类型感知
- 当前补全匹配大小写敏感
- `line_edit` / `editor` / `ide_editor` 当前都没有 `Shift + 方向键` 扩选逻辑
- `row.measure(...)`、`column.measure(...)` 的测量策略偏“够用”，不是完整约束布局系统

---

## 13. 什么时候该用哪个控件

- 想要静态说明文本：`label`
- 想要简单占位：`spacer`
- 想要一条线：`divider`
- 想要背景框：`box`
- 想要纵向布局：`column`
- 想要横向布局：`row`
- 想要叠层：`stack`
- 想要对齐单子节点：`align`
- 想要点击行为：`button`
- 想要单行输入：`line_edit`
- 想要简单多行文本编辑：`editor`
- 想要带高亮和补全的脚本编辑：`ide_editor`
- 想要自己画：`custom`

---

## 14. 给上层库作者的建议

如果你准备在上面继续写 `sWidgets` 的更高层组件：

- 把状态对象和 widget 对象分开
- 布局层用 `width` / `height` / `flex` 这些约定字段就行，不要强行模拟类继承
- 复杂交互优先写成状态对象 API，再由 widget 调状态对象
- 自定义控件优先用 `custom(...)`，只在需要时复用现有状态对象
- 如果要做真正可用的复杂编辑器，下一步应该补：
  - 水平滚动
  - 语义补全
  - 诊断与错误标注
  - 更完整的键盘扩选
