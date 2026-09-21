# Lesson 22 — Practice: Switching Context by Hand

**Concepts behind this exercise:** [KNOWLEDGE.md](KNOWLEDGE.md)
**Video:** <https://youtu.be/TEq3-p0GWGI>

---

## Projects in this lesson

| Directory | Toolchain | Target |
|---|---|---|
| `tm4c123-keil/` | KEIL MDK (`lesson.uvprojx`) | EK-TM4C123GXL LaunchPad |
| `CMSIS/`, `ek-tm4c123gxl/` | — | support code |

> **Prerequisite:** re-watch/re-read **Lesson 18** (Cortex-M interrupt entry and
> the stack frame). Everything here depends on it.

Start from a copy of the Lesson 21 project. Revert to the **sequential** version
and delete the event-driven alternative — this and the next few lessons focus on
sequential architectures.

## Required project settings

1. **Turn OFF the Floating Point Hardware** (as in Lesson 18). The FPU
   complicates the interrupt frame, and the frame is the whole subject here.
2. **Give the heap a non-zero size** — the KEIL debugger's *semihosting* feature
   wants some.

---

## Part A — Why sequential code can't do it

### 1. Try the naive approach

Copy-paste the green-LED blinking code, change the colour to blue and the
delays. Build and run.

**Result:** both LEDs blink, but **in sequence** — green on/off, then blue
on/off. Not simultaneously.

That is exactly what sequential code does: you extended the **hard-coded event
sequence**.

> To blink them independently while keeping the simple sequential structure, you
> need **two background loops running simultaneously**.

### 2. Create two background loops

```c
void main_blinky1(void) {
    while (1) {
        BSP_ledGreenOn();
        BSP_delay(BSP_TICKS_PER_SEC / 4U);
        BSP_ledGreenOff();
        BSP_delay(3U * BSP_TICKS_PER_SEC / 4U);
    }
}

void main_blinky2(void) {
    while (1) {
        BSP_ledBlueOn();
        BSP_delay(BSP_TICKS_PER_SEC / 2U);
        BSP_ledBlueOff();
        BSP_delay(BSP_TICKS_PER_SEC / 3U);
    }
}
```

Call both from `main()` so the compiler doesn't eliminate them as unused.

**But you can't just call them one after the other** — the compiler knows the
first never returns and eliminates the second as **unreachable**. Guard it:

```c
if (x) { main_blinky1(); }     /* x is a volatile variable */
main_blinky2();
```

Run it: only the **blue** LED blinks, from `main_blinky2()`.

---

## Part B — Hack the return address

### 3. Set up the view

Breakpoint at the **end of `SysTick_Handler`** in `bsp.c` (it fires 100×/second,
from Lesson 21).

When hit, open **Memory1**, dock it on the right, and scroll to the address in
**SP**.

### 4. Locate the stacked PC

Grab the interrupt stack frame layout from the TivaC datasheet and align it with
your memory view — **flipped upside down**, since the ARM stack grows toward low
addresses.

The **7th entry from the top** is the **PC** — the value loaded into PC on return.

Verify it: note the value (e.g. `0x40E`), step through the **`BX LR`**, and
confirm you return exactly there. That is a **normal** interrupt return to the
point of preemption.

### 5. Cheat

Hit the breakpoint again, and this time **overwrite that stack entry** with the
address of `main_blinky1` (e.g. `0x7C6`).

Step through `BX LR` — **you return to `main_blinky1`**, a completely different
place from the preemption point.

Remove the breakpoint and run: **the green LED is blinking.**

Repeat, switching back to `main_blinky2`: **the blue LED blinks.**

> You just switched the CPU back and forth between two background loops using an
> interrupt. That demonstrates: (1) it's possible; (2) the mechanism is the
> **interrupt hardware already in the processor**; (3) this is **multitasking on
> a single CPU**.

### 6. Understand why this is illegal

Colour-code the registers mentally: green for blinky1's, blue for blinky2's.

| Case | Saved | Restored | Returns to | Verdict |
|---|---|---|---|---|
| Normal interrupt | blinky1's | blinky1's | blinky1 | fine |
| Your hack | blinky1's | **blinky1's** | **blinky2** | **wrong** |

> It happens to work for dead-simple blinky threads, but **will break for more
> complex threads that use more registers**.

**The fix: give each thread its own private stack**, so register sets stay
separate.

---

## Part C — Private stacks

### 7. Create a stack per thread

A stack is just RAM plus a pointer to its top:

```c
uint32_t stack_blinky1[40];
uint32_t *sp_blinky1 = &stack_blinky1[40];   /* ONE WORD PAST THE END */

uint32_t stack_blinky2[40];
uint32_t *sp_blinky2 = &stack_blinky2[40];
```

One past the end, because **the ARM stack grows down** — from the end of the
array toward its beginning.

### 8. Fabricate an interrupt stack frame

Stop calling the thread functions. Instead, **pre-fill each stack** so it looks
as if the thread had been preempted just before its first instruction. Use the
datasheet's exception-frame layout as a template.

Three rules to apply:

- **Start at the high-memory end.**
- **8-byte alignment** is required (Lesson 18). A **40-word** array ends on an
  8-byte boundary — so **no aligner entry is needed**.
- ARM uses a **full stack**: SP points at the **last used** entry. To push:
  **decrement first, then dereference and write.**

