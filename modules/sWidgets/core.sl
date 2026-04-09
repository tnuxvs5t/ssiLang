W = import("../wingui.sl")
SSTL = import("../sstl.sl")

_widget_timer_seq = 4096

fn _next_widget_timer_id() {
@_widget_timer_seq = @_widget_timer_seq + 1
return @_widget_timer_seq
}

_profile_buckets = {}
_prefix_index_cache = {}

fn _profile_begin() => sys.now_ms()

fn _profile_end(name, started_ms) {
elapsed = sys.now_ms() - started_ms
bucket = @_profile_buckets?[name]
if bucket == null {
bucket = {calls: 0, total_ms: 0, max_ms: 0}
}
bucket.calls = bucket.calls + 1
bucket.total_ms = bucket.total_ms + elapsed
if elapsed > bucket.max_ms {
bucket.max_ms = elapsed
}
@_profile_buckets[name] = bucket
return elapsed
}

fn _profile_bucket(name) {
bucket = @_profile_buckets?[name]
if bucket == null {
return {calls: 0, total_ms: 0, max_ms: 0, avg_ms: 0}
}
avg = bucket.calls == 0 => 0 || bucket.total_ms / bucket.calls
out = %copy bucket
out.avg_ms = avg
return out
}

fn _profile_snapshot() {
out = {}
for key in @_profile_buckets.keys() {
out[key] = _profile_bucket(key)
}
return out
}

fn _fmt_profile_bucket(name, bucket) {
return name + ": calls=" + str(int(bucket.calls)) + ", avg=" + str(int(bucket.avg_ms * 1000 + 0.5) / 1000.0) + "ms, max=" + str(int(bucket.max_ms * 1000 + 0.5) / 1000.0) + "ms"
}

VK_BACK = 8
VK_TAB = 9
VK_RETURN = 13
VK_ESCAPE = 27
VK_SPACE = 32
VK_A = 65
VK_C = 67
VK_V = 86
VK_X = 88
VK_END = 35
VK_HOME = 36
VK_LEFT = 37
VK_UP = 38
VK_RIGHT = 39
VK_DOWN = 40
VK_DELETE = 46

fn _copy_map(v, label) {
if v == null => return {}
if type(v) != "map" => error(label + " must be map")
return %copy v
}

fn merge(base, patch) {
out = _copy_map(base, "sWidgets.merge base")
if patch != null {
if type(patch) != "map" => error("sWidgets.merge patch must be map")
for k in patch.keys() {
out[k] = patch[k]
}
}
return out
}

fn _rect(x, y, w, h) => {x: x, y: y, w: w, h: h}

fn _contains(rect, x, y) {
return x >= rect.x and x < rect.x + rect.w and y >= rect.y and y < rect.y + rect.h
}

fn _inset(rect, pad) {
return {
x: rect.x + pad.left,
y: rect.y + pad.top,
w: rect.w - pad.left - pad.right,
h: rect.h - pad.top - pad.bottom
}
}

fn _as_box(rect) {
return {
x: rect.x == null => rect.left == null => 0 || rect.left || rect.x,
y: rect.y == null => rect.top == null => 0 || rect.top || rect.y,
w: rect.w == null => rect.width == null => 0 || rect.width || rect.w,
h: rect.h == null => rect.height == null => 0 || rect.height || rect.h
}
}

fn _append_all(dst, src) {
if src == null => return dst
for item in src {
dst.push(item)
}
return dst
}

fn _clamp(v, lo, hi) {
if v < lo => return lo
if v > hi => return hi
return v
}

fn _max(a, b) => a > b => a || b
fn _min(a, b) => a < b => a || b

fn _normalize_padding(opts, fallback) {
pad = opts.padding
if pad == null {
pad = fallback
}

if type(pad) == "number" {
return {
left: pad,
top: pad,
right: pad,
bottom: pad
}
}

if type(pad) == "map" {
left = pad.left == null => 0 || pad.left
top = pad.top == null => 0 || pad.top
right = pad.right == null => left || pad.right
bottom = pad.bottom == null => top || pad.bottom
return {
left: left,
top: top,
right: right,
bottom: bottom
}
}

return {
left: fallback,
top: fallback,
right: fallback,
bottom: fallback
}
}

fn _measure_text(gui, text, font_cfg) => gui.measure_text(text, {font: font_cfg})
fn _font_or(opts, fallback) => opts.font == null => fallback || opts.font
fn _color_or(opts, key, fallback) => opts?[key] == null => fallback || opts[key]
fn _num_or(opts, key, fallback) => opts?[key] == null => fallback || opts[key]
fn _str_or(opts, key, fallback) => opts?[key] == null => fallback || opts[key]

fn _font_cache_sig(font_cfg) {
face = font_cfg.face == null => "" || font_cfg.face
size = font_cfg.size == null => 0 || font_cfg.size
weight = font_cfg.weight == null => 0 || font_cfg.weight
italic = font_cfg.italic == true
underline = font_cfg.underline == true
return face + "|" + str(size) + "|" + str(weight) + "|" + str(italic) + "|" + str(underline)
}

fn _char_len(text) => len(text.chars())

fn _slice_text(text, start, stop) {
chars = text.chars()
out = []
a = _clamp(start, 0, len(chars))
b = _clamp(stop, a, len(chars))
for i in range(a, b) {
out.push(chars[i])
}
return out.join("")
}

fn _insert_text(text, index, piece) {
chars = text.chars()
add = piece.chars()
pos = _clamp(index, 0, len(chars))
out = []
for i in range(pos) {
out.push(chars[i])
}
for ch in add {
out.push(ch)
}
for i in range(pos, len(chars)) {
out.push(chars[i])
}
return out.join("")
}

fn _delete_range(text, start, stop) {
chars = text.chars()
a = _clamp(start, 0, len(chars))
b = _clamp(stop, a, len(chars))
out = []
for i in range(a) {
out.push(chars[i])
}
for i in range(b, len(chars)) {
out.push(chars[i])
}
return out.join("")
}

fn _line_texts(text) {
xs = text.split("\n")
if len(xs) == 0 => return [""]
return xs
}

fn _line_col_of(text, index) {
chars = text.chars()
pos = _clamp(index, 0, len(chars))
line = 0
col = 0
for i in range(pos) {
if chars[i] == "\n" {
line = line + 1
col = 0
} else {
col = col + 1
}
}
return {line: line, col: col}
}

fn _index_from_line_col(text, line, col) {
chars = text.chars()
want_line = _clamp(line, 0, 1000000)
want_col = _clamp(col, 0, 1000000)
cur_line = 0
cur_col = 0
idx = 0

while idx < len(chars) {
if cur_line == want_line and cur_col == want_col => return idx
if chars[idx] == "\n" {
if cur_line == want_line => return idx
cur_line = cur_line + 1
cur_col = 0
} else {
cur_col = cur_col + 1
}
idx = idx + 1
}

return idx
}

fn _prefix_col(gui, font_cfg, text, x) {
started_ms = _profile_begin()
chars = text.chars()
if x <= 0 {
_profile_end("_prefix_col", started_ms)
return 0
}
count = len(chars)
if count == 0 {
_profile_end("_prefix_col", started_ms)
return 0
}
cache_key = _font_cache_sig(font_cfg) + "\n" + text
pack = @_prefix_index_cache?[cache_key]
if pack == null {
build_started_ms = _profile_begin()
prefix = ""
widths = [0]
i = 0
while i < count {
prefix = prefix + chars[i]
widths.push(_measure_text(gui, prefix, font_cfg).width)
i = i + 1
}
pack = {widths: widths}
@_prefix_index_cache[cache_key] = pack
_profile_end("_prefix_col.build", build_started_ms)
}
widths = pack.widths
last_w = widths[count]
if x >= last_w {
_profile_end("_prefix_col", started_ms)
return count
}
lo = 1
hi = count
while lo < hi {
mid = int((lo + hi) / 2)
if widths[mid] <= x {
lo = mid + 1
} else {
hi = mid
}
}
next_idx = lo
prev_idx = next_idx - 1
left_gap = x - widths[prev_idx]
right_gap = widths[next_idx] - x
if left_gap <= right_gap {
_profile_end("_prefix_col", started_ms)
return prev_idx
}
_profile_end("_prefix_col", started_ms)
return next_idx
}

fn _repeat_text(piece, count) {
out = []
for i in range(count) {
out.push(piece)
}
return out.join("")
}

fn _ascii_lower_char(ch) {
return ch
}

fn _ascii_lower(text) => text

fn _is_digit(ch) => ch >= "0" and ch <= "9"
fn _is_alpha(ch) => (ch >= "a" and ch <= "z") or (ch >= "A" and ch <= "Z")
fn _is_ident_start(ch) => _is_alpha(ch) or ch == "_"
fn _is_ident_part(ch) => _is_ident_start(ch) or _is_digit(ch)
fn _is_space(ch) => ch == " " or ch == "\t"

fn _leading_ws(text) {
chars = text.chars()
i = 0
while i < len(chars) and _is_space(chars[i]) {
i = i + 1
}
return _slice_text(text, 0, i)
}

fn _rtrim_ws(text) {
chars = text.chars()
i = len(chars)
while i > 0 and _is_space(chars[i - 1]) {
i = i - 1
}
return _slice_text(text, 0, i)
}

fn _word_set(xs) {
out = {}
for x in xs {
out[x] = true
}
return out
}

_SSI_KEYWORD_LIST = [
"fn",
"if",
"else",
"while",
"for",
"in",
"return",
"break",
"continue",
"and",
"or",
"not",
"true",
"false",
"null",
"import"
]

_SSI_BUILTIN_LIST = [
"print",
"len",
"type",
"int",
"float",
"str",
"error",
"pcall",
"range",
"sort",
"map",
"filter",
"reduce",
"join",
"abs",
"min",
"max",
"json_encode",
"json_parse",
"sys"
]

_SSI_MEMBER_LIST = [
"push",
"pop",
"slice",
"map",
"filter",
"reduce",
"find",
"contains",
"join",
"reverse",
"trim",
"split",
"chars",
"starts_with",
"ends_with",
"replace",
"keys",
"values",
"has",
"get",
"set",
"deref",
"wait",
"is_alive",
"pid"
]

_SSI_KEYWORDS = _word_set(_SSI_KEYWORD_LIST)
_SSI_BUILTINS = _word_set(_SSI_BUILTIN_LIST)
_SSI_MEMBERS = _word_set(_SSI_MEMBER_LIST)

fn _push_unique_suggest(dst, seen, label, kind) {
key = _ascii_lower(label) + "|" + kind
if seen?[key] == true => return null
seen[key] = true
dst.push({label: label, kind: kind})
return null
}

