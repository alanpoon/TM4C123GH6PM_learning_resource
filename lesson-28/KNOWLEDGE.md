# Lesson 28 — Knowledge: RTOS Part 7 — Mutual Exclusion Mechanisms

**Video:** <https://youtu.be/kcpVI3IjUUM> · **Transcript:** <https://www.state-machine.com/course/lesson-28.txt>
**Hands-on companion:** [PRACTICE.md](PRACTICE.md)

---

## The big idea

Threads **share** variables, functions and peripherals — and every instance of
sharing is a place where their paths cross, threatening conflicts and collisions.
The RTOS offers several **mutual exclusion mechanisms**, each with its own cost
and its own failure mode.

---

## 1. Why RTOS threads can share anything

> An RTOS **does nothing to prevent** threads sharing variables and hardware
> registers.

This differs from desktop/server operating systems (Windows, Linux), where
processes **cannot easily share** memory because they run in **separate address
spaces**. RTOS threads are **lightweight and all run in the same address space**.

> The C compiler is **completely unaware** of the context-switch magic and the
> stack-pointer changes happening under the covers. It treats thread functions
> like any other functions — so they **can** access any variable, memory or
> hardware register visible to them.

## 2. Race conditions between threads

Lesson 20 showed a race between main code and an interrupt. The same thing
happens — **even more so** — **between any two concurrent threads** in a
preemptive kernel.

The telltale signature in a trace: two LEDs changing **simultaneously**, at
nanosecond resolution. That means both were changed by the **same CPU
instruction** — a stale read-modify-write.

The sequence: low-priority thread reads `GPIOF->DATA` → is preempted → a
higher-priority thread toggles its own LED → the low-priority thread resumes and
writes back an **outdated** value, switching **both** LEDs.

## 3. Mechanism 1 — Critical section

Disabling interrupts still works in an RTOS, because it **cuts the CPU off from
the outside world entirely**, so preemption cannot happen.

But the naive unconditional disable/enable of Lesson 20 has **two drawbacks**:

1. **It is inconsistent with the RTOS's interrupt-disabling policy.** QXK
   implements "zero-latency" by disabling interrupts **selectively** up to a
   priority level. A blanket disable adds latency to interrupts that should
   never be disabled.
2. **It does not nest.** Hide a critical section inside a function, call that
   function from within another critical section, and the inner exit
   **re-enables interrupts prematurely** — creating a race in code that was
   supposed to be protected.

### The QXK critical section

```c
QF_CRIT_STAT_TYPE istatus;      /* automatic variable: saved interrupt status */

QF_CRIT_ENTRY(istatus);         /* save the current status, THEN disable      */
    /* ...short critical section... */
QF_CRIT_EXIT(istatus);          /* RESTORE the saved status                   */
```

Because it **saves and restores** the previous status rather than
unconditionally enabling, it **nests** — and it respects the kernel's selective
policy.

### When to use it

> A critical section is a **very powerful** mutual exclusion mechanism — the RTOS
> uses it to protect its own internal variables. But **exactly because it is so
> powerful, it is applicable only to very short sections of code.** Anything
> longer than a few microseconds — more than a handful of machine instructions —
> **increases interrupt latency** in your system.

## 4. A resource held for hundreds of microseconds

The Morse-code example: blink the green LED with dots and dashes.

| Element | Duration |
|---|---|
| **dot** | the fundamental unit (~40 µs here) |
| **dash** | 3 dot-times |
| space within a letter | 1 dot-time |
| space between letters | 3 dot-times |
| space between words | 7 dot-times |

A message is encoded as a **bitmask**, one bit per dot-time. `BSP_sendMorseCode()`
examines the most significant bit (1 → LED on, 0 → off), **busy-waits one
dot-time**, shifts left, and repeats until the mask is zero.

> A **40 µs** delay is **far too short for the blocking RTOS delay**, which works
> only in multiples of the system clock tick. **The only option for microsecond
> delays is a busy-wait loop.**

Two threads share this function: high-priority **blinky1** sends the urgent
**"SOS"** (deadline: once every 2 ms) and low-priority **blinky3** sends
**"TEST"** twice about every 5 ms.

**Unprotected, the messages collide and both get damaged** — producing garbage
like `TESDOS` — and **SOS misses its hard real-time deadline**.

Because the resource is held for nearly a millisecond, a critical section is out
of the question.

