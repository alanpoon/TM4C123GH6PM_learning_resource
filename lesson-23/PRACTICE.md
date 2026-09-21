# Lesson 23 — Practice: Building MiROS, Part 1

**Concepts behind this exercise:** [KNOWLEDGE.md](KNOWLEDGE.md)
**Video:** <https://youtu.be/PKml9ki3178>

---

## Projects in this lesson

| Directory | Toolchain | Target |
|---|---|---|
| `tm4c123-keil/` | KEIL MDK (`lesson.uvprojx`) | EK-TM4C123GXL LaunchPad |
| `stm32c031-keil/` | KEIL MDK | STM32 NUCLEO-C031C6 |
| `CMSIS/`, `ek-tm4c123gxl/`, `nucleo-c031c6/` | — | support code |

Start from a copy of the Lesson 22 project.

## Files you create

Add a project group **MiROS** with two files:

| File | Contents |
|---|---|
| `miros.h` | the RTOS **API** — inclusion guards, licence comment, types, prototypes |
| `miros.c` | the complete **implementation** |

> Give both a header comment with a brief description, copyright and licensing
> terms. The course uses **GPL**, with the standard warranty disclaimer, and
> states the code is **a teaching aid, not recommended for commercial
> applications**.

---

## Part A — Represent and start threads

### 1. Define the TCB

Look at `main.c`: each thread needs a private stack pointer, and probably more
later. Capture that:

```c
typedef struct {
    void *sp;        /* to be extended as MiROS grows */
} OSThread;
```

This is the **Thread Control Block (TCB)** in standard RTOS terminology.

> The **`OS`** prefix marks what belongs to the operating system and **reduces
> name collisions** in large projects.

In `main.c`, include `miros.h` and replace the raw stack pointers with `OSThread`
objects.

### 2. Define the thread-start API

```c
typedef void (*OSThreadHandler)(void);   /* pointer to fn: no args, returns void */

void OSThread_start(
    OSThread *me,
    OSThreadHandler threadHandler,
    void *stkSto, uint32_t stkSize);
```

> `me` is the pointer-to-self convention explained in Lesson 29 (OOP in C).

### 3. Implement it

Move the hand-written frame-building code from `main.c` into `miros.c`, and
stitch it together:

```c
void OSThread_start(OSThread *me, OSThreadHandler threadHandler,
                    void *stkSto, uint32_t stkSize)
{
    /* stack grows HIGH -> LOW: start from the end */
    uint32_t *sp = (uint32_t *)((((uint32_t)stkSto + stkSize) / 8) * 8);
    /*                           ^^^^ round DOWN to an 8-byte boundary    */

    *(--sp) = (1U << 24);                    /* xPSR: THUMB bit  */
    *(--sp) = (uint32_t)threadHandler;       /* PC (see the bug in Part E!) */
    *(--sp) = 0x0000000EU;                   /* LR   */
    /* ... R12, R3, R2, R1, R0 ... */

    me->sp = sp;                             /* save the top of the frame */

    /* optional: pre-fill the unused stack with a known pattern */
    for (uint32_t *p = stkLimit; p < sp; ++p) {
        *p = 0xDEADBEEFU;
    }
}
```

Two details worth understanding:

- **The rounding.** The caller may not know about Cortex-M's 8-byte alignment
  requirement, so the kernel must not assume the supplied buffer ends aligned.
  Integer `/8` then `*8` rounds down.
- **`0xDEADBEEF` pre-fill.** It makes the stack visible in a memory view and lets
  you later determine **worst-case stack usage**.

### 4. Call it

Replace the hand-built frames in `main.c` with two calls to `OSThread_start()` —
one per blinky. Build: clean.

---

## Part B — Choose the context-switch exception

### 5. Understand why not SysTick

The switch must happen during an **interrupt return**. Coding it inside
`SysTick_Handler` would force you to add it to **every ISR**, and would destroy
Cortex-M's nicest property — that ISRs are **pure C functions**. (The switch
itself can't be written in standard C anyway: it needs assembly for the stack
frames and SP.)

### 6. Trigger PendSV manually

Add an empty `PendSV_Handler()` at the end of `miros.c`, build, and debug.

First verify `OSThread_start()` produced the right stacks — find blinky1's and
blinky2's frames in the memory view.

Then breakpoint in `SysTick_Handler` and run. When hit:

1. Scroll the memory view to **`0xE000ED04`** — the **ICSR** in the System
   Control Block.
2. The datasheet says **bit 28** pends PendSV, so write **`0x10000000`**.
3. Move the SysTick breakpoint to the **next instruction**, and set another in
   **`PendSV_Handler`** — the two-path trick from Lesson 18.
4. Run.

