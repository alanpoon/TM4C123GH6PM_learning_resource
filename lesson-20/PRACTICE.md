# Lesson 20 — Practice: Creating a Race Condition on Purpose

**Concepts behind this exercise:** [KNOWLEDGE.md](KNOWLEDGE.md)
**Video:** <https://youtu.be/3ha72Y8pyD4>

---

## Projects in this lesson

| Directory | Toolchain | Target |
|---|---|---|
| `tm4c123-ccs-gnu/` | CCS + GNU-ARM | EK-TM4C123GXL LaunchPad |
| `CMSIS/`, `ek-tm4c123gxl/` | — | support code |

### Directory layout established in Lesson 19

```
embedded_programming/
├── ccs/         <- Eclipse workspaces
├── CMSIS/       <- CMSIS headers (from the course downloads)
└── lesson19/    <- project + code
```

### Opening the project in Eclipse

Eclipse projects can't just be double-clicked:

1. Copy `lesson19` → `lesson20`.
2. Launch CCS with the **`ccs`** workspace.
3. **Delete the old `lesson` project** from the workspace — Eclipse won't accept
   two projects with the same name. **Do not** tick *"delete project contents on
   disk"*.
4. **File → Import → Existing Projects into Workspace**, browse to `lesson20`,
   **Select All**, **Finish**.

## The starting code

| File | Contains |
|---|---|
| `main.c` | `while (1)` loop turning the **green** LED on and off |
| `bsp.c` | `SysTick_Handler()` firing twice a second, toggling the **blue** LED |

Open both side by side.

---

## Part A — Introduce the shared resource

### 1. Switch from `DATA_Bits` to the shared `DATA` register

Comment out the two lines using the `DATA_Bits` array (Lesson 7) and write them
the long way, through the **shared `DATA` register** that controls all 8 GPIO
lines:

```c
/* set the bit */
GPIOF->DATA = GPIOF->DATA | LED_GREEN;

/* clear the bit */
GPIOF->DATA = GPIOF->DATA & ~LED_GREEN;
```

They are written at length deliberately, to make the **read-modify-write**
sequence visible. The compact forms do **exactly the same thing**:

```c
GPIOF->DATA |= LED_GREEN;
GPIOF->DATA &= ~LED_GREEN;
```

> The OR/AND is needed so the other 7 bits — controlling other things — are not
> disturbed. (Revisit Lesson 6 if the bitwise operators are hazy.)

### 2. Confirm everything "works"

Build, flash, and check:

- The LEDs still blink.
- Single-stepping the main loop switches green on and off as programmed.
- A breakpoint in `SysTick_Handler` shows blue toggling every time.

Each piece of code, inspected individually, is correct. **What's not to like?**

### 3. Watch the board for a while

The blue LED's blinking is **no longer regular** — sometimes it pauses for a full
second or more.

---

## Part B — Catch the race in the act

### 4. Find the read-modify-write in machine code

Breakpoint on the line that turns the green LED **off**, and open the
**disassembly** and **registers** views. Single-step in assembly:

```
LDR  r3, =GPIOF_DATA   ; address of the DATA register
LDR  r2, [r3]          ; READ the DATA register
BIC  r2, r2, #8        ; MODIFY: clear the green bit (mask 8 = LED_GREEN)
STR  r2, [r3]          ; WRITE back
```

> Note that **one short line of C** generated all of these instructions.

### 5. Preempt at the worst possible moment

Stop **before** the `BIC` instruction — an interrupt can happen at any time, so
why not here?

Trigger SysTick manually: open the **NVIC** register set, scroll to
**`NVIC_INT_CTRL`**, write **1** to the **`PENDSTSET`** field, press Enter.

### 6. Set the two-path breakpoints

- one **immediately after the `BIC`** instruction;
- one **inside `SysTick_Handler`**.

Restore the Core Registers view.

### 7. Run free (not single-step)

> Single-stepping disables the interrupt check. **Click Resume.**

**Result: the breakpoint inside `SysTick_Handler` is hit first.** The interrupt
preempted the main code at exactly that point.

### 8. Watch the update get destroyed

Step through the handler:

- it turns the **blue LED on**, as intended;
- it returns to the **`BIC`** instruction.

Now step the remaining main-loop instructions:

