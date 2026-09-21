# Lesson 25 — Knowledge: RTOS Part 4 — Efficient Blocking of Threads

**Video:** <https://youtu.be/JurV5BgjQ50> · **Transcript:** <https://www.state-machine.com/course/lesson-25.txt>
**Hands-on companion:** [PRACTICE.md](PRACTICE.md)

---

## The big idea

Replace the wasteful polling delay with **blocking**: switch the context **away**
from a delayed thread and back only when the delay expires. In between the thread
consumes **no CPU cycles at all**.

> From the **thread's** perspective nothing changes — it calls a delay function
> that doesn't return until the delay elapses. From the **system's** perspective
> it makes all the difference: those cycles become available to threads that
> actually have work to do.

---

## 1. Blocking must live in the RTOS

The blocking delay needs the scheduler and the context switch, so it becomes an
RTOS service: **`OS_delay()`**.

> Note the trend: **functionality migrating from the application to the
> system-level software**, where it can be handled far more efficiently. You will
> meet this pattern repeatedly in more advanced techniques.

## 2. The thread life cycle

A thread now needs a **Ready / Not-Ready** property, because a **Blocked thread
must never be scheduled in**.

```
   Dormant ──OSThread_start()──▶ ┌──── Ready (super-state) ────┐
                                 │  Preempted ⇄ Running ①      │
                                 └──────────────┬──────────────┘
                                     OS_delay() │   ▲ OS_tick()
                                                ▼   │
                                             Blocked
```

| State | Meaning |
|---|---|
| **Dormant** | object and stack allocated, but `OSThread_start()` not yet called |
| **Preempted** | looks exactly as though preempted by an interrupt |
| **Running** | **exactly one** thread at a time on a single CPU |
| **Blocked** | **not ready** to run |
| **Ready** (super-state) | Preempted or Running — ready and willing to run |

> The transition **into Blocked** is unique: it is **the only transition a thread
> takes voluntarily, itself**. All others are forced on it without its
> cooperation or consent.

A blocked thread **cannot unblock itself** — that must be managed centrally by
the RTOS, periodically, from the clock-tick interrupt: **`OS_tick()`**.

## 3. The idle thread — a necessary consequence

> **A system of blocking threads is incomplete and necessarily requires one
> special thread that is always ready to run and cannot block.**

With two threads, the scheduler copes while at least one is Ready. But if **both
block**, the CPU must still be running exactly one thread — and none is Ready.

The solution is the **idle thread**, run whenever no other thread is Ready (the
**idle condition**). It has the usual superloop structure but calls the
**`OS_onIdle()`** callback, where the application can do something useful. Its
life cycle has **no Blocked state**.

Design details:

- It is started from **`OS_init()`**, which guarantees it is **index 0** in the
  thread array.
- Its **stack is supplied by the application**, not pre-allocated by the RTOS —
  the kernel cannot know how much stack `OS_onIdle()` will need.

## 4. Per-thread timeout counters

Add a private **timeout counter** to the TCB:

- `OS_delay()` loads it with the number of ticks;
- every `OS_tick()` decrements all **non-zero** counters;
- a counter reaching zero **unblocks** its thread.

Separate counters let delays run **in parallel and independently**. Counters
already at zero are simply left alone.

## 5. `OS_readySet` — a bitmask instead of per-thread flags

A boolean "ready" flag in each TCB would be the obvious design, but **grouping
the flags into one 32-bit bitmask is more efficient**:

```
OS_readySet bit n-1  ⟷  OS_thread[n]
```

The **idle thread at index 0 is skipped** (it is always ready), so the bits are
**shifted by one** relative to the array indices: index 1 ↔ bit 0, index 2 ↔ bit
1, index 32 ↔ bit 31.

| Bitmask value | Meaning |
|---|---|
| bits 0, 1, n−1 set | threads 1, 2 and n are ready |
| only bit 1 set | thread 2 is ready |
| **all bits zero** | **the idle condition** |

> Checking for the idle condition becomes a **single comparison against zero**.

(The bigger payoff comes in Lesson 26, where the bit numbers become **thread
priorities**.)

## 6. The scheduler with blocking

```c
if (OS_readySet == 0U) {        /* idle condition -- the most frequent path */
    OS_currIdx = 0U;            /* the idle thread */
}
else {
    do {
        ++OS_currIdx;
        if (OS_currIdx == OS_threadNum) {
            OS_currIdx = 1U;    /* wrap to 1, SKIPPING the idle thread */
        }
    } while ((OS_readySet & (1U << (OS_currIdx - 1U))) == 0U);
}
OS_next = OS_thread[OS_currIdx];
```

To test whether thread N is ready: synthesize a mask with only bit **N−1** set
and AND it with `OS_readySet`; a zero result means **not ready**, so keep going.

## 7. `OS_delay()`

```c
void OS_delay(uint32_t ticks) {
    __disable_irq();
    Q_REQUIRE(OS_curr != OS_thread[0]);       /* NOT the idle thread! */
    OS_curr->timeout = ticks;
    OS_readySet &= ~(1U << (OS_currIdx - 1U));  /* make NOT ready      */
    OS_sched();                                  /* switch away now    */
    __enable_irq();
}
```

- `OS_sched()` is designed to be called with interrupts disabled; the switch
  happens right after they are re-enabled (Lesson 23).
- Calling it from the **idle thread must be forbidden** — the idle thread must
  never block.