```c
*(--sp_blinky1) = (1U << 24);                  /* xPSR: THUMB bit (24) set */
*(--sp_blinky1) = (uint32_t)&main_blinky1;     /* PC = thread function     */
*(--sp_blinky1) = 0x0000000EU;                 /* LR  \                    */
*(--sp_blinky1) = 0x0000000CU;                 /* R12  | recognizable      */
*(--sp_blinky1) = 0x00000003U;                 /* R3   | numbers make the  */
*(--sp_blinky1) = 0x00000002U;                 /* R2   | frame easy to     */
*(--sp_blinky1) = 0x00000001U;                 /* R1   | spot in memory    */
*(--sp_blinky1) = 0x00000000U;                 /* R0  /                    */
```

- **xPSR bit 24** is the **THUMB state** bit. Cortex-M can't be in any other
  state, but historically it must be set.
- **PC** must be the **thread function's address** — `&` on a function name gives
  a **pointer to function**, cast to `uint32_t` to fit on the stack.
- The other registers don't matter (a thread never returns); the recognizable
  values are purely for debugging.

Do the same for blinky2 with `&main_blinky2`.

Finally, keep `main()` alive with an empty `while (1) {}`.

### 9. Verify the setup

Run free: **no LEDs blink** — `main()` is spinning in its empty loop.

But the stacks are initialized. Check in **Memory1**: find blinky1's frame and
blinky2's frame. Open **Watch1** and add `sp_blinky1` and `sp_blinky2`.

---

## Part D — Switch context properly

### 10. Switch to blinky1

Breakpoint at the end of `SysTick_Handler`. When hit, **change the CPU's SP
register to the value of `sp_blinky1`**.

Step through `BX LR` → **you are in the blinky1 thread.** Remove the breakpoint
and run: **green LED blinking.**

Notice what you did *not* do: **you never touched the stack contents.**

### 11. Switch to blinky2 — the full procedure

Breakpoint at the end of SysTick again. This time, **before** overwriting SP:

1. **Copy the CPU's SP into `sp_blinky1`** — that really is blinky1's current top
   of stack, and it must be saved before switching away.
2. **Then** write `sp_blinky2` into the CPU's SP.

Step through `BX LR` → you are in blinky2. Run → **blue LED blinking.**

### The procedure, in general

> Break at the end of SysTick → copy the CPU's SP into the **current** thread's
> stack-pointer variable → copy the **next** thread's stack pointer into the
> CPU's SP → return from interrupt.

### 12. Observe what gets preserved

Switch back to blinky1 and look at where you resume: **not** the start of
`main_blinky1`, but **precisely the point of preemption** — a specific location
in `BSP_tickCtr()`, called from `BSP_delay()`, called from `main_blinky1()`.

**The whole call chain lives on that thread's private stack.**

Run free and watch: the two threads now execute independently — e.g. blinky1
running while blinky2 sits preempted with the blue LED on.

---

## Part E — The R4–R11 problem

### 13. Understand the flaw

The Cortex-M frame follows the **AAPCS** (Lesson 18): it saves only the registers
a **function call may clobber**, and **not R4–R11**, which a function must
preserve.

That's fine for a **normal** ISR, which runs to completion and returns to the
code it preempted — if it uses R7 it saves and restores R7 itself.

**But your ISR returns to a different thread.** That thread may also use R7, and
since you are executing only a *fragment* of it — not a complete function — it is
**not obliged to honour the AAPCS**. By the time blinky1 resumes, **R7 may be
clobbered**. Same for all of R4–R11.

### 14. Extend the manual procedure

**When fabricating** each thread's stack, append the 8 extra registers to the
frame.

**When saving the current thread:**

- push **R11 down to R4** on top of the ISR frame;
- **subtract `0x20`** from SP before storing it in the thread's stack pointer.

**When restoring the next thread:**

- restore **R11 down to R4** from the thread's stack;
- **add `0x20`** to the thread's stack pointer before writing it to the CPU's SP.

Tedious by hand — which is exactly why Lesson 23 automates it in software.

---

## Exercises

1. **Break the hack.** Give blinky1 a local variable the compiler keeps in R7,
   then use the Part B (PC-only) hack. Can you observe corruption?
2. **Stack sizing.** How many words does one thread actually need here? Shrink
   the array until it breaks, and explain the failure.
3. **Misalign it.** Size a stack array to 41 words and see what the frame
   alignment does.
4. **Drop the THUMB bit.** Clear bit 24 in a fabricated xPSR and predict the
   fault before running it.
5. **Third thread.** Add `main_blinky3` with its own stack and rotate among all
   three by hand.
6. **Round-robin on paper.** Write pseudocode for an ISR that rotates
   automatically among N threads. (Compare it with Lesson 23.)
7. **Count the cost.** How many CPU cycles does the full save/restore of R4–R11
   plus the SP swap take?

---

## Self-check

- [ ] I reproduced the "blink in sequence" failure of naive sequential code
- [ ] I found the stacked PC and redirected an interrupt return with it
- [ ] I can explain why changing only the PC is illegal
- [ ] I fabricated an interrupt stack frame with the THUMB bit and the thread
      address
- [ ] I switched threads by swapping only SP, without touching stack contents
- [ ] I saw a thread resume at its preemption point with its call chain intact
- [ ] I can state why R4–R11 must be saved by the kernel, not the hardware
