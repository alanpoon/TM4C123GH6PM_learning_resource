# Lesson 19 — Practice: Porting to GNU-ARM and Eclipse

**Concepts behind this exercise:** [KNOWLEDGE.md](KNOWLEDGE.md)
**Video:** <https://youtu.be/BBF3ZMi8WK4>

---

## Projects in this lesson

| Directory | Toolchain | Target |
|---|---|---|
| `tm4c123-ccs-gnu/` | **CCS + GNU-ARM** | EK-TM4C123GXL LaunchPad |
| `tm4c123-keil-gnu/` | KEIL + GNU-ARM | EK-TM4C123GXL LaunchPad |
| `stm32c031-keil-gnu/` | KEIL + GNU-ARM | STM32 NUCLEO-C031C6 |
| `CMSIS/` | — | CMSIS headers (now supplied by you) |
| `ek-tm4c123gxl/`, `nucleo-c031c6/` | — | device support code |

> The finished, corrected projects are here. The walkthrough below reconstructs
> how they were built, which is where the learning is.

---

## Part A — Install and create the project

### 1. Install Code Composer Studio

Search ti.com for **CCS** (URLs change; search rather than bookmark). It is free
and unlimited **with the GNU-GCC toolset**. Registration plus export-control
forms are required.

During component selection:

- expand **32-bit ARM MCUs** → select **Tiva-C Series** support;
- explicitly select the **GCC ARM Compiler**.

> Avoid install paths with spaces or non-standard characters.

### 2. Choose a workspace

On first launch CCS asks for a **workspace** — an Eclipse concept grouping
related projects.

> Use a **dedicated workspace for this course** rather than one default workspace
> for everything. Put it alongside your lessons, e.g. `embedded_programming/ccs`.

### 3. Create the project

| Step | Choice |
|---|---|
| Target | **Tiva-C** family, the exact TM4C on your board |
| Connection | **Stellaris-ICDI** (the reason CCS was chosen) |
| Name | **`lesson`** — generic, reused by cloning for later lessons |
| Location | **not** the default inside the workspace — use your lessons directory, `.../lesson19` |
| **Compiler** | **GNU**, *not* the default TI compiler ← **critical step** |

Build with the **hammer** button: 0 problems.

---

## Part B — Audit the generated code (don't skip)

### 4. Read the generated startup code

It is typical vendor startup code, with all the shortcomings from Lessons 13–15:

1. **Proprietary exception names** — not CMSIS-compliant.
2. **The vector table needs hand-editing** whenever you start or stop using a
   handler (e.g. to use `SysTick_Handler` you'd edit the table *and* add a
   prototype).
3. **Exception handlers are endless loops.** If one ever runs, the system
   freezes — **denial of service**, unacceptable in production.

### 5. Read the generated linker script (`.lds`)

Its job is the same as IAR's `.icf` (Lesson 14): tell the linker where ROM and
RAM are and where to place sections.

Find where it puts the stack: **the last section in RAM**.