fn _scan_identifiers(text) {
out = []
seen = {}
chars = text.chars()
i = 0
while i < len(chars) {
ch = chars[i]
if ch == "#" {
while i < len(chars) and chars[i] != "\n" {
i = i + 1
}
continue
}
if ch == "\"" or ch == "'" {
quote = ch
start = i
i = i + 1
while i < len(chars) {
if chars[i] == "\\" and i + 1 < len(chars) {
i = i + 2
continue
}
if chars[i] == quote {
i = i + 1
break
}
i = i + 1
}
continue
}
if _is_ident_start(ch) {
start = i
i = i + 1
while i < len(chars) and _is_ident_part(chars[i]) {
i = i + 1
}
word = _slice_text(text, start, i)
if not _SSI_KEYWORDS?[word] and not _SSI_BUILTINS?[word] {
_push_unique_suggest(out, seen, word, "id")
}
continue
}
i = i + 1
}
return out
}

fn _completion_target(text, caret) {
chars = text.chars()
pos = _clamp(caret, 0, len(chars))
start = pos
while start > 0 and _is_ident_part(chars[start - 1]) {
start = start - 1
}
stop = pos
while stop < len(chars) and _is_ident_part(chars[stop]) {
stop = stop + 1
}
prev = pos > 0 => chars[pos - 1] || ""
return {
start: start,
stop: stop,
prefix: _slice_text(text, start, pos),
full: _slice_text(text, start, stop),
after_dot: prev == "."
}
}

fn _completion_items(text, caret, extra_words, force) {
target = _completion_target(text, caret)
prefix = target.prefix
prefix_l = _ascii_lower(prefix)
items = []
seen = {}

fn accept_word(word, kind) {
if not force and prefix == "" and not target.after_dot => return null
if not force and prefix == "" and target.after_dot {
_push_unique_suggest(items, seen, word, kind)
return null
}
if prefix == "" or _ascii_lower(word).starts_with(prefix_l) {
_push_unique_suggest(items, seen, word, kind)
}
return null
}

source_keywords = target.after_dot => [] || _SSI_KEYWORD_LIST
source_builtins = target.after_dot => [] || _SSI_BUILTIN_LIST
source_members = target.after_dot => _SSI_MEMBER_LIST || []

for word in source_keywords {
accept_word(word, "kw")
}
for word in source_builtins {
accept_word(word, "builtin")
}
for word in source_members {
accept_word(word, "member")
}
for word in extra_words {
accept_word(word.label, word.kind)
}
for word in _scan_identifiers(text) {
accept_word(word.label, word.kind)
}

return {
target: target,
items: items
}
}

fn _syntax_tokenize_line(text) {
started_ms = _profile_begin()
chars = text.chars()
tokens = []
i = 0
while i < len(chars) {
ch = chars[i]
if ch == "#" {
tokens.push({text: _slice_text(text, i, len(chars)), kind: "comment"})
break
}
if ch == "\"" or ch == "'" {
quote = ch
start = i
i = i + 1
while i < len(chars) {
if chars[i] == "\\" and i + 1 < len(chars) {
i = i + 2
continue
}
if chars[i] == quote {
i = i + 1
break
}
i = i + 1
}
tokens.push({text: _slice_text(text, start, i), kind: "string"})
continue
}
if _is_digit(ch) {
start = i
i = i + 1
while i < len(chars) and (_is_digit(chars[i]) or chars[i] == ".") {
i = i + 1
}
tokens.push({text: _slice_text(text, start, i), kind: "number"})
continue
}
if _is_ident_start(ch) {
start = i
i = i + 1
while i < len(chars) and _is_ident_part(chars[i]) {
i = i + 1
}
word = _slice_text(text, start, i)
kind = _SSI_KEYWORDS?[word] == true => "keyword" || _SSI_BUILTINS?[word] == true => "builtin" || "text"
tokens.push({text: word, kind: kind})
continue
}
if i + 1 < len(chars) {
pair = ch + chars[i + 1]
if pair == "=>" or pair == "->" or pair == "|>" or pair == "==" or pair == "!=" or pair == "<=" or pair == ">=" or pair == "?." {
tokens.push({text: pair, kind: "operator"})
i = i + 2
continue
}
}
kind = (_is_space(ch) or ch == "\n") => "text" || "operator"
tokens.push({text: ch, kind: kind})
i = i + 1
}
_profile_end("_syntax_tokenize_line", started_ms)
return tokens
}

fn _is_control_input(evt) {
if evt.text == "" => return true
if evt.code != null and evt.code < 32 => return true
return false
}

fn _clipboard_get(gui) {
wrap = pcall(() -> gui.clipboard_get())
if wrap.ok => return wrap.value
return ""
}

fn _clipboard_set(gui, text) {
pcall(() -> gui.clipboard_set(text == null => "" || text))
return null
}

fn _single_line_paste_text(text) {
clean = text == null => "" || text
clean = clean.replace("\r", "")
clean = clean.replace("\n", " ")
return clean
}

fn _handle_text_shortcuts(evt, ctx, state, opts) {
if evt.type != "key_down" or evt.ctrl != true => return false
key = evt.key
if key == VK_A {
state.select_all()
return true
}
if key == VK_C {
_clipboard_set(ctx.gui, state.selected_text())
return true
}
if key == VK_X {
sel = state.selected_text()
if sel != "" {
_clipboard_set(ctx.gui, sel)
state.insert("")
}
return true
}
if key == VK_V {
clip = _clipboard_get(ctx.gui)
if opts != null and opts.multiline != true {
clip = _single_line_paste_text(clip)
}
if clip != "" {
state.insert(clip)
}
return true
}
return false
}

fn default_theme() {
return {
window_bg: W.rgb(245, 247, 252),
panel_bg: W.rgb(255, 255, 255),
panel_border: W.rgb(214, 220, 232),
text: W.rgb(35, 41, 52),
muted_text: W.rgb(94, 102, 118),
accent: W.rgb(76, 118, 255),
accent_hover: W.rgb(96, 136, 255),
accent_active: W.rgb(60, 102, 236),
accent_border: W.rgb(58, 94, 208),
selection_bg: W.rgb(209, 223, 255),
gutter_bg: W.rgb(240, 243, 250),
gutter_text: W.rgb(126, 136, 154),
current_line_bg: W.rgb(247, 250, 255),
popup_bg: W.rgb(252, 253, 255),
popup_border: W.rgb(189, 199, 220),
popup_active_bg: W.rgb(229, 236, 255),
syntax_keyword: W.rgb(128, 67, 197),
syntax_builtin: W.rgb(22, 121, 160),
syntax_string: W.rgb(177, 88, 28),
syntax_comment: W.rgb(111, 129, 103),
syntax_number: W.rgb(33, 123, 84),
syntax_operator: W.rgb(83, 96, 120),
button_text: W.rgb(255, 255, 255),
input_bg: W.rgb(255, 255, 255),
input_border: W.rgb(194, 202, 218),
input_focus_border: W.rgb(76, 118, 255),
caret: W.rgb(35, 41, 52),
font: W.font("Segoe UI", 16),
title_font: W.font("Segoe UI", 22, {weight: 700}),
button_font: W.font("Segoe UI", 16, {weight: 700}),
mono_font: W.font("Consolas", 16)
}
}

fn text_state(initial, opts) {
conf = _copy_map(opts, "sWidgets.text_state opts")
text = initial == null => "" || initial
caret = 0
anchor = null
preferred_col = null
multiline = conf.multiline == true

fn notify() {
if conf.on_change != null {
conf.on_change(@text)
}
return null
}

fn clamp_caret() {
@caret = _clamp(@caret, 0, _char_len(@text))
return @caret
}

fn clamp_anchor() {
if @anchor == null => return null
@anchor = _clamp(@anchor, 0, _char_len(@text))
if @anchor == @caret {
@anchor = null
}
return @anchor
}

fn selection_range() {
clamp_caret()
clamp_anchor()
if @anchor == null => return null
if @anchor < @caret => return {start: @anchor, end: @caret}
return {start: @caret, end: @anchor}
}

fn clear_selection() {
@anchor = null
return null
}

fn select_all() {
if _char_len(@text) <= 0 {
@caret = 0
@anchor = null
@preferred_col = null
return null
}
begin_selection(0)
select_to(_char_len(@text))
return selection_range()
}

fn begin_selection(index) {
@caret = _clamp(index, 0, _char_len(@text))
@anchor = @caret
@preferred_col = null
return @caret
}

fn select_to(index) {
if @anchor == null {
@anchor = @caret
}
@caret = _clamp(index, 0, _char_len(@text))
clamp_anchor()
@preferred_col = null
return @caret
}

fn delete_selection_raw() {
sel = selection_range()
if sel == null => return null
@text = _delete_range(@text, sel.start, sel.end)
@caret = sel.start
@anchor = null
@preferred_col = null
return sel
}

fn get() => @text

fn set(value) {
@text = value == null => "" || value
clamp_caret()
@anchor = null
@preferred_col = null
notify()
return @text
}

fn caret_pos() {
clamp_caret()
return @caret
}

fn selected_text() {
sel = selection_range()
if sel == null => return ""
return _slice_text(@text, sel.start, sel.end)
}

fn set_caret(index) {
@caret = _clamp(index, 0, _char_len(@text))
@anchor = null
@preferred_col = null
return @caret
}

fn insert(piece) {
delete_selection_raw()
@text = _insert_text(@text, @caret, piece)
@caret = @caret + _char_len(piece)
@anchor = null
@preferred_col = null
notify()
return @text
}

fn backspace() {
if delete_selection_raw() != null {
notify()
return true
}
if @caret <= 0 => return false
@text = _delete_range(@text, @caret - 1, @caret)
@caret = @caret - 1
@anchor = null
@preferred_col = null
notify()
return true
}

fn delete_forward() {
if delete_selection_raw() != null {
notify()
return true
}
if @caret >= _char_len(@text) => return false
@text = _delete_range(@text, @caret, @caret + 1)
@anchor = null
@preferred_col = null
notify()
return true
}

fn move_left() {
sel = selection_range()
if sel != null {
@caret = sel.start
@anchor = null
@preferred_col = null
return @caret
}
if @caret > 0 {
@caret = @caret - 1
}
@anchor = null
@preferred_col = null
return @caret
}

fn move_right() {
sel = selection_range()
if sel != null {
@caret = sel.end
@anchor = null
@preferred_col = null
return @caret
}
if @caret < _char_len(@text) {
@caret = @caret + 1
}
@anchor = null
@preferred_col = null
return @caret
}

fn move_home() {
lc = _line_col_of(@text, @caret)
@caret = _index_from_line_col(@text, lc.line, 0)
@anchor = null
@preferred_col = 0
return @caret
}

fn move_end() {
lc = _line_col_of(@text, @caret)
lines = _line_texts(@text)
cur = lc.line < len(lines) => lines[lc.line] || ""
@caret = _index_from_line_col(@text, lc.line, _char_len(cur))
@anchor = null
@preferred_col = _char_len(cur)
return @caret
}

fn move_vertical(delta) {
lc = _line_col_of(@text, @caret)
lines = _line_texts(@text)
target_line = _clamp(lc.line + delta, 0, len(lines) - 1)
want_col = @preferred_col == null => lc.col || @preferred_col
cur_line = lines[target_line]
next_col = _clamp(want_col, 0, _char_len(cur_line))
@caret = _index_from_line_col(@text, target_line, next_col)
@anchor = null
@preferred_col = want_col
return @caret
}

fn line_col() => _line_col_of(@text, @caret)

fn set_line_col(line, col) {
@caret = _index_from_line_col(@text, line, col)
@anchor = null
@preferred_col = col
return @caret
}

return {
get: get,
set: set,
caret: caret_pos,
selection: selection_range,
clear_selection: clear_selection,
select_all: select_all,
begin_selection: begin_selection,
select_to: select_to,
set_caret: set_caret,
selected_text: selected_text,
insert: insert,
backspace: backspace,
delete_forward: delete_forward,
move_left: move_left,
move_right: move_right,
move_home: move_home,
move_end: move_end,
move_vertical: move_vertical,
line_col: line_col,
set_line_col: set_line_col,
multiline: multiline
}
}

