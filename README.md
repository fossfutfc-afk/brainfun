# brainfun

A high-performance Brainfuck interpreter with multi-layered optimizations, a macro preprocessor, and an eye toward a minimal BF IDE.

## Tools

| Binary | Purpose |
|--------|---------|
| `brainfun` | Brainfuck interpreter |
| `brainduck` | Macro preprocessor (`.bd` → BF) |
| `bfc` | Strips comments/whitespace, outputs pure BF |

## Quick Start

```bash
# build (C++17, gcc/clang)
g++ -std=c++17 -O2 brainfun.cpp -o brainfun
g++ -std=c++17 -O2 brainduck.cpp -o brainduck
g++ -std=c++17 -O2 bfc.cpp -o bfc

# run a BF program
./brainfun hello.bf
./brainfun -o hello.bf          # with optimizations

# use macros
./brainduck demo.bd
./brainduck -e demo.bd          # expand macros, output BF to stdout

# compact a BF file
./bfc verbose.bf > tight.bf
```

## brainfun CLI

```
Usage: brainfun [options] <file.bf|file.brainfuck>

Options:
  -v, --version  show version
  -h, --help     show this help
  -mode=pre      pre-scan bracket table (O(n) build, O(1) runtime)
  -mode=lazy     lazy bracket cache (default, O(1) hot path)
  -size=N        tape size (default 30000)
  -warn          warn if operators appear near non-operator text
  -o             enable optimizations
```

## Optimization Pipeline (`-o`)

All optimizations run in a single pre-scan pass before execution begins.

### 1. Pattern Recognition — O(n) loop → O(1)

Common BF idioms are detected and replaced with direct cell operations:

| Pattern | Action | Complexity |
|---------|--------|------------|
| `[-]` | `cell = 0` | O(n) → O(1) |
| `[->+<]` | `right += cell; cell = 0` | O(n) → O(1) |
| `[-<+>]` | `left += cell; cell = 0` | O(n) → O(1) |

These are the building blocks of virtually every non-trivial BF program
(multiplication, copying, addition all rely on move loops).

### 2. Run-Length Compression

Consecutive identical operators are fused into a single bulk operation:

```
+++++   →  cell += 5
-----   →  cell -= 5
>>>     →  ptr += 3
<<<     →  ptr -= 3
```

This is particularly impactful inside loops where repeated increments
form the inner loop body.

### 3. Bracket Dispatch Strategy

Two strategies, selectable at runtime:

**Pre mode** (`-mode=pre`): One O(n) scan during construction builds a complete
bracket jump table. Every `[` and `]` resolves in O(1) at runtime. Best for
programs that will be run many times (IDE debugging, benchmarks).

**Lazy mode** (`-mode=lazy`, default): The jump table is built on-demand. Only
bracket pairs that are actually executed consume scan time. Unreached code paths
cost nothing. Best for single-run programs.

### 4. Buffered I/O

`putchar` per byte means one syscall per `.` instruction. A 4096-byte internal
buffer reduces syscalls by ~1000x for I/O-heavy programs. Input is line-buffered
and auto-flushed before each read to maintain ordering.

### 5. Enum Dispatch

BF's 8 operators (`> < + - , . [ ]`) are sparse in ASCII (values 43–93).
Switching on raw `char` can cause the compiler to emit a binary decision tree
instead of a jump table. A `char → 0..7` enum mapping guarantees a dense
dispatch table at every optimization level.

### 6. Pointer Wrapping

The tape pointer wraps at boundaries (`>` at right edge → 0, `<` at 0 →
tape end), matching standard BF semantics regardless of tape size.

## brainduck — Macro Preprocessor

Brainduck lets you write BF with named macros. A `.bd` file mixes
standard BF and `@macroname` calls:

```
// brainduck demo
@hello          // prints "Hello World!"
```

### Built-in Macros

**Inline** (operate on the current cell, can be placed anywhere):

| Macro | Expansion | Effect |
|-------|-----------|--------|
| `@clear` | `[-]` | Zero current cell |
| `@copy` | `[->+>+<<]>>[-<<+>>]<<` | Copy current cell to the right |
| `@move` | `>[-]<[->+<]` | Move current cell to the right (overwrite) |
| `@add_to` | `[->+<]` | Add current cell to the right, zero source |

**Standalone** (complete I/O programs):

| Macro | Purpose |
|-------|---------|
| `@hello` | Print "Hello World!" |
| `@to_upper` | Convert lowercase to uppercase until Enter |
| `@add` | Single-digit addition |
| `@mul` | Single-digit multiplication |
| `@div` | Division |

### Comments

`//` to end-of-line is stripped before macro expansion.
The characters `> < + - . , [ ]` must **never** appear in comments — BF has no
comment syntax, and these will be executed as commands. Use `-warn` to detect leaks.

## bfc — Compactor

Strips everything except the 8 BF operators:

```bash
./bfc verbose.bf           # file input
./bfc < verbose.bf         # pipe input
./brainfun $(./bfc program.bf)  # one-liner flex
```

## Architecture

```
bf_common.h          Shared interpreter core (template<bool Lazy>)
  basic_interpreter<false> → interpreter_pre
  basic_interpreter<true>  → interpreter_lazy

brainfun.cpp         CLI + brainfun_app class
brainduck.cpp        Macro engine + expander
bfc.cpp              Compactor (21 lines)
```

## Test Suite

```
test/helloworld.bf      Classic Hello World
test/digits.bf          Print 0123456789
test/alphabet_full.bf   Print A-Z
test/nested.bf          Nested loop proof (6x8=48)
test/cmp.bf             Three-way comparison (double magic loop)
test/logic.bf           BF if/else pattern documentation
```

## License

This is a personal project. Do whatever you want with it.
