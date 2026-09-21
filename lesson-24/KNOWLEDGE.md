# Lesson 24 — Knowledge: RTOS Part 3 — Automating the Scheduling

**Video:** <https://youtu.be/PX_LDyiXs5Y> · **Transcript:** <https://www.state-machine.com/course/lesson-24.txt>
**Hands-on companion:** [PRACTICE.md](PRACTICE.md)

---

## The big idea

Automate the *decision* of which thread runs next — the **round-robin** policy —
turning MiROS into a working **time-sharing** system. Along the way: assertions,
a proper initialization timeline, **composability**, and a measurement of the
kernel's real cost.

---

## 1. Hard-code it first, then refactor

Start by hard-coding the choice:

```c
OS_next = (OS_curr == &blinky1) ? &blinky2 : &blinky1;
```

It works — both LEDs blink simultaneously and independently — which proves
automatic scheduling is viable and shows what the end result should look like.

> Then improve the **internal design** without changing the **behaviour**. That
> is **refactoring**.

## 2. The thread array

Some RTOSes organize threads in a **linked list** traversed by the scheduler.
MiROS uses a simpler brute-force approach that suits its future direction:

```c
OSThread *OS_thread[32 + 1];   /* pre-allocated array of thread pointers */
uint8_t OS_threadNum;          /* how many have been started             */
uint8_t OS_currIdx;            /* index of the current thread            */
```

> **MiROS can handle up to 32 threads.** Why exactly 32 becomes clear in later
> lessons (it is the width of a priority bitmask).

`OSThread_start()` **registers** each new thread:

```c
OS_thread[OS_threadNum] = me;
++OS_threadNum;
```

## 3. Assertions

Registering threads assumes the array doesn't overflow. Such assumptions should
be **enforced**.

Returning an error code to the caller is the usual approach, but **the caller can
simply ignore the problem**.

### Why not standard `assert()`

The C standard `assert()` prints a message to the screen and **exits the
application**. Neither makes sense in deeply embedded programming — **there is no
screen, and you cannot exit**.

### The embedded-friendly alternative

```c
#include "qassert.h"        /* from qpc/include -- add it to the include path */

Q_DEFINE_THIS_FILE          /* defines the file-name string for assertions    */

Q_ASSERT(OS_threadNum < Q_DIM(OS_thread));
```

- **`Q_ASSERT(expr)`** evaluates the expression and, if false, calls the callback
  **`Q_onAssert()`**.
- **`Q_DEFINE_THIS_FILE`** supplies the module name string.
- **`Q_DIM(array)`** yields the array's dimension — no need to invent a
  `MAX_THREAD` symbol.
- **`Q_ERROR()`** is a more descriptive way to write "this must never be reached"
  than `Q_ASSERT(0)`.

### `Q_onAssert()` — your last line of defence

Already defined in `bsp.c` (the startup code uses assertions). It **can and
should be carefully customized** to your project:

> This is your last line of defence **after the code has already failed**. Do
> damage control, **log or output the location** (the `module` and `loc`
> parameters), and then typically **reset the system** — to avoid the
> denial-of-service failure mode.

(Assertions and **Design by Contract** get full lessons: 47–48.)

## 4. Round-robin scheduling

```c
void OS_sched(void) {
    ++OS_currIdx;
    if (OS_currIdx == OS_threadNum) {
        OS_currIdx = 0U;             /* wrap around */
    }
    OS_next = OS_thread[OS_currIdx];

    if (OS_next != OS_curr) {
        /* pend PendSV */
    }
}
```

Threads run in **circular order**, one clock tick each — a **time-sharing**
system.

## 5. Composability

Adding a third thread (red LED, different timings) touches **only `main.c`** — no
change to existing threads and none to the RTOS. The kernel registers each new
thread automatically.

> This property is called **composability**. Note that **threads became
> composable only after adding the RTOS** — without it you could not combine them
> to run seemingly simultaneously and independently.

## 6. The initialization timeline

Currently interrupts are configured and enabled in `BSP_init()`. **That is too
early.**

> If an interrupt fires before the end of `main()`, it may **trigger a context
> switch that takes control away from `main()` and never returns** — so some
> initialization never runs and some threads are never started.

**The correct order:**

```
BSP_init();                 /* hardware, but NOT interrupts */
OS_init();
OSThread_start(...);        /* start ALL threads             */
OS_run();                   /* only now: enable interrupts   */
```

