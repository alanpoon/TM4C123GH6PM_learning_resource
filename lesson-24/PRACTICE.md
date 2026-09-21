# Lesson 24 — Practice: Round-Robin Scheduling and Measuring the Kernel

**Concepts behind this exercise:** [KNOWLEDGE.md](KNOWLEDGE.md)
**Video:** <https://youtu.be/PX_LDyiXs5Y>

---

## Projects in this lesson

| Directory | Toolchain | Target |
|---|---|---|
| `tm4c123-keil/` | KEIL MDK (`lesson.uvprojx`) | EK-TM4C123GXL LaunchPad |
| `tm4c123-gnu/` | GNU-ARM (`Makefile`) | EK-TM4C123GXL LaunchPad |
| `stm32c031-keil/` | KEIL MDK | STM32 NUCLEO-C031C6 |
| `CMSIS/`, `ek-tm4c123gxl/`, `nucleo-c031c6/` | — | support code |

Start from a copy of the Lesson 23 project.

> You also need the **`qpc`** directory (from the course downloads) — `qassert.h`
> lives in `qpc/include`, which must be on your **include search path**.

---

## Part A — Hard-code, verify, then refactor

### 1. Hard-code the scheduling decision

In `OS_sched()`:

```c
OS_next = (OS_curr == &blinky1) ? &blinky2 : &blinky1;
```

The build fails — `blinky1`/`blinky2` are unknown in `miros.c`. Add `extern`
declarations. (Normally these belong in a header; here you're just testing the
idea.)

### 2. Run it

**Both the green and the blue LED blink simultaneously and independently.**

Now you know what the end result should look like, and that automatic scheduling
is workable.

> Next you improve the **internal design** without changing the **behaviour** —
> that is **refactoring**.

---

## Part B — Register threads in an array

### 3. Add the bookkeeping

```c
OSThread *OS_thread[32 + 1];   /* MiROS supports up to 32 threads */
uint8_t   OS_threadNum;        /* how many started so far          */
uint8_t   OS_currIdx;          /* current index, wraps around      */
```

> Some RTOSes use a **linked list** the scheduler traverses. This brute-force
> array suits MiROS's future direction. (Why exactly 32 becomes clear later.)

### 4. Register each thread

In `OSThread_start()`:

```c
OS_thread[OS_threadNum] = me;
++OS_threadNum;
```

### 5. Enforce the assumption with an assertion

You are implicitly assuming the array doesn't overflow. Enforce it.

> Returning an error code is the usual approach — but **the caller can simply
> ignore the problem.**

Don't use standard `assert()`: it prints to a screen and exits the application —
**neither is possible in deeply embedded code**.

Set up the embedded-friendly version:

1. Add **`qpc/include`** to the include search path.
2. `#include "qassert.h"` in `miros.c`.
3. Put **`Q_DEFINE_THIS_FILE`** at the top (defines the module-name string).
4. Assert:

```c
Q_ASSERT(OS_threadNum < Q_DIM(OS_thread));
```

> **`Q_DIM(array)`** gives the array's dimension — no need to invent a
> `MAX_THREAD` symbol.

### 6. Review `Q_onAssert()`

It's already in `bsp.c` (the startup code uses assertions). Read it and treat it
seriously:

> This is your **last line of defence after the code has already failed**. Do
> damage control, **log or output the assertion's location** (the `module` and
> `loc` parameters), and then typically **reset the system** to avoid a
> denial-of-service failure.

Customize it for your project. (Full treatment in Lessons 47–48.)

---

## Part C — Round-robin

### 7. Implement the policy

```c
void OS_sched(void) {
    ++OS_currIdx;
    if (OS_currIdx == OS_threadNum) {
        OS_currIdx = 0U;
    }
    OS_next = OS_thread[OS_currIdx];

    if (OS_next != OS_curr) {
        *(uint32_t volatile *)0xE000ED04 = (1U << 28);   /* pend PendSV */
    }
}
```

Build and run — same behaviour as the hard-coded version, but the threads are no
longer named in the kernel.

### 8. Prove composability

Add a **third** thread: `blinky3`, blinking the **red** LED with different on/off
timings (producing interesting colour patterns when combined with the others).

Note what you had to touch: **only `main.c`**. No existing thread changed, and no
RTOS code changed — MiROS registers the new thread automatically.

> That property is **composability**. Threads became composable **only after
> adding the RTOS** — without it you could not combine them to run seemingly
> simultaneously and independently.

---

## Part D — Fix the initialization timeline

### 9. Spot the problem

Interrupts are currently configured and enabled inside `BSP_init()`. **Too
early.**

> If an interrupt fires before the end of `main()`, it may trigger a context
> switch that **takes control away from `main()` and never returns** — so some
> initialization never runs and some threads are never started.

**Correct order: configure and enable interrupts only after all threads have been
started**, i.e. at the end of `main()`.

### 10. Replace `while (1)` with `OS_run()`

```c
void OS_run(void) {
    OS_onStartup();          /* application callback: configure + enable IRQs */

    __disable_irq();
    OS_sched();              /* run the first thread */
    __enable_irq();

    Q_ERROR();               /* must never get here */
}
```