## 5. Mechanism 2 — Semaphore (and why it fails)

A semaphore initialized with count **1** (resource available) provides mutual
exclusion: wait before the shared access, signal after. Being binary, **only one
thread at a time gets past the wait** — exactly the mutual exclusion required.

It works: messages are intact, and the delayed SOS still makes its deadline…
**until it doesn't.**

### Unbounded priority inversion

The failing scenario:

1. blinky3 (low) holds the semaphore, sending "TEST".
2. The button press readies blinky2 (medium), which **preempts blinky3**.
3. The next tick readies blinky1 (**highest**) — which **immediately blocks on
   the semaphore**, because blinky3 holds it.
4. **blinky2 now runs for as long as it likes**, while blinky1 — the highest
   priority thread — waits. blinky1 **misses its deadline**.

> This is **unbounded priority inversion**, and it is a **catastrophic failure**
> in a hard real-time system.

**The root cause:** the semaphore is **unaware of thread priorities**, so nothing
prevents the inversion.

> That is unsurprising: semaphores were invented in the era of **time-sharing
> systems**, where thread priority simply wasn't used. Without priorities,
> priority inversion cannot happen — it became a problem only when semaphores
> were forced to work with priority-based kernels.

**Conclusion: the classic semaphore, while fine for *synchronization* (Lesson 27),
is NOT a good mechanism for *mutual exclusion* in priority-based systems.**

## 6. Mechanism 3 — Selective scheduler locking

```c
QSchedStatus sstat = QXK_schedLock(5U);   /* ceiling = priority of blinky1 */
    /* ...access the shared resource... */
QXK_schedUnlock(sstat);
```

- The parameter is the **priority ceiling**: threads **at or below** it are not
  scheduled; **threads above it run as usual**.
- **ISRs run above all threads and are unaffected — no impact on interrupt
  latency.**
- The ceiling **must be at least as high as the highest-priority thread that uses
  the resource**.
- It may only be called **from a thread**, which then **owns** the lock.
- It **returns the previous lock status**, so — like the critical section — locks
  can **nest**.

It is a **non-blocking** mechanism, like a critical section, but far less
pervasive: only threads up to the ceiling are held back.

**Result:** the TEST message runs undisturbed (blinky2 is below the ceiling and
is not scheduled), then blinky1 sends SOS, and only afterwards does blinky2 run.
**Unbounded priority inversion is prevented.**

> Many RTOSes provide only **crude scheduler locking of all threads**, which is
> unfortunately too pervasive. Modern kernels including QXK provide
> **selective** locking up to a ceiling, letting higher-priority threads that
> don't share the resource run **completely undisturbed**.

**The one limitation:** a thread **cannot block while holding the lock**.

> Blocking while accessing a shared resource — calling a blocking delay or a
> semaphore wait — is **bad practice and should be avoided**. But it does happen
> in real projects, and QXK will **assert** if a thread tries to block while
> owning a scheduler lock.

## 7. Mechanism 4 — Mutex

**MUTEX** = **MUT**ual **EX**clusion. Sometimes called a "mutual-exclusion
semaphore" — but:

> **Don't think of a mutex as a kind of semaphore.** Think of it as an RTOS
> object **specifically designed for protecting shared resources in the most
> generic case, where threads might block while accessing the resource.**

```c
QXMutex_init(&Morse_mutex, 6U);       /* priority ceiling */
...
QXMutex_lock(&Morse_mutex, QXTHREAD_NO_TIMEOUT);
    /* ...shared access... */
QXMutex_unlock(&Morse_mutex);
```

> The **lock()/unlock()** naming was chosen deliberately to resemble **scheduler
> locking** and to differ from the **wait()/signal()** of semaphores.

### QXK's priority-ceiling mutex

- Initialized with the **priority ceiling** of the resource.
- The ceiling must be **at least as high** as the highest-priority thread using
  the resource, **and in QXK it must be unique** — it cannot equal a priority
  already used by a thread. Hence **6**, one level above blinky1's 5.
- Because it is "a bit similar to a thread", it **must be initialized after
  `QF_init()`** — which may mean **reordering the calls in `main()`**.

**How it works:** on `lock()`, the owning thread is **promoted to the ceiling
priority**, protecting it from preemption by blinky2 *and even blinky1*. On
`unlock()` its priority **drops back**, and the scheduler picks the highest
ready thread.

