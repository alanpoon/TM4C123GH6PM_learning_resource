# Lesson 15 — Knowledge: Startup Code Part 3 — Vector Table and Fault Handlers

**Video:** <https://youtu.be/42HbCf5cz5A> · **Transcript:** <https://www.state-machine.com/course/lesson-15.txt>
**Hands-on companion:** [PRACTICE.md](PRACTICE.md)

---

## The big idea

Fill the vector table properly — stack pointer, reset handler, every exception
and every device interrupt — and write fault handlers that are **fit for
production**, not the endless loops that ship in most vendor startup code.

> Two habits this lesson is really about: **exercise infrequently-executed code
> deliberately** (fault injection), and **route all errors through one handler**.

---

## 1. Initializing the stack pointer entry

The first vector-table entry is the initial **SP**. Hard-coding an address would
break compatibility with the IDE's linker-script editor, where the stack size is
configurable.

The solution uses a linker-generated symbol:

> **The IAR linker generates `section-name$$Base` and `section-name$$Limit`
> symbols for every section in the program**, placed at the section's base and
> limit addresses.

```
CSTACK$$Base   ── low address  ┐
                               │  stack grows DOWN on ARM
CSTACK$$Limit  ── high address ┘  <-- initial SP goes here
```

Because the ARM stack grows from high RAM toward low RAM, the initial SP must be
**`CSTACK$$Limit`**.

### `extern` — declaration vs. definition

Using the symbol in C fails: *`CSTACK$$Limit` is undefined*. That makes sense —
**section symbols are created by the linker, after compilation**, so the
compiler upstream knows nothing about them.

You must **declare** the symbol without **defining** it. Until now every
declaration you wrote was also a definition:

```c
int CSTACK$$Limit;          /* WRONG: defines a NEW variable, hiding the linker's */
extern int CSTACK$$Limit;   /* right: declares it, allocates nothing              */
```

> **`extern` introduces a symbol to the compiler without creating storage for
> it.** The declared type still must be given, though here it is irrelevant —
> only the *address* matters.

Then cast explicitly, because the table's element type is `int`:

```c
(int)&CSTACK$$Limit
```

## 2. Initializing the reset handler

The second entry is the address copied into **PC** at reset — where the CPU
starts executing.

Reuse the library's startup code: **`__iar_program_start`** (Lesson 13).

### Taking the address of a function

C lets you take the address of a **function** just as of a variable. You need a
prototype so the compiler knows the symbol, then an explicit cast:

```c
extern void __iar_program_start(void);
...
(int)&__iar_program_start
```

The error before the cast mentions the type `void (*)(void)` — a **pointer to a
function taking no arguments and returning nothing**. (Function pointers get
their own treatment later.)

### `&` on a function name is optional — but preferred

A function name **not followed by parentheses** cannot be mistaken for a call,
so C allows `__iar_program_start` without `&`. Many vendor vector tables omit it.

> **Recommended style: keep the `&`.** It states plainly that you mean an
> *address*. Know that the other form exists, because you will meet it.

## 3. The standard Cortex-M exceptions

After Reset come the entries with **negative IRQ numbers** — common to **all**
Cortex-M processors:

`NMI`, `HardFault`, `MemManage`, `BusFault`, `UsageFault`, `SVCall`,
`DebugMonitor`, `PendSV`, `SysTick`

These handle **exceptions** — e.g. the stack overflow of Lesson 10 dropped SP
below RAM and triggered **HardFault**. The datasheet's **"Fault Handling"**
section documents which errors map to which exception.

> ⚠️ **The standard exceptions are NOT contiguous.** The table contains
> **"Reserved"** gaps, normally initialized to **zero**. Your custom table must
> preserve the **exact layout**.

### CMSIS handler names are standardized

Handler names like `HardFault_Handler` and `SysTick_Handler` are **part of the
CMSIS standard**, and their prototypes are already in the device CMSIS header —
you needn't declare them yourself.

> Using standard names matters when combining your startup code with an **RTOS**
> or other third-party software, which expect exactly these names.

## 4. Fault handlers fit for production

The conventional exception handler is an **endless loop**. Convenient for
debugging — break in and you find yourself spinning inside it.

> Unfortunately **most people leave such code in the final product**. That is why
> devices "freeze" and can't even be switched off until the battery is pulled — a
> classic **denial of service** caused by badly coded exception handlers.

### The `assert_failed()` pattern

Instead, call a **single common error handler**:

```c
void assert_failed(char const *file, int line);
...
void HardFault_Handler(void) {
    assert_failed("HardFault", __LINE__);
}
```

- `assert_failed` is widely used in ARM code libraries and is already prototyped
  in the device CMSIS header.
- It is for **unrecoverable errors** where you do not want to continue. Its job
  is **damage control** and, most commonly, **resetting the machine**.
- Its two arguments — a **file/fault name** and a **line number** — let you
  locate the error, and record it in an **error log** if possible.
