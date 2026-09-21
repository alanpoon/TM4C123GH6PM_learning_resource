# Lesson 26 — Knowledge: RTOS Part 5 — What Is "Real-Time"?

**Video:** <https://youtu.be/kLxxXNCrY60> · **Transcript:** <https://www.state-machine.com/course/lesson-26.txt>
**Hands-on companion:** [PRACTICE.md](PRACTICE.md)

---

## The big idea

Round-robin is a **fairness** policy — and fairness is exactly wrong for
real-time. Replacing it with a **preemptive, priority-based scheduler** makes
MiROS worthy of the name RTOS, and brings a body of theory (**RMA/RMS**) that can
**mathematically prove** a set of threads will meet their deadlines.

---

## 1. What "real-time" means

Until now, any computation was equally useful and you cared only whether it was
**correct**. Real-time adds **timeliness**:

> A computation performed **too late** (or too early) is **less useful, and can
> be as harmful as an outright wrong computation.**

| System type | Usefulness after the deadline |
|---|---|
| **Hard real-time** | drops to **negative infinity** — worse than useless (which would be merely zero). A missed deadline is a **system failure**. Deploying an airbag too late is not useless, it is disastrous. |
| **Soft real-time** | still useful, but the value **diminishes with time** — e.g. a text message expected within ~20 seconds. |

This lesson focuses on **hard** real-time.

### Real-time systems are usually periodic

Recognized since the 1960s–70s (the Apollo program ran identical real-time
computers in the command and lunar modules; Margaret Hamilton led much of that
software effort). Lunar descent needed thruster corrections **every few
milliseconds**; industrial control runs from milliseconds to tens of seconds;
engine control is synchronized to engine revolutions.

### The standard symbols

| Symbol | Meaning |
|---|---|
| **Cₙ** | **computation time** of thread n |
| **Tₙ** | **period** of thread n |
| **Uₙ = Cₙ / Tₙ** | **CPU utilization** of thread n |

## 2. Why round-robin misses deadlines

With two loaded threads — T1 (C₁ = 1.2 ms, T₁ = 2 ms, U₁ = 60 %) and T2
(C₂ = 3.6 ms, T₂ = 54 ms, U₂ = 6.6 %) — the trace shows:

1. T1 starts, then is **scheduled out on the next clock tick**.
2. T2 runs for one tick.
3. T1 is scheduled in again and finishes, then blocks.
4. T1 next becomes ready on the **third** tick from its previous activation —
   **missing its 2 ms deadline by one tick**.

> Round-robin was designed for **time-sharing**, where the goal is to share the
> CPU **fairly**. So it takes the CPU away from T1 when its time slice expires.
>
> **That is not what you want in a hard real-time system.** If T1 controls a
> lunar module's descent, missing even one 2 ms deadline can be catastrophic.
> You don't care about fairness — you care about the deadline.

You need a scheduler that **knows how important each thread is**.

## 3. Preemptive, priority-based scheduling with static priorities

- Each thread gets a **unique priority number** when started; it **never
  changes**.
- **The scheduler always runs the highest-priority thread that is ready to run.**

Under this policy, on the same workload:

- T2 becoming ready does **not** disturb T1, because T1 has higher priority —
  so **T1 easily meets its 2 ms deadline**.
- T2 runs only when T1 **voluntarily blocks** in `OS_delay()`, and is preempted
  again the moment T1 unblocks.
- Yet T2 **still completes well before its 54 ms deadline**.

> **Both threads meet their hard real-time deadlines.**

### Blocking is absolutely critical here

> A high-priority thread runs **for as long as it wants**, and no lower-priority
> thread can run until it **blocks and voluntarily relinquishes the CPU**.
> **Without blocking, no lower-priority thread would EVER run.**

That is why efficient blocking (Lesson 25) had to come first.

## 4. Priority numbering