fn line_edit_state(initial, opts) => text_state(initial, opts)

fn editor_state(initial, opts) {
conf = _copy_map(opts, "sWidgets.editor_state opts")
return text_state(initial, {
multiline: true,
on_change: conf.on_change
})
}

fn ide_editor_state(initial, opts) {
conf = _copy_map(opts, "sWidgets.ide_editor_state opts")
base = editor_state(initial, {
on_change: conf.on_change
})
state = %copy base
extra_words = []
if conf.words != null and type(conf.words) == "list" {
for word in conf.words {
if type(word) == "string" {
extra_words.push({label: word, kind: "extra"})
}
}
}

completion = {
open: false,
items: [],
index: 0,
start: 0,
stop: 0,
prefix: ""
}

fn hide_completion() {
@completion = {
open: false,
items: [],
index: 0,
start: base.caret(),
stop: base.caret(),
prefix: ""
}
return null
}

fn completion_active() => @completion.open == true and len(@completion.items) > 0
fn completion_items() => %deep @completion.items
fn completion_index() => @completion.index

fn current_completion() {
if not completion_active() => return null
return @completion.items[@completion.index]
}

fn refresh_completion(force) {
pack = _completion_items(base.get(), base.caret(), extra_words, force == true)
items = pack.items
target = pack.target
if len(items) == 0 {
hide_completion()
return false
}
@completion = {
open: true,
items: items,
index: 0,
start: target.start,
stop: target.stop,
prefix: target.prefix
}
return true
}

fn move_completion(delta) {
if not completion_active() => return null
next = _clamp(@completion.index + delta, 0, len(@completion.items) - 1)
@completion.index = next
return next
}

fn set_completion_index(index) {
if not completion_active() => return null
@completion.index = _clamp(index, 0, len(@completion.items) - 1)
return @completion.index
}

fn accept_completion() {
item = current_completion()
if item == null => return null
base.begin_selection(@completion.start)
base.select_to(@completion.stop)
base.insert(item.label)
hide_completion()
return item.label
}

fn completion_summary() {
item = current_completion()
if item == null => return null
return item.label + " [" + item.kind + "]"
}

raw_set = base.set
fn set_with_reset(value) {
out = raw_set(value)
hide_completion()
return out
}

state.set = set_with_reset
state.hide_completion = hide_completion
state.refresh_completion = refresh_completion
state.completion_active = completion_active
state.completion_items = completion_items
state.completion_index = completion_index
state.current_completion = current_completion
state.move_completion = move_completion
state.set_completion_index = set_completion_index
state.accept_completion = accept_completion
state.completion_summary = completion_summary

hide_completion()
return state
}

fn _widget_base(kind, spec) {
conf = _copy_map(spec, "sWidgets._widget_base spec")
parent = null
box = _rect(0, 0, 0, 0)
self = null

fn parent_widget() => @parent
fn set_parent(next) {
@parent = next
return null
}
fn bounds() => %copy @box
fn set_bounds(next) {
@box = %copy next
return null
}
fn contains(x, y) => _contains(@box, x, y)

fn children() {
if conf.children != null => return conf.children()
return []
}

fn focusable() => conf.focusable == true

fn measure(ctx, avail_w) {
if conf.measure != null => return conf.measure(ctx, avail_w)
return {w: avail_w, h: 0}
}

fn layout(ctx, next_box) {
set_bounds(next_box)
if conf.layout == null => return []
ops = conf.layout(ctx, next_box)
if ops == null => return []
return ops
}

fn on_event(evt, ctx) {
if conf.on_event != null => return conf.on_event(evt, ctx)
return false
}

fn on_focus(ctx) {
if conf.on_focus != null {
conf.on_focus(ctx)
}
return null
}

fn on_blur(ctx) {
if conf.on_blur != null {
conf.on_blur(ctx)
}
return null
}

fn hit(x, y) {
if not contains(x, y) => return null
xs = children()
i = len(xs) - 1
while i >= 0 {
found = xs[i].hit(x, y)
if found != null => return found
i = i - 1
}
if conf.interactive == true or conf.focusable == true => return @self
return null
}

self = {
kind: kind,
parent: parent_widget,
set_parent: set_parent,
bounds: bounds,
set_bounds: set_bounds,
contains: contains,
children: children,
focusable: focusable,
measure: measure,
layout: layout,
on_event: on_event,
on_focus: on_focus,
on_blur: on_blur,
hit: hit
}

return self
}

fn label(text, opts) {
conf = merge({
color: null,
font: null,
padding: 0
}, opts)

fn measure(ctx, avail_w) {
font_cfg = _font_or(conf, ctx.theme.font)
metrics = _measure_text(ctx.gui, text, font_cfg)
pad = _normalize_padding(conf, 0)
return {
w: avail_w,
h: metrics.height + pad.top + pad.bottom
}
}

fn layout(ctx, box) {
font_cfg = _font_or(conf, ctx.theme.font)
color = _color_or(conf, "color", ctx.theme.text)
pad = _normalize_padding(conf, 0)
return [
W.text(box.x + pad.left, box.y + pad.top, text, {
color: color,
font: font_cfg
})
]
}

return _widget_base("label", {
measure: measure,
layout: layout
})
}

fn spacer(height) {
fn measure(ctx, avail_w) => {w: avail_w, h: height}
return _widget_base("spacer", {measure: measure})
}

fn divider(opts) {
conf = merge({
color: null,
thickness: 1,
padding: 0
}, opts)

fn measure(ctx, avail_w) {
pad = _normalize_padding(conf, 0)
return {
w: avail_w,
h: conf.thickness + pad.top + pad.bottom
}
}

fn layout(ctx, box) {
pad = _normalize_padding(conf, 0)
y = box.y + pad.top
return [
W.fill_rect(box.x + pad.left, y, box.w - pad.left - pad.right, conf.thickness, _color_or(conf, "color", ctx.theme.panel_border))
]
}

return _widget_base("divider", {
measure: measure,
layout: layout
})
}

fn box(child, opts) {
conf = merge({
padding: 0,
bg: null,
border: null,
border_width: 1,
on_event: null,
focusable: false
}, opts)

if child != null {
child.set_parent(null)
}

self = null

fn child_list() {
if child == null => return []
return [child]
}

fn measure(ctx, avail_w) {
pad = _normalize_padding(conf, 0)
inner_w = avail_w - pad.left - pad.right
child_h = child == null => 0 || child.measure(ctx, inner_w).h
height = conf.height
if height == null {
height = child_h + pad.top + pad.bottom
}
return {w: avail_w, h: height}
}

fn layout(ctx, box_rect) {
ops = []
pad = _normalize_padding(conf, 0)
if conf.bg != null {
ops.push(W.fill_rect(box_rect.x, box_rect.y, box_rect.w, box_rect.h, conf.bg))
}
if conf.border != null {
ops.push(W.rect(box_rect.x, box_rect.y, box_rect.w, box_rect.h, conf.border, conf.border_width))
}
if child != null {
inner = _inset(box_rect, pad)
_append_all(ops, child.layout(ctx, inner))
}
return ops
}

self = _widget_base("box", {
children: child_list,
measure: measure,
layout: layout,
on_event: conf.on_event,
interactive: conf.on_event != null,
focusable: conf.focusable
})

if child != null {
child.set_parent(self)
}

return self
}

fn column(children, opts) {
conf = merge({
spacing: 0,
padding: 0,
bg: null,
border: null,
border_width: 1,
on_event: null,
focusable: false
}, opts)

xs = children == null => [] || children
self = null

fn child_list() => xs

fn measure(ctx, avail_w) {
pad = _normalize_padding(conf, 0)
inner_w = avail_w - pad.left - pad.right
total = pad.top + pad.bottom
for i in range(len(xs)) {
total = total + xs[i].measure(ctx, inner_w).h
if i + 1 < len(xs) {
total = total + conf.spacing
}
}
return {w: avail_w, h: total}
}

fn layout(ctx, box_rect) {
ops = []
pad = _normalize_padding(conf, 0)
if conf.bg != null {
ops.push(W.fill_rect(box_rect.x, box_rect.y, box_rect.w, box_rect.h, conf.bg))
}
if conf.border != null {
ops.push(W.rect(box_rect.x, box_rect.y, box_rect.w, box_rect.h, conf.border, conf.border_width))
}

inner = _inset(box_rect, pad)
heights = []
fixed = 0
flex_total = 0

for child in xs {
if child.height != null {
heights.push(child.height)
fixed = fixed + child.height
} else if child.flex != null and child.flex > 0 {
heights.push(null)
flex_total = flex_total + child.flex
} else {
h = child.measure(ctx, inner.w).h
heights.push(h)
fixed = fixed + h
}
}

gaps = len(xs) <= 1 => 0 || conf.spacing * (len(xs) - 1)
remain = inner.h - fixed - gaps
if remain < 0 {
remain = 0
}

used = 0
left = flex_total
for i in range(len(xs)) {
if heights[i] == null {
share = left <= 0 => 0 || int((remain - used) * xs[i].flex / left)
heights[i] = share
used = used + share
left = left - xs[i].flex
}
}

y = inner.y
for i in range(len(xs)) {
child_h = heights[i]
_append_all(ops, xs[i].layout(ctx, _rect(inner.x, y, inner.w, child_h)))
y = y + child_h
if i + 1 < len(xs) {
y = y + conf.spacing
}
}
return ops
}

self = _widget_base("column", {
children: child_list,
measure: measure,
layout: layout,
on_event: conf.on_event,
interactive: conf.on_event != null,
focusable: conf.focusable
})

for child in xs {
child.set_parent(self)
}

return self
}

