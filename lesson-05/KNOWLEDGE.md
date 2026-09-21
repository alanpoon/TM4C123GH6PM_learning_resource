# Lesson 5 — Knowledge: The Preprocessor and `volatile`

**Video:** <https://youtu.be/5MzilJ2-MGY> · **Transcript:** <https://www.state-machine.com/course/lesson-05.txt>
**Hands-on companion:** [PRACTICE.md](PRACTICE.md)

---

## The big idea

Two independent tools that together make the blinky program both *readable* and
*correct at any optimization level*: the **C preprocessor** gives hardware
addresses names, and **`volatile`** stops the compiler from optimizing away
accesses that matter for reasons the compiler cannot see.

---

## 1. The C preprocessor

The preprocessor is conceptually a **separate first pass of plain text
substitution**, run before the real compilation. It:

- removes every line starting with `#` — the compiler never sees them;
- substitutes macros **only where they are actually used**; the compiler sees
  the replacement text and never the macro name.

### `#define`

```c
#define RCGC_GPIO   (*((unsigned int *)0x400FE608U))
```

The macro name is followed by the text that replaces it.

Because substitution is purely textual, **a macro need not be any complete
element of the C language**. `#define FOO (unsigned int *)0x4002` is legal; it
only has to make sense *in the context where it is pasted*. An unused macro can
even be syntactic nonsense — the compiler never sees it.

### The parenthesization rule

> **Always wrap a macro's replacement text in parentheses.**

Without parentheses, the meaning of a macro changes depending on the
surrounding context — operator precedence silently rewrites your intent.
Parentheses make `RCGC_GPIO` mean "dereference of this pointer" in *every*
context.

### Macros built from macros

```c
#define GPIOF_BASE  0x40025000U
#define GPIOF_DIR   (*((unsigned int *)(GPIOF_BASE + 0x400U)))
#define GPIOF_DEN   (*((unsigned int *)(GPIOF_BASE + 0x51CU)))
#define GPIOF_DATA  (*((unsigned int *)(GPIOF_BASE + 0x3FCU)))
```

This mirrors how the datasheet documents registers (base + offset) and makes the
code self-checking against the manual.

**No run-time cost.** You might worry that the CPU now adds base to offset at
run time — it does not. The compiler performs **constant folding**: any
expression computable at compile time is computed then. The generated `LDR`
loads the full address `0x400253FC` directly, exactly as with a literal.

## 2. Comments

Comments exist only for humans; the compiler ignores them. C99 supports two forms:

```c
/* traditional C comment, may span lines */
// C++-style comment, ends at end of line
```

A comment may appear anywhere a space could — in fact every comment is replaced
by a single space before compilation. Both forms work inside macro definitions.

## 3. Header files and `#include`

```c
#include "tm4c.h"
```

`#include` textually inserts the named file. The `.h` extension marks a
**header file**, designed for inclusion into `.c` files.

You do **not** need to define register macros yourself — MCU vendors ship them.
The file for this board (`tm4c.h`, called `lm4f120h5qr.h` in the original video)
defines macros using the datasheet's own register names:

- `SYSCTL_RCGCGPIO_R`
- `GPIO_PORTF_DIR_R`
- `GPIO_PORTF_DEN_R`
- `GPIO_PORTF_DATA_R`

> If you are ever unsure a vendor macro is the register you want, check its
> **address** against the datasheet.

### Two differences from hand-rolled macros

1. **`unsigned long` instead of `unsigned int`.** On a 32-bit machine like ARM,
   both are 32 bits wide, so they are equivalent here. (Integer types get proper
   treatment in Lesson 11.)
2. **`volatile`** — the important one.

## 4. `volatile`

> **`volatile` tells the compiler that an object may change even though no
> statement in the program appears to change it.**

### Why I/O registers must be volatile

Two bits of the Port F data register are wired to the **user switches** on the
board. Their value changes when a human presses a button — nothing in the
program causes it. Most I/O registers are volatile for similar reasons
(hardware status flags, incoming data, timers).

### What the qualifier actually forbids

For **non-volatile** objects, the compiler may optimize access: read the value
into a CPU register once, work in the register for a while, and write back
later — or not at all.

For **volatile** objects it may not. **Every read the source asks for is a real
read; every write is a real write**, in the order written.

### `volatile` on ordinary variables

It is equally useful for "normal" variables, to suppress optimizations you don't
want. The delay loop is the classic case:

```c
int counter = 0;
while (counter < 1000000) { ++counter; }
```

From the compiler's perspective this loop **contributes nothing** — the final
value of `counter` is discarded. At higher optimization levels the compiler is
entirely within its rights to **delete both delay loops**, and it does. The LED
then appears permanently on, and single-stepping shows the on/off instructions
still present but the loops gone.

The fix:

```c
int volatile counter = 0;
```

Now every increment must actually happen.

### Placement

`volatile` may be written **before or after the type**:

```c
volatile int counter;   /* both legal */
int volatile counter;   /* recommended in this course */
```

Placing it *after* the type is recommended because it reads consistently with
pointer declarations, where position changes meaning.

---

## Key takeaways

1. The preprocessor is dumb text substitution that happens before compilation.
2. Parenthesize macro bodies — always.
3. Constant folding makes `base + offset` macros free at run time.
4. Prefer the vendor's header file to hand-written register macros.
5. `volatile` = "this may change behind your back; do every access, for real."
6. With `volatile` applied correctly, your program runs the same at **any**
   optimization level.

---

## Glossary

| Term | Meaning |
|---|---|
| Preprocessor | Text-substitution pass run before compilation |
| Macro | A `#define`d name replaced by its text |
| Header file (`.h`) | File intended for `#include` into `.c` files |
| Constant folding | Compile-time evaluation of constant expressions |
| `volatile` | Qualifier forbidding optimization of reads/writes |

---

## Pitfalls to remember

- **Unparenthesized macros** break in unexpected contexts — a classic C bug.
- **Macros are not functions.** No type checking, no scope, arguments may be
  evaluated more than once.
- **Missing `volatile` on a delay loop** → code that works at `-O0` and breaks at
  `-O2`. This class of bug is notoriously hard to find.
- **Missing `volatile` on an I/O register** → reads get cached in a register and
  you never see the hardware change.
- **`volatile` is not a concurrency tool.** It prevents optimization; it does
  *not* provide atomicity. Race conditions are Lesson 20.

---

**Next:** Lesson 6 introduces the bitwise OR/AND operators, so you can change
one LED without clobbering the others.