### `Q_REQUIRE()` — a precondition

This assertion type is a **precondition**: a requirement that must hold **before**
the service is called. `qassert.h` provides **`Q_REQUIRE()`**, which behaves
exactly like `Q_ASSERT()` but **conveys the intent precisely**.

## 8. `OS_tick()`

```c
void OS_tick(void) {
    for (uint8_t n = 1U; n < OS_threadNum; ++n) {    /* skip the idle thread */
        if (OS_thread[n]->timeout != 0U) {
            --OS_thread[n]->timeout;
            if (OS_thread[n]->timeout == 0U) {
                OS_readySet |= (1U << (n - 1U));      /* unblock */
            }
        }
    }
}
```

Two design notes:

- **No critical section is needed inside `OS_tick()`**, because it is called from
  a *single* ISR (`SysTick_Handler`). An ISR cannot be preempted by a thread, so
  the counters — changed only in `OS_delay()` — cannot change underneath it.
- **`OS_sched()` need not be called from `OS_tick()`**, because
  `SysTick_Handler` calls the scheduler at its end anyway, which will schedule
  any newly unblocked thread.

## 9. Final adjustments

- Every started thread **except the idle thread** must be explicitly marked
  ready by setting its bit in `OS_readySet` — the transition into the **Ready**
  super-state.
- Add `OS_delay()` and `OS_tick()` prototypes to `miros.h`.
- The application now uses `OS_delay()` / `OS_tick()` instead of `BSP_delay()` /
  `BSP_tickCtr()`.

## 10. What the measurements show

With `OS_onIdle()` toggling a pin (50 MHz TM4C123):

| Measurement | Time |
|---|---|
| Typical `SysTick_Handler` | ~3 µs |
| **Longest** `SysTick_Handler` (when a thread is unblocked) | ~**4.5 µs** — 3× longer than before blocking, due to the more complex `OS_tick()`/`OS_sched()` |
| Context switch | still **< 2 µs** |
| Blocking in `OS_delay()` (mark not-ready + schedule) | ~3.8 µs |
| **Longest time spent outside the idle thread** | only **10 µs** |

> **The CPU spends almost all of its time in the idle thread**, with a few
> microseconds here and there for the real threads.

Breaking in at random lands in `OS_onIdle()`; a breakpoint in the scheduler shows
`OS_readySet == 0` most of the time.

## 11. Low-power sleep in `OS_onIdle()`

The idle thread still wastes every cycle it gets. The best remedy is to **put the
CPU and its peripherals into a low-power sleep mode**.

> You will never achieve a truly low-power design with the CPU and peripherals
> running at full speed. For battery-operated applications you **must** use sleep
> modes — and **`OS_onIdle()` is the ideal, central place** to enter them.

Cortex-M provides **`WFI` (Wait For Interrupt)**, which stops the CPU clock until
an interrupt occurs — available via the CMSIS function **`__WFI()`**.

With `__WFI()` in place, the idle pin toggles **exactly once after each
interrupt**: the CPU stops inside every `OS_onIdle()` call and is woken by the
next SysTick.

---

## Key takeaways

1. Blocking replaces polling: the thread consumes no cycles while waiting.
2. Blocking must be an **RTOS service**, because it needs the scheduler.
3. Threads gain a **Ready/Not-Ready** property and a **Blocked** state.
4. Blocking is the **only voluntary** transition in a thread's life cycle.
5. A blocking system **requires an idle thread** that can never block.
6. Per-thread **timeout counters** let delays run independently.
7. The **`OS_readySet` bitmask** makes the idle check a single comparison.
8. `OS_tick()` runs in a single ISR, so it needs no critical section.
9. `Q_REQUIRE()` expresses a **precondition** — the caller's obligation.
10. **`OS_onIdle()` is where low-power sleep (`__WFI()`) belongs.**

> At this point MiROS implements a round-robin time-sharing scheduler with
> blocking — the state of the art of computer systems in the **early 1960s**.

---

## Glossary

| Term | Meaning |
|---|---|
| Blocking | Efficiently waiting by relinquishing the CPU |
| Blocked / Ready | Not schedulable / schedulable |
| Dormant / Preempted / Running | Stages of a thread's life cycle |
| Super-state | A state enclosing other states (see Lesson 40) |
| Idle thread | Always-ready thread run when nothing else is |
| Idle condition | No non-idle thread is ready |
| `OS_readySet` | Bitmask of ready threads |
| Timeout counter | Per-thread down-counter driving delays |
| Precondition / `Q_REQUIRE` | Assertion about the caller's obligation |
| `WFI` / `__WFI()` | Wait-For-Interrupt low-power instruction |

---

## Pitfalls to remember

- **Forgetting the idle thread** — the scheduler has nothing to run when all
  threads block.
- **Letting the idle thread block** — guard it with a precondition.
- **Including the idle thread in round-robin** — wrap to index 1, not 0.
- **Off-by-one in the bit/index mapping** — bits are shifted by one.
- **Not marking newly started threads ready.**
- **Adding critical sections to `OS_tick()`** — unnecessary, and costly, when it
  runs in a single ISR.
- **Calling `OS_sched()` from `OS_tick()`** — redundant.

---

**Next:** Lesson 26 replaces round-robin with a **preemptive, priority-based**
scheduler — the point at which the "real-time" in RTOS becomes real.
