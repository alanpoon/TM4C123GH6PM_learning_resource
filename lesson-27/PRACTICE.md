# Lesson 27 — Practice: Porting to QXK and Using a Semaphore

**Concepts behind this exercise:** [KNOWLEDGE.md](KNOWLEDGE.md)
**Video:** <https://youtu.be/IrDcBZX0AdY>

---

## Projects in this lesson

| Directory | Toolchain / kernel | Target |
|---|---|---|
| `tm4c123-qxk-keil/` | KEIL MDK + **QXK** | EK-TM4C123GXL LaunchPad |
| `tm4c123-qxk-gnu/` | GNU-ARM + QXK (`Makefile`) | EK-TM4C123GXL LaunchPad |
| `tm4c123-freertos-keil/` | KEIL + **FreeRTOS** | EK-TM4C123GXL LaunchPad |
| `tm4c123-ucos2-keil/` | KEIL + **µC/OS-II** | EK-TM4C123GXL LaunchPad |
| `stm32c031-qxk-keil/` | KEIL + QXK | STM32 NUCLEO-C031C6 |
| `qpc/`, `freertos/`, `ucos2/` | — | kernel sources and ports |
| `CMSIS/`, `ek-tm4c123gxl/`, `nucleo-c031c6/` | — | support code |

> The same application ported to **three different RTOSes** — a direct
> demonstration of what porting means.

Start from a copy of the Lesson 26 project.

**Goal:** the blue LED should start toggling **only after you press SW1**.

---

## Part A — Swap the kernel

### 1. Remove MiROS

Remove the MiROS files from the project and rename the group to **QPC**. Then
**delete `miros.h` and `miros.c` from disk**, so nothing can silently keep using
them.

### 2. Add the QP/C sources

Make sure `qpc/` sits alongside your lesson folders (Lesson 21 explains how to
get it). Then right-click the **QPC** group → *Add existing files*, and add:

| From | What |
|---|---|
| `qpc/src/qf/` | **all files** — state machines, event-driven and component-based programming (explained in later lessons) |
| `qpc/src/qxk/` | **all files** — the QXK preemptive blocking kernel |
| `qpc/ports/arm-cm/qxk/arm/` | **`qxk_port.c`** — the port for Cortex-M + ARM-KEIL |

> The ports tree looks complex, but that's typical of professional RTOSes ported
> to many CPUs and toolchains — and it means your combination is probably already
> there (`arm`, `config` (CLANG), `gnu`, `iar`).

### 3. Let the compiler drive the port

> Porting is **replacing the foundation under a house with minimal disturbance to
> the house itself.** Build repeatedly and fix what the compiler reports.

**First:** replace `#include "miros.h"` with `#include "qpc.h"`.

**Then:** `qf_port.h` isn't found — it is included from `qpc.h` and lives in the
**same port directory** as `qxk_port.c`. Add that directory to the **include
search path**.

Build again: **all the QP/C sources compile.** The remaining errors are genuine
**API mismatches**.

### 4. Work through the API mapping

| Error about | Replace with | Note |
|---|---|---|
| `OSThread` | **`QXThread`** | same concept |
| `OS_delay()` | **`QXThread_delay()`** | |
| `OS_init()` | **`QF_init()`** | **no idle stack needed** — QXK reuses the main C stack, so you recover that RAM |
| `OSThread_start()` | **constructor + `QXTHREAD_START()`** | see below |
| `OS_run()` | **`QF_run()`** | also never returns |

**Starting a thread in QXK is two steps:**

```c
QXThread_ctor(&blinky1, Q_XTHREAD_CAST(&main_blinky1), 0U);
/*            object     thread function                ^ clock tick rate */

QXTHREAD_START(&blinky1,
               5U,                        /* priority                */
               (void *)0, 0U,             /* message queue buf, size */
               stack_blinky1, sizeof(stack_blinky1),
               (void *)0);                /* argument to the thread  */
```

- A thread can only be started **after the object is initialized** by its
  **constructor** — the reason becomes clear in the OOP lessons (29–32).
- `QXTHREAD_START` is an **all-caps macro**, also for OOP reasons.
- QXThreads can have **dedicated message queues**; blinky1 doesn't use one.

### 5. Fix the thread signatures