- `BIC` clears the green bit **in `r2`** — the stale value read *before* the
  interrupt;
- `STR` writes `r2` back…

**…and both the green *and* the blue LEDs go dark.**

The code was supposed to change only green. The ISR's update to blue was **read
before it happened and overwritten after**.

> Imagine the ISR switching on a **nuclear reactor's cooling system** instead of
> an LED. The interrupt turns cooling on; a fraction of a microsecond later the
> main loop turns it off. The reactor melts down.

---

## Part C — Fix 1: mutual exclusion

### 9. Add critical sections

```c
__disable_irq();
GPIOF->DATA |= LED_GREEN;
__enable_irq();

__disable_irq();
GPIOF->DATA &= ~LED_GREEN;
__enable_irq();
```

### 10. Check the cost

In the disassembly:

| Intrinsic | Instruction | Cycles |
|---|---|---|
| `__disable_irq()` | **`CPSID i`** | 1 |
| `__enable_irq()` | **`CPSIE i`** | 1 |

**One instruction each, no call overhead** — that is what an *intrinsic* function
buys you. The code between them is a **critical section**.

### 11. Repeat the experiment exactly

1. Single-step to the `BIC` instruction.
2. Trigger SysTick via `NVIC_INT_CTRL.PENDSTSET`.
3. Breakpoint after the `BIC`, and another in `SysTick_Handler`.
4. **Resume.**

**This time the `STR` breakpoint is hit first** — SysTick did **not** preempt the
main code.

### 12. Confirm the interrupt wasn't lost

Keep going: the SysTick interrupt **fires as soon as interrupts are re-enabled**.
Step through it — it turns the blue LED on, then returns to the top of the loop.

> Blocked interrupts are **delayed, not discarded**.

The critical sections **serialized** access to `GPIOF->DATA` and made it
**atomic**. The three pieces of code can now run before or after each other, but
never in the middle.

---

## Part D — Fix 2: eliminate the sharing (better)

### 13. Revert to `DATA_Bits`

Comment out today's code (keep it for experimenting) and restore the original:

```c
GPIOF_AHB->DATA_Bits[LED_GREEN] = LED_GREEN;   /* set   */
GPIOF_AHB->DATA_Bits[LED_GREEN] = 0;           /* clear */
```

### 14. Verify in the disassembly

Each line is a **single atomic `STR`** — **no read-modify-write at all**, and no
critical section needed.

Because every combination of the 8 GPIO bits has its **own register**,
`DATA_Bits[LED_GREEN]` and `DATA_Bits[LED_BLUE]` are **different registers**.
There is nothing shared to race over.

### 15. Appreciate the hardware design

> This is **why** TI's engineers built the GPIO registers that peculiar, complex
> way: to separate the bits, avoid sharing, and eliminate software race
> conditions. They did the heavy lifting in hardware so your life is easier.

---

## Exercises

1. **Quantify the race.** With the `DATA` version, count how often blue "skips"
   over a minute. Does it match the width of the vulnerable window?
2. **Shrink the window.** Move `__disable_irq()` to cover only the `STR`. Does the
   race come back? Why?
3. **Nesting trap.** Call a function containing a critical section from inside
   another critical section. What goes wrong at the inner `__enable_irq()`?
4. **Save and restore.** Write critical-section macros that save the previous
   PRIMASK and restore it, so nesting is safe. (Look at how an RTOS does this.)
5. **Latency cost.** Put a long delay inside a critical section and measure the
   jitter it adds to SysTick.
6. **Another shared resource.** Create a race over a plain `uint32_t` counter
   incremented in both `main()` and the ISR. Fix it both ways.
7. **Other hardware.** Find how the STM32's `BSRR` register solves the same
   problem, and compare it with TI's `DATA_Bits`.

---

## Self-check

- [ ] I identified the LDR/BIC/STR sequence generated by one line of C
- [ ] I preempted between the read and the write and watched an update vanish
- [ ] I can define a race condition in one sentence
- [ ] I fixed it with a critical section and verified `CPSID i` / `CPSIE i`
- [ ] I confirmed a blocked interrupt fires later rather than being lost
- [ ] I restored `DATA_Bits` and verified the write is a single atomic `STR`
- [ ] I can explain why not sharing beats mutual exclusion