| Scheme | Used by | Meaning of 0 |
|---|---|---|
| **Inverse** | NUCLEUS, ThreadX, MicroC/OS, embOS and many others, for historical reasons | **highest** priority |
| **Direct** | **MiROS** | idle thread; **higher number = higher priority** |

> The inverse scheme "leads to constant confusion when talking about higher and
> lower thread priorities."

## 5. Finding the highest-priority ready thread: `LOG2()`

With `OS_readySet` bits now representing **priorities**, the highest-priority
ready thread is found by **counting leading zeros**:

```
priority = 32 - CLZ(OS_readySet)
```

| `OS_readySet` | CLZ | priority |
|---|---|---|
| only bit 1 set (Blinky2) | 30 | **2** |
| only bit 4 set (Blinky1, prio 5) | 27 | **5** |
| bit N−1 set | 32 − N | **N** |
| **all zeros (idle condition)** | 32 | **0** — the idle thread ✓ |

The formula works for the idle condition too, with no special case.

> Mathematically **32 − CLZ(x)** finds the most significant one-bit — the integer
> approximation of **log₂**. Hence it is coded as the **`LOG2()`** macro.

**And it is fast:** Cortex-M4 implements **`CLZ` in hardware**, one cycle. The
compiler exposes it as the intrinsic **`__clz()`**. The whole
highest-priority-ready calculation compiles to **two instructions**:

| Instruction | Cycles |
|---|---|
| `CLZ` — count leading zeros | 1 |
| `RSB` (reverse subtract) — convert to a priority | 1 |

`LOG2()` is guaranteed to yield 0…32, so its result can index `OS_thread[]`
**without range checking**.

## 6. Redesigning the data structures

### `OS_thread[]` indexed by priority

| | Round-robin (L25) | Priority-based (L26) |
|---|---|---|
| Index meaning | consecutive registration order | **the thread's priority** |
| Gaps | none | **allowed** — priorities need not be consecutive |

So `OSThread_start()` takes a `uint8_t prio` parameter (0…32 fits comfortably),
stores it in the TCB, and uses it as the array index.

The precondition becomes stronger — not only must the priority be **in range**,
it must not already be **in use**:

```c
Q_REQUIRE((prio < Q_DIM(OS_thread)) && (OS_thread[prio] == (OSThread *)0));
```

> As a **precondition**, this is the **caller's** obligation: it is the
> application programmer's job to assign a unique priority to each thread.

### `OS_delayedSet`

`OS_tick()` can no longer scan the array consecutively — there may be large gaps.
Instead add a second bitmask, **`OS_delayedSet`**, holding the **delayed**
threads, and iterate only over its **one-bits** using `LOG2()`:

```c
uint32_t workingSet = OS_delayedSet;      /* temporary: bits get removed */
while (workingSet != 0U) {
    OSThread *t = OS_thread[LOG2(workingSet)];
    uint32_t bit = (1U << (t->prio - 1U));
    Q_ASSERT((t != (OSThread *)0) && (t->timeout != 0U));

    --t->timeout;
    if (t->timeout == 0U) {
        OS_readySet   |= bit;      /* ready     */
        OS_delayedSet &= ~bit;     /* no longer delayed */
    }
    workingSet &= ~bit;            /* processed */
}
```

`OS_currIdx` and `OS_threadNum` are no longer needed and can be removed.

`OS_delay()` correspondingly uses the **current thread's priority** instead of
`OS_currIdx`, and now also **sets the bit in `OS_delayedSet`**.

## 7. RMA / RMS — how to assign priorities

With two threads there are only two possibilities, and only one works: giving
**T1 (the shorter period) the higher priority**. The rule that emerges:

> **Assign higher priorities to threads with shorter periods** (hence shorter
> deadlines).

This was discovered in **1973** by **C. L. Liu and James W. Layland**,
*"Scheduling Algorithms for Multiprogramming in a Hard-Real-Time Environment"* —
later generalized as **Rate-Monotonic Analysis (RMA)** / **Rate-Monotonic
Scheduling (RMS)**.

