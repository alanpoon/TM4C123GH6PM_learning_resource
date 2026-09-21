# Lesson 16 — Practice: Replacing the Delay Loop with a SysTick Interrupt

**Concepts behind this exercise:** [KNOWLEDGE.md](KNOWLEDGE.md)
**Video:** <https://youtu.be/jP1JymlHUtc>

---

## Projects in this lesson

| Directory | Toolchain | Target |
|---|---|---|
| `tm4c123-iar/` | IAR EWARM (`workspace.eww`) | EK-TM4C123GXL LaunchPad |
| `CMSIS/` | — | CMSIS headers |

## Files at the end

```
startup_tm4c.c   -- vector table (lesson 15)
bsp.c            -- BSP: assert_failed() + SysTick_Handler()
bsp.h            -- NEW: LED masks, SYS_CLOCK_HZ  (replaces delay.h)
main.c           -- configuration + empty while(1)
                 -- delay.c / delay.h REMOVED
```

---

## Part A — Restructure the polling version first

> Change one thing at a time. Get the single-delay polling version working
> **before** introducing interrupts.

### 1. Clean up

Strip out the Lesson 13–15 experiments and get blinky back to its basic form:
hardware init, blue LED on, then the `while (1)` loop toggling red with delays.

### 2. Collapse two delays into one

The current loop is LED-on → delay → LED-off → delay. Observe that before each
delay the LED simply needs to **change state**. So use the **toggle idiom**:

```c
GPIOF_AHB->DATA_Bits[LED_RED] ^= LED_RED;   /* XOR flips the bit */
delay(HALF_SECOND);
```

Check the XOR truth table: `0 ^ 1 = 1`, `1 ^ 1 = 0`.

Also comment out the blue LED, so red is the only one in play.

> **Why this matters:** with one delay per iteration, the loop body *minus the
> delay* is **exactly the code the interrupt handler will contain**.

### 3. Test the polling version

Build, flash, and verify:

- A breakpoint on the XOR line: the LED toggles each time you hit it.
- Run free: the LED blinks about once a second.

---

## Part B — Set up SysTick

### 4. Find SysTick in the datasheet

Search the [datasheet](../resources/TM4C123GH6PM_Datasheet.pdf) for **SysTick**.
It is a hardware peripheral clocked by the CPU clock, with three registers:

| Datasheet | CMSIS | Role |
|---|---|---|
| `STCURRENT` | `SysTick->VAL` | 24-bit down-counter, −1 per CPU clock |
| `STRELOAD` | `SysTick->LOAD` | reload value = interval |
| `STCTRL` | `SysTick->CTRL` | clock source / interrupt enable / counter enable |

The registers are already mapped in the CMSIS header (`core_cm4.h`, pulled in by
the device header). **Only `STCTRL` really needs looking up** — for its bit
definitions.

### 5. Determine the clock frequency

The TM4C123 runs at **16 MHz** by default, from the on-board crystal — look at
`Y2` on the board if you want to see it printed.

Define it once, in `bsp.h`:

```c
#define SYS_CLOCK_HZ  16000000U
```

### 6. Compute the reload value

```c
SysTick->LOAD = (SYS_CLOCK_HZ / 2U) - 1U;   /* half a second */
```

Two checks before you move on:

- **Why `-1`?** SysTick counts **through zero**; without it you'd count one clock
  too many.
- **Does it fit?** The counter is only **24 bits**. Compute 8,000,000−1 in hex
  with a calculator: `0x7A11FF` — three bytes. It just fits. Always check this
  for long intervals.

### 7. Write the configuration

```c
SysTick->LOAD = (SYS_CLOCK_HZ / 2U) - 1U;
SysTick->VAL  = 0U;             /* clear-on-write: any value clears it */
SysTick->CTRL = (1U << 2) | (1U << 1) | (1U << 0);
                /* clock source | interrupt enable | counter enable */
```

---

## Part C — Write the handler

### 8. Fill in `SysTick_Handler()`

You already have an empty body in `bsp.c` (from Lesson 15's weak aliases). The
handler needs to do **exactly what the polling loop body did minus the delay**:

```c
void SysTick_Handler(void) {
    GPIOF_AHB->DATA_Bits[LED_RED] ^= LED_RED;
}
```

That's all — **the delay happens outside the CPU**, in the SysTick peripheral,
autonomously.

### 9. Fix the compile error by moving definitions to `bsp.h`

The build fails: `LED_RED` is defined in `main.c` and unknown in `bsp.c`.

The right fix is a shared header. Adapt the now-obsolete `delay.h`:

1. Put board-specific definitions in it — **LED pin masks** and **`SYS_CLOCK_HZ`**.
2. Save it as **`bsp.h`**.
3. `#include "bsp.h"` in both `main.c` and `bsp.c`.
4. **Remove `delay.c` from the project.**

Rebuild: the linker now complains `delay()` is unavailable. Good — delete the
calls. You don't need busy-waiting any more.

### 10. Keep the empty loop

```c
while (1) {
}
```

Do **not** delete it, even though it does nothing.

> **The CPU must spend its time somewhere** between interrupts. Later you will
> put useful work here, or put the CPU to sleep (Lesson 52).

### 11. Enable interrupts

Peripherals don't quite connect straight to the CPU — every CPU can **block
interrupts in software**. On Cortex-M that's the **`PRIMASK`** bit, which must be
cleared:

```c
__enable_interrupt();   /* IAR intrinsic: clears PRIMASK */
```

> Forgetting this is one of the most common reasons a perfectly configured
> interrupt never fires.

---

## Part D — Verify

### 12. Run it

Build cleanly and flash. Expected:

- The red LED blinks about once a second — **with no delay loop**.
- **Break in:** the program is spinning in the empty `while (1)`.
- **Breakpoint in `SysTick_Handler`:** the LED toggles every time it is hit.

That last observation is the whole lesson: the CPU is free, and the timer does
the waiting.

---

## Exercises

1. **Change the rate.** Make the LED blink at 5 Hz. What is the new `LOAD` value?
2. **Find the limit.** What is the **longest** interval SysTick can produce at
   16 MHz before overflowing 24 bits? Verify by trying to exceed it.
3. **Forget `-1`.** Remove it and measure whether you can detect the difference.
   Explain why or why not.
4. **Disable interrupts.** Comment out `__enable_interrupt()` and observe. Then
   check `PRIMASK` in the register view.
5. **Prove the CPU is free.** Put a fast counter in the `while (1)` loop and
   compare how many iterations it reaches versus the busy-wait version.
6. **Two colours.** Toggle green in the ISR and red in the main loop. What does
   the timing tell you about who runs when?
7. **Measure entry cost.** Using a GPIO pin and a logic analyser (or cycle
   counting), estimate the interrupt entry overhead. Compare against the ≥12
   cycles quoted for Cortex-M.

---

## Self-check

- [ ] My polling version used a single XOR toggle plus one delay, and worked
- [ ] I can name all three SysTick registers and what each does
- [ ] I computed `LOAD` correctly and verified it fits in 24 bits
- [ ] Board constants live in `bsp.h`; `delay.c`/`delay.h` are gone
- [ ] `SysTick_Handler()` contains only the toggle
- [ ] I called `__enable_interrupt()` and know what `PRIMASK` does
- [ ] Breaking in finds the CPU idle in `while (1)`, not busy-waiting
