# Lesson 21 — Practice: Building a Proper BSP, Blocking vs. Non-Blocking

**Concepts behind this exercise:** [KNOWLEDGE.md](KNOWLEDGE.md)
**Video:** <https://youtu.be/AoLLKbvEY8Q>

---

## Projects in this lesson

| Directory | Toolchain | Target |
|---|---|---|
| `tm4c123-keil/` | **KEIL MDK** (`lesson.uvprojx`) | EK-TM4C123GXL LaunchPad |
| `stm32c031-keil/` | KEIL MDK | STM32 NUCLEO-C031C6 |
| `CMSIS/`, `ek-tm4c123gxl/`, `nucleo-c031c6/` | — | support code |

> **Toolchain change.** From this lesson the course uses **ARM-KEIL MDK** rather
> than Eclipse/CCS. The free **MDK-Lite** edition is code-size limited but
> perfectly adequate for every project in this course. Get it from arm.com
> (*Development Tools → Microcontroller and Embedded Development*) or search for
> "Keil MDK".

### First-run setup

1. Open the provided `lesson.uvprojx`.
2. You may need to **register the licence** — *Get LIC via Internet* for
   MDK-Lite Evaluation.
3. You may need a **Software Pack**: open the **Pack Installer** and select
   **Texas Instruments → TivaC Series → TM4C123x Series**.

> Later lessons in this group also need the **`qpc` framework** directory, which
> supplies CMSIS and the startup code. Unzip it alongside your lessons.

---

## Part A — Understand the improved delay

### 1. Build and run the starting point

The program blinks the green LED: on for about ¼ second, off for about ¾ second.
It is the Lesson 8 blinky with one change — `delay()` is now **`BSP_delay()`**,
defined in the **BSP**.

### 2. Read `BSP_delay()`

Unlike the crude counting loop, this is based on the **SysTick interrupt**, so
timing no longer depends on how fast the compiler's code happens to run.

```c
#define BSP_TICKS_PER_SEC 100U            /* in bsp.h */

static uint32_t volatile l_tickCtr;

void SysTick_Handler(void) {
    ++l_tickCtr;
}

uint32_t BSP_tickCtr(void) {
    uint32_t tickCtr;
    __disable_irq();                      /* critical section (lesson 20!) */
    tickCtr = l_tickCtr;
    __enable_irq();
    return tickCtr;
}

void BSP_delay(uint32_t ticks) {
    uint32_t start = BSP_tickCtr();
    while ((BSP_tickCtr() - start) < ticks) {
    }
}
```

Check each design decision:

- **`volatile`** — the ISR changes it with no visible instruction doing so.
- **The critical section** — without it, `BSP_tickCtr()` races with the ISR.
- **Rollover** — two's complement arithmetic makes the subtraction correct even
  when the counter wraps from `0xFFFFFFFF` to 0. Convince yourself on paper.
- **Still polling** — every CPU cycle until the delay expires is wasted.

---

## Part B — Take the BSP to the next level

### 3. Move initialization into the BSP

Hardware initialization is board-specific. Create `BSP_init()` in `bsp.c`, move
all the init code there, and call it from `main()`.

### 4. Move LED control into the BSP

Switching LEDs is board-specific too — a different board has different pins. Add:

```c
void BSP_ledGreenOn(void);
void BSP_ledGreenOff(void);
void BSP_ledBlueOn(void);
void BSP_ledBlueOff(void);
void BSP_ledRedOn(void);
void BSP_ledRedOff(void);
```

Call them from the background loop, and add every prototype to `bsp.h`.

### 5. Strip `main.c` of hardware details

Remove the MCU header include, the LED pin masks, everything board-specific —
move it all into `bsp.c`. The background loop no longer depends on any of it.

Build: clean.

### 6. Appreciate what you gained

Your `main.c` is now completely **insulated from the board**:

> The background code specifies **WHAT** needs to be done; the BSP specifies
> **HOW** to do it.

1. **`main.c` is smaller and self-explanatory** — it barely needs comments.
2. **It is portable**: run it on a different board or toolchain by swapping
   `bsp.c` only, without changing a line of application code. It could even run
   on a desktop PC.

---

## Part C — See where the time goes

### 7. Break in at random