The compiler complains the thread function pointer is incompatible — **QXK
thread functions take one parameter**, called **`me`** by convention, giving
access to the thread object from inside the function:

```c
void main_blinky1(QXThread * const me) { ... }
```

Adjust **all** thread functions, then repeat steps 4–5 for the remaining threads.

### 6. Fix `bsp.c`

The diagnostics move from compiler errors to **linker** errors:

| Old | New |
|---|---|
| scheduler call at the end of `SysTick_Handler` | **`QXK_ISR_EXIT()`** — performs preemptive scheduling inside a critical section |
| — | **`QXK_ISR_ENTRY()`** at ISR entry, **before any other QXK API call** |
| `OS_tick()` | **`QF_TICK_X(0U, ...)`** — same tick rate `0` as in the constructors |
| `OS_onStartup()` | **`QF_onStartup()`** |
| `OS_onIdle()` | **`QXK_onIdle()`** — identical semantics, only the name changes |
| — | **`QF_onCleanup()`** — define it **empty**; it exists only for applications that exit, and deeply embedded ones never do |

---

## Part B — Interrupt priorities and kernel awareness

### 7. Understand why SysTick can't stay at priority 0

Under MiROS you set SysTick to priority **0** — the highest. Under QXK that is
**wrong**, and the reasoning is worth following:

**Interrupt latency** = request → first ISR instruction. It includes:

1. interrupts are asynchronous and recognized only at instruction boundaries;
2. **interrupt entry** time;
3. **critical sections** where the RTOS has disabled interrupts to protect its
   own variables.

> What matters is the worst case: **maximum latency = longest critical section +
> entry time.**

Some ISRs can't afford that. If an ISR **never calls RTOS APIs**, it can't
interfere with the kernel, so it needn't be penalized:

| | Calls RTOS API | Disabled by the kernel |
|---|---|---|
| **kernel-unaware** | **never** | **never** |
| **kernel-aware** | yes | yes (longer worst-case latency) |

> "Zero interrupt latency" for kernel-unaware ISRs doesn't mean instantaneous —
> it means the **RTOS adds nothing** to their latency.

This needs **selective** masking:

| Core | Mechanism |
|---|---|
| Cortex-M0/M0+ | only `PRIMASK` — all-or-nothing |
| **Cortex-M3/M4/M7** | also **`BASEPRI`** — masks only up to a given priority |

QP/C's Cortex-M3+ port uses `BASEPRI` and defines
**`QF_AWARE_ISR_CMSIS_PRI`** as the boundary.

### 8. Set SysTick correctly

`SysTick_Handler` calls QP/C APIs, so it **must be kernel-aware**: priority
**`QF_AWARE_ISR_CMSIS_PRI` or a bigger number** (= lower Cortex-M priority). Set
it in `QF_onStartup()` and update the comment to explain why.

> See *"Setting ARM Cortex-M Interrupt Priorities in QP"* for the priority maps
> with 3-bit and 4-bit NVICs.

### 9. Verify the port before adding features

Build clean, then sanity-check in the debugger — **focus on threads, interrupts,
and the idle callback**:

- First breakpoint hit: **inside blinky1** — reasonable, it's the highest
  priority thread.
- Next: **`SysTick_Handler`** — interrupts are being serviced.
- Watch **how the interrupt returns**: a `POP` to `PC`, landing in a BSP function
  called from blinky1.
- Next: **blinky2** — the lower-priority thread runs too.

To inspect kernel state, delete the stale MiROS variables from the Watch window
and find the QXK equivalent: open `qpc/include/qxk.h`, find the structure of
**global kernel attributes** at the top, and watch **`QXK_attr_`**. Expand it —
**`curr`** points at blinky2, as expected.

- Last: the **idle callback**, where the red LED is toggled.

Run free and check the logic analyser with the Lesson 26 setup: ISR every 1 ms;
T1 ~1.2 ms meeting its 2-tick deadline; T2 meeting its 54-tick deadline; IDL only
when nothing else runs.

> **QXK behaves exactly like the final MiROS version.** The port is verified.

---

## Part C — Add a semaphore

### 10. Define and initialize it

```c
/* in main.c */
QXSemaphore SW1_sema;
...
QXSemaphore_init(&SW1_sema,
                 0U,    /* initial count: NOT signalled */
                 1U);   /* max count: BINARY semaphore  */
```

