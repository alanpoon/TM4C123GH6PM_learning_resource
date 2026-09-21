# Lesson 10 — Knowledge: Stack Overflow and Other Pitfalls of Functions

**Video:** <https://youtu.be/jmzvued3w3Y> · **Transcript:** <https://www.state-machine.com/course/lesson-10.txt>
**Hands-on companion:** [PRACTICE.md](PRACTICE.md)

---

## The big idea

Four ways functions blow up — **uninitialized locals**, **stack overflow**,
**stack corruption**, and **returning a pointer to a local** — plus the rule
that explains the first three: C trusts you completely and checks nothing.

---

## 1. The stack contains garbage

Allocating a local array on the stack costs a single `SUB SP, SP, #n`. That
instruction **makes room but does not clean it** — no cycles are wasted zeroing
memory.

So every local variable starts out holding whatever was there before: remnants
of previous calls, or even leftovers from the flash loader that programmed the
chip.

> **You cannot assume an automatic variable has any particular initial value.
> You must explicitly initialize every automatic variable.**

The metaphor, extended: the call stack is a stack of dishes — **and they are all
dirty**. It is disgusting to use them before washing them.

## 2. Stack overflow

The stack grows toward lower addresses. When it grows past the **start of RAM**
(`0x20000000`), there is no memory below — and the CPU faults.

### How it presents

- The program **freezes** and stops hitting breakpoints.
- Breaking in manually shows **SP below the valid RAM range**.
- The program is spinning in an endless loop inside **`BusFault_Handler`**.

`BusFault_Handler` is not your code — it is an **exception handler** supplied by
the toolchain's startup code and linked into your program. A **BusFault** is the
CPU's hardware mechanism for "forced to access nonexistent memory". The default
handlers are endless loops, but you can supply your own (Lesson 15).

> **Habit to form:** when a program hangs inside a hardware exception,
> **check the stack pointer first.**

Be warned: overflow does not always fault. It may merely **corrupt other data**
without running out of memory — much harder to detect and diagnose.

### Sizing the stack

Stack size is a **linker** setting, not a compiler one. In IAR:
**Options → Linker → Config → Override default → Edit → Stack/Heap Sizes**.

- Default stack: **2 KB** (specified in hex, decimal also accepted).
- **1 KB** is adequate for the projects at this stage of the course.
- Saving the edited settings writes a project-specific linker script
  (`project.icf`) which must be kept with the project.

> **Develop the habit of sizing the stack adequately for your application.**

### The heap

The **heap** is the region of RAM used for dynamic allocation via `malloc()` and
`free()`.

> In real-time embedded programming the heap typically causes **more harm than
> good**. Set the heap size to **0** and don't use it.

## 3. Stack corruption — out-of-bounds array indexing

```c
unsigned foo[6];
foo[n] = n;        /* if n == 7, this writes past the end */
```

**C does not check array indices.** It trusts that you know what you are doing.

Writing two slots past the end of a 6-element array on the stack lands squarely
on the **saved LR** — so the function can no longer return correctly.

### Why this is so nasty

The *crime* is simple; the *story afterwards* is the problem. The corrupted
return address sends the CPU off executing garbage, and the system can limp on
for **thousands of clock cycles** before it finally dies. In the video's
worked example:

1. `POP {R4, PC}` restores the corrupted value `7` into the PC (it succeeds only
   because the value is **odd** — an even value would fault immediately, see
   Lesson 8).
2. PC becomes `6` — inside the **exception/interrupt vector table**, which is
   *data*, but the CPU happily decodes it as 16-bit instructions.
3. `main()` happens to follow the vector table in Flash, so the CPU falls into
   **`main()` through the back door** and starts over.
4. `main()` never returned, so its stack frame is never popped. Each cycle
   leaks a frame; the stack **grows slowly** until it overflows → BusFault.