- **`__LINE__`** is the standard preprocessor macro that expands to the current
  line number.
- CMSIS provides **`NVIC_SystemReset()`** to perform the reset.

> A single common error handler means **one breakpoint catches all faults,
> errors and assertions** while debugging. Always keep a breakpoint in
> `assert_failed`.

## 5. `__stackless` — the subtle bug in fault handlers

A naive `HardFault_Handler` **doesn't work after a stack overflow**. Its first
instruction is a `PUSH` — which faults again, re-entering HardFault immediately.
The result is an implicit endless loop, and `assert_failed` is never reached:
exactly the denial of service you were trying to avoid.

> **You must not touch the stack inside a fault handler, because the stack may be
> the very thing that is broken.**

IAR's extension:

```c
__stackless void HardFault_Handler(void) { ... }
```

`__stackless` tells the compiler **not to use the stack** for that function. The
function violates the calling convention such that **it is impossible to return
from it** — which is fine, because you never intend to return from a fault
handler. Apply it to **all fault handlers and to `assert_failed`**.

## 6. Weak aliases for the non-fault exceptions

Some vector entries are **not faults**: `SVC_Handler`, `DebugMon_Handler`,
`PendSV_Handler`, `SysTick_Handler`. For these you want to have it both ways:

- allow the handler to be **defined elsewhere** in the program, if needed;
- otherwise supply a **default** that flags "an undefined handler was invoked".

The compiler's **weak alias** feature does exactly that:

> If a symbol is still **undefined at the end of linking**, the alias is used.
> If the symbol **is** defined in the project, the weak alias is **ignored** —
> and the linker does **not** report a multiply-defined symbol.

So `SVC_Handler` falls back to `Unused_Handler` unless you define it. (You must,
of course, define `Unused_Handler` itself, and prototype it.)

The same technique aliases **all the device interrupt handlers** to
`Unused_Handler`.

## 7. The Board Support Package

`assert_failed` needs a home — a new file, **`bsp.c`**, the **Board Support
Package**.

The BSP holds everything **specific to your board**:

- the error and assertion handling policy,
- the board's interrupt handlers,
- later, LED/button functions and clock configuration.

It includes the device CMSIS header, and grows steadily through the rest of the
course.

## 8. Device interrupt vectors

Beyond the standard exceptions come the device-specific **IRQ** entries, listed
in the datasheet. Adding them is **tedious but introduces nothing new** — copy
the list, preserve the **Reserved** entries exactly, rely on the CMSIS header for
prototypes, and weak-alias every handler to `Unused_Handler`.

## 9. Fault injection

Faults don't occur naturally in a blinky program — you would be waiting for a
cosmic ray. So **make them happen at will**. This is called **fault injection**:
set a breakpoint, set SP to the start of RAM, and step — the next `PUSH`
overflows and takes the HardFault.

> **Develop the habit of exercising all parts of your code, especially the parts
> that execute infrequently** — fault handlers above all.

---

## Key takeaways

1. The linker creates `SECTION$$Base`/`$$Limit` symbols; the initial SP is
   `CSTACK$$Limit` because ARM stacks grow downward.
2. `extern` declares without defining — the way to reference linker symbols.
3. You can take the address of a function; keep the `&` for clarity.
4. Preserve the **Reserved** gaps in the vector table exactly.
5. Use CMSIS-standard handler names — RTOSes depend on them.
6. Replace endless-loop handlers with a common `assert_failed()` that resets.
7. Fault handlers must be `__stackless` — the stack may be the problem.
8. **Weak aliases** give you optional handlers with safe defaults.
9. Put board-specific code in a **BSP**.
10. **Inject faults** to test the code that rarely runs.

---

## Glossary

| Term | Meaning |
|---|---|
| `CSTACK$$Limit` | Linker symbol at the high end of the stack section |
| `extern` | Declares a symbol without allocating storage |
| Function pointer | `void (*)(void)` — address of a function |
| Exception | CPU-detected fault or system event with a vector |
| `__LINE__` | Preprocessor macro expanding to the current line number |
| `assert_failed()` | Common unrecoverable-error handler |
| `NVIC_SystemReset()` | CMSIS function that resets the MCU |
| `__stackless` | IAR extension: function uses no stack, cannot return |
| Weak alias | Fallback definition used only if the symbol stays undefined |
| BSP | Board Support Package |
| Fault injection | Deliberately causing a fault to test its handler |

---

## Pitfalls to remember

- **Defining instead of declaring** a linker symbol — you shadow the real one.
- **Omitting the Reserved gaps** — every later vector shifts and the table is
  garbage.
- **Endless-loop fault handlers in production** — denial of service.
- **A fault handler that touches the stack** — re-faults immediately.
- **Non-standard handler names** — breaks RTOS integration.
- **Never testing your fault handlers.**
- **Flashing a malformed vector table** — can lock the debugger out of the board.

---

**Next:** Lesson 16 finally introduces **interrupts** — replacing the brain-dead
delay loop with a SysTick alarm.