**Result: `PendSV_Handler` is hit first.** PendSV was triggered — but it
**preempted the still-active SysTick**, which is the **wrong order**. Continue
and you hit SysTick, then `main`.

> You want SysTick to **complete**, and *then* PendSV to switch context.

---

## Part C — Fix the priorities

### 7. Inspect SYSPRI3

View **`0xE000ED20`** in memory. Default: SysTick = **`0xE0`**, PendSV = **`0`**.

> **Cortex-M priorities run backwards: a higher number means lower preemption
> priority.** That's why PendSV (0) preempted SysTick (`0xE0`).

### 8. Flip them and re-test

Give **SysTick 0** and **PendSV `0xE0`**. Repeat the experiment from step 6.

**Now SysTick is hit first** — it is not preempted — and PendSV still runs
afterwards, returning to `main`. Exactly what you want.

### 9. Note the implemented-bits quirk

Write **`0xFF`** into PendSV's priority byte — it reads back as **`0xE0`**.
Cortex-M implements priority only in the **highest-order bits**:

| MCU | Bits | `0xFF` reads back as |
|---|---|---|
| TivaC | 3 | `0xE0` |
| STM32 | 4 | `0xF0` |

> **Remember:** PendSV needs the **lowest priority of everything**, and writing
> **`0xFF`** achieves that on **every** Cortex-M variant.
> See *"Cutting Through the Confusion with Arm Cortex-M Interrupt Priorities"*.

### 10. Codify it in `OS_init()`

```c
void OS_init(void) {
    /* set PendSV to the lowest priority (raw address, for portability
       across Cortex-M0/M3/M4/M7 -- the byte is at the same address in all) */
    *(uint32_t volatile *)0xE000ED20 |= (0xFFU << 16);
}
```

Prototype it in `miros.h` and call it from `main()`.

In `bsp.c`, **raise SysTick's priority** away from the lowest level (to 0), since
the lowest is reserved for PendSV. Here you're committed to the TM4C anyway, so
CMSIS's `NVIC_SetPriority()` is fine.

---

## Part D — The scheduler stub

### 11. Track current and next

```c
OSThread * volatile OS_curr;
OSThread * volatile OS_next;
```

> **`volatile` goes *after* the asterisk** so that **the pointer** is volatile.
> Before the asterisk would give a non-volatile pointer to a volatile struct —
> wrong.

### 12. Write `OS_sched()`

```c
void OS_sched(void) {
    /* ...decide OS_next... (manual for now) */
    if (OS_next != OS_curr) {
        *(uint32_t volatile *)0xE000ED04 = (1U << 28);   /* pend PendSV */
    }
}
```

Only pend when the next thread actually differs.

### 13. Decide the critical-section policy

Race conditions around `OS_curr`/`OS_next` are the **hardest part of writing an
RTOS**. Two options:

| Option | Verdict |
|---|---|
| Disable interrupts inside `OS_sched()` | simple but problematic |
| **Require callers to already be in a critical section** | **preferred** — the scheduler often runs with interrupts already disabled |

So call it like this at the end of `SysTick_Handler`:

```c
__disable_irq();
OS_sched();
__enable_irq();
```

---

## Part E — Write `PendSV_Handler`

### 14. Sketch it in C and steal the disassembly

Write the logic in C first, then copy the compiler's output as a starting point:

```
__disable_irq();
if (OS_curr != (OSThread *)0) {
    /* push r4-r11 */                 <- comment; can't be C
    OS_curr->sp = sp;                 <- fake local 'sp'
}
sp = OS_next->sp;
OS_curr = OS_next;
/* pop r4-r11 */
__enable_irq();
```

> The `OS_curr != 0` check matters: **`OS_curr` is zero out of reset**, since no
> thread is running yet.

### 15. Walk it through the debugger first

- Step over `BSP_init()` and `OS_init()`; verify SysTick = 0 and PendSV = `0xE0`.
- Breakpoint in `SysTick_Handler`, run. Verify the **`CPSID I`** instruction
  disables interrupts.
- Step into `OS_sched()`. Watch `OS_curr` and `OS_next` — both zero.
- **Manually set `OS_next`** to the address of your `blinky1` object.
- Check the **Call Stack**: `OS_sched` ← `SysTick_Handler` ← `main`.
- Step out; verify **`CPSIE I`** re-enables interrupts.
- Breakpoint in `PendSV_Handler` and run — it is hit, **directly preempting
  `main`**. The mechanism works.

> Worried about exiting one interrupt and entering another? For **back-to-back**
> exceptions the core skips the register pop and push — a hardware optimization
> called **tail-chaining**. The cost is comparable to a plain function call.

### 16. Convert to assembly

