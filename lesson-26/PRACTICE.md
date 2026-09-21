# Lesson 26 — Practice: A Preemptive Priority-Based Scheduler

**Concepts behind this exercise:** [KNOWLEDGE.md](KNOWLEDGE.md)
**Video:** <https://youtu.be/kLxxXNCrY60>

---

## Projects in this lesson

| Directory | Toolchain | Target |
|---|---|---|
| `tm4c123-keil/` | KEIL MDK (`lesson.uvprojx`) | EK-TM4C123GXL LaunchPad |
| `tm4c123-gnu/` | GNU-ARM (`Makefile`) | EK-TM4C123GXL LaunchPad |
| `stm32c031-keil/` | KEIL MDK | STM32 NUCLEO-C031C6 |

Start from a copy of the Lesson 25 project.

> A **logic analyser** makes this lesson far more convincing. Without one, follow
> the reasoning and the recorded observations.

---

## Part A — Make round-robin fail, visibly

### 1. Speed up the tick

Set the system clock tick to **1000 Hz** — one tick per millisecond.

### 2. Give the threads real work

Right now a thread just flips an LED (a microsecond) and blocks. Simulate a
realistic CPU load:

```c
void main_blinky1(void) {
    while (1) {
        for (uint32_t i = 3*1500U; i != 0U; --i) {   /* ~1.2 ms of work */
            BSP_ledGreenOn();
            BSP_ledGreenOff();
        }
        OS_delay(1U);        /* block until the very next tick */
    }
}
```

The loop runs ~**1.2 ms**, deliberately slightly longer than one tick. With a
1-tick delay after it, the loop body repeats every **2 ms**.

Make `blinky2` run **3× longer** (~3.6 ms) followed by `OS_delay(50U)`, giving a
period of about **54 ms**.

### 3. Note the standard symbols

| Thread | C (computation) | T (period) | U = C/T |
|---|---|---|---|
| **Blinky1** | 1.2 ms | 2 ms | **60 %** |
| **Blinky2** | 3.6 ms | 54 ms | **6.6 %** |

### 4. Make the idle thread visible

**Comment out `__WFI()`** in `OS_onIdle()` — otherwise the CPU stops and you
can't see the idle toggling in the analyser.

### 5. Read the trace

Four traces: **ISR** (SysTick), **T1** (Blinky1), **T2** (Blinky2), **IDL**
(idle).

- ISR repeats every **1 ms**.
- T1 runs ~1.2 ms, repeating every **2 ms**.
- T2 runs ~3–4 ms (excluding gaps), repeating every **55 ms**.
- IDL runs only when nothing else does.

### 6. Find the deadline miss

Zoom into the moment **both** blinkies are ready at once:

1. Blinky1 starts, but is **scheduled out on the next clock tick**.
2. Blinky2 runs for one tick.
3. Blinky1 is scheduled in again, finishes, and blocks.
4. Blinky2 uses the rest of the slice.
5. On the next tick Blinky1 becomes ready — but this is the **THIRD** tick since
   its previous activation. **It missed its 2 ms deadline by one tick.**

> Round-robin was built for **time-sharing**, where **fairness** is the goal, so
> it takes the CPU away from Blinky1 when its slice expires. In a hard real-time
> system that's wrong: if Blinky1 controls a lunar module's descent, one missed
> 2 ms deadline can be catastrophic.

---

## Part B — Implement priorities

### 7. Add priority to the TCB and the API

```c
typedef struct {
    void *sp;
    uint32_t timeout;
    uint8_t prio;          /* NEW: 0..32 fits comfortably in uint8_t */
} OSThread;

void OSThread_start(OSThread *me, uint8_t prio,
                    OSThreadHandler threadHandler,
                    void *stkSto, uint32_t stkSize);
```

Bump the MiROS version number.

### 8. Re-purpose `OS_thread[]`

| | Lesson 25 | Now |
|---|---|---|
| Index means | registration order | **the thread's priority** |
| Gaps | none | **allowed** |

So use `prio` as the index and store it in the TCB. Example layout: idle at 0,
Blinky2 at 2, Blinky1 at 5, ThreadN at N.