> "Rate monotonic" simply means the priority assignment maps **increasing thread
> rates to increasing thread priorities** — a fancy name for the rule above.

### The three RMA guidelines

1. **Assign priorities monotonically** — higher rate ⇒ higher priority.
2. **Know each thread's CPU utilization**, Uₙ = Cₙ / Tₙ (measured execution time
   over period).
3. **Sum them.** If the total is below the theoretical **utilization bound
   U(n)**, **all threads are guaranteed to meet their deadlines** — proven by Liu
   and Layland in 1973.

U(n) depends on the number of threads and, for large n, approaches **ln 2 ≈ 0.69**.

> **In practice: stay below ~70 % CPU utilization and your thread set is
> schedulable.**

Worked example:

```
U = 1.2/2 + 3.6/54 = 0.60 + 0.066 = 0.666   ->  below the bound ✓
```

### Extensions and practice

- Basic RMA assumes **periodic threads with constant execution time**. It extends
  to aperiodic threads with variable execution time by using the **worst case**:
  the **shortest** time between activations and the **longest** execution time.
- In practice only a **few highest-priority threads** have hard deadlines; the
  rest have soft requirements. Apply RMA to the hard ones and prioritize the
  soft ones lower.

### Why this scheduler won

> A high-priority thread can **always immediately preempt** all lower-priority
> threads, so it is **insensitive to changes in the execution time or period of
> lower-priority threads**. The preemptive priority-based scheduler **decouples
> threads in the time domain**.

For these reasons it became **the norm**, supported in most RTOSes to this day.

---

## Key takeaways

1. Real-time = correctness **plus** timeliness; a hard deadline missed is a
   failure.
2. Round-robin optimizes **fairness**, which causes deadline misses.
3. Priority-based preemptive scheduling always runs the **highest-priority ready
   thread**.
4. **Blocking is essential** — without it, lower-priority threads never run.
5. MiROS uses **direct** priority numbering (higher number = higher priority).
6. `LOG2()` via the hardware **`CLZ`** finds the highest-priority ready thread in
   **two instructions**.
7. `OS_thread[]` is indexed by **priority**, which must be **unique** — a
   precondition.
8. **RMA:** shorter period ⇒ higher priority; keep total utilization under ~70 %.
9. Preemptive priority scheduling **decouples threads in the time domain**.

> MiROS has now advanced from the 1960s to the **1970s**.

---

## Glossary

| Term | Meaning |
|---|---|
| Hard / soft real-time | Deadline miss = failure / degraded usefulness |
| Cₙ, Tₙ, Uₙ | Computation time, period, CPU utilization |
| Static priority | Assigned at start, never changed |
| Preemptive | A higher-priority ready thread displaces a running one |
| Direct / inverse priority numbering | Higher number = higher / lower priority |
| `CLZ` | Count Leading Zeros instruction |
| `LOG2()` | `32 - CLZ(x)`; index of the most significant one-bit |
| `OS_delayedSet` | Bitmask of currently delayed threads |
| RMA / RMS | Rate-Monotonic Analysis / Scheduling |
| Utilization bound | U(n) → ln 2 ≈ 0.69 for large n |

---

## Pitfalls to remember

- **Assuming fairness is good** — it is the enemy of deadlines.
- **A high-priority thread that never blocks** — it starves everything below it.
- **Duplicate priorities** — caught by the precondition; the application's job.
- **Confusing the two priority-numbering conventions** when reading other RTOS
  documentation.
- **Scanning the whole thread array** when priorities are sparse.
- **Exceeding ~70 % utilization** and expecting deadlines to hold.
- **Forgetting worst-case analysis** for aperiodic or variable-time threads.

---

## Further reading

- Liu & Layland (1973), *Scheduling Algorithms for Multiprogramming in a
  Hard-Real-Time Environment*
- *Rate Monotonic Analysis for Real-Time Systems*

---

**Next:** Lesson 27 advances to the **1980s** — inter-thread synchronization and
communication.