fn row(children, opts) {
conf = merge({
spacing: 0,
padding: 0,
bg: null,
border: null,
border_width: 1,
align_y: "start",
on_event: null,
focusable: false
}, opts)

xs = children == null => [] || children
self = null

fn child_list() => xs

fn natural_width(child, ctx, avail_w) {
if child.width != null => return child.width
if child.flex != null and child.flex > 0 => return null
return child.measure(ctx, avail_w).w
}

fn measure(ctx, avail_w) {
pad = _normalize_padding(conf, 0)
inner_w = avail_w - pad.left - pad.right
total_w = pad.left + pad.right
max_h = 0
for i in range(len(xs)) {
w = natural_width(xs[i], ctx, inner_w)
        if w == null {
            w = 0
        }
m = xs[i].measure(ctx, w)
total_w = total_w + w
max_h = _max(max_h, m.h)
if i + 1 < len(xs) {
total_w = total_w + conf.spacing
}
}
return {w: avail_w, h: max_h + pad.top + pad.bottom}
}

fn layout(ctx, box_rect) {
ops = []
pad = _normalize_padding(conf, 0)
if conf.bg != null {
ops.push(W.fill_rect(box_rect.x, box_rect.y, box_rect.w, box_rect.h, conf.bg))
}
if conf.border != null {
ops.push(W.rect(box_rect.x, box_rect.y, box_rect.w, box_rect.h, conf.border, conf.border_width))
}

inner = _inset(box_rect, pad)
widths = []
fixed = 0
flex_total = 0

for child in xs {
if child.width != null {
widths.push(child.width)
fixed = fixed + child.width
} else if child.flex != null and child.flex > 0 {
widths.push(null)
flex_total = flex_total + child.flex
} else {
w = child.measure(ctx, inner.w).w
widths.push(w)
fixed = fixed + w
}
}

gaps = len(xs) <= 1 => 0 || conf.spacing * (len(xs) - 1)
remain = inner.w - fixed - gaps
if remain < 0 {
remain = 0
}

for i in range(len(xs)) {
if widths[i] == null {
share = flex_total <= 0 => 0 || int(remain * xs[i].flex / flex_total)
widths[i] = share
flex_total = flex_total - xs[i].flex
remain = remain - share
}
}

x = inner.x
for i in range(len(xs)) {
child_w = widths[i]
child_h = xs[i].measure(ctx, child_w).h
y = inner.y
if conf.align_y == "center" {
y = inner.y + int((inner.h - child_h) / 2)
}
if conf.align_y == "end" {
y = inner.y + inner.h - child_h
}
if conf.align_y == "stretch" {
child_h = inner.h
}
_append_all(ops, xs[i].layout(ctx, _rect(x, y, child_w, child_h)))
x = x + child_w
if i + 1 < len(xs) {
x = x + conf.spacing
}
}

return ops
}

self = _widget_base("row", {
children: child_list,
measure: measure,
layout: layout,
on_event: conf.on_event,
interactive: conf.on_event != null,
focusable: conf.focusable
})

for child in xs {
child.set_parent(self)
}

return self
}

fn stack(children, opts) {
conf = merge({
padding: 0,
bg: null,
border: null,
border_width: 1,
on_event: null,
focusable: false
}, opts)

xs = children == null => [] || children
self = null

fn child_list() => xs

fn measure(ctx, avail_w) {
pad = _normalize_padding(conf, 0)
inner_w = avail_w - pad.left - pad.right
max_h = 0
for child in xs {
max_h = _max(max_h, child.measure(ctx, inner_w).h)
}
return {w: avail_w, h: max_h + pad.top + pad.bottom}
}

fn layout(ctx, box_rect) {
ops = []
pad = _normalize_padding(conf, 0)
if conf.bg != null {
ops.push(W.fill_rect(box_rect.x, box_rect.y, box_rect.w, box_rect.h, conf.bg))
}
if conf.border != null {
ops.push(W.rect(box_rect.x, box_rect.y, box_rect.w, box_rect.h, conf.border, conf.border_width))
}

inner = _inset(box_rect, pad)
for child in xs {
_append_all(ops, child.layout(ctx, inner))
}
return ops
}

self = _widget_base("stack", {
children: child_list,
measure: measure,
layout: layout,
on_event: conf.on_event,
interactive: conf.on_event != null,
focusable: conf.focusable
})

for child in xs {
child.set_parent(self)
}

return self
}

fn align(child, opts) {
conf = merge({
x: "start",
y: "start"
}, opts)

self = null

fn child_list() {
if child == null => return []
return [child]
}

fn measure(ctx, avail_w) {
if child == null => return {w: avail_w, h: 0}
h = child.measure(ctx, avail_w).h
    if conf.height != null {
        h = conf.height
    }
return {w: avail_w, h: h}
}

fn layout(ctx, box_rect) {
if child == null => return []
child_size = child.measure(ctx, box_rect.w)
child_w = conf.child_width == null => child_size.w || conf.child_width
child_h = conf.child_height == null => child_size.h || conf.child_height
x = box_rect.x
y = box_rect.y

if conf.x == "center" {
x = box_rect.x + int((box_rect.w - child_w) / 2)
}
if conf.x == "end" {
x = box_rect.x + box_rect.w - child_w
}
if conf.x == "stretch" {
child_w = box_rect.w
}

if conf.y == "center" {
y = box_rect.y + int((box_rect.h - child_h) / 2)
}
if conf.y == "end" {
y = box_rect.y + box_rect.h - child_h
}
if conf.y == "stretch" {
child_h = box_rect.h
}

return child.layout(ctx, _rect(x, y, child_w, child_h))
}

self = _widget_base("align", {
children: child_list,
measure: measure,
layout: layout
})

if child != null {
child.set_parent(self)
}

return self
}

fn button(text, on_click, opts) {
conf = merge({
padding: {left: 16, top: 10, right: 16, bottom: 10},
font: null,
bg: null,
hover_bg: null,
active_bg: null,
border: null,
text_color: null,
focusable: true
}, opts)

self = null

fn measure(ctx, avail_w) {
font_cfg = _font_or(conf, ctx.theme.button_font)
metrics = _measure_text(ctx.gui, text, font_cfg)
pad = _normalize_padding(conf, conf.padding)
h = metrics.height + pad.top + pad.bottom
if h < 40 {
h = 40
}
return {w: avail_w, h: h}
}

fn layout(ctx, box_rect) {
font_cfg = _font_or(conf, ctx.theme.button_font)
metrics = _measure_text(ctx.gui, text, font_cfg)
fill = _color_or(conf, "bg", ctx.theme.accent)
if ctx.is_hot(self) {
fill = _color_or(conf, "hover_bg", ctx.theme.accent_hover)
}
if ctx.is_active(self) {
fill = _color_or(conf, "active_bg", ctx.theme.accent_active)
}
border = _color_or(conf, "border", ctx.theme.accent_border)
text_color = _color_or(conf, "text_color", ctx.theme.button_text)
x = int(box_rect.x + (box_rect.w - metrics.width) / 2)
y = int(box_rect.y + (box_rect.h - metrics.height) / 2)
ops = [
W.fill_rect(box_rect.x, box_rect.y, box_rect.w, box_rect.h, fill),
W.rect(box_rect.x, box_rect.y, box_rect.w, box_rect.h, border, 1),
W.text(x, y, text, {
color: text_color,
font: font_cfg
})
]
if ctx.has_focus(self) {
ops.push(W.rect(box_rect.x + 3, box_rect.y + 3, box_rect.w - 6, box_rect.h - 6, W.rgb(255, 255, 255), 1))
}
return ops
}

fn on_event_local(evt, ctx) {
if evt.type == "mouse_up" and evt.button == "left" and ctx.is_active(self) and ctx.is_hot(self) {
if on_click != null {
on_click()
}
return true
}
if evt.type == "key_down" and (evt.key == VK_RETURN or evt.key == VK_SPACE) {
if on_click != null {
on_click()
}
return true
}
return false
}

self = _widget_base("button", {
measure: measure,
layout: layout,
on_event: on_event_local,
interactive: true,
focusable: conf.focusable
})

return self
}

fn line_edit(state, opts) {
conf = merge({
padding: {left: 12, top: 10, right: 12, bottom: 10},
font: null,
color: null,
bg: null,
border: null,
focus_border: null,
caret_color: null,
selection_bg: null,
placeholder: null,
placeholder_color: null,
mouse_move_timer_ms: 16
}, opts)

self = null
mouse_move_timer_id = _next_widget_timer_id()
mouse_move_timer_active = false
pending_drag_move = null

fn measure(ctx, avail_w) {
font_cfg = _font_or(conf, ctx.theme.font)
metrics = _measure_text(ctx.gui, "Mg", font_cfg)
pad = _normalize_padding(conf, conf.padding)
h = metrics.height + pad.top + pad.bottom
if h < 40 {
h = 40
}
return {w: avail_w, h: h}
}

fn layout(ctx, box_rect) {
font_cfg = _font_or(conf, ctx.theme.font)
metrics = _measure_text(ctx.gui, "Mg", font_cfg)
pad = _normalize_padding(conf, conf.padding)
focused = ctx.has_focus(self)
value = state.get()
show_placeholder = value == "" and conf.placeholder != null and not focused
shown = show_placeholder => conf.placeholder || value
color = show_placeholder => _color_or(conf, "placeholder_color", ctx.theme.muted_text) || _color_or(conf, "color", ctx.theme.text)
border = focused => _color_or(conf, "focus_border", ctx.theme.input_focus_border) || _color_or(conf, "border", ctx.theme.input_border)
sel = show_placeholder => null || state.selection()
text_x = box_rect.x + pad.left
text_y = int(box_rect.y + (box_rect.h - metrics.height) / 2)
ops = [
W.fill_rect(box_rect.x, box_rect.y, box_rect.w, box_rect.h, _color_or(conf, "bg", ctx.theme.input_bg)),
W.rect(box_rect.x, box_rect.y, box_rect.w, box_rect.h, border, 1)
]
if sel != null {
sel_x1 = text_x + ctx.gui.measure_text(_slice_text(value, 0, sel.start), {font: font_cfg}).width
sel_x2 = text_x + ctx.gui.measure_text(_slice_text(value, 0, sel.end), {font: font_cfg}).width
ops.push(W.fill_rect(sel_x1, text_y, sel_x2 - sel_x1, metrics.height, _color_or(conf, "selection_bg", ctx.theme.selection_bg)))
}
ops.push(W.text(text_x, text_y, shown, {
color: color,
font: font_cfg
}))
if focused {
caret = state.caret()
caret_x = text_x + ctx.gui.measure_text(_slice_text(value, 0, caret), {font: font_cfg}).width
ops.push(W.fill_rect(caret_x, text_y, 1, metrics.height, _color_or(conf, "caret_color", ctx.theme.caret)))
}
return ops
}

fn on_event_local(evt, ctx) {
box_rect = self.bounds()
pad = _normalize_padding(conf, conf.padding)
font_cfg = _font_or(conf, ctx.theme.font)
text_x = box_rect.x + pad.left

fn stop_mouse_move_timer() {
if mouse_move_timer_active != true => return null
pcall(() -> ctx.window.native.kill_timer(mouse_move_timer_id))
@mouse_move_timer_active = false
return null
}

fn apply_drag_move(move_evt) {
col = _prefix_col(ctx.gui, font_cfg, state.get(), move_evt.x - text_x)
state.select_to(col)
return true
}

if evt.type == "mouse_down" and evt.button == "left" {
stop_mouse_move_timer()
@pending_drag_move = null
col = _prefix_col(ctx.gui, font_cfg, state.get(), evt.x - text_x)
state.begin_selection(col)
return true
}
if evt.type == "mouse_move" and ctx.is_active(self) {
if conf.mouse_move_timer_ms <= 0 {
return apply_drag_move(evt)
}
@pending_drag_move = {x: evt.x}
if mouse_move_timer_active != true {
ctx.window.native.set_timer(mouse_move_timer_id, conf.mouse_move_timer_ms)
@mouse_move_timer_active = true
}
return true
}
if evt.type == "timer" and evt.timer_id == mouse_move_timer_id {
stop_mouse_move_timer()
if not ctx.is_active(self) or pending_drag_move == null {
@pending_drag_move = null
return false
}
move_evt = @pending_drag_move
@pending_drag_move = null
return apply_drag_move(move_evt)
}
if evt.type == "mouse_up" and evt.button == "left" {
stop_mouse_move_timer()
if ctx.is_active(self) and pending_drag_move != null {
move_evt = @pending_drag_move
@pending_drag_move = null
return apply_drag_move(move_evt)
}
@pending_drag_move = null
}
if evt.type == "key_down" {
if _handle_text_shortcuts(evt, ctx, state, {multiline: false}) {
return true
}
if evt.key == VK_LEFT {
state.move_left()
return true
}
if evt.key == VK_RIGHT {
state.move_right()
return true
}
if evt.key == VK_HOME {
state.set_caret(0)
return true
}
if evt.key == VK_END {
state.set_caret(_char_len(state.get()))
return true
}
if evt.key == VK_BACK {
state.backspace()
return true
}
if evt.key == VK_DELETE {
state.delete_forward()
return true
}
if evt.key == VK_RETURN {
if conf.on_submit != null {
conf.on_submit(state.get())
}
return true
}
}
if evt.type == "char" {
if _is_control_input(evt) => return true
state.insert(evt.text)
return true
}
return false
}

self = _widget_base("line_edit", {
measure: measure,
layout: layout,
on_event: on_event_local,
interactive: true,
focusable: true
})

return self
}