> Runaway programs corrupt their own state for thousands of cycles. Such bugs
> are very hard to reproduce and debug because of all the coincidences along the
> way. Treat stack corruption with respect.

**Debugging technique:** don't single-step thousands of instructions. Set
**strategic breakpoints** — start at the point you know is broken (here, the
function's return), then work outward.

## 4. Arguments are passed by value

Function arguments behave exactly like local variables that the *caller*
initializes:

| | Initialized by |
|---|---|
| Argument | the caller, at the call site |
| Local variable | code inside the function |

Both may be freely modified inside the function — but **C passes arguments by
value**: only a *copy* is given to the function.

> **A function can never change the caller's original argument.**

### Pointer arguments

When you *do* need to modify the caller's variables — the classic case is `swap`
— pass **pointers**:

```c
void swap(int *x, int *y) {       /* just add the stars */
    int tmp = *x;
    *x = *y;
    *y = tmp;
}

swap(&x, &y);                     /* and take the addresses at the call site */
```

The addresses arrive in **R0 and R1**, per the AAPCS.

## 5. Never return a pointer to a local

```c
int *swap(int *x, int *y) {
    int tmp[2];        /* on the stack */
    ...
    return tmp;        /* WRONG — the compiler warns */
}
```

When the function returns, its stack frame is released. The returned pointer now
points **above the stack pointer** — to memory that the very next call will
overwrite. In the video, the array survived just long enough to *look* correct,
and then the first `delay()` call destroyed it, so the second `delay()` received
`0` instead of 500,000.

> Local variables **go out of scope** when the function returns. They no longer
> exist and cannot be accessed.

### The fix: `static`

```c
static int tmp[2];   /* allocated outside the stack */
```

`static` in front of a local variable tells the compiler to allocate it **outside
the stack**, so it **outlives** any call to the function. With that change the
warning disappears and the returned pointer stays valid. (Such a variable lives
in ordinary RAM, right at the start of the RAM region.)

## 6. Footnote: the `return 0` from `main()`

The long-standing "unreachable code" warning comes from a genuine conflict:

- The C standard requires `main()` to return `int`.
- The standard also requires every non-`void` function to return its type
  explicitly — GCC warns if the `return` is missing.
- But the compiler correctly sees that `while (1)` makes the `return`
  unreachable.

Both cannot be satisfied at once. The course opts for a clean build by
**commenting out** `return 0;` in `main()`.

---

## Key takeaways

1. Stack memory is **dirty** — initialize every automatic variable.
2. Stack overflow shows up as a hang in `BusFault_Handler` with SP below RAM —
   check SP first when a program hangs in an exception.
3. Size the stack deliberately (linker setting); set the **heap to 0**.
4. C does not check array bounds; an out-of-bounds write can clobber the saved
   return address and cause chaos thousands of cycles later.
5. Arguments are passed **by value**; use pointer arguments to modify the
   caller's data.
6. **Never return a pointer to a local** — use `static` if the data must outlive
   the call.

---

## Glossary

| Term | Meaning |
|---|---|
| Automatic variable | An ordinary local, allocated on the stack |
| Stack overflow | Stack grows beyond its allotted memory |
| BusFault | CPU exception for accessing nonexistent memory |
| Exception handler | Function invoked by the CPU on a fault |
| Heap | RAM region for `malloc`/`free` |
| Pass by value | The callee receives a copy, not the original |
| Scope | The region of the program where a name is valid |
| `static` local | Local variable allocated outside the stack, outliving the call |

---

## Pitfalls to remember

- **Assuming zero-initialized locals.**
- **Silent array overruns** — no warning, no runtime check.
- **Recursion with large local arrays** — the fastest route to overflow.
- **Returning `&local` or a local array.**
- **Using the heap in a real-time system.**
- **Single-stepping a runaway program** instead of placing strategic breakpoints.

---

**Next:** Lesson 11 moves to standard integer types (`stdint.h`) and the hazards
of mixing integer types.