### 9. Strengthen the precondition

It is no longer enough that the priority is in range — it must not already be
taken:

```c
Q_REQUIRE((prio < Q_DIM(OS_thread)) && (OS_thread[prio] == (OSThread *)0));
```

Move it to the **top** of the function and use **`Q_REQUIRE`**, because it is a
**precondition** — an obligation of the **caller**. Assigning unique priorities
is the application programmer's job.

### 10. Pick a numbering convention

| Scheme | Used by | Priority 0 means |
|---|---|---|
| Inverse | NUCLEUS, ThreadX, MicroC/OS, embOS, … | **highest** |
| **Direct** | **MiROS** | idle thread; **bigger number = higher priority** |

> The inverse scheme causes constant confusion when talking about "higher" and
> "lower" priorities. MiROS uses **direct** numbering.

### 11. Write `LOG2()`

To find the highest-priority ready thread, **count leading zeros**:

```
priority = 32 - CLZ(OS_readySet)
```

| `OS_readySet` | CLZ | priority |
|---|---|---|
| only bit 1 (Blinky2) | 30 | 2 |
| only bit 4 (Blinky1, prio 5) | 27 | 5 |
| bit N−1 | 32−N | N |
| **all zeros (idle)** | 32 | **0** ✓ |

The idle condition needs **no special case**.

Cortex-M4 has **`CLZ` in hardware**. Find it in the TivaC datasheet, then search
your compiler's help for `CLZ` — KEIL exposes the intrinsic **`__clz()`**:

```c
#define LOG2(x)  (32U - __clz(x))
```

### 12. Rewrite the scheduler

```c
if (OS_readySet == 0U) {
    OS_next = OS_thread[0];              /* idle thread — no OS_currIdx now */
}
else {
    OS_next = OS_thread[LOG2(OS_readySet)];
    Q_ASSERT(OS_next != (OSThread *)0);
}
```

> `LOG2()` is guaranteed to return 0…32, so it can index `OS_thread[]` **without
> range checking**.

### 13. Add `OS_delayedSet` and rewrite `OS_tick()`

`OS_tick()` can't scan consecutively any more — priorities may be sparse. Add a
second bitmask of **delayed** threads and iterate only over its one-bits:

```c
uint32_t workingSet = OS_delayedSet;     /* temp copy: bits get removed */
while (workingSet != 0U) {
    OSThread *t = OS_thread[LOG2(workingSet)];
    uint32_t bit = (1U << (t->prio - 1U));

    Q_ASSERT((t != (OSThread *)0) && (t->timeout != 0U));

    --t->timeout;
    if (t->timeout == 0U) {
        OS_readySet   |= bit;      /* ready now        */
        OS_delayedSet &= ~bit;     /* no longer delayed */
    }
    workingSet &= ~bit;            /* processed         */
}
```

The `bit` temporary removes the obvious duplication.

Delete the now-unused `OS_currIdx` and `OS_threadNum`.

### 14. Update `OS_delay()`

Replace `OS_currIdx` with the **current thread's priority**, and also **set the
bit in `OS_delayedSet`**:

```c
uint32_t bit = (1U << (OS_curr->prio - 1U));
OS_curr->timeout = ticks;
OS_readySet   &= ~bit;
OS_delayedSet |=  bit;
OS_sched();
```

### 15. Fix the application

The build fails — `OSThread_start()`'s signature changed. Every thread needs a
**unique** priority:

| Thread | Priority |
|---|---|
| idle | **0** (explicitly, in `OS_init()`) |
| Blinky2 | **2** |
| Blinky1 | **5** |

> Priorities **need not be consecutive**. What matters is that they are
> **unique** and that Blinky1 outranks Blinky2.

---

## Part C — Verify

### 16. Inspect the generated code

Breakpoint inside `OS_sched()` where `LOG2()` is used. In the disassembly, after
loading the addresses of `OS_thread` and `OS_readySet`, **the entire
highest-priority calculation is two instructions**:

| Instruction | Does | Cycles |
|---|---|---|
| `CLZ` | counts leading zeros into r0 | 1 |
| `RSB` (reverse subtract) | converts to a priority number | 1 |

The result here is **5** — Blinky1's priority. Confirm `OS_next` is set to
Blinky1's address.

### 17. Read the new trace

Run free, then look at the logic analyser:

- **Blinky1 runs completely undisturbed**, even when Blinky2 becomes ready.
- **Blinky1 always meets its deadline** — and so does Blinky2, despite being
  preempted several times.
- The idle thread runs only when no thread or ISR is active.

The trace matches the priority-based timeline exactly.

### 18. Notice why blocking is indispensable

> A high-priority thread runs **for as long as it wants**; **no** lower-priority
> thread can run until it **blocks**. Without blocking, **no lower-priority
> thread would ever run.**

Confirm it in the trace: Blinky2 and idle run **only** when Blinky1 blocks; idle
runs **only** when both blinkies are blocked.

This is completely unlike round-robin, which ran every thread in succession even
when none blocked — and it is why efficient blocking (Lesson 25) had to come
first.

---

## Part D — Assigning priorities: RMA

### 19. Try the wrong assignment

Swap the priorities so **Blinky1 is lower** than Blinky2. Blinky1 misses its
deadline as soon as Blinky2 becomes ready.

**The rule:** *assign higher priorities to threads with shorter periods* (hence
shorter deadlines).

### 20. Apply the RMA test

This rule is **Rate-Monotonic Analysis** (Liu & Layland, 1973). The three steps:

1. **Assign priorities monotonically** — higher rate ⇒ higher priority.
2. **Measure each thread's utilization** Uₙ = Cₙ / Tₙ.
3. **Sum them** and compare against the bound U(n), which for large n approaches
   **ln 2 ≈ 0.69**.

For your threads:

```
U = 1.2/2 + 3.6/54 = 0.60 + 0.066 = 0.666    <  0.69   ✓ schedulable
```

> **Practical rule: stay below ~70 % CPU utilization.**

### 21. Know the extensions

- Basic RMA assumes **periodic threads with constant execution time**. For
  aperiodic or variable threads use the **worst case**: **shortest** time between
  activations, **longest** execution time.
- In practice only a few top-priority threads have hard deadlines. Apply RMA to
  those; prioritize soft-real-time threads below them.

> The beauty of this scheduler: a high-priority thread **immediately preempts**
> everything below it, so it is **insensitive to changes in the execution time or
> period of lower-priority threads**. It **decouples threads in the time domain**
> — which is why it became the norm in most RTOSes.

---

## Exercises

1. **Push past the bound.** Increase Blinky1's computation time until total
   utilization exceeds 70 % and find the first missed deadline.
2. **Duplicate priority.** Start two threads at the same priority and confirm
   `Q_REQUIRE` catches it.
3. **Never block.** Remove `OS_delay()` from Blinky1 and watch everything below
   it starve.
4. **Sparse priorities.** Use priorities 1, 7, 31 and confirm `OS_tick()` still
   works efficiently — how many loop iterations does it do?
5. **Count the cost.** Compare the cycle count of the new `OS_sched()` against
   the round-robin version.
6. **Inverse numbering.** Sketch what would change if MiROS adopted the inverse
   convention. Which lines of `LOG2()`-based code break?
7. **Three threads.** Add a third periodic thread, apply RMA to assign its
   priority, predict schedulability, then verify on the analyser.

---

## Self-check

- [ ] I reproduced a round-robin deadline miss and can explain why it happens
- [ ] `OSThread_start()` takes a unique priority, enforced by `Q_REQUIRE`
- [ ] `OS_thread[]` is indexed by priority and may have gaps
- [ ] `LOG2()` uses the hardware `CLZ` and compiles to two instructions
- [ ] `OS_tick()` iterates over `OS_delayedSet` bits, not the whole array
- [ ] Blinky1 (shorter period) has the higher priority and meets its deadline
- [ ] I can explain why blocking is essential to priority-based scheduling
- [ ] I computed total CPU utilization and compared it against ~70 %