Points to understand:

- **`OS_onStartup()` is a callback** — the RTOS declares it, the **application
  defines it**.
- The scheduler is called here **outside** an interrupt. That's fine: a context
  switch can only occur on an exception return, but `OS_sched()` doesn't switch
  directly — **it pends PendSV**, which then returns to the first thread
  correctly.
- PendSV fires the moment interrupts are re-enabled, so **control never returns**
  here. `Q_ERROR()` is a more descriptive way of writing `Q_ASSERT(0)`.

### 11. Define the callback

Add the new prototypes (`OS_run`, `OS_onStartup`) to `miros.h`.

The build fails: `OS_onStartup()` is missing — a good reminder that this
application-specific function belongs in **`bsp.c`**. Cut the interrupt
configuration out of `BSP_init()` and paste it in as the body.

> The final "enable interrupts" line is now **redundant** — `OS_run()` disables
> and re-enables them anyway. Delete it.

Build: no errors, no warnings.

### 12. Step through it

- Breakpoint in `OS_run()`: watch it disable interrupts and call the scheduler.
- In `OS_sched()`: watch `OS_currIdx` increment, the wrap-around check, and
  `OS_next` get set (to `blinky2` on the first pass).
- Breakpoint in `PendSV_Handler`: watch it return to that thread.
- Remove the breakpoints: **all three LEDs blink as the three threads run
  simultaneously.**

---

## Part E — Measure the kernel

> Needs a logic analyser or mixed-signal oscilloscope. Without one, read the
> numbers below and reproduce the reasoning.

### 13. Wire it up

Connect to the TivaC LaunchPad: the **red**, **blue** and **green** LED pins
(PF1–PF3), a couple of **ground** pins, and **PF4** as a spare **TEST_PIN**.

The first view (D1–D4 ≈ PF1–PF4) shows the LEDs blinking — but far too slowly to
measure a context switch.

### 14. Make the activity fast

Comment out the **`BSP_delay()`** calls in the thread handlers, so each thread
toggles its pin continuously with no delay.

Add a trigger: drive **TEST_PIN up and down in `SysTick_Handler`**, and configure
PF4 as an output in `BSP_init()`.

### 15. Read the trace

On the board all LEDs now **glow at varying intensity** — switching far too fast
for the eye.

In the analyser:

- pins toggle rapidly;
- **the activity is mutually exclusive** — only one pin toggles at a time while
  the others hold steady;
- switching happens **only when D4 (TEST_PIN) fires**.

Trigger on **the rising edge of D4** to centre each context switch on screen, and
zoom in.

### 16. Take the measurements

> Rule of thumb: **1 MHz of clock ≈ 1 tick per microsecond.** The TivaC runs at
> **50 MHz** → **50 cycles/µs**.

| Measurement | Time | Cycles |
|---|---|---|
| Last thread activity → SysTick entry | ~400 ns | 20 |
| Inside `SysTick_Handler` | ~1.6 µs | 80 |
| SysTick exit → next thread's first activity | ~1.5 µs | **75** |
| **Suspend one thread → resume another** | **~3.5 µs** | **175** |

### 17. Compute the overhead

```
3.5 µs × 100 ticks/s ÷ 1,000,000 µs/s = 0.00035   →  under 0.04 %
```

Even at a **1 kHz** tick rate: about **0.3 %**.

> The RTOS overhead is **quite small** — the real waste is still `BSP_delay()`
> burning CPU cycles, which Lesson 25 fixes with **blocking**.

---

## Exercises

1. **Overflow the array.** Start 34 threads and confirm `Q_ASSERT` fires. Where
   do you land?
2. **Customize `Q_onAssert()`.** Make it record the module and line somewhere
   that survives a reset, then reset the MCU.
3. **Fourth thread.** Add one and confirm you only touched `main.c`.
4. **Tick rate.** Raise `BSP_TICKS_PER_SEC` to 1000 and re-derive the overhead.
   Measure it if you can.
5. **Early interrupts.** Move interrupt enabling back into `BSP_init()` and add a
   deliberate delay before the last `OSThread_start()`. What breaks?
6. **Unequal shares.** Round-robin gives every thread the same slice. Sketch what
   you'd change to give one thread twice the CPU — and why that's still not
   "real-time" (Lesson 26).
7. **Shrink PendSV.** Optimize the repetitions in your assembly handler and
   re-measure the 75-cycle figure.

---

## Self-check

- [ ] Threads register themselves in `OS_thread[]` via `OSThread_start()`
- [ ] Array overflow is caught by `Q_ASSERT`, not by an ignorable error code
- [ ] `OS_sched()` implements round-robin with a correct wrap-around
- [ ] Adding a third thread required changes only in `main.c`
- [ ] Interrupts are enabled in `OS_onStartup()`, called from `OS_run()`
- [ ] `OS_run()` ends with `Q_ERROR()` and never returns
- [ ] Three LEDs blink simultaneously and independently
- [ ] I can state the context-switch cost in cycles and the resulting overhead
