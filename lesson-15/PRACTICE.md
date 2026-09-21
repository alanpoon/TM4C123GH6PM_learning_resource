# Lesson 15 — Practice: A Production-Quality Vector Table

**Concepts behind this exercise:** [KNOWLEDGE.md](KNOWLEDGE.md)
**Video:** <https://youtu.be/42HbCf5cz5A>

---

## Projects in this lesson

| Directory | Toolchain | Target |
|---|---|---|
| `tm4c123-iar/` | IAR EWARM (`workspace.eww`) | EK-TM4C123GXL LaunchPad |
| `CMSIS/` | — | CMSIS headers |

> A real board is required. The `__stackless` and weak-alias extensions used here
> are **IAR-specific**; KEIL and GCC have equivalents (`__attribute__((weak))`,
> `__attribute__((naked))`) — see the later lessons' projects for those.

## Files at the end of this lesson

```
startup_tm4c.c   -- __vector_table + exception handlers  (from lesson 14)
bsp.c            -- NEW: Board Support Package, assert_failed()
main.c           -- unchanged blinky
```

---

## Part A — The stack pointer entry

### 1. Check the datasheet layout first

Open the vector-table section of the
[TM4C123GH6PM datasheet](../resources/TM4C123GH6PM_Datasheet.pdf).

> Remember: datasheet memory pictures are drawn with **low addresses at the
> bottom** — upside down compared to a C initializer or a debugger view.

Entry 0 = initial **Stack Pointer**. Entry 1 = **Reset** handler.

### 2. Find out how the library did it

Load the **Lesson 13** workspace (which still used the library startup), flash
it, and look at address 0 in the disassembly. The first entry is
**`CSTACK$$Limit`** — so that symbol is known to the linker.

### 3. Read the IAR help

**Help → Contents → "IAR C language extensions"**. You will find:

> The linker generates `section-name$$Base` and `section-name$$Limit` symbols for
> every section, placed at the section's base and limit addresses.

Because the ARM stack grows from **high RAM to low RAM**, the initial SP must be
**`CSTACK$$Limit`** (the high end).

### 4. Hit the "undefined symbol" error

Back in your own startup file, try to use it. `F7` fails: *`CSTACK$$Limit` is
undefined.*

That is correct — **section symbols are created by the linker, after
compilation**. The compiler cannot know them.

### 5. Declare without defining

```c
int CSTACK$$Limit;          /* WRONG — defines a NEW variable, hiding the linker's */
extern int CSTACK$$Limit;   /* right — declares only, no storage allocated         */
```

The type is required but irrelevant here; only the **address** matters.

Now the compiler complains that a `int *` cannot initialize an `int`. Add the
cast:

```c
(int)&CSTACK$$Limit
```

It compiles **and links** — proof the linker resolved the symbol.

### 6. Verify on the board

Flash and check:

- Disassembly at address 0 shows **`CSTACK$$Limit`**.
- **SP** reads something like `0x20000410`.
- The **map file** shows `CSTACK$$Limit` at exactly that address.

### 7. Prove you didn't break the IDE integration

Change the stack size via **Project Options → Linker Configuration File editor**.
Watch `project.icf` update, rebuild, and confirm `CSTACK$$Limit` moved (e.g. to
`0x20000428`). Re-flash and confirm **SP matches the new value**.

---

## Part B — The Reset handler entry

### 8. Reuse the library startup code

The library table pointed Reset at **`__iar_program_start`** — the startup code
from Lesson 13, which does everything you need. Reuse it.

```c
extern void __iar_program_start(void);   /* prototype = declaration */
...
(int)&__iar_program_start
```

Without the cast the compiler complains about `void (*)(void)` — a **pointer to
a function taking no arguments and returning nothing**.

### 9. Note the ampersand-free variant

```c
(int)__iar_program_start    /* also legal — a function name without () can't be a call */
```

Many vendor vector tables are written this way, so you need to recognize it.

> **Recommended: keep the `&`.** It makes "address of" explicit. The rest of the
> table uses ampersands.

### 10. Verify

Flash and confirm the program stops at `__iar_program_start` (PC = e.g. `0x1B8`),
that address 0 shows both `CSTACK$$Limit` and `__iar_program_start`, and that
**the LED still blinks**.

---

## Part C — The standard exceptions

### 11. Copy the layout exactly

From the datasheet, fill in the entries with **negative IRQ numbers**: `NMI`,
`HardFault`, `MemManage`, `BusFault`, `UsageFault`, `SVCall`, `DebugMonitor`,
`PendSV`, `SysTick`.

> ⚠️ **They are not contiguous.** Initialize every **Reserved** slot to **zero**
> and preserve the exact layout. One missing gap shifts every later vector.

Include the device CMSIS header — **all the handler prototypes are already
there**, and the names are **part of the CMSIS standard** (important for later
RTOS integration).

### 12. Write the fault handlers — the production way

The conventional handler is an endless loop. Convenient for debugging, but
shipping it gives you devices that freeze until the battery is pulled — **denial
of service**.

Instead, route every fault through one function:

```c
void HardFault_Handler(void) {
    assert_failed("HardFault", __LINE__);
}
```

`__LINE__` is the standard macro expanding to the current line number. Code the
other fault handlers identically.

### 13. Weak-alias the non-fault exceptions

