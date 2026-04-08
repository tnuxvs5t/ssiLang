# ssiLang v3 Concrete Design

This document freezes the v3 surface language before the implementation is rebuilt.
The primary design goal is to solve two long-standing problems without adding runtime cost:

1. Updating outer lexical state is awkward and non-obvious.
2. Higher-order code is too noisy because trivial lambdas require explicit names.

v3 solves these by separating two concerns:

- `@` is only for context.
- `$` is only for placeholders.

That separation is the core consistency rule. No syntax in v3 is allowed to blur it.

## 1. Design Principles

### 1.1 One symbol, one job

- `@` answers: "which context am I referring to?"
- `$` answers: "which argument position is this?"

### 1.2 No hidden runtime model change

v3 is mostly syntax + lexical lookup sugar.
The runtime model remains a tree of lexical environments plus ordinary values.

### 1.3 Local stays local

- `x = ...` still binds or rebinds in the current environment.
- `@x = ...` never creates a local. It targets an existing outer lexical binding.

### 1.3.1 Actual lexical boundaries

In the current v3 implementation, not every `{ ... }` creates a fresh lexical environment.

New lexical environments are introduced by:

- function calls
- context blocks and context expressions

Ordinary control-flow blocks attached to `if`, `while`, and `for` are grouping blocks executed in the current lexical environment.

This is intentional in the concrete implementation because it keeps loop accumulators and ordinary imperative updates ergonomic, while `@x` remains focused on true outer lexical access across closure/context boundaries.

### 1.4 Placeholder sugar must desugar cleanly

Placeholder lowering is boundary-based.
At a given expression boundary, if that whole expression contains `$`, that whole expression desugars to an ordinary lambda with deterministic arity.

Explicit lambda/function bodies form new lowering boundaries.

## 2. Syntax Overview

v3 introduces four new forms:

- `@x`
- `@`
- `@(expr) { ... }`
- `$`, `$1`, `$2`, ...

Examples:

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

label = @(user) => @.name + "#" + str(@.id)
```

## 3. `@` Family: Context Syntax

`@` never means placeholder. `@` never auto-creates a function.

### 3.1 `@x`: outer lexical lookup

`@x` means: resolve `x` in the nearest enclosing lexical environment outside the current one.

Read:

```sl
fn outer() {
    base = 10
    fn inner(x) {
        return x + @base
    }
    return inner
}
```

Write:

```sl
fn counter(start) {
    total = start
    fn add(n) {
        @total = @total + n
        return @total
    }
    return add
}
```

Rules:

- Search starts at `current_env.parent`, never at `current_env`.
- The nearest matching outer binding wins.
- If no outer binding exists, evaluation fails.
- `@x = ...` fails if `x` does not already exist in an outer lexical environment.
- `@x` is a valid assignment target.

This intentionally replaces the old pattern where `&ref` was needed just to mutate captured state.

### 3.2 `@`: current value context

Bare `@` means the current anonymous value context.

Examples:

```sl
@(user) {
    print(@.name)
    print(@.age)
}

