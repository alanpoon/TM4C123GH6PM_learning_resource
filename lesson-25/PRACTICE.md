# Lesson 25 — Practice: Adding Blocking and the Idle Thread

**Concepts behind this exercise:** [KNOWLEDGE.md](KNOWLEDGE.md)
**Video:** <https://youtu.be/JurV5BgjQ50>

---

## Projects in this lesson

| Directory | Toolchain | Target |
|---|---|---|
| `tm4c123-keil/` | KEIL MDK (`lesson.uvprojx`) | EK-TM4C123GXL LaunchPad |
| `tm4c123-gnu/` | GNU-ARM (`Makefile`) | EK-TM4C123GXL LaunchPad |
| `stm32c031-keil/` | KEIL MDK | STM32 NUCLEO-C031C6 |

Start from a copy of the Lesson 24 project.

---

## Part A — Create the idle thread

### 1. Understand why it's needed

Trace the argument yourself before writing code:

- Two threads; while at least one is Ready, the scheduler has something to run.
- One blocks → the other still runs.
- **Both block → the CPU must run exactly one thread, but none is Ready.**

So a blocking system **requires** a special thread that is always ready and can
never block: the **idle thread**.

### 2. Write it

Copy the boilerplate of a blinky thread into `miros.c` and rename it
`idleThread`. Keep the superloop structure, but call a callback:

```c
void main_idleThread(void) {
    while (1) {
        OS_onIdle();        /* application-level processing */
    }
}
```

### 3. Start it from `OS_init()`

That guarantees it is the **very first thread**, at **index 0** of
`OS_thread[]` — which the bitmask scheme below depends on.

### 4. Let the application supply its stack

The RTOS could pre-allocate the idle stack, but **it cannot know how much stack
`OS_onIdle()` will need**. So defer it to the application, exactly as
`OSThread_start()` does.

Update the `OS_init()` signature in `miros.h` and add the `OS_onIdle()`
prototype.

The build now fails (the API changed). Fix it by:

- passing an idle stack to `OS_init()` in `main.c`;
- defining `OS_onIdle()` in `bsp.c` — empty for now, just to link.

---

## Part B — Blocking machinery

### 5. Add a timeout counter to the TCB

```c
typedef struct {
    void *sp;
    uint32_t timeout;      /* NEW */
} OSThread;
```

- `OS_delay()` loads it with the tick count.
- Every `OS_tick()` decrements all **non-zero** counters.
- Reaching zero **unblocks** the thread.

Separate counters let delays run **in parallel and independently**.

### 6. Add the `OS_readySet` bitmask

A boolean flag per TCB would be logical — but **grouping the flags into one
32-bit bitmask is more efficient**:

```
OS_readySet bit (n-1)   ⟷   OS_thread[n]
```

The **idle thread at index 0 is skipped** (always ready), so bits are **shifted
by one**: index 1 ↔ bit 0, … index 32 ↔ bit 31.

| Value | Meaning |
|---|---|
| bits 0, 1, n−1 set | threads 1, 2, n ready |
| only bit 1 | thread 2 ready |
| **zero** | **the idle condition** — checkable in one instruction |

### 7. Teach the scheduler about readiness

```c
if (OS_readySet == 0U) {         /* idle condition — the most frequent path */
    OS_currIdx = 0U;
}
else {
    do {
        ++OS_currIdx;
        if (OS_currIdx == OS_threadNum) {
            OS_currIdx = 1U;     /* wrap to 1 — SKIP the idle thread */
        }
    } while ((OS_readySet & (1U << (OS_currIdx - 1U))) == 0U);
}
OS_next = OS_thread[OS_currIdx];
```

To test thread N: synthesize a mask with only bit **N−1** set, AND it with
`OS_readySet`; zero means not ready, so keep going.

### 8. Implement `OS_delay()`

Same signature as the old `BSP_delay()`:

```c
void OS_delay(uint32_t ticks) {
    __disable_irq();
    Q_REQUIRE(OS_curr != OS_thread[0]);            /* never the idle thread */
    OS_curr->timeout = ticks;
    OS_readySet &= ~(1U << (OS_currIdx - 1U));     /* make NOT ready        */
    OS_sched();                                     /* switch away NOW      */
    __enable_irq();
}
```

> `OS_sched()` is designed to be called with interrupts disabled; the switch
> happens right after they are re-enabled (Lesson 23).

**`Q_REQUIRE()`** is a **precondition** — a requirement the **caller** must
satisfy. It behaves exactly like `Q_ASSERT()`, but the name states the intent.

### 9. Implement `OS_tick()`

```c
void OS_tick(void) {
    for (uint8_t n = 1U; n < OS_threadNum; ++n) {   /* skip the idle thread */
        if (OS_thread[n]->timeout != 0U) {
            --OS_thread[n]->timeout;
            if (OS_thread[n]->timeout == 0U) {
                OS_readySet |= (1U << (n - 1U));    /* unblock */
            }
        }
    }
}
```

Two things **not** to add, and why:

- **No critical section.** `OS_tick()` runs in a **single ISR**
  (`SysTick_Handler`), and an ISR cannot be preempted by a thread — so the
  counters (changed only in `OS_delay()`) can't change underneath it.
