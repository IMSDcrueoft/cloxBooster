# cloxBooster
![Version](https://img.shields.io/badge/version-1.2.0-blue)

**cloxBooster** is a trimmed-down fork of the [LoxFlux](https://github.com/IMSDcrueoft/LoxFlux) project(ver 0.10.1). It is a stack-based bytecode virtual machine reimplementation of the cLox interpreter described in "Crafting Interpreters".

- **Design goal**: cloxBooster stays as close to cLox as possible, keeping the core Lox language semantics intact. A few minor syntax extensions remain (documented below).
- **Origin**: derived from the LoxFlux project — see the [original project repository](https://github.com/IMSDcrueoft/LoxFlux) for the full-featured version (modules, arrays, string builders, lambda, bitwise operations, etc.).

## Introduction of Lox

Lox is a programming language designed for learning purposes. It is conceived as a small and easy-to-understand language, perfect for those who want to build their own interpreter or compiler. The Lox language and its associated tutorials were originally introduced by Bob Nystrom in his online book "Crafting Interpreters". ([Book URL](https://craftinginterpreters.com/))

## Resources of Lox

- [Crafting Interpreters - Official Website](https://craftinginterpreters.com/)
- [Crafting Interpreters - Official Github](https://github.com/munificent/craftinginterpreters)

## List of cloxBooster features

#### Performance

- **Shared constants**: Use a shared constant table instead of a function holding its own constant table individually.
- **Compact constant encoding**: Constant indexes are encoded in 2 bytes in the bytecode (shared constants table, with deduplication), supporting up to 65,536 constants.
- **Compact local variable encoding**: Local variable slot indexes are encoded in a single byte in the bytecode (including the super-instruction `*_LOCAL` family and `OP_MOVE_LOCAL`), keeping every local-access instruction at 2 bytes. Up to 254 nested local variables per function are supported.
- **Constant deduplication**: For both numbers and strings.
- **Optimized global variable access**: Achieves `O(1)` time complexity, the access overhead is close to that of local variables. With dynamic update key indexes, direct index fetching can be achieved in almost all cases. Indexes are rarely invalidated, unless you frequently declare new global variables.
- **Optional object header compression**: Object headers are compressed from 16 bytes to 8 bytes by compressing the 64-bit pointer to 48 bits.
- **Optional NaN Boxing**: Compress the generic type value from 16 bytes to 8 bytes(from clox).
- **Inline `init()`**: The inline caching class init() method helps reduce the overhead of object creation.
- **Flip-up GC marking**: Flipping tags can avoid reverting to the write of tags during the recycling process, and favor concurrent tags (if actually implemented).
- **Detached static and dynamic objects**: Static objects such as strings/functions, they don't usually bloat very much, so I think it's a viable option not to recycle them.
- **Compilation-time optimizations**: Provides basic constant folding and super instruction.
- **Instruction Dispatching**: Use `direct threading code` instead of `switch case` in compilers that support compute goto(clang & gcc).

---

#### Performance test — v1.0.0

_(AMD Ryzen7-5800X, Windows 11, Use ClangCL/LLVM 20 for cloxBooster & clox)_

All values are in **seconds** (cloxBooster values are reported by the `clock()` global).
|program|cloxBooster - [1.0.0]|clox|
|---|---|---|
|fib30|0.058s|0.076s|
|fib35|0.597s|0.874s|
|fib40|6.561s|9.677s|
|loop 1e8|0.702s|1.109s|
|global loop 1e8|0.93s|2.044s|
|binary_trees|2.575s|1.996s|
|instantiation|0.953s|0.945s|
|invocation|0.22s|0.235s|
|method_call|0.135s|0.167s|
|properties|0.296s|0.377s|
|trees|4.888s|3.553s|
|zoo|0.238s|0.282s|
|zoo_batch(10sec)|6850batch|5398batch|


---

**Note:** The `clox` benchmark results for `loop 1e8` and `global loop 1e8` are from **LLVM 19**. LLVM 20 introduces a misoptimization that causes severe performance regression (~80% slowdown) on these specific loops.

---

### Comment

- **Line & Block comment support**: Using `//` `/* */`.

---

### Numbers and Strings

- **Binary literal**: Use `0b` or `0B` prefix, e.g., `0b1010`.
- **Hexadecimal literal**: Use `0x` or `0X` prefix, e.g., `0xFF`.
- **Scientific notation**: Supports formats like `1.2e+3` and `123E-2`.
- **Escape characters**: Supports escaping with backslash `\`, such as `\"` for double quotes; Example: `"\"hello world\""` renders as `"hello world"`.

---

### Variable

- **Allows multiple definitions of variable constants**: (e.g., `var a = 1,b = 2,c = a + b;`).
- **`const` keyword support**: Supported within blocks.

---

### Loop

- **do-while**: Supported `while`,`for` and `do-while` loop.
- **`break` and `continue` keywords**: Supported within loops.

---

### Instance

- **Object Literals**: Supports defining object literals directly with `{k1:v1,"k2":v2}`,making object creation more intuitive and convenient(Nesting is supported).
- **Delete property**: Remove key-value pairs by assigning nil to the object.

---

### Subscript

- **Object Key Access**: Subscript notation supports accessing object properties by key.
- **Syntax**: Use square brackets `[]` with a string representing the key name.
- **Assignment via Subscript for Objects**: Modify object properties by assigning new values using the subscript operator.
- **String Indexing**: Access a single byte (ASCII code) of a string with a numeric subscript; out-of-bounds access returns a `nil`.

---

### Native Globals

- **`clock`**: Returns the number of seconds elapsed since the program started (based on the C `clock()` function, `clock() / CLOCKS_PER_SEC`). Useful for benchmarking scripts.
```
var start = clock();
// ... do something ...
print clock() - start; // elapsed seconds
```

---

### REPL

- **Support for line break input**: Use `\` for multi-line input in REPL.
- **Commands**:
  - `/help` : Print help information.
  - `/exit` : Exit the REPL.
  - `/clear`: Clean the console.
  - `/eval` : Load file and Run.

## Licenses
The project **cloxBooster** is based on `MIT` and has no third-party dependencies.
  - Copyright (c) 2025-2026 IMSDCrueoft
  - License: `MIT`

## Other expectations
1. performance improvements
