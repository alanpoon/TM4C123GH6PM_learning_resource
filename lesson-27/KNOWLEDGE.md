# Lesson 27 — Knowledge: RTOS Part 6 — Synchronization and Communication

**Video:** <https://youtu.be/IrDcBZX0AdY> · **Transcript:** <https://www.state-machine.com/course/lesson-27.txt>
**Hands-on companion:** [PRACTICE.md](PRACTICE.md)

---

## The big idea

Threads that run completely independently are like **trains on separate circular
tracks**. In real life the tracks cross, which requires **synchronization and
communication** — to provide timely service and to avoid collisions.

The railroad solved it with the **semaphore**: open, and a train passes; closed,
and it waits.

> Inter-thread mechanisms are **the most complex elements of any RTOS and are
> really tricky to develop yourself.** So this lesson retires the toy MiROS and
> ports the application to the professional-grade **QXK** kernel in **QP/C**.

---

## 1. History

**Edsger Dijkstra** adapted the railroad semaphore for software in the **1960s**,
for a time-sharing system he was designing. It was later extended to
priority-based schedulers and RTOSes, which **went mainstream in the early
1980s**.

## 2. Porting an application

> **Porting** an application from one RTOS to another is **like replacing the
> foundation under a house with minimal disturbance to the house itself.** It is
> a valuable exercise in its own right.

The method: remove the old kernel, add the new one, and **let the compiler tell
you exactly what needs adjusting.**

### The API mapping

| MiROS | QXK (QP/C) |
|---|---|
| `miros.h` | `qpc.h` |
| `OSThread` | **`QXThread`** |
| `OS_delay()` | **`QXThread_delay()`** |
| `OS_init(idleStack, size)` | **`QF_init()`** — *no idle stack needed* |
| `OSThread_start(...)` | **constructor** + **`QXTHREAD_START()`** macro |
| `OS_run()` | **`QF_run()`** — also never returns |
| `OS_sched()` at ISR end | **`QXK_ISR_EXIT()`** (+ `QXK_ISR_ENTRY()` at entry) |
| `OS_tick()` | **`QF_TICK_X(0U, ...)`** |
| `OS_onStartup()` | **`QF_onStartup()`** |
| `OS_onIdle()` | **`QXK_onIdle()`** — identical semantics |
| — | **`QF_onCleanup()`** — only for applications that exit; empty for deeply embedded |

### Structural differences worth understanding

- **QXK reuses the main C stack for the idle thread**, so that stack space is
  recovered — a more clever design than MiROS's separate idle stack.
- **A thread must be *constructed* before it is started.** The **constructor**
  associates the `QXThread` object with its thread function and with a **clock
  tick rate** (here `0`). Why this is needed becomes clear in the
  object-oriented-programming lessons (29–32).
- **`QXTHREAD_START()`** (all capitals — a macro, again for OOP reasons) takes
  more parameters: the thread object, its priority, a **message queue buffer and
  size** (QXThreads can have dedicated queues; unused here), the **stack buffer
  and size**, and a **pointer argument passed to the thread**.
- **Thread functions take one parameter**, called **`me`** by convention, giving
  access to the associated thread object from inside the function.

### Where the ports live

A professional RTOS has a **ports** directory structured by CPU and toolchain —
`qpc/ports/arm-cm/qxk/arm|gnu|iar|...`. Complex, but it means that if you switch
CPU or toolchain, you will most likely find your combination there.

## 3. Interrupt latency

> **Interrupt latency** is the delay from the interrupt request to the **first
> instruction of the ISR**.

It has several contributions:

1. Interrupts are **asynchronous** (Lesson 17) and the CPU can only recognize
   them at **instruction boundaries**.
2. **Interrupt entry** itself takes time (≥12 cycles on Cortex-M).
3. **Critical sections** — the RTOS must occasionally disable interrupts to
   protect its own variables (Lessons 20, 23).

> As always in real-time, what matters is the **worst case**: the **maximum
> interrupt latency = longest critical section + interrupt entry time**.

### Kernel-aware vs. kernel-unaware interrupts

That maximum may be too long for some ISRs. If an ISR **makes no RTOS API
calls**, it cannot interfere with the kernel — so it need not be penalized by the
kernel's critical sections.

| | Can call RTOS API | Disabled by the kernel | Latency |
|---|---|---|---|
| **Kernel-unaware** | **no** | **never** | unaffected by the RTOS |
| **Kernel-aware** | yes | yes | longer maximum latency |

> "Zero interrupt latency" for kernel-unaware interrupts does **not** mean they
> are handled instantaneously — that is impossible. It means **the presence of
> the RTOS has zero impact on their latency**.

### `BASEPRI` — selective interrupt disabling

This distinction requires a CPU that can disable interrupts **selectively**.

| Core | Mechanism |
|---|---|
| Cortex-M0 / M0+ | only **`PRIMASK`** — disables **all** interrupts |
| Cortex-M3 / M4 / M7 | also **`BASEPRI`** — masks interrupts only **up to a specified priority level**; anything above is **never disabled** |

QP/C's Cortex-M3+ port uses `BASEPRI`, and defines the constant
**`QF_AWARE_ISR_CMSIS_PRI`**, which separates the two groups.

> **Consequence:** an ISR that calls RTOS APIs — such as `SysTick_Handler` —
> **must be kernel-aware**, i.e. have priority `QF_AWARE_ISR_CMSIS_PRI` **or a
> bigger number** (= lower Cortex-M priority). Leaving SysTick at priority 0
> (as MiROS did) would make it kernel-**unaware**, which is **incorrect**.
>
> See the application note *"Setting ARM Cortex-M Interrupt Priorities in QP"*.