first = @(arr) => @[0]
```

Rules:

- Bare `@` evaluates to the current context value.
- If no value context is active, evaluation fails.
- Because `@` is a value, normal postfix access works:
  - `@.name`
  - `@[0]`
  - `@?.name`
  - `@?[1]`

### 3.3 `@(expr) { ... }`: context block

This form evaluates `expr`, pushes it as the current value context, then runs the block in a child lexical environment.

```sl
@(config.server) {
    host = @.host
    port = @.port
}
```

Semantics:

1. Evaluate `expr` in the current environment.
2. Create a child environment.
3. Store the evaluated value as that child environment's current value context.
4. Execute the block in that child environment.

Properties:

- Locals created in the block stay in the block.
- Outer lexical variables are still reachable with `@x`.
- The current value context is reachable with bare `@`.

Note:

- this block does create a child lexical environment
- plain `if` / `while` / `for` blocks do not

### 3.4 `@(expr) => value_expr`: context expression

This is the expression form of a context block.

```sl
label = @(user) => @.name + "#" + str(@.id)
```

Semantics:

1. Evaluate `expr`.
2. Create a child environment with that value as the current value context.
3. Evaluate `value_expr` in the child environment.
4. Return the result.

This is not a lambda. It runs immediately.

### 3.5 Context nesting

Context scopes stack naturally.

```sl
@(request) {
    @( @.user ) {
        print(@.name)
        print(@request_id)
    }
}
```

Interpretation:

- Inner bare `@` refers to `request.user`.
- `@request_id` still refers to an outer lexical binding named `request_id`.

## 4. `$` Family: Placeholder Syntax

`$` never means context. `$` never performs lexical lookup.

### 4.1 `$` and `$n`

- `$` is shorthand for `$1`.
- `$1`, `$2`, `$3`, ... refer to positional anonymous parameters.

Examples:

```sl
inc = $ + 1
add = $1 + $2
pick_name = $.name
pair = [$1, $2]
```

### 4.2 Auto-lambda desugaring

Placeholder desugaring happens at the current expression boundary, not at the smallest sub-expression that happens to mention `$`.

Examples:

```sl
$ + 1
```

desugars to:

```sl
x1 -> x1 + 1
```

and:

```sl
$1 + $2
```

desugars to:

```sl
(x1, x2) -> x1 + x2
```

The exact generated parameter names are implementation-private.

Examples that must hold:

```sl
len($.notes)
```

desugars as if it were:

```sl
x1 -> len(x1.notes)
```

not:

```sl
len(x1 -> x1.notes)
```

and:

```sl
@(settings) => $.name + @.suffix
```

means:

```sl
@(settings) => (x1 -> x1.name + @.suffix)
```

because the body of the context expression is its own boundary.

### 4.3 Arity rule

The arity of a placeholder expression is:

- `1` if only bare `$` appears.
- Otherwise the maximum index used among `$n`.

Examples:

- `$ + 1` -> arity 1
- `$1 + $2` -> arity 2
- `$3` -> arity 3

### 4.4 Mixing `$` and `$n`

Bare `$` is identical to `$1`.

```sl
$ + $2
```

means:

```sl
$1 + $2
```

### 4.5 Placeholder scope boundary

Placeholder desugaring is lexical and stops at explicit function boundaries.
The practical rule is:

- statement/expression forms first build a raw expression tree
- then lowering decides whether the current expression node becomes a lambda
- lowering does not cross explicit lambda/function bodies
- certain higher-order call argument positions may be normalized independently

Examples:

```sl
map($ + 1)
reduce($1 + $2, 0)
```

The placeholder expression becomes the lambda argument passed to the call.

An explicit lambda remains an explicit lambda:

```sl
x -> x + 1
```

No extra transformation should be applied inside already-lambda-owned parameter lists.

Examples that must hold:

```sl
map($ + 1)
filter($.age >= 18)
reduce($1 + $2, 0)
map(len($.notes))
```

These are interpreted respectively as:

```sl
map(x1 -> x1 + 1)
filter(x1 -> x1.age >= 18)
reduce((x1, x2) -> x1 + x2, 0)
map(x1 -> len(x1.notes))
```

### 4.6 Placeholder usage in practice

```sl
nums |> map($ * 2)
nums |> filter($ > 0)
nums |> reduce($1 + $2, 0)
users |> map($.name)
users |> find($.id == 42)
```

### 4.7 Map key rule

Map keys are strings.

That means:

- `{name: "Alice"}` is valid
- `m["name"]` is valid
- `m[1]` is a runtime error
- `m[1] = v` is a runtime error
- `&ref m[1]` is a runtime error

Safe access does not change key typing rules:

- `m?["name"]` is valid
- `m?[1]` is still a runtime error

### 4.8 Sequence index rule

List and string indices are integer-valued numbers.

That means:

- `arr[0]` is valid
- `arr[1.5]` is a runtime error
- `arr["0"]` is a runtime error
- `"abc"[1]` is valid
- `"abc"["1"]` is a runtime error
- `arr[1] = v` uses the same integer-index rule
- `&ref arr[1]` uses the same integer-index rule

Safe access does not suppress type errors on the base value or on the index itself.
It only suppresses null-base / missing / out-of-range cases that are already valid for that container kind.

That means:

- `arr?[99]` returns `null`
- `m?["missing"]` returns `null`
- `5?[0]` is a runtime error
- `arr?["x"]` is a runtime error

## 5. Interaction Between `@` and `$`

This is the important part: both forms may appear in the same expression, but they keep their separate meanings.

```sl
fn make_adder(base) {
    return $ + @base
}
```

Desugaring:

- `$` creates a lambda.
- `@base` captures outer lexical state.

Equivalent to:

```sl
fn make_adder(base) {
    return x -> x + @base
}
```

Inside a context block:

```sl
@(user) => $.name + ":" + @.role
```

is valid when the outer expression expects a function.

Interpretation:

- `$` is the placeholder parameter.
- `@` is the current context value introduced by `@(user)`.

This is legal and useful. The symbols do not conflict.

## 6. Assignment Rules

v3 assignment is intentionally strict.

### 6.1 Local assignment

```sl
x = 1
```

Means: bind or rebind `x` in the current environment.

### 6.2 Outer lexical assignment

```sl
@x = 1
```

Means: assign to an existing binding named `x` in an outer lexical environment.

### 6.3 Context-member assignment

Because bare `@` is an ordinary value, member and index assignment work naturally:

```sl
@(cfg) {
    @.port = @.port + 1
    @[0] = 42
}
```

Whether the assignment mutates shared structure depends on the normal container semantics.

## 7. Concrete Parsing Decisions

This section is intentionally implementation-oriented.

### 7.1 New tokens

The lexer must add:

- `tAT` for `@`
- `tDOLLAR` for `$`

The old `@@` token is removed from v3.

### 7.2 New AST concepts

The parser/runtime should represent at least these concepts:

- outer lexical identifier: `@x`
- current context value: `@`
- context block/expression: `@(expr) { ... }`, `@(expr) => expr`
- placeholder node: `$`, `$n`

The parser may either:

- build explicit placeholder nodes and later rewrite them into lambdas, or
- lower them directly during parse.

Both are acceptable, but the final runtime model should execute ordinary closures.

### 7.3 Recommended desugaring strategy

Recommended approach:

1. Parse placeholder usages as dedicated placeholder nodes.
2. After building an expression subtree, if it contains placeholders at the current boundary, rewrite that subtree into a lambda node.
3. Do not let placeholder scanning cross explicit function/lambda boundaries.

### 7.4 Context block parse shape

`@(expr)` is reserved for context introduction, not for parenthesized access.

Allowed forms:

- `@(expr) { ... }`
- `@(expr) => expr`

Disallowed:

- standalone `@(expr)` as an ordinary expression

This keeps the grammar sharp and avoids making `@` ambiguous.

## 8. Runtime Model

### 8.1 Environment extension

Each environment should additionally store:

- whether a current value context exists
- the current value context itself

Lexical environment creation points in the concrete runtime are:

- function invocation
- context block / context expression

Plain statement blocks do not allocate a fresh environment.

Lookup operations:

- bare `@` searches upward for the nearest environment carrying a current value context
- `@x` searches upward for the nearest outer lexical binding named `x`, starting from `parent`

### 8.2 Function calls

No semantic change is required beyond ordinary closure capture.

Placeholder-generated lambdas are ordinary closures after desugaring.

### 8.3 Performance expectation

There should be no asymptotic runtime regression.

- `@x` is lexical parent-chain lookup, comparable to existing variable lookup.
- `@` is nearest-context lookup on the same environment chain.
- `$` disappears after parse/desugaring.

## 9. Old Syntax: Compatibility and Cleanup

v3 is a major version. Surface cleanup is intentional.

### 9.1 Remove `@@`

`@@` used to be a probe/debug prefix.
That syntax is removed because `@` is now reserved for context.

Replacement:

```sl
debug(x)
probe(x)
```

Decision:

- `debug(...)` and `probe(...)` are ordinary builtins in v3
- `@@` does not exist in the language anymore

### 9.2 Remove `::`

`::` is removed from v3.
`|>` is the only flow operator in the language standard.

### 9.3 Keep `&ref`, but narrow its purpose

`&ref` remains useful as a real first-class reference value.
It is no longer the preferred tool for mutating captured outer state.

Good v3 use cases:

- storing references in containers
- passing mutable references through APIs
- aliasing map/list slots intentionally

### 9.4 Keep `%copy`, `%deep`, `%net`

These features are orthogonal to the context redesign and remain unchanged.

## 10. Examples

### 10.1 Captured-state counter

```sl
fn counter(start) {
    total = start
    fn inc(n) {
        @total = @total + n
        return @total
    }
    return inc
}

c = counter(10)
print(c(1))
print(c(5))
```

### 10.2 Clean list pipeline

```sl
result =
    [1, 2, 3, 4, 5]
    |> filter($ > 2)
    |> map($ * 10)
    |> join(", ")

print(result)
```

### 10.3 Context block

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

### 10.4 Placeholder + outer lexical state

```sl
fn scale_by(factor) {
    return $ * @factor
}

times3 = scale_by(3)
print(times3(9))
```

### 10.5 Placeholder + context block

```sl
labeler = @(settings) => $.name + ":" + @.suffix

print(labeler({name: "A"}))
```

## 11. Non-Goals

v3 does not attempt to solve these in the same change:

- introducing a full object system
- changing container copy semantics
- adding statement-level pattern matching
- changing the import system

## 12. Implementation Order

Recommended rebuild order:

1. add lexer support for `@` and `$`
2. add AST nodes / parser support for `@x`, `@`, `@(expr)...`, placeholders
3. add placeholder desugaring to lambda
4. add runtime support for current value context and outer lexical assignment
5. update tests
6. update README to the v3 surface

This document is the source of truth for the v3 rebuild.