fn editor(state, opts) {
conf = merge({
padding: {left: 12, top: 10, right: 12, bottom: 10},
font: null,
color: null,
bg: null,
border: null,
focus_border: null,
caret_color: null,
selection_bg: null,
mouse_move_timer_ms: 16
}, opts)

self = null
mouse_move_timer_id = _next_widget_timer_id()
mouse_move_timer_active = false
pending_drag_move = null

fn line_height(ctx) {
font_cfg = _font_or(conf, ctx.theme.mono_font)
return _measure_text(ctx.gui, "Mg", font_cfg).height
}

fn measure(ctx, avail_w) {
h = line_height(ctx)
lines = _line_texts(state.get())
total = len(lines) * h + _normalize_padding(conf, conf.padding).top + _normalize_padding(conf, conf.padding).bottom
if total < 120 {
total = 120
}
return {w: avail_w, h: total}
}

fn layout(ctx, box_rect) {
font_cfg = _font_or(conf, ctx.theme.mono_font)
pad = _normalize_padding(conf, conf.padding)
lh = _measure_text(ctx.gui, "Mg", font_cfg).height
lines = _line_texts(state.get())
focused = ctx.has_focus(self)
border = focused => _color_or(conf, "focus_border", ctx.theme.input_focus_border) || _color_or(conf, "border", ctx.theme.input_border)
sel = state.selection()
ops = [
W.fill_rect(box_rect.x, box_rect.y, box_rect.w, box_rect.h, _color_or(conf, "bg", ctx.theme.input_bg)),
W.rect(box_rect.x, box_rect.y, box_rect.w, box_rect.h, border, 1)
]

text_x = box_rect.x + pad.left
text_y = box_rect.y + pad.top
if sel != null {
start_lc = _line_col_of(state.get(), sel.start)
end_lc = _line_col_of(state.get(), sel.end)
for line in range(start_lc.line, end_lc.line + 1) {
line_text = line < len(lines) => lines[line] || ""
line_start = line == start_lc.line => start_lc.col || 0
line_end = line == end_lc.line => end_lc.col || _char_len(line_text)
sel_x1 = text_x + ctx.gui.measure_text(_slice_text(line_text, 0, line_start), {font: font_cfg}).width
sel_x2 = text_x + ctx.gui.measure_text(_slice_text(line_text, 0, line_end), {font: font_cfg}).width
ops.push(W.fill_rect(sel_x1, text_y + line * lh, sel_x2 - sel_x1, lh, _color_or(conf, "selection_bg", ctx.theme.selection_bg)))
}
}
for i in range(len(lines)) {
ops.push(W.text(text_x, text_y + i * lh, lines[i], {
color: _color_or(conf, "color", ctx.theme.text),
font: font_cfg
}))
}

if focused {
lc = state.line_col()
cur_line = lc.line < len(lines) => lines[lc.line] || ""
caret_x = text_x + ctx.gui.measure_text(_slice_text(cur_line, 0, lc.col), {font: font_cfg}).width
caret_y = text_y + lc.line * lh
ops.push(W.fill_rect(caret_x, caret_y, 1, lh, _color_or(conf, "caret_color", ctx.theme.caret)))
}

return ops
}

fn on_event_local(evt, ctx) {
box_rect = self.bounds()
pad = _normalize_padding(conf, conf.padding)
font_cfg = _font_or(conf, ctx.theme.mono_font)
lh = _measure_text(ctx.gui, "Mg", font_cfg).height
text_x = box_rect.x + pad.left
text_y = box_rect.y + pad.top

fn point_index(x, y) {
lines = _line_texts(state.get())
line = int((y - text_y) / lh)
line = _clamp(line, 0, len(lines) - 1)
col = _prefix_col(ctx.gui, font_cfg, lines[line], x - text_x)
return _index_from_line_col(state.get(), line, col)
}

fn stop_mouse_move_timer() {
if mouse_move_timer_active != true => return null
pcall(() -> ctx.window.native.kill_timer(mouse_move_timer_id))
@mouse_move_timer_active = false
return null
}

fn apply_drag_move(move_evt) {
state.select_to(point_index(move_evt.x, move_evt.y))
return true
}

if evt.type == "mouse_down" and evt.button == "left" {
stop_mouse_move_timer()
@pending_drag_move = null
state.begin_selection(point_index(evt.x, evt.y))
return true
}
if evt.type == "mouse_move" and ctx.is_active(self) {
if conf.mouse_move_timer_ms <= 0 {
return apply_drag_move(evt)
}
@pending_drag_move = {x: evt.x, y: evt.y}
if mouse_move_timer_active != true {
ctx.window.native.set_timer(mouse_move_timer_id, conf.mouse_move_timer_ms)
@mouse_move_timer_active = true
}
return true
}
if evt.type == "timer" and evt.timer_id == mouse_move_timer_id {
stop_mouse_move_timer()
if not ctx.is_active(self) or pending_drag_move == null {
@pending_drag_move = null
return false
}
move_evt = @pending_drag_move
@pending_drag_move = null
return apply_drag_move(move_evt)
}
if evt.type == "mouse_up" and evt.button == "left" {
stop_mouse_move_timer()
if ctx.is_active(self) and pending_drag_move != null {
move_evt = @pending_drag_move
@pending_drag_move = null
return apply_drag_move(move_evt)
}
@pending_drag_move = null
}
if evt.type == "key_down" {
if _handle_text_shortcuts(evt, ctx, state, {multiline: true}) {
return true
}
if evt.key == VK_LEFT {
state.move_left()
return true
}
if evt.key == VK_RIGHT {
state.move_right()
return true
}
if evt.key == VK_HOME {
state.move_home()
return true
}
if evt.key == VK_END {
state.move_end()
return true
}
if evt.key == VK_UP {
state.move_vertical(-1)
return true
}
if evt.key == VK_DOWN {
state.move_vertical(1)
return true
}
if evt.key == VK_BACK {
state.backspace()
return true
}
if evt.key == VK_DELETE {
state.delete_forward()
return true
}
if evt.key == VK_RETURN {
state.insert("\n")
return true
}
}
if evt.type == "char" {
if _is_control_input(evt) => return true
state.insert(evt.text)
return true
}
return false
}

self = _widget_base("editor", {
measure: measure,
layout: layout,
on_event: on_event_local,
interactive: true,
focusable: true
})

return self
}