> With no blocking in the protected code, the timing diagrams for **scheduler
> locking** and **priority-ceiling mutex** are **identical**.

## 8. Priority Ceiling vs. Priority Inheritance

QXK implements the **Priority-Ceiling Protocol**. **Most RTOSes instead implement
the Priority-Inheritance Protocol.**

| | Priority Ceiling (QXK) | Priority Inheritance (most RTOSes) |
|---|---|---|
| Initialization | needs a **ceiling priority** | **none** — priorities adjust automatically |
| At lock time (A) | owner is **immediately promoted** to the ceiling | **no change yet** — so a medium-priority thread **can preempt** |
| Promotion trigger | unconditional | only when a **higher-priority thread contends**, which the owner then **inherits** |
| Context switches (example) | **2** | **4** |
| Deadline risk | lower | **higher** — the high-priority thread finishes later, due to the initial medium-priority preemption |
| RTOS implementation | simpler | **much more complex to implement correctly** |
| Timing analysis | **simpler** for hard real-time | harder |

> For all these reasons the **priority-ceiling protocol is the preferred
> strategy**, and the only one supported in QXK.

## 9. The bigger picture — shared-state concurrency

> The **"Shared-State Concurrency"** model based on a traditional preemptive RTOS
> has dominated since the **1980s**, but it has several negative implications:
>
> - **First-order effects of sharing:** race conditions and collisions in the
>   time domain or data space. Unprotected, these lead directly to **system
>   failure**.
> - **Second-order effects:** the mutual exclusion mechanisms introduced to avoid
>   those failures each have **their own negative implications**, mostly on
>   **real-time performance** — leading to missed deadlines, which again means
>   failure in hard real-time systems.
>
> The 1990s brought alternative architectures that **avoid sharing entirely**,
> eliminating the need for complex mutual-exclusion mechanisms. Those are the
> subject of the lessons on **event-driven programming and active objects**.

---

## Key takeaways

1. RTOS threads share one address space; the compiler and kernel do nothing to
   stop them sharing.
2. Races occur between threads, not just between threads and ISRs.
3. **Critical section:** powerful, must nest by saving/restoring status, and is
   only for **a handful of instructions**.
4. Microsecond delays require **busy-waiting**; the RTOS delay is tick-granular.
5. **Semaphores are for synchronization, not mutual exclusion** — they cause
   **unbounded priority inversion**.
6. **Selective scheduler locking** is non-blocking, ceiling-limited, doesn't
   affect ISRs — but the owner **must not block**.
7. **Mutex** is the general solution when blocking is possible; QXK's is a
   **priority-ceiling** mutex needing a **unique** ceiling, initialized **after
   `QF_init()`**.
8. **Priority ceiling beats priority inheritance**: fewer context switches,
   earlier completion, simpler implementation and analysis.
9. Shared-state concurrency has inherent second-order costs — later lessons
   avoid sharing altogether.

---

## Glossary

| Term | Meaning |
|---|---|
| Mutual exclusion | Ensuring only one thread uses a resource at a time |
| Critical section | Code executed with interrupts disabled |
| Nesting | Inner sections restoring, not overriding, the outer state |
| Priority inversion | A high-priority thread waiting on a low-priority one |
| **Unbounded** priority inversion | …for an unbounded time, via a medium-priority thread |
| Priority ceiling | Highest priority of any thread using the resource |
| Scheduler lock | Preventing scheduling up to a ceiling |
| Mutex | Object for mutual exclusion, allowing blocking |
| Priority-Ceiling / -Inheritance Protocol | Promote on lock / promote on contention |
| Shared-state concurrency | The classic RTOS programming model |

---

## Pitfalls to remember

- **A critical section that doesn't nest** — premature re-enabling.
- **Long critical sections** — interrupt latency.
- **Using a semaphore for mutual exclusion** — unbounded priority inversion.
- **A ceiling set too low** — a higher-priority sharer slips through.
- **Blocking while holding a scheduler lock** — QXK asserts.
- **A mutex ceiling equal to an existing thread priority** — QXK requires
  uniqueness.
- **Initializing a mutex before `QF_init()`**.
- **Believing tests prove absence of races** — they can only fail to find them.

---

## Further reading

Articles on priority inversion and mutual exclusion linked in the video
description.

---

**Next:** Lessons 29–32 cover **object-oriented programming** — the other major
development of the 1980s — in both C and C++.