Run the program, then hit **Stop** at an arbitrary moment.

**You land in `BSP_tickCtr()`.** Open the **Call Stack** view:

```
BSP_tickCtr()  <- called from
BSP_delay()    <- called from
main()
```

Try again a few times. **You have almost no chance of catching this program doing
anything else than delaying.**

### 8. Draw the flowchart

Sketch the background loop's flow of control. The polling loops appear as
**arrows going backwards** — draw those with heavy lines, because that is where
all the time goes.

This code is both:

- **blocking** — it waits for a timeout **in-line** and makes no progress until
  it arrives. When the event does arrive, the code proceeds naturally, because
  the code **downstream of the blocking call is exactly the right context** for
  that event.
- **sequential** — the event sequence is **hard-coded in the instruction
  sequence**: a ¼-second timeout after green-on, a ¾-second timeout after
  green-off.

---

## Part D — The non-blocking alternative

### 9. Deactivate the sequential version

Wrap it in `#if 0 ... #endif` so you can flip between the two.

### 10. Add a non-blocking background loop

Rewrite the same behaviour with **no polling loop** — each pass through the loop
runs to completion, checking the tick counter and acting when it is time. (The
project structures this as a **polling state machine**.)

### 11. Compare the behaviour

Build and debug: **it blinks exactly as before**.

But break in at random now — **you always stop inside the main loop**, never
inside some other function.

### 12. Compare the flowcharts

In the non-blocking version **no arrow goes backwards**. All arrows point
forward, and the heavy lines are in the main loop itself.

| | Sequential/blocking | Event-driven/non-blocking |
|---|---|---|
| Loop iterations per second | **~1** | **hundreds of thousands** |
| Where time is spent | inside blocking calls | in the main loop |
| Can it react to a new event promptly? | no — it's stuck waiting | **yes**, in arrival order |
| Event sequence | hard-coded in instructions | not hard-coded |
| Complexity | lower | **higher** |

> The price of that flexibility and timeliness is **apparent higher complexity**.

### 13. A warning about real code

The non-blocking version here is a **state machine**. That is unfortunately
**not the norm** — most real projects use deeply nested IF-THEN-ELSE over many
global variables: *spaghetti code*, a *big ball of mud*. State machines get
proper treatment in Lessons 35–42.

---

## Part E — Recognize the architecture elsewhere

### 14. Find foreground/background in Arduino

Open the Arduino blinky and its library's `main()`:

```c
int main(void) {
    init();
    setup();
    for (;;) {        /* `for (;;)` is the C idiom for "forever" */
        loop();
    }
}
```

It is the same architecture, hidden in the library. Find Arduino's foreground
too: the system clock-tick **ISR** incrementing counters, and the matching
polling **`delay()`** — probably the most-used function in Arduino programs.

---

## Exercises

1. **Prove the race.** Remove the critical section from `BSP_tickCtr()` and try
   to catch a wrong reading. How long did it take?
2. **Rollover test.** Preset `l_tickCtr` near `0xFFFFFFFF` in the debugger and
   verify `BSP_delay()` still works across the wrap.
3. **Port the BSP.** Implement `bsp.c` for `stm32c031-keil/` without changing
   `main.c`. How much had to change?
4. **Add a third blinker.** Extend the non-blocking version with a second LED at
   a different rate. Then try the same in the blocking version — what happens?
   (This is Lesson 22's opening problem.)
5. **Measure loop rate.** Count iterations per second in both versions and
   compare with the figures above.
6. **Tick rate.** Change `BSP_TICKS_PER_SEC` to 1000 and find everything affected.
7. **ISR length.** Add work to `SysTick_Handler` until the background loop
   visibly suffers. What does that tell you about pushing timing into ISRs?

---

## Self-check

- [ ] `main.c` contains no board-specific details — all in `bsp.c`/`bsp.h`
- [ ] I can explain why `l_tickCtr` needs both `static` and `volatile`
- [ ] I understand why the tick-counter read needs a critical section
- [ ] Breaking in randomly lands in `BSP_tickCtr()` for the blocking version
- [ ] Breaking in randomly lands in the main loop for the non-blocking version
- [ ] I can define *blocking* and *sequential* in my own words
- [ ] I found the foreground/background structure inside Arduino's library