`SVC_Handler`, `DebugMon_Handler`, `PendSV_Handler`, `SysTick_Handler` aren't
faults. You want them **overridable**, with a safe default:

- Declare weak aliases pointing them at **`Unused_Handler`**.
- Define `Unused_Handler` (and prototype it at the top of the file).

> If the symbol is still undefined at the end of linking, the alias is used. If
> your program **does** define it, the alias is ignored — and no
> multiply-defined-symbol error.

### 14. Create the BSP

Build now: `startup_tm4c.c` compiles cleanly, but the **linker** reports
`assert_failed` undefined. Give it a home:

1. Create **`bsp.c`** — the **Board Support Package** — and add it to the project.
2. Include the device CMSIS header.
3. Define `assert_failed()`. Real damage control depends on your project; revisit
   this once you design an error-recovery strategy. Typically it ends in a reset:

```c
void assert_failed(char const *file, int line) {
    /* damage control / error logging here */
    NVIC_SystemReset();      /* CMSIS-provided reset */
}
```

The build now succeeds with **no errors and no warnings**.

### 15. Verify on the target

- Disassembly at address 0 matches your initializer — fault handlers **and**
  reserved zeros.
- The non-fault handlers all display as `DebugMon_Handler` because they share
  `Unused_Handler`'s address; the debugger shows the alphabetically first name.
- **The LED still blinks.**

### 16. Test the aliasing

Define your own `SysTick_Handler` somewhere in the project. Rebuild — it builds
cleanly, and the disassembly now shows **`SysTick_Handler`** instead of the
`DebugMon_Handler` alias. The LED still blinks.

---

## Part D — Fault injection (don't skip this)

You have tested **neither** the fault handlers **nor** `assert_failed`. Faults
don't happen on their own in a blinky program — so cause one deliberately.

### 17. Inject a stack overflow

1. Breakpoint at the start of `delay()` and run.
2. At the breakpoint — **before** the `PUSH` — set **SP to `0x20000000`** (the
   start of RAM).
3. Single-step **carefully**.

As SP drops below RAM, the program jumps to **`HardFault_Handler`**. Expected
so far.

### 18. Watch it fail

Step again — **you don't move forward**. Run at full speed and break in: you are
still at the top of `HardFault_Handler`.

**Why?** Its first instruction is a **`PUSH`**, which faults again and re-enters
HardFault. `assert_failed` is never called. You have an implicit endless loop —
**exactly the denial of service you set out to avoid**.

### 19. Fix it with `__stackless`

> You must not touch the stack in a fault handler, because the stack may be the
> very thing that's broken.

```c
__stackless void HardFault_Handler(void) { ... }
__stackless void assert_failed(char const *file, int line) { ... }
```

`__stackless` tells the compiler not to use the stack. Such a function violates
the calling convention so that **it cannot return** — which is fine, you never
wanted to.

Apply it to **every fault handler and to `assert_failed`**.

### 20. Re-test

Rebuild, confirm the LED blinks, then repeat steps 17–18. This time:

- HardFault is taken;
- there is **no `PUSH`**, so execution reaches **`assert_failed`**;
- `assert_failed` doesn't touch the stack either, so **`NVIC_SystemReset()`**
  succeeds — and you land back at **`__iar_program_start`**, your Reset handler.

No more denial of service.

> Most vendor startup code never gets this far. It's adequate for debugging and
> inadequate for shipping.

---

## Part E — The device interrupt vectors

### 21. Add every IRQ

Find the complete **IRQ list** in the datasheet (a couple of pages above the
vector-table layout). Add every entry to your table.

This is **tedious but introduces no new tricks**:

- preserve the **Reserved** entries exactly;
- prototypes come from the CMSIS header;
- weak-alias every interrupt handler to **`Unused_Handler`**.

A final build should be clean.

---

## Exercises

1. **Break the layout.** Delete one Reserved zero, rebuild, and work out which
   handler now runs when a given exception fires.
2. **Log it.** Extend `assert_failed` to store the file and line in a RAM
   location that survives reset, then read it back after a fault.
3. **Inject a different fault.** Cause a **UsageFault** (e.g. a divide by zero
   with the trap enabled) and confirm the right handler runs.
4. **Breakpoint discipline.** Set a permanent breakpoint in `assert_failed` and
   verify that it catches faults from several different sources.
5. **Weak-alias test.** Define `PendSV_Handler` in `main.c` and confirm the alias
   is silently dropped — no multiply-defined error.
6. **Stackless check.** Compare the disassembly of a handler with and without
   `__stackless`. Which instructions disappear?
7. **Port it.** Look at how a KEIL or GCC project in a later lesson expresses
   weak symbols and stackless functions.

---

## Self-check

- [ ] SP is initialized from `CSTACK$$Limit` and tracks the IDE's stack setting
- [ ] Reset points at `__iar_program_start` and the board still blinks
- [ ] Every Reserved slot in my table is zero and the layout matches the datasheet
- [ ] All fault handlers call `assert_failed()`; none is an endless loop
- [ ] Non-fault handlers are weak-aliased to `Unused_Handler` and are overridable
- [ ] `bsp.c` exists and defines `assert_failed()` ending in `NVIC_SystemReset()`
- [ ] I injected a stack overflow, saw the re-fault, and fixed it with `__stackless`
- [ ] All device IRQ vectors are present and aliased