fn ide_editor(state, opts) {
conf = merge({
padding: {left: 8, top: 8, right: 8, bottom: 8},
font: null,
line_no_font: null,
color: null,
bg: null,
border: null,
focus_border: null,
caret_color: null,
selection_bg: null,
gutter_bg: null,
gutter_text: null,
current_line_bg: null,
popup_bg: null,
popup_border: null,
popup_active_bg: null,
scrollbar_bg: null,
scrollbar_thumb: null,
scrollbar_thumb_hover: null,
scrollbar_thumb_active: null,
tab_text: "    ",
popup_max_items: 8,
gutter_min_width: 48,
scrollbar_width: 12,
mouse_move_timer_ms: 16
}, opts)

if state.refresh_completion == null or state.accept_completion == null {
error("sWidgets.ide_editor requires ide_editor_state")
}

self = null
scroll_line = 0
popup_offset = 0
follow_caret = true
scroll_dragging = false
scroll_drag_start_y = 0
scroll_drag_start_line = 0
mouse_move_timer_id = _next_widget_timer_id()
mouse_move_timer_active = false
pending_drag_move = null
cached_text = null
cached_lines = [""]
line_render_cache = {}
measure_cache = {}

fn editor_font(ctx) => _font_or(conf, ctx.theme.mono_font)
fn line_no_font(ctx) => conf.line_no_font == null => W.font("Consolas", 13) || conf.line_no_font
fn line_height(ctx) => measure_cached(ctx, "Mg", editor_font(ctx)).height
fn total_lines() => len(lines_now())

fn font_sig(font_cfg) {
face = font_cfg.face == null => "" || font_cfg.face
size = font_cfg.size == null => 0 || font_cfg.size
weight = font_cfg.weight == null => 0 || font_cfg.weight
italic = font_cfg.italic == true
underline = font_cfg.underline == true
return face + "|" + str(size) + "|" + str(weight) + "|" + str(italic) + "|" + str(underline)
}

fn sync_text_cache() {
text = state.get()
if text != @cached_text {
@cached_text = text
@cached_lines = _line_texts(text)
@line_render_cache = {}
}
return text
}

fn measure_cached(ctx, text, font_cfg) {
key = font_sig(font_cfg) + "\n" + text
cached = @measure_cache?[key]
if cached != null => return cached
metrics = ctx.gui.measure_text(text, {font: font_cfg})
@measure_cache[key] = metrics
return metrics
}

fn width_cached(ctx, text, font_cfg) => measure_cached(ctx, text, font_cfg).width

fn line_render_info(ctx, line_text) {
font_cfg = editor_font(ctx)
key = font_sig(font_cfg) + "\n" + line_text
cached = @line_render_cache?[key]
if cached != null => return cached
tokens = _syntax_tokenize_line(line_text)
widths = []
for token in tokens {
widths.push(token.text == "" => 0 || width_cached(ctx, token.text, font_cfg))
}
entry = {
tokens: tokens,
widths: widths
}
@line_render_cache[key] = entry
return entry
}

fn visible_capacity(ctx, box_rect) {
pad = _normalize_padding(conf, conf.padding)
inner_h = box_rect.h - pad.top - pad.bottom
cap = int(inner_h / line_height(ctx))
if cap < 1 {
cap = 1
}
return cap
}

fn lines_now() {
sync_text_cache()
return @cached_lines
}

fn max_scroll(ctx, box_rect) {
max_v = total_lines() - visible_capacity(ctx, box_rect)
if max_v < 0 {
max_v = 0
}
return max_v
}

fn clamp_scroll(ctx, box_rect) {
@scroll_line = _clamp(@scroll_line, 0, max_scroll(ctx, box_rect))
return @scroll_line
}

fn ensure_caret_visible(ctx, box_rect) {
cap = visible_capacity(ctx, box_rect)
lc = state.line_col()
if lc.line < @scroll_line {
@scroll_line = lc.line
}
if lc.line >= @scroll_line + cap {
@scroll_line = lc.line - cap + 1
}
clamp_scroll(ctx, box_rect)
return @scroll_line
}

fn scroll_to(ctx, box_rect, line) {
@scroll_line = line
@follow_caret = false
clamp_scroll(ctx, box_rect)
return @scroll_line
}

fn scroll_by(ctx, box_rect, delta) {
return scroll_to(ctx, box_rect, @scroll_line + delta)
}

fn sync_popup_offset() {
if not state.completion_active() {
@popup_offset = 0
return 0
}
count = len(state.completion_items())
max_off = count - conf.popup_max_items
if max_off < 0 {
max_off = 0
}
idx = state.completion_index()
if idx < @popup_offset {
@popup_offset = idx
}
if idx >= @popup_offset + conf.popup_max_items {
@popup_offset = idx - conf.popup_max_items + 1
}
@popup_offset = _clamp(@popup_offset, 0, max_off)
return @popup_offset
}

fn gutter_width(ctx) {
font_cfg = line_no_font(ctx)
digits = str(len(lines_now()))
width = width_cached(ctx, digits, font_cfg) + 18
if width < conf.gutter_min_width {
width = conf.gutter_min_width
}
return width
}

fn has_scrollbar(ctx, box_rect) => max_scroll(ctx, box_rect) > 0

fn scrollbar_rect(ctx, box_rect) {
if not has_scrollbar(ctx, box_rect) => return null
pad = _normalize_padding(conf, conf.padding)
return {
x: box_rect.x + box_rect.w - pad.right - conf.scrollbar_width,
y: box_rect.y + pad.top,
w: conf.scrollbar_width,
h: box_rect.h - pad.top - pad.bottom
}
}

fn scrollbar_thumb_rect(ctx, box_rect) {
track = scrollbar_rect(ctx, box_rect)
if track == null => return null
cap = visible_capacity(ctx, box_rect)
total = total_lines()
max_v = max_scroll(ctx, box_rect)
thumb_h = int(track.h * cap / total)
if thumb_h < 24 {
thumb_h = 24
}
if thumb_h > track.h {
thumb_h = track.h
}
thumb_y = track.y
if max_v > 0 and track.h > thumb_h {
thumb_y = track.y + int((track.h - thumb_h) * @scroll_line / max_v)
}
return {
x: track.x,
y: thumb_y,
w: track.w,
h: thumb_h
}
}

fn text_metrics(ctx, box_rect) {
pad = _normalize_padding(conf, conf.padding)
gut = gutter_width(ctx)
scrollbar_w = has_scrollbar(ctx, box_rect) => conf.scrollbar_width + 6 || 0
return {
pad: pad,
gutter: gut,
text_x: box_rect.x + pad.left + gut + 10,
text_y: box_rect.y + pad.top,
right_x: box_rect.x + box_rect.w - pad.right - scrollbar_w
}
}

fn caret_screen(ctx, box_rect) {
ensure_caret_visible(ctx, box_rect)
info = text_metrics(ctx, box_rect)
lines = lines_now()
lc = state.line_col()
cur_line = lc.line < len(lines) => lines[lc.line] || ""
x = info.text_x + width_cached(ctx, _slice_text(cur_line, 0, lc.col), editor_font(ctx))
y = info.text_y + (lc.line - @scroll_line) * line_height(ctx)
return {x: x, y: y}
}

fn popup_rect(ctx, box_rect) {
if not state.completion_active() => return null
sync_popup_offset()
items = state.completion_items()
count = len(items) - @popup_offset
if count <= 0 => return null
if count > conf.popup_max_items {
count = conf.popup_max_items
}
item_h = line_height(ctx) + 6
width = 180
for i in range(@popup_offset, @popup_offset + count) {
item = items[i]
text_w = width_cached(ctx, item.label, editor_font(ctx))
kind_w = width_cached(ctx, item.kind, line_no_font(ctx))
candidate = text_w + kind_w + 30
if candidate > width {
width = candidate
}
}
caret = caret_screen(ctx, box_rect)
y = caret.y + line_height(ctx) + 4
h = count * item_h + 8
x = caret.x + 6
if x + width > box_rect.x + box_rect.w - 4 {
x = box_rect.x + box_rect.w - width - 4
}
if x < box_rect.x + 4 {
x = box_rect.x + 4
}
if y + h > box_rect.y + box_rect.h - 4 {
y = caret.y - h - 4
}
if y < box_rect.y + 4 {
y = box_rect.y + 4
}
return {x: x, y: y, w: width, h: h, item_h: item_h, count: count}
}

fn popup_hit_index(ctx, box_rect, x, y) {
rect = popup_rect(ctx, box_rect)
if rect == null or not _contains(rect, x, y) => return null
row = int((y - rect.y - 4) / rect.item_h)
row = _clamp(row, 0, rect.count - 1)
return @popup_offset + row
}

fn scrollbar_hit(ctx, box_rect, x, y) {
track = scrollbar_rect(ctx, box_rect)
if track == null or not _contains(track, x, y) => return null
thumb = scrollbar_thumb_rect(ctx, box_rect)
if thumb != null and _contains(thumb, x, y) {
return {part: "thumb", track: track, thumb: thumb}
}
return {part: "track", track: track, thumb: thumb}
}

fn scroll_from_track_y(ctx, box_rect, y) {
track = scrollbar_rect(ctx, box_rect)
thumb = scrollbar_thumb_rect(ctx, box_rect)
if track == null or thumb == null => return @scroll_line
max_v = max_scroll(ctx, box_rect)
if max_v <= 0 => return scroll_to(ctx, box_rect, 0)
usable = track.h - thumb.h
if usable <= 0 => return scroll_to(ctx, box_rect, 0)
offset = y - track.y - int(thumb.h / 2)
offset = _clamp(offset, 0, usable)
line = int(max_v * offset / usable)
return scroll_to(ctx, box_rect, line)
}

fn point_index(ctx, box_rect, x, y) {
clamp_scroll(ctx, box_rect)
info = text_metrics(ctx, box_rect)
lines = lines_now()
lh = line_height(ctx)
line = @scroll_line + int((y - info.text_y) / lh)
line = _clamp(line, 0, len(lines) - 1)
col = _prefix_col(ctx.gui, editor_font(ctx), lines[line], x - info.text_x)
return _index_from_line_col(sync_text_cache(), line, col)
}

fn syntax_color(ctx, kind) {
if kind == "keyword" => return ctx.theme.syntax_keyword
if kind == "builtin" => return ctx.theme.syntax_builtin
if kind == "string" => return ctx.theme.syntax_string
if kind == "comment" => return ctx.theme.syntax_comment
if kind == "number" => return ctx.theme.syntax_number
if kind == "operator" => return ctx.theme.syntax_operator
return _color_or(conf, "color", ctx.theme.text)
}

fn scrollbar_thumb_color(ctx) {
if scroll_dragging {
return _color_or(conf, "scrollbar_thumb_active", ctx.theme.accent_active)
}
thumb = scrollbar_thumb_rect(ctx, self.bounds())
if thumb != null and ctx.is_hot(self) {
return _color_or(conf, "scrollbar_thumb_hover", ctx.theme.accent_hover)
}
return _color_or(conf, "scrollbar_thumb", ctx.theme.panel_border)
}

fn hide_completion() {
state.hide_completion()
@popup_offset = 0
return null
}

fn refresh_completion(force) {
opened = state.refresh_completion(force)
sync_popup_offset()
return opened
}

fn insert_newline() {
lc = state.line_col()
lines = lines_now()
cur = lc.line < len(lines) => lines[lc.line] || ""
left = _slice_text(cur, 0, lc.col)
indent = _leading_ws(cur)
trimmed = _rtrim_ws(left)
extra = (trimmed.ends_with("{") or trimmed.ends_with("[") or trimmed.ends_with("(")) => conf.tab_text || ""
state.insert("\n" + indent + extra)
@follow_caret = true
hide_completion()
return true
}

fn stop_mouse_move_timer(ctx) {
if mouse_move_timer_active != true => return null
pcall(() -> ctx.window.native.kill_timer(mouse_move_timer_id))
@mouse_move_timer_active = false
return null
}

fn apply_drag_move(ctx, box_rect, move_evt) {
if scroll_dragging {
track = scrollbar_rect(ctx, box_rect)
thumb = scrollbar_thumb_rect(ctx, box_rect)
max_v = max_scroll(ctx, box_rect)
if track != null and thumb != null and max_v > 0 and track.h > thumb.h {
delta_y = move_evt.y - @scroll_drag_start_y
usable = track.h - thumb.h
delta_line = int(max_v * delta_y / usable)
scroll_to(ctx, box_rect, @scroll_drag_start_line + delta_line)
}
return true
}
state.select_to(point_index(ctx, box_rect, move_evt.x, move_evt.y))
@follow_caret = true
hide_completion()
return true
}

fn measure(ctx, avail_w) {
lh = line_height(ctx)
h = len(lines_now()) * lh + _normalize_padding(conf, conf.padding).top + _normalize_padding(conf, conf.padding).bottom
if h < 180 {
h = 180
}
return {w: avail_w, h: h}
}

fn layout(ctx, box_rect) {
started_ms = _profile_begin()
font_cfg = editor_font(ctx)
line_font = line_no_font(ctx)
lh = line_height(ctx)
pad = _normalize_padding(conf, conf.padding)
if follow_caret {
ensure_caret_visible(ctx, box_rect)
} else {
clamp_scroll(ctx, box_rect)
}
sync_popup_offset()
lines = lines_now()
text_now = sync_text_cache()
sel = state.selection()
focused = ctx.has_focus(self)
border = focused => _color_or(conf, "focus_border", ctx.theme.input_focus_border) || _color_or(conf, "border", ctx.theme.input_border)
info = text_metrics(ctx, box_rect)
cap = visible_capacity(ctx, box_rect)
start_line = @scroll_line
stop_line = start_line + cap
if stop_line > len(lines) {
stop_line = len(lines)
}

ops = [
W.fill_rect(box_rect.x, box_rect.y, box_rect.w, box_rect.h, _color_or(conf, "bg", ctx.theme.input_bg)),
W.rect(box_rect.x, box_rect.y, box_rect.w, box_rect.h, border, 1),
W.fill_rect(box_rect.x + pad.left, box_rect.y + pad.top, info.gutter, box_rect.h - pad.top - pad.bottom, _color_or(conf, "gutter_bg", ctx.theme.gutter_bg)),
W.fill_rect(box_rect.x + pad.left + info.gutter + 4, box_rect.y + pad.top, 1, box_rect.h - pad.top - pad.bottom, ctx.theme.panel_border)
]

lc = state.line_col()
if lc.line >= start_line and lc.line < stop_line {
cur_y = info.text_y + (lc.line - start_line) * lh
ops.push(W.fill_rect(info.text_x - 4, cur_y, box_rect.x + box_rect.w - pad.right - info.text_x, lh, _color_or(conf, "current_line_bg", ctx.theme.current_line_bg)))
}

if sel != null {
start_lc = _line_col_of(text_now, sel.start)
end_lc = _line_col_of(text_now, sel.end)
for line in range(start_line, stop_line) {
if line < start_lc.line or line > end_lc.line => continue
line_text = lines[line]
line_start = line == start_lc.line => start_lc.col || 0
line_end = line == end_lc.line => end_lc.col || _char_len(line_text)
sel_x1 = info.text_x + width_cached(ctx, _slice_text(line_text, 0, line_start), font_cfg)
sel_x2 = info.text_x + width_cached(ctx, _slice_text(line_text, 0, line_end), font_cfg)
ops.push(W.fill_rect(sel_x1, info.text_y + (line - start_line) * lh, sel_x2 - sel_x1, lh, _color_or(conf, "selection_bg", ctx.theme.selection_bg)))
}
}

for line in range(start_line, stop_line) {
y = info.text_y + (line - start_line) * lh
line_text = lines[line]
line_label = str(line + 1)
line_w = width_cached(ctx, line_label, line_font)
ops.push(W.text(box_rect.x + pad.left + info.gutter - line_w - 8, y, line_label, {
color: _color_or(conf, "gutter_text", ctx.theme.gutter_text),
font: line_font
}))

sx = info.text_x
render_info = line_render_info(ctx, line_text)
tokens = render_info.tokens
token_widths = render_info.widths
for ti in range(len(tokens)) {
token = tokens[ti]
if token.text == "" => continue
ops.push(W.text(sx, y, token.text, {
color: syntax_color(ctx, token.kind),
font: font_cfg
}))
sx = sx + token_widths[ti]
}
}

if focused and lc.line >= start_line and lc.line < stop_line {
cur_line = lc.line < len(lines) => lines[lc.line] || ""
caret_x = info.text_x + width_cached(ctx, _slice_text(cur_line, 0, lc.col), font_cfg)
caret_y = info.text_y + (lc.line - start_line) * lh
ops.push(W.fill_rect(caret_x, caret_y, 1, lh, _color_or(conf, "caret_color", ctx.theme.caret)))
}

track = scrollbar_rect(ctx, box_rect)
thumb = scrollbar_thumb_rect(ctx, box_rect)
if track != null and thumb != null {
ops.push(W.fill_rect(track.x, track.y, track.w, track.h, _color_or(conf, "scrollbar_bg", ctx.theme.gutter_bg)))
ops.push(W.fill_rect(thumb.x, thumb.y, thumb.w, thumb.h, scrollbar_thumb_color(ctx)))
}

rect = popup_rect(ctx, box_rect)
if rect != null {
items = state.completion_items()
ops.push(W.fill_rect(rect.x, rect.y, rect.w, rect.h, _color_or(conf, "popup_bg", ctx.theme.popup_bg)))
ops.push(W.rect(rect.x, rect.y, rect.w, rect.h, _color_or(conf, "popup_border", ctx.theme.popup_border), 1))
for i in range(rect.count) {
idx = @popup_offset + i
item = items[idx]
y = rect.y + 4 + i * rect.item_h
if idx == state.completion_index() {
ops.push(W.fill_rect(rect.x + 2, y, rect.w - 4, rect.item_h, _color_or(conf, "popup_active_bg", ctx.theme.popup_active_bg)))
}
ops.push(W.text(rect.x + 8, y + 2, item.label, {
color: syntax_color(ctx, item.kind == "kw" => "keyword" || item.kind == "builtin" => "builtin" || item.kind == "member" => "operator" || "text"),
font: font_cfg
}))
kind_w = width_cached(ctx, item.kind, line_font)
ops.push(W.text(rect.x + rect.w - kind_w - 8, y + 3, item.kind, {
color: ctx.theme.muted_text,
font: line_font
}))
}
}

_profile_end("ide_editor.layout", started_ms)
return ops
}

fn on_event_local(evt, ctx) {
box_rect = self.bounds()

if evt.type == "timer" and evt.timer_id == mouse_move_timer_id {
stop_mouse_move_timer(ctx)
if not ctx.is_active(self) or pending_drag_move == null {
@pending_drag_move = null
return false
}
move_evt = @pending_drag_move
@pending_drag_move = null
return apply_drag_move(ctx, box_rect, move_evt)
}

if evt.type == "mouse_wheel" {
scroll_by(ctx, box_rect, -int(evt.delta / 120) * 3)
return true
}

if evt.type == "mouse_down" and evt.button == "left" {
stop_mouse_move_timer(ctx)
@pending_drag_move = null
popup_idx = popup_hit_index(ctx, box_rect, evt.x, evt.y)
if popup_idx != null {
state.set_completion_index(popup_idx)
state.accept_completion()
@popup_offset = 0
return true
}
scroll_hit = scrollbar_hit(ctx, box_rect, evt.x, evt.y)
if scroll_hit != null {
hide_completion()
if scroll_hit.part == "thumb" {
@scroll_dragging = true
@scroll_drag_start_y = evt.y
@scroll_drag_start_line = @scroll_line
return true
}
scroll_from_track_y(ctx, box_rect, evt.y)
return true
}
state.begin_selection(point_index(ctx, box_rect, evt.x, evt.y))
@follow_caret = true
hide_completion()
return true
}

if evt.type == "mouse_move" and ctx.is_active(self) {
if conf.mouse_move_timer_ms <= 0 {
return apply_drag_move(ctx, box_rect, evt)
}
@pending_drag_move = {x: evt.x, y: evt.y}
if mouse_move_timer_active != true {
ctx.window.native.set_timer(mouse_move_timer_id, conf.mouse_move_timer_ms)
@mouse_move_timer_active = true
}
return true
}

if evt.type == "mouse_up" and evt.button == "left" {
stop_mouse_move_timer(ctx)
did_move = false
if ctx.is_active(self) and pending_drag_move != null {
move_evt = @pending_drag_move
@pending_drag_move = null
did_move = apply_drag_move(ctx, box_rect, move_evt)
} else {
@pending_drag_move = null
}
if scroll_dragging {
@scroll_dragging = false
return true
}
if did_move == true {
return true
}
}

if evt.type == "key_down" {
if _handle_text_shortcuts(evt, ctx, state, {multiline: true}) {
@follow_caret = true
hide_completion()
return true
}
if evt.ctrl == true and evt.key == VK_SPACE {
@follow_caret = true
refresh_completion(true)
return true
}

if state.completion_active() {
if evt.key == VK_UP {
state.move_completion(-1)
sync_popup_offset()
return true
}
if evt.key == VK_DOWN {
state.move_completion(1)
sync_popup_offset()
return true
}
if evt.key == VK_TAB or evt.key == VK_RETURN {
state.accept_completion()
@popup_offset = 0
return true
}
if evt.key == VK_ESCAPE {
hide_completion()
return true
}
}

if evt.key == VK_LEFT {
state.move_left()
@follow_caret = true
hide_completion()
return true
}
if evt.key == VK_RIGHT {
state.move_right()
@follow_caret = true
hide_completion()
return true
}
if evt.key == VK_HOME {
state.move_home()
@follow_caret = true
hide_completion()
return true
}
if evt.key == VK_END {
state.move_end()
@follow_caret = true
hide_completion()
return true
}
if evt.key == VK_UP {
state.move_vertical(-1)
@follow_caret = true
hide_completion()
return true
}
if evt.key == VK_DOWN {
state.move_vertical(1)
@follow_caret = true
hide_completion()
return true
}
if evt.key == VK_BACK {
state.backspace()
@follow_caret = true
refresh_completion(false)
return true
}
if evt.key == VK_DELETE {
state.delete_forward()
@follow_caret = true
refresh_completion(false)
return true
}
if evt.key == VK_TAB {
state.insert(conf.tab_text)
@follow_caret = true
hide_completion()
return true
}
if evt.key == VK_RETURN {
return insert_newline()
}
}

if evt.type == "char" {
if _is_control_input(evt) => return true
state.insert(evt.text)
@follow_caret = true
if evt.text == "." or _is_ident_part(evt.text) {
refresh_completion(false)
} else {
hide_completion()
}
return true
}

return false
}

self = _widget_base("ide_editor", {
measure: measure,
layout: layout,
on_event: on_event_local,
interactive: true,
focusable: true
})

return self
}

