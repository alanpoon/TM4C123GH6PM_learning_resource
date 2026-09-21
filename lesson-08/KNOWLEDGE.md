# Lesson 8 — Knowledge: Functions and the Call Stack

**Video:** <https://youtu.be/Ju4KivZIL1g> · **Transcript:** <https://www.state-machine.com/course/lesson-08.txt>
**Hands-on companion:** [PRACTICE.md](PRACTICE.md)

---

## The big idea

> If you only ever learn **one** aspect of the low-level behaviour of C or C++,
> make it the **call stack**. It is the key to understanding functions,
> interrupts, context switching and the RTOS.

---

## 1. Functions and the DRY principle

The blinky program repeats its delay loop twice. That violates **DRY — Don't
Repeat Yourself**: duplicated code can drift out of sync.

A **function** (also called a procedure, subroutine or sub-program) is a
reusable piece of code that can be executed from many points in a program.

### Signature and definition

```c
void delay(void) {   /* return type, name, argument list = the SIGNATURE */
    ...              /* the body goes between the braces                 */
}
```

**Calling** a function = jump to its code, execute it, and return to the
instruction just after the call:

```c
delay();   /* parentheses are required even with no arguments */
```

### Prototypes

A **prototype** is the signature followed by a semicolon instead of a body:

```c
void delay(void);
```

The compiler must see a prototype **before** the definition or any call.

> **Enable "Require prototypes" in your compiler options.** It turns a whole
> class of silent mistakes into errors.

With that option on, an **empty** argument list is rejected:

```c
void delay();      /* REJECTED: "arguments unspecified", a C89 legacy */
void delay(void);  /* correct: explicitly no arguments */
```

For backwards compatibility, `()` means "arguments not specified, could be
anything" — far too weak. Always write `(void)`.

## 2. How a call works at the machine level

A function call is **one instruction**: **`BL`** (Branch with Link).

- Like any branch, `BL` changes the **PC**.
- **Side effect:** it saves the address of the next instruction into **R14**,
  the **Link Register (LR)** — the place to return to.

> `BL` is a **4-byte** instruction, while most Thumb-2 instructions are 2 bytes.
> The Cortex-M instruction set (**Thumb-2**) is mostly 2-byte, occasionally
> 4-byte instructions.

### The odd return address

The value stored in LR is **odd** (e.g. `0x9D` when the return address is
`0x9C`). This is not a bug:

- The return uses **`BX LR`** (Branch and eXchange), which sets the PC from a
  register, but **forces the PC's least significant bit to 0** — return
  addresses must be even.
- The LSB of LR is therefore not part of the address. It is the **instruction
  set exchange bit**: `1` = Thumb, `0` = ARM.
- Cortex-M supports **only Thumb-2**. Returning with that bit cleared attempts
  to switch to the unsupported ARM state and raises a **BusFault** exception.

So the odd bit is historical legacy — but it must be set.

## 3. The stack

**SP** (Stack Pointer) is an alias for **R13**. It is the hardware
implementation of the C call stack.

> A **stack** is an area of RAM that grows and shrinks **from one end only**.
> That end is the **top of stack**, and SP holds its address.

Metaphor: a stack of dishes — you add and remove only from the top.

**On ARM the stack grows toward *lower* addresses** and shrinks toward higher
ones. (In a memory view, growth appears *upward*. Other architectures may grow
the other way.)

### What the stack is used for

1. **Local variables** of the functions currently executing.
2. **Return addresses** (saved LR values).

### Entry and exit are mirror images

```
SUB SP, SP, #4   ; grow the stack — make room for a local
...
ADD SP, SP, #4   ; shrink it back — free that room
BX  LR           ; return
```

> **Any stack operation performed on entry must be exactly reversed before
> returning.** A function that leaks stack space corrupts its caller.

## 4. Leaf and non-leaf functions

A **leaf function** calls no other function (like a leaf of a tree). It can
leave the return address sitting in LR for the whole of its lifetime.

A **non-leaf function** executes `BL`, which **clobbers LR**. It must therefore
save the old LR first — and the natural place is the stack:

```
PUSH {LR}    ; saves listed registers, decrements SP automatically and atomically
...
POP  {PC}    ; (see Lesson 9) pops straight into the PC to return
```

`main()` is itself a function. The moment it calls `delay()`, it stops being a
leaf and must start pushing LR.

## 5. Function arguments

Arguments let each call supply different initial values for the function's
parameters:

```c
void delay(int iter);          /* prototype */

void delay(int iter) {
    int volatile counter = 0;
    while (counter < iter) { ++counter; }
}

delay(1000000);   /* long  */
delay(500000);    /* short */
```

Once a function takes arguments, **every** call must supply them. Change the
prototype and the compiler immediately flags every mismatched call — that is
precisely what prototypes are for.

### How arguments are passed

The **first argument is passed in R0**, and the return value comes back in R0.
In the disassembly you see a constant loaded into R0 immediately before the
`BL`. (The full set of rules is the AAPCS — Lesson 9.)

## 6. Why optimization must be low here

At high optimization the compiler **inlines** small functions — it pastes the
body at the call site and eliminates the call entirely, effectively undoing what
you just did. Set optimization **low** while studying calls.

## 7. Why functions matter

> When you design functions properly, you can ignore **how** a job is done and
> focus only on **what** is being done. That is a lot simpler.

---

## Key takeaways

1. Functions eliminate repetition (DRY) and let you reason at the level of
   *what*, not *how*.
2. Always write `(void)` for no arguments, and enable "require prototypes".
3. `BL` calls: it branches **and** saves the return address in LR.
4. LR's least significant bit is the Thumb-state bit, not part of the address.
5. SP (R13) points at the top of a downward-growing stack holding **locals** and
   **return addresses**.
6. Non-leaf functions must PUSH LR before calling anything.
7. Every stack adjustment on entry must be undone on exit.

---

## Glossary

| Term | Meaning |
|---|---|
| Signature | Return type + name + argument list |
| Prototype | Signature followed by `;` |
| `BL` | Branch with Link — the call instruction |
| LR / R14 | Link Register — holds the return address |
| SP / R13 | Stack Pointer — address of the top of stack |
| Leaf function | A function that calls no other function |
| PUSH / POP | Store/load a register list and adjust SP atomically |
| Inlining | Optimization that replaces a call with the function's body |
| Thumb-2 | The Cortex-M instruction set (mostly 2-byte instructions) |

---

## Pitfalls to remember

- **`void f();` instead of `void f(void);`** — a C89 loophole that disables
  argument checking.
- **Clearing the LSB of LR** → BusFault on Cortex-M.
- **Forgetting that non-leaf functions clobber LR.**
- **Studying calls at high optimization** — inlining hides everything.
- **The stack is finite.** Locals and return addresses consume RAM; deep call
  chains exhaust it (Lesson 10).

---

**Next:** Lesson 9 splits the program into multiple files, writes a recursive
function, and presents the ARM Application Procedure Call Standard.