- **No `OS_sched()` call.** `SysTick_Handler` already calls the scheduler at its
  end, which will pick up any newly unblocked thread.

### 10. Final adjustments

- In `OSThread_start()`, mark every started thread **except the idle thread**
  ready by setting its bit in `OS_readySet` — the transition into the **Ready**
  super-state.
- Bump the MiROS version number in both `miros.c` and `miros.h`.
- Add `OS_delay()` and `OS_tick()` prototypes to `miros.h`.
- In the application, **use `OS_delay()` and `OS_tick()`** instead of
  `BSP_delay()` and `BSP_tickCtr()`.

---

## Part C — Verify the behaviour changed even though the output didn't

### 11. Run it

Build and run free: **all LEDs blink exactly as before.**

But it works completely differently:

- **Break in at random** → you land in **`OS_onIdle()`**.
- **Breakpoint in the scheduler** → **`OS_readySet` is zero** most of the time,
  so the scheduler picks the idle thread.

> Most of the time **all threads are blocked** and the system sits in the idle
> condition. Only occasionally does a thread unblock and get scheduled.

---

## Part D — Measure it

> Needs a logic analyser. Otherwise read the figures and follow the reasoning.

### 12. Instrument the idle thread

Put pin-toggling code in `OS_onIdle()` so the idle activity is visible.

Pins are running out, so **reuse the RED LED** — and to avoid a conflict,
**comment out starting the `blinky3` thread** in `main.c`, leaving it **Dormant**.

On the board: green and blue blink as before; **red glows at low intensity**.

### 13. Read the trace

Connect D1–D4 to PF1–PF4. `SysTick` drives D4 as the trigger.

- **D1 (red)** toggles rapidly → `OS_onIdle()` is called constantly.
- Green and blue change very slowly by comparison.
- **SysTick usually takes ~3 µs, but occasionally much longer.**

### 14. Explain the long SysTicks

Search the accumulated traces for a long one. You will find that it coincides
with the **green LED changing** and the idle activity being **delayed by over
5 µs**.

The explanation:

- **Most ticks** merely update timeout counters and **do not switch away from the
  idle thread** — visible from the idle activity continuing before and after.
- **Occasionally** a thread unblocks and is scheduled. After SysTick it runs,
  does its work (toggling an LED), then calls `OS_delay()` and **voluntarily
  relinquishes the CPU**, switching back to idle.

### 15. The numbers

| Measurement | Time |
|---|---|
| Longest `SysTick_Handler` | ~**4.5 µs** — 3× the pre-blocking figure, due to the more complex `OS_tick()`/`OS_sched()` |
| Context switch | still **< 2 µs** |
| Blocking in `OS_delay()` | ~3.8 µs |
| **Longest time outside the idle thread** | only **10 µs** |

> Conclusion: the CPU spends **almost all its time in the idle thread**, with a
> few microseconds here and there for real work.

---

## Part E — Sleep instead of spinning

### 16. Put the CPU to sleep

The idle thread still wastes every cycle it gets.

> You will never achieve a truly low-power design with the CPU and peripherals
> running at full speed. For battery-powered applications you **must** use sleep
> modes — and **`OS_onIdle()` is the ideal, central place** for it.

Cortex-M provides **`WFI` (Wait For Interrupt)**, which stops the CPU clock until
an interrupt arrives:

```c
void OS_onIdle(void) {
    __WFI();     /* CMSIS intrinsic */
}
```

### 17. Observe the difference

On the board: green and blue blink as before, but **the red LED appears
inactive**.

In the logic analyser it *does* toggle — **exactly once after each interrupt**.
The CPU now **stops inside every `OS_onIdle()` call** and is woken only by the
next SysTick.

---

## Exercises

1. **Remove the idle thread.** Block all threads and see exactly how the system
   fails.
2. **Block from idle.** Call `OS_delay()` in `OS_onIdle()` and confirm
   `Q_REQUIRE` catches it.
3. **Bit arithmetic.** Write out the `OS_readySet` value for three threads at
   indices 1, 2, 3 with only thread 2 ready. Check it in the debugger.
4. **Break the skip.** Wrap `OS_currIdx` to 0 instead of 1 and describe what goes
   wrong.
5. **Add the critical section anyway.** Put one in `OS_tick()` and measure the
   cost. Was it ever needed?
6. **Measure sleep.** If your setup allows current measurement, compare supply
   current with and without `__WFI()`.
7. **Idle work.** Use `OS_onIdle()` for something useful (e.g. a background
   checksum) and reason about why that conflicts with low-power design.

---

## Self-check

- [ ] An idle thread exists, is started from `OS_init()`, and sits at index 0
- [ ] Its stack is supplied by the application
- [ ] Each TCB has a timeout counter, decremented by `OS_tick()`
- [ ] `OS_readySet` maps thread index n to bit n−1, skipping the idle thread
- [ ] The scheduler skips not-ready threads and detects the idle condition in
      one comparison
- [ ] `OS_delay()` uses `Q_REQUIRE` to forbid blocking the idle thread
- [ ] I can explain why `OS_tick()` needs no critical section
- [ ] Breaking in at random lands in `OS_onIdle()`
- [ ] `__WFI()` in `OS_onIdle()` visibly changes the idle trace