Select the compiler-generated machine code for `PendSV_Handler`, exit the
debugger, and paste it in. Then:

1. Mark the function assembly: **`__asm void PendSV_Handler(void)`** (KEIL
   Compiler 5; other compilers have equivalents).
2. Delete the C code; turn the interleaved C statements into **comments**.
3. Strip each disassembly line down to **mnemonic + operands** (drop the address
   and the opcode).
4. Replace the "push registers" comment with **`PUSH {r4-r11}`**, and the
   corresponding one with **`POP {r4-r11}`**.
5. Find the `CBZ` branch target, place an assembly label there (e.g.
   **`PendSV_restore`**), and use it in the `CBZ`.
6. Replace the compiler's fake `sp` register (e.g. `r0`) with the **real SP**.
7. Leave the `CPSIE I` and `BX lr` as they are.

Build → **error: the assembler doesn't recognize `OS_curr` / `OS_next`.** Fix it
with explicit **`IMPORT`** directives.

Builds and links cleanly. Tidy up the comments before moving on. (You'll notice
repetition and optimization opportunities — worth doing, since this code runs
often, but that's Lesson 24's business.)

---

## Part F — Test, and find the bug

### 17. Step through your creation

Breakpoint in `SysTick_Handler`, run, step into `OS_sched()`, manually set
`OS_next` to `blinky1`, and continue into `PendSV_Handler`. Step one instruction
at a time:

- First time through, `OS_curr` in R1 is **zero**, so the `CBZ` branch is taken.
- The SP loaded from `OS_next` looks reasonable — scroll the memory view there
  and confirm the stack content.
- `OS_curr` gets set to `OS_next` — visible in Watch1.
- The `POP` restores R4–R11 to exactly the values you fabricated in
  `OSThread_start()`.
- SP moves; scroll to the new top of stack.
- Interrupts are re-enabled; you are at the `BX lr`.

### 18. Hit the HardFault

Step the `BX lr` and… **you land in `HardFault_Handler`.**

**Don't panic — debug.** Everything worked up to the return, so reset and repeat
to that instruction, then reason about what can make a return fail:

1. **A bad LR?** It reads `0xFFFFFFF9` — correct (Lesson 18).
2. **So it must be the stacked PC.** Look at it: it starts with **`0x2`** — a
   **RAM** address, not ROM. Very suspect.
3. Find the code that produced it — the PC line in `OSThread_start()`.

**The bug:** `threadHandler` is **already a pointer to function**, so taking its
address with `&` is wrong.

### 19. Fix and re-test

Remove the `&`, rebuild, and repeat the whole test. The stacked PC is now clearly
a **ROM** address.

Step the `BX lr` → **you are inside `main_blinky1`.** Continue: you hit the
scheduler breakpoint again, and **the green LED is on**.

Remove the `OS_sched` breakpoint and run free: **the green LED blinks** —
blinky1 is running.

Restore the breakpoint, manually schedule **blinky2**, verify its stack content,
step the `BX lr`, and confirm the switch. Stop in the scheduler — **the blue LED
is on**. Run free: **the blue LED blinks.**

Switch back and forth as many times as you like.

---

## Exercises

1. **Worst-case stack.** Run for a while, then inspect how much of the
   `0xDEADBEEF` pattern survives. How deep did each thread actually go?
2. **Break the alignment.** Pass a stack buffer whose end is not 8-byte aligned
   and confirm the rounding saves you.
3. **Wrong priority.** Set PendSV to priority 0 and observe exactly how the
   system misbehaves.
4. **Skip the null check.** Remove the `OS_curr != 0` test and predict the first
   fault before running it.
5. **Count the cost.** From the disassembly, count the instructions in your
   `PendSV_Handler`. Where is the obvious redundancy?
6. **Port it.** Set PendSV's priority on the STM32C031 project. What does `0xFF`
   read back as there?
7. **Tail-chaining.** Measure the SysTick-exit → PendSV-entry gap with a GPIO pin
   and compare it against a full exception entry.

---

## Self-check

- [ ] `miros.h` / `miros.c` exist with licence headers and inclusion guards
- [ ] `OSThread_start()` rounds the stack down to 8 bytes and fabricates the frame
- [ ] `OS_init()` sets PendSV to the lowest priority using a raw address
- [ ] SysTick's priority is raised away from the lowest level
- [ ] `OS_curr` / `OS_next` are declared with `volatile` **after** the asterisk
- [ ] `OS_sched()` only pends PendSV when the next thread differs
- [ ] `PendSV_Handler` is assembly, pushes/pops R4–R11, and swaps SP
- [ ] I found and fixed the `&threadHandler` bug by reasoning from the stacked PC
- [ ] I switched manually between blinky1 and blinky2 and saw both LEDs blink