Initialize at the **top of `main()`**.

> Look up `QXSemaphore` in the online docs (state-machine.com → Products →
> QP/C framework → search). It is a kernel object: a structure plus
> `QXSemaphore_`-prefixed functions. Key members: **`count`** (an up/down token
> counter), the **max count**, and **`waitSet`** (which threads are waiting —
> like MiROS's ready-set, but up to **64** priority levels).
>
> **Binary** (max = 1) is the usual choice **for signalling**: the count is only
> 0 (not signalled) or 1 (signalled).

### 11. Wait on it in blinky2

```c
QXSemaphore_wait(&SW1_sema, QXTHREAD_NO_TIMEOUT);
```

`wait()` takes a timeout; **`QXTHREAD_NO_TIMEOUT`** waits indefinitely.

**Delete the blocking delay** — the semaphore is now the blocking mechanism.

### 12. Wire up the SW1 button

From the [board user manual](../resources/Tiva-C_Launchpad_User_Manual.pdf)
schematics (second page), trace **SW1 → PF4**.

In `bsp.c` (the right home for board-specific code):

```c
#define BTN_SW1  (1U << 4)
```

> **Conflict:** PF4 is currently the `TEST_PIN` toggled by SysTick for the logic
> analyser. **Remove all uses of `TEST_PIN`.**

Configure the pin:

- **direction: input**
- **digital function enabled**
- **pull-up resistor enabled** — so the pin idles **high** and pressing SW1 pulls
  it **low**
- **interrupt on the falling edge** — that is what "button pressed" looks like

### 13. Write the GPIO ISR

Your first ISR unrelated to the clock tick. Start by copying `SysTick_Handler`:

1. **Rename it** — look up the correct name in the startup code's vector table.
2. **Keep `QXK_ISR_ENTRY` / `QXK_ISR_EXIT`** — this is definitely kernel-aware.
3. **Delete the `QF_TICK_X` call.**
4. **Check the interrupt source** — this ISR also fires for other GPIO-F pins if
   they are configured to interrupt.
5. **Clear the interrupt in the GPIO peripheral**, explicitly, so it is ready for
   the next one.
6. Inside the `if`, **signal the semaphore**:

```c
QXSemaphore_signal(&SW1_sema);
```

### 14. Set its priority and enable it

Like SysTick, this **kernel-aware** ISR must have its priority set **below the
kernel-aware level** — do it in `QF_onStartup()`, at the same level as SysTick or
one lower (a **bigger** number on Cortex-M).

Then **explicitly enable the GPIO interrupt in the NVIC**.

### 15. Fix the last two build errors

| Error | Fix |
|---|---|
| `SW1_sema` undefined in `bsp.c` | it's defined in `main.c` — put its **declaration in `bsp.h`**, which both include |
| `QXSemaphore` type undefined | **include `bsp.h` *after* `qpc.h`**. While you're there, drop `stdint.h` — `qpc.h` already includes it |

Build: zero errors, zero warnings.

---

## Part D — Verify the semaphore

### 16. Sanity-check with breakpoints

Set breakpoints at the **semaphore wait** in blinky2 and the **semaphore signal**
in the GPIO ISR.

1. First hit: **the wait** — blinky2 is about to block. Good.
2. **Move the breakpoint to the first instruction *after* the wait.**
3. Continue → **nothing is hit.** Correct: the thread is blocked, so code past
   the wait is unreachable.
4. **Press SW1** → the breakpoint at **signal** is hit → the GPIO interrupt is
   configured correctly.
5. Continue → you **immediately** hit the breakpoint past the wait → the thread
   unblocked.
6. Run → nothing until SW1 is pressed again, then the same sequence.

**blinky2's execution is synchronized to the button.**

### 17. Adjust the logic analyser

Run free and look — **at first, nothing.** Switch the trigger to **AUTO** and you
will see: the green LED toggling from blinky1 and the red from the idle callback,
but the **ISR pin stuck high** and the blue LED inactive.

That is expected: the old trigger used the **rising** edge of PF4 driven by
SysTick. PF4 is now SW1, which **idles high and goes low on press**.

**Change the trigger to the falling edge of pin 4** — the same edge that triggers
your GPIO interrupt.

### 18. Read the traces

Press SW1 repeatedly and study the captures:

- Usually a press pulls the ISR line low and the **blue LED toggles for about
  8.5 ms**, ±a few ms for preemption.
- If the press arrives while **blinky1 is not running**, blinky2 starts
  **immediately**.
- If it arrives **while blinky1 is running**, blinky2 must **wait until blinky1
  voluntarily blocks** — QXK is preemptive and RMS-compliant, so the
  higher-priority thread runs as long as it wants.

> **Remember: unblocking a semaphore waited on inside a lower-priority thread may
> be delayed by all higher-priority threads.**

---

## Part E — Switch bounce as a semaphore stress test

### 19. Spot the anomaly

Some traces show blinky2 running **twice as long as usual**. Zoom in: the falling
edge of the ISR line is **not clean** — it shows spikes, visible even better in
the analog waveform (probe PF4 from the bottom of the board).

> **All mechanical switches bounce** momentarily before making permanent contact.
> To a fast CPU those bounces look like multiple presses and releases.
>
> **Never ship production software without debouncing your switches** (search the
> web for *debouncing*). The noisy signal is used here **deliberately**, as a
> stress test that reveals how a semaphore really works.

### 20. Analyse: three signals, two loops

Three falling edges → **three signals** — yet blinky2 looped only **twice**. Use
the **token model**:

> **signal** adds a token, **but only up to the maximum count**.
> **wait** removes a token if available, otherwise **blocks**.

| Event | Semaphore | blinky2 |
|---|---|---|
| start | 0 tokens | **blocked** in `wait()` |
| bounce 1 → signal | +1 | unblocks and, still inside `wait()`, **takes** the token |
| bounce 2 → signal | +1 | running; hasn't called `wait()` yet → token **stays** |
| bounce 3 → signal | **dropped** (binary, already full) | still running |
| loops, calls `wait()` | token taken | **does not block** — second pass |
| loops, calls `wait()` | empty | **blocks** |

### 21. Analyse: three signals, one loop

Trace 6 shows the same three edges (bouncing on **release** this time) but only
**one** pass:

| Event | Semaphore | Threads |
|---|---|---|
| start | 0 tokens | blinky2 blocked |
| bounce 1 → signal | +1 | blinky2 **can't run** — higher-priority blinky1 is running, so `wait()` isn't executed and the token **isn't removed** |
| bounces 2, 3 → signal | **dropped** | blinky1 still running |
| blinky1 **blocks** in `delay()` | | QXK schedules blinky2, which unblocks and **removes** the token |
| blinky2 loops, calls `wait()` | empty | **blocks** |

> Identical stimulus, different outcome — **preemption** is the difference.

---

## Exercises

1. **Counting semaphore.** Change the max count to 5 and repeat the bounce
   analysis. How does the behaviour change?
2. **Debounce it.** Implement proper debouncing (software timer or state machine)
   and confirm one press = one signal.
3. **Add a timeout.** Replace `QXTHREAD_NO_TIMEOUT` with a finite timeout and
   handle the timeout case.
4. **Kernel-unaware ISR.** Set the GPIO interrupt above `QF_AWARE_ISR_CMSIS_PRI`
   and observe what breaks when it calls `QXSemaphore_signal()`.
5. **Priority inversion preview.** Make blinky1 run longer and measure how long
   blinky2's unblocking is delayed.
6. **Port again.** Open `tm4c123-freertos-keil/` and `tm4c123-ucos2-keil/` and
   build the API-mapping table for those kernels.
7. **Read the kernel.** Find `QXSemaphore_wait()` in the QP/C sources and trace
   how it manipulates `count` and `waitSet`.

---

## Self-check

- [ ] MiROS is gone; the app builds and runs on QXK with identical timing
- [ ] Threads are constructed, then started with `QXTHREAD_START()`
- [ ] Thread functions take the `me` parameter
- [ ] `QXK_ISR_ENTRY`/`EXIT` bracket every kernel-aware ISR
- [ ] SysTick and GPIO priorities are at or below `QF_AWARE_ISR_CMSIS_PRI`
- [ ] I can explain kernel-aware vs. kernel-unaware and what `BASEPRI` enables
- [ ] blinky2 blocks on `QXSemaphore_wait()` and is released by SW1
- [ ] I traced a bounce event through the token model and explained the outcome
