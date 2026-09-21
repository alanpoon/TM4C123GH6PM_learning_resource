# Lesson 9 — Knowledge: Modules, Recursion and the AAPCS

**Video:** <https://youtu.be/_I-SeeC07Jo> · **Transcript:** <https://www.state-machine.com/course/lesson-09.txt>
**Hands-on companion:** [PRACTICE.md](PRACTICE.md)

---

## The big idea

Functions do more than remove repetition — they let a program be **split across
separately compiled files**. This lesson also watches a recursive function build
and unwind the stack, and finally names the contract that makes all calls work:
the **AAPCS**.

---

## 1. Modules: splitting a program into files

Keeping an entire program in one file is impractical for anything non-trivial.
Splitting it into **modules**:

- makes the overall structure visible, and lets the compiler enforce it;
- **speeds up builds** — only changed files are recompiled.

> The ability to build programs from separately compiled source files is one of
> the most powerful features of C.

### The pattern

| File | Contains |
|---|---|
| `delay.c` | the **definition** (the code) |
| `delay.h` | the **prototype** (the interface) |
| `main.c` | `#include "delay.h"` and the calls |

Moving `delay()` into `delay.c` produces a "no prototype" error. Copying the
prototype into `delay.c` *works* but is a **lousy fix** — two copies of the same
declaration can drift apart. That is DRY violated again.

**The right solution:** put the prototype in a header, and `#include` it
from *both* `main.c` *and* `delay.c`. Including it in the implementation file
too is what lets the compiler check that the definition matches the interface.

> Creating the file on disk is not enough — it must also be **added to the
> project** so the toolchain compiles and links it.

### Include guards

Headers include other headers, so a header can easily be included more than
once. The standard protection:

```c
#ifndef __DELAY_H__      /* not defined the first time -> proceed */
#define __DELAY_H__      /* now it IS defined                     */

void delay(int iter);

#endif                   /* skipped entirely on any later inclusion */
```

Naming conventions for the guard macro vary (here: capitalized file name with
leading/trailing underscores). Most library headers, including the vendor's,
carry such guards.

## 2. Returning values

```c
unsigned fact(unsigned n);   /* prototype: returns unsigned, takes unsigned */
```

Design **top-down**: write the prototype and the use cases first, then the
implementation.

Three ways to use a returned value:

```c
x = fact(0U);                 /* assign it               */
x = fact(3U) + fact(2U);      /* use it in an expression */
(void)fact(5U);               /* explicitly discard it   */
```

The last form only makes sense if the function has meaningful **side effects**.
The explicit `(void)` cast documents "I know it returns something; I don't want
it."

> Think about the **allowed range of arguments** while writing the prototype.
> For factorial the smallest defined argument is `0U` — and note the `U` suffix,
> because the parameter type is unsigned.

A function produces its result with **`return`** statements followed by
expressions.

## 3. Compiler errors vs. linker errors

Calling `fact()` before defining it anywhere builds fine **through the compiler**
and then fails at the **link** stage: *function `fact()` not found*.

> The **linker** is the build step that joins all compilation units together.
> The compiler *cannot* detect this error, because it has no idea which file the
> definition might live in.

Learning to tell compiler errors from linker errors saves real time. (The build
process gets a full treatment in Lesson 14.)

## 4. Recursion

A **recursive** function calls itself. Factorial has both definitions:

```
iterative:  n! = 1 * 2 * 3 * ... * n
recursive:  0! = 1 ;  n! = n * (n-1)!  for n > 0
```

In C:

```c
unsigned fact(unsigned n) {
    if (n == 0U) {
        return 1U;
    }
    else {
        return n * fact(n - 1U);
    }
}
```

### What the stack does

Watching `fact(5)` in the debugger shows the essential mechanism:

- Each call **pushes** `R4` and `LR` before recursing.
- The nested call happens **before** the previous one returns and pops — so each
  activation **nests on top of** the stack space of the one before it.
- The stack builds a clearly visible repeating pattern of **decrementing
  arguments and return addresses**.
- When recursion bottoms out, the calls return one at a time and the stack
  **unwinds**.
- `fact(5)` reaches **6 levels** of nesting and returns `0x78` = 120.

### `POP {PC}` — two birds, one stone

Non-leaf functions often return with:

```
PUSH {R4, LR}
...
POP  {R4, PC}     ; restores R4 AND returns by loading LR's saved value into PC
```

Popping directly into the PC **is** the return. Like `BX`, this is a special
case: the odd saved value (`0x49`) becomes an even PC (`0x48`), with the LSB
consumed as the Thumb-state bit.

### Recursion is a poor fit for embedded systems

> Deep call sequences should be **avoided** in embedded programming, precisely
> because they consume a lot of precious RAM for the stack.

The recursive factorial is a *teaching device* — ideal for watching the stack
grow. In production, prefer the **iterative** version or, better still, a
**lookup table**.

## 5. The AAPCS — ARM Application Procedure Call Standard

Caller and callee must agree on many small things: the return address arrives in
LR, the first argument arrives in R0, the return value comes back in R0. Those
agreements form a formal contract — the **AAPCS** (searchable online).

### Register responsibilities

| Registers | Role | Rule |
|---|---|---|
| **R0–R3, R12** | argument passing, return values, scratch | **caller-saved** — a function may clobber them freely |
| **R4–R11** | general purpose | **callee-saved** — a function may use them, but must save them on the stack and restore them before returning |
| R13 (SP), R14 (LR), R15 (PC) | special purpose | — |

This convention is what lets a caller keep a value in R4–R11 and trust it to
**survive a function call**.

`fact()` demonstrates both halves: R0 carries the argument *and* the return
value *and* the argument to the nested call — so the function moves its own `n`
into **R4** (which it saved on entry) to survive the recursive call, ready for
the final multiplication.

> The AAPCS becomes essential background when you get to **interrupt handling**
> on ARM (Lesson 18).

---

## Key takeaways

1. Split programs into modules: definition in `.c`, prototype in `.h`, include
   the header in both.
2. Never duplicate a prototype — that's DRY violated.
3. Guard every header against multiple inclusion.
4. Unresolved functions are **linker** errors, not compiler errors.
5. Recursion works because each activation nests on the stack — and that is
   exactly why it is expensive.
6. AAPCS: R0–R3/R12 are caller-saved scratch; **R4–R11 must be preserved**.

---

## Glossary

| Term | Meaning |
|---|---|
| Module / compilation unit | A separately compiled `.c` file |
| Header file | `.h` file holding an interface, meant for `#include` |
| Include guard | `#ifndef`/`#define`/`#endif` protection against double inclusion |
| Linker | Build step that joins compilation units and resolves symbols |
| Recursion | A function calling itself |
| Stack unwinding | Nested calls returning and releasing their stack frames |
| AAPCS | ARM Application Procedure Call Standard |
| Caller-saved / callee-saved | Who is responsible for preserving a register |

---

## Pitfalls to remember

- **Duplicating a prototype** instead of sharing a header.
- **Missing include guards** → duplicate-definition errors in larger programs.
- **Creating a file but not adding it to the project** → linker error.
- **Recursion in embedded code** → unpredictable stack depth. See Lesson 10.
- **Assuming R4–R11 are free** in hand-written assembly — the AAPCS says
  otherwise.

---

**Next:** Lesson 10 covers pointer arguments, scope, and what happens when the
stack overflows.