## 4. Semaphores

A **`QXSemaphore`** is a kernel object: the structure plus functions with the
`QXSemaphore_` prefix.

| Member | Meaning |
|---|---|
| **`count`** | up/down counter of how many times the semaphore has been signalled and waited on |
| max count | the configured capacity |
| **`waitSet`** | which threads are waiting — like MiROS's ready-set bitmask, but up to **64** priority levels in QXK |

### The API

```c
QXSemaphore_init(&SW1_sema, 0U, 1U);   /* object, initial count, max count */
QXSemaphore_wait(&SW1_sema, QXTHREAD_NO_TIMEOUT);
QXSemaphore_signal(&SW1_sema);
```

- Initialize at the **top of `main()`**.
- **Binary semaphore** = max count **1**, so the count is only 0 (not signalled)
  or 1 (signalled) — the usual choice **for signalling**.
- `wait()` takes a **timeout**; `QXTHREAD_NO_TIMEOUT` waits indefinitely.

### The token model

> Think of a semaphore as a protocol of **exchanging tokens**:
>
> - **signal** adds a token — **but only up to the configured maximum count**;
> - **wait** removes a token if any are available, **or blocks** if none are.

### Priority interacts with semaphores

> **Unblocking of a semaphore waited on inside a lower-priority thread may be
> delayed by all higher-priority threads.**

QXK is a preemptive priority-based kernel fully compliant with RMS, so a
higher-priority thread runs as long as it wants before a signalled lower-priority
thread gets the CPU.

## 5. A worked stress test: switch bouncing

> **All mechanical switches sometimes momentarily bounce** before making
> permanent contact. To a fast CPU those bounces look like multiple presses and
> releases.
>
> **Never ship production software without properly debouncing your switches.**
> (Search the web for *debouncing*.)

Here the noisy signal is used **deliberately**, as a stress test that reveals how
a semaphore really behaves. **Three** falling edges → **three** signals — but the
waiting thread may loop **twice** or **once**, depending on preemption:

**Scenario A (thread looped twice):**

| Event | Semaphore |
|---|---|
| initially | 0 tokens → blinky2 **blocks** in `wait()` |
| bounce 1 → signal | +1 token → blinky2 **unblocks** and, still inside `wait()`, **takes** the token |
| bounce 2 → signal | +1 token; blinky2 is running and hasn't called `wait()` yet, so the token **stays** |
| bounce 3 → signal | semaphore already holds 1 token and is **binary** → the signal is **dropped** |
| blinky2 loops, calls `wait()` | token available → **removed, no blocking**; second pass |
| blinky2 loops, calls `wait()` | empty → **blocks** |

**Scenario B (thread looped once):**

| Event | Semaphore |
|---|---|
| initially | 0 tokens → blinky2 blocked |
| bounce 1 → signal | +1 token — but blinky2 **cannot run**: the higher-priority blinky1 is running, so `wait()` is not performed and the token is **not** removed |
| bounces 2 and 3 → signal | semaphore already full → **dropped** |
| blinky1 finally **blocks** in `delay()` | QXK schedules blinky2, which unblocks and **removes** the token |
| blinky2 loops, calls `wait()` | empty → **blocks** |

> Same three signals, different outcome — the difference is **preemption**.

---

## Key takeaways

1. Real threads must synchronize; semaphores are the classic mechanism.
2. Inter-thread mechanisms are hard — use a proven kernel.
3. Porting = swap the foundation, let the compiler find the mismatches.
4. **Maximum interrupt latency = longest critical section + entry time.**
5. **Kernel-aware** ISRs may call RTOS APIs but have longer worst-case latency;
   **kernel-unaware** ISRs must never call them.
6. Cortex-M3+ `BASEPRI` enables selective masking; set kernel-aware ISR
   priorities at or below `QF_AWARE_ISR_CMSIS_PRI`.
7. A semaphore is a **token protocol**: signal adds (up to max), wait removes or
   blocks.
8. A **binary** semaphore (max 1) **drops** extra signals.
9. Unblocking a low-priority waiter can be **delayed by higher-priority threads**.
10. **Debounce your switches** in production code.

---

## Glossary

| Term | Meaning |
|---|---|
| Semaphore | Synchronization object holding a count of tokens |
| Binary semaphore | Maximum count of 1 |
| Signal / wait | Add a token / remove a token or block |
| `waitSet` | Bitmask of threads waiting on a semaphore |
| Porting | Moving an application to a different RTOS |
| Interrupt latency | Request → first ISR instruction |
| Kernel-aware / -unaware ISR | May / may not call RTOS APIs |
| `PRIMASK` / `BASEPRI` | Global / selective interrupt masking |
| `QF_AWARE_ISR_CMSIS_PRI` | Priority boundary between the two ISR groups |
| Debouncing | Filtering mechanical switch noise |

---

## Pitfalls to remember

- **Leaving a kernel-aware ISR at priority 0** — the kernel never disables it,
  so its RTOS calls are unprotected.
- **Calling RTOS APIs from a kernel-unaware ISR.**
- **Omitting `QXK_ISR_ENTRY` / `QXK_ISR_EXIT`.**
- **Expecting every signal to be counted** — a binary semaphore drops extras.
- **Assuming a signalled thread runs immediately** — priority decides.
- **Not debouncing switches.**
- **Header order** — `bsp.h` must be included *after* `qpc.h` if it uses QP types.

---

**Next:** Lesson 28 covers **mutual exclusion** — sharing resources safely among
threads.