> **That is a mistake.** The stack grows toward **lower** addresses, so an
> overflow silently corrupts the RAM sections above it. This is the likely cause
> of failure in the **Toyota unintended acceleration** cases (see *"Are we
> shooting ourselves in the foot with stack overflow?"*).
>
> **Better: put the stack first in RAM.** Overflow then runs into unmapped memory
> and raises a **HardFault** — you find out immediately.

---

## Part C — Port your code

### 6. Copy the reusable files

From `lesson18`, copy in: **`bsp.h`, `bsp.c`, `main.c`, `startup_tm4c.c`**, and
the TM4C device header.

They appear in the project **immediately** — Eclipse automatically includes every
source file in the project directory.

> **The flip side:** you now have **two startup files**. Delete one.

### 7. First build → missing CMSIS

Error: cannot find **`core_cm4.h`**. CMSIS ships with IAR but **not** with CCS.

1. Create a **`CMSIS/Include`** directory alongside your lessons, so every future
   project can share it.
2. Tell the compiler where it is: right-click the project → **Properties → GNU
   Compiler → Directories → include paths → `+`**.

**Do not browse to it** — that produces an **absolute** path that only works on
your machine. Use an Eclipse build variable instead:

```
${PROJECT_LOC}/../CMSIS/Include
```

> Forward slashes work on Windows too and are more universal.

### 8. Second build → IAR extensions rejected

Most remaining errors come from the **startup code**, which used **IAR-specific
extensions** GCC doesn't recognize.

Replace it with the **GNU startup code and its matching linker script** — they
must match, so copy both, overwriting the generated `.lds` and removing the old
startup file. Eclipse picks up the changes immediately.

### 9. Review the GNU startup code

Walk through it and find each piece:

| Feature | How |
|---|---|
| CMSIS compliance | names per CMSIS 4.3.0 |
| Vector table placement | `__attribute__((section(".isr_vector")))` — GNU's equivalent of IAR's `@` |
| Section is at address 0 | the linker script makes `.isr_vector` the **first section in ROM**, and ROM starts at 0 |
| Initial SP | `(int)&__stack_end__` — a **linker-provided** symbol, declared `extern` at the top of the file (same idea as `CSTACK$$Limit`, Lesson 15) |
| Stack location | **`.stack` is the first section in RAM** — overflow faults instead of corrupting |
| Stack size | the `STACK_SIZE` symbol at the top of the linker script |
| Handlers | every CMSIS-named handler present, weak-aliased to `Default_Handler` |
| Fault handlers | inline assembly that **avoids the stack**, stores fault info in **r0/r1**, branches to **`assert_handler`** |
| No stack usage | `__attribute__((naked))` |

**Understand `weak` + `alias`:**

- **`weak`** — this definition may be overridden; the linker discards it quietly
  instead of reporting a duplicate symbol.
- **`alias`** — if the symbol isn't defined, use the named alias instead.

So if you define `SysTick_Handler`, the linker takes yours. If you don't, it
takes `Default_Handler`. **Either way you never edit the vector table.**

> The standard fault handlers are *not* aliased — they are defined right in the
> startup code, so your application needn't supply them.

### 10. Third build → `__stackless` in `bsp.c`

Replace the IAR keyword with the GNU equivalent:

```c
/* IAR */  __stackless void assert_failed(...)
/* GNU */  __attribute__((naked)) void assert_failed(...)
```

### 11. Fourth build → `__enable_interrupt()`

Another IAR intrinsic. The GNU/CMSIS equivalent:

```c
__enable_irq();
```

### 12. Build again — clean

No errors. **That is your first port of deeply embedded code between toolsets.**

---

## Part D — Debug in Eclipse

### 13. Run it

Plug in the board and press the **bug** button to flash and start debugging. The
debugger stops at `main()`.

- **Go** → the LED changes colour once a second.
- **Pause** → the code stops in the background loop.

### 14. Learn the Debug Perspective

The layout you now see is Eclipse's **Debug Perspective** (as opposed to the
**Edit Perspective**). It provides the same views you used in IAR —
**disassembly**, registers, memory.

- Single-step and watch the LED turn on and off.
- Breakpoint inside `SysTick_Handler` to see how often it fires.
- **Stop** returns you to the Edit Perspective.

### 15. Make a change

Change the toggled LED from red to blue. Rebuild, debug, and watch the new
colour — confirmation that the ported code is genuinely yours to modify.

---

## Exercises

1. **Move the stack back.** Edit the linker script to put the stack last in RAM,
   overflow it, and compare the symptom with the stack-first arrangement.
2. **Prove the weak alias works.** Delete your `SysTick_Handler`, rebuild, and
   confirm it links cleanly — then find what runs instead.
3. **Resize the stack.** Change `STACK_SIZE` and verify the new `__stack_end__`
   in the map file.
4. **Portability table.** Write out every IAR extension used in Lessons 13–18 and
   its GNU equivalent, from memory.
5. **Absolute vs relative.** Add an absolute include path, move the project
   directory, and watch it break. Then fix it with `${PROJECT_LOC}`.
6. **Compare startup code.** Diff the CCS-generated startup against the
   production-quality one. List every substantive difference.
7. **Third toolchain.** Open `tm4c123-keil-gnu/` and see what changes again when
   GNU-ARM is driven by KEIL instead of Eclipse.

---

## Self-check

- [ ] CCS is installed with the **GNU** compiler and Tiva-C support
- [ ] My project uses a **relative** include path to `CMSIS/Include`
- [ ] I replaced `__stackless` and `__enable_interrupt()` with GNU equivalents
- [ ] My vector table uses `section(".isr_vector")` and needs no hand-editing
- [ ] The stack is the **first** section in RAM
- [ ] Fault handlers avoid the stack and route to `assert_handler`
- [ ] The ported code builds cleanly and blinks on the board