fn custom(spec) {
conf = _copy_map(spec, "sWidgets.custom spec")
return _widget_base("custom", conf)
}

fn app(opts) {
gui = W.start(opts)
windows = {}
running = false
style = default_theme()
conf = _copy_map(opts, "sWidgets.app opts")

fn normalize_redraw_interval_ms(value) {
ms = value == null => 16 || value
if ms > 0 and ms < 16 {
return 16
}
return ms
}

default_redraw_interval_ms = normalize_redraw_interval_ms(conf.redraw_interval_ms)
default_redraw_timer_id = conf.redraw_timer_id == null => 1 || conf.redraw_timer_id
default_defer_redraw = conf.defer_redraw == null => true || conf.defer_redraw
script_event_queue = SSTL.queue()
script_prefetch_limit = conf.script_prefetch_limit == null => 64 || conf.script_prefetch_limit
script_pulled_total = 0
script_dispatched_total = 0
script_queue_high_watermark = 0
script_last_event_type = null
timer_handlers = {}

fn note_script_queue() {
size_now = script_event_queue.size()
if size_now > @script_queue_high_watermark {
@script_queue_high_watermark = size_now
}
return size_now
}

fn queue_script_event(evt) {
@script_event_queue.push(evt)
@script_pulled_total = @script_pulled_total + 1
note_script_queue()
return null
}

fn pump_script_events(limit) {
want = limit == null => @script_prefetch_limit || limit
if want <= 0 => return 0
added = 0
while added < want {
evt = @gui.poll_event(0)
if evt == null => break
queue_script_event(evt)
added = added + 1
}
return added
}

fn pop_script_event(timeout_seconds) {
if @script_event_queue.empty() {
evt = @gui.poll_event(timeout_seconds)
if evt == null => return null
queue_script_event(evt)
}
if @script_prefetch_limit > 1 {
pump_script_events(@script_prefetch_limit - 1)
}
return @script_event_queue.pop()
}

fn stats() {
gui_stats = pcall(() -> @gui.stats())
wingui_queue_len = null
wingui_fps = null
wingui_profile = {}
if gui_stats.ok {
wingui_queue_len = gui_stats.value.queue_len
wingui_fps = gui_stats.value.fps
    wingui_profile = gui_stats.value.profile == null => {} || gui_stats.value.profile
}
return {
wingui_queue_len: wingui_queue_len,
wingui_fps: wingui_fps,
wingui_profile: wingui_profile,
script_queue_len: script_event_queue.size(),
script_queue_high_watermark: @script_queue_high_watermark,
script_pulled_total: @script_pulled_total,
script_dispatched_total: @script_dispatched_total,
script_last_event_type: @script_last_event_type,
sw_profile: _profile_snapshot()
}
}

fn set_timer_handler(timer_id, handler) {
@timer_handlers[str(timer_id)] = handler
return null
}

fn clear_timer_handler(timer_id) {
@timer_handlers.remove(str(timer_id))
return null
}

fn profile_report() {
st = stats()
lines = []
lines.push("[profile] WQ=" + str(st.wingui_queue_len == null => -1 || int(st.wingui_queue_len)) + " SQ=" + str(int(st.script_queue_len)) + "/" + str(int(st.script_queue_high_watermark)) + " WFPS=" + str(st.wingui_fps == null => -1 || int(st.wingui_fps + 0.5)))
for name in ["render_scene", "measure_text", "poll_event"] {
bucket = st.wingui_profile?[name]
if bucket != null {
lines.push("[wingui] " + _fmt_profile_bucket(name, bucket))
}
}
for name in ["redraw_window", "dispatch", "dispatch.mouse_move", "_prefix_col", "_prefix_col.build", "_syntax_tokenize_line", "ide_editor.layout"] {
bucket = st.sw_profile?[name]
if bucket != null {
lines.push("[sWidgets] " + _fmt_profile_bucket(name, bucket))
}
}
return lines.join("\n")
}

fn render_ctx(win) {
fn is_hot(widget) => widget != null and widget == win.hot
fn is_active(widget) => widget != null and widget == win.active
fn has_focus(widget) => widget != null and widget == win.focus

return {
gui: @gui,
theme: win.theme,
is_hot: is_hot,
is_active: is_active,
has_focus: has_focus
}
}

fn focus_owner(widget) {
cur = widget
while cur != null {
if cur.focusable() => return cur
cur = cur.parent()
}
return null
}

fn event_ctx(win) {
fn is_hot(widget) => widget != null and widget == win.hot
fn is_active(widget) => widget != null and widget == win.active
fn has_focus(widget) => widget != null and widget == win.focus
fn request_focus(widget) {
next = focus_owner(widget)
if next == win.focus => return null

old = win.focus
win.focus = next

if old != null {
old.on_blur(event_ctx(win))
}
if next != null {
next.on_focus(event_ctx(win))
}
return null
}

return {
app: @app_obj,
gui: @gui,
window: win,
theme: win.theme,
is_hot: is_hot,
is_active: is_active,
has_focus: has_focus,
request_focus: request_focus
}
}

fn redraw_window(win) {
started_ms = _profile_begin()
if win.closed => return null
client = win.native.client_rect()
box_rect = _rect(0, 0, client.width, client.height)
ops = [W.clear(win.background)]
_append_all(ops, win.root.layout(render_ctx(win), box_rect))
win.dirty = false
win.native.present(ops)
_profile_end("redraw_window", started_ms)
return ops
}

fn stop_redraw_timer(win) {
if win.redraw_timer_active != true => return null
pcall(() -> win.native.kill_timer(win.redraw_timer_id))
win.redraw_timer_active = false
return null
}

fn request_redraw(win, immediate) {
if win == null or win.closed => return null
win.dirty = true
force_now = immediate == true or win.defer_redraw != true or win.redraw_interval_ms <= 0
if force_now {
stop_redraw_timer(win)
return redraw_window(win)
}
if win.redraw_timer_active != true {
win.native.set_timer(win.redraw_timer_id, win.redraw_interval_ms)
win.redraw_timer_active = true
}
return null
}

fn close_window(win) {
if win.closed => return null
win.closed = true
stop_redraw_timer(win)
pcall(() -> win.native.close())
@windows.remove(str(win.id))
if len(@windows) == 0 {
@running = false
}
return null
}

fn bubble(win, target, evt) {
cur = target
ctx = event_ctx(win)
while cur != null {
used = cur.on_event(evt, ctx)
if used == true => return true
cur = cur.parent()
}
return false
}

fn dispatch(win, evt) {
started_ms = _profile_begin()
if win == null or win.closed => return null

if evt.type == "resize" {
request_redraw(win, false)
_profile_end("dispatch", started_ms)
return evt
}

if evt.type == "paint" {
_profile_end("dispatch", started_ms)
return evt
}

if evt.type == "timer" {
if evt.timer_id == win.redraw_timer_id {
stop_redraw_timer(win)
if win.dirty == true {
redraw_window(win)
}
_profile_end("dispatch", started_ms)
return evt
}
handler = @timer_handlers?[str(evt.timer_id)]
if handler != null {
used = handler(evt, event_ctx(win))
if used == true {
request_redraw(win, false)
}
_profile_end("dispatch", started_ms)
return evt
}
target = win.focus == null => win.root || win.focus
used = bubble(win, target, evt)
if used == true {
request_redraw(win, false)
}
_profile_end("dispatch", started_ms)
return evt
}

if evt.type == "mouse_leave" {
changed = win.hot != null
win.hot = null
if changed {
request_redraw(win, false)
}
_profile_end("dispatch", started_ms)
return evt
}

if evt.type == "mouse_move" {
move_started_ms = _profile_begin()
next_hot = win.root.hit(evt.x, evt.y)
changed = next_hot != win.hot
win.hot = next_hot
target = win.active == null => win.hot || win.active
used = bubble(win, target, evt)
if changed or used == true {
request_redraw(win, false)
}
_profile_end("dispatch.mouse_move", move_started_ms)
_profile_end("dispatch", started_ms)
return evt
}

if evt.type == "mouse_wheel" {
target = win.hot == null => win.focus || win.hot
used = bubble(win, target, evt)
if used == true {
request_redraw(win, false)
}
_profile_end("dispatch", started_ms)
return evt
}

if evt.type == "mouse_down" and evt.button == "left" {
next_hot = win.root.hit(evt.x, evt.y)
hot_changed = next_hot != win.hot
win.hot = next_hot
active_changed = win.active != win.hot
win.active = win.hot
old_focus = win.focus
event_ctx(win).request_focus(win.hot)
bubble(win, win.hot, evt)
focus_changed = old_focus != win.focus
if hot_changed or active_changed or focus_changed {
request_redraw(win, false)
}
_profile_end("dispatch", started_ms)
return evt
}

if evt.type == "mouse_up" and evt.button == "left" {
next_hot = win.root.hit(evt.x, evt.y)
hot_changed = next_hot != win.hot
win.hot = next_hot
target = win.active == null => win.hot || win.active
bubble(win, target, evt)
active_changed = win.active != null
win.active = null
if hot_changed or active_changed {
request_redraw(win, false)
}
_profile_end("dispatch", started_ms)
return evt
}

if evt.type == "key_down" or evt.type == "key_up" or evt.type == "char" {
used = bubble(win, win.focus, evt)
if used == true {
request_redraw(win, false)
}
_profile_end("dispatch", started_ms)
return evt
}

if evt.type == "close" {
close_window(win)
_profile_end("dispatch", started_ms)
return evt
}

_profile_end("dispatch", started_ms)
return evt
}

fn create_window(opts2) {
conf = _copy_map(opts2, "sWidgets.create_window opts")
if conf.root == null => error("sWidgets.create_window requires root widget")

native = @gui.create_window({
title: conf.title == null => "sWidgets" || conf.title,
width: conf.width == null => 800 || conf.width,
height: conf.height == null => 600 || conf.height,
visible: conf.visible == null => true || conf.visible,
resizable: conf.resizable == null => true || conf.resizable
})

conf.root.set_parent(null)

win = {
id: native.id,
native: native,
root: conf.root,
hot: null,
active: null,
focus: null,
closed: false,
dirty: false,
redraw_timer_active: false,
redraw_interval_ms: conf.redraw_interval_ms == null => default_redraw_interval_ms || normalize_redraw_interval_ms(conf.redraw_interval_ms),
redraw_timer_id: conf.redraw_timer_id == null => default_redraw_timer_id || conf.redraw_timer_id,
defer_redraw: conf.defer_redraw == null => default_defer_redraw || conf.defer_redraw,
theme: conf.theme == null => @style || conf.theme,
background: conf.background == null => @style.window_bg || conf.background
}

fn redraw() => request_redraw(@win, true)
fn close() => close_window(@win)
fn dispatch_local(evt) => dispatch(@win, evt)
fn set_root(root_widget) {
root_widget.set_parent(null)
@win.root = root_widget
request_redraw(@win, true)
return null
}
fn focused() => @win.focus

win.redraw = redraw
win.close = close
win.dispatch = dispatch_local
win.set_root = set_root
win.focused = focused

@windows[str(win.id)] = win
request_redraw(win, true)
return win
}

fn step(timeout_seconds) {
evt = pop_script_event(timeout_seconds)
if evt == null => return null
@script_dispatched_total = @script_dispatched_total + 1
@script_last_event_type = evt.type
win = @windows?[str(evt.window_id)]
if win != null {
dispatch(win, evt)
}
return evt
}

fn run() {
@running = true
while @running {
if len(@windows) == 0 => break
step(0.05)
}
return null
}

fn stop() {
ids = @windows.keys()
for key in ids {
win = @windows?[key]
if win != null {
pcall(() -> win.native.close())
}
}
@windows = {}
@running = false
return @gui.stop()
}

app_obj = {
gui: gui,
theme: style,
create_window: create_window,
stats: stats,
profile_report: profile_report,
set_timer_handler: set_timer_handler,
clear_timer_handler: clear_timer_handler,
step: step,
run: run,
stop: stop
}

return app_obj
}