### `OS_run()`

`OS_run()` replaces the ugly `while (1)` at the end of `main()`. It transfers
control to the RTOS:

```c
void OS_run(void) {
    OS_onStartup();          /* CALLBACK: configure and enable interrupts */

    __disable_irq();
    OS_sched();              /* run the first thread */
    __enable_irq();

    Q_ERROR();               /* should never get here */
}
```

- **`OS_onStartup()` is a callback** — declared by the RTOS but **defined by the
  application** (in `bsp.c`). Move the interrupt configuration there, cut from
  `BSP_init()`. The final "enable interrupts" line becomes redundant, since
  `OS_run()` disables and re-enables them anyway.
- The scheduler is called here **outside** any interrupt context. That is fine:
  a context switch can only happen on an exception return, but **`OS_sched()`
  doesn't switch directly — it pends PendSV**, which then returns to the first
  thread correctly.
- PendSV runs the instant interrupts are re-enabled, so **control never returns**
  to `OS_run()` — hence the always-failing assertion `Q_ERROR()`.

## 7. How fast is it?

Measured with a mixed-signal oscilloscope / logic analyser on the TM4C123
LaunchPad at **50 MHz**, with the `BSP_delay()` calls commented out (so each
thread toggles its pin continuously) and `SysTick_Handler` driving a spare
**TEST_PIN (PF4)** as the trigger.

> Rule of thumb: **every MHz of clock frequency ≈ one clock tick per
> microsecond.** At 50 MHz that is **50 cycles/µs**.

| Measurement | Time | Cycles |
|---|---|---|
| Last thread activity → SysTick entry | ~400 ns | **20** |
| Time inside `SysTick_Handler` | ~1.6 µs | **80** |
| SysTick exit → next thread's first activity (the context switch) | ~1.5 µs | **75** |
| **Total: suspending one thread → resuming another** | **~3.5 µs** | **175** |

### RTOS overhead

```
3.5 µs × 100 ticks/s ÷ 1,000,000 µs/s = 0.00035  →  less than 0.04 %
```

Even at a **1 kHz** tick rate the overhead is only about **0.3 %**.

> The overhead of the RTOS is **quite small**.

### What the trace shows

The pins toggle rapidly, and the activity is **mutually exclusive** — only one
pin toggles at a time while the others hold steady. Switching happens **only**
when the TEST_PIN fires, confirming the tick drives scheduling.

---

## Key takeaways

1. Hard-code first, verify behaviour, then **refactor** the design.
2. Threads are registered in a **pre-allocated array**; MiROS supports up to 32.
3. Use **embedded-friendly assertions** (`Q_ASSERT`), not standard `assert()`.
4. **Round-robin** = advance the index, wrap around, pick that thread.
5. Threads become **composable** once an RTOS exists.
6. **Enable interrupts only after all threads are started** — at the end of
   `main()`, via `OS_run()`.
7. `OS_run()` never returns; assert that fact.
8. A context switch costs ~**175 cycles**; RTOS overhead is well under 1 %.

---

## Glossary

| Term | Meaning |
|---|---|
| Round-robin | Scheduling threads in circular order |
| Time-sharing | Each thread gets a slice of CPU time in turn |
| Refactoring | Improving internal design without changing behaviour |
| `Q_ASSERT` / `Q_ERROR` / `Q_DIM` | Embedded-friendly assertion macros |
| `Q_onAssert()` | Application callback invoked on a failed assertion |
| Callback | Function declared by the RTOS, defined by the application |
| Composability | Ability to add components without changing existing ones |
| `OS_run()` | Hands control to the kernel; never returns |

---

## Pitfalls to remember

- **Enabling interrupts too early** — a context switch can strand `main()`.
- **Standard `assert()`** in embedded code — no screen, no exit.
- **Ignoring returned error codes** — assertions can't be ignored.
- **Hard-coding threads in the scheduler** — destroys composability.
- **Forgetting the index wrap-around** in round-robin.
- **Expecting `OS_run()` to return.**
- **Still busy-waiting in `BSP_delay()`** — the huge remaining waste, fixed in
  Lesson 25.

---

**Next:** Lesson 25 replaces the wasteful `BSP_delay()` polling with **blocking**
— switching the context away from a delayed thread and back only when the delay
expires.
