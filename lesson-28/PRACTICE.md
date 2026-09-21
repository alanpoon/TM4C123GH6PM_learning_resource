# Lesson 28 — Practice: Breaking and Fixing Shared Resources

**Concepts behind this exercise:** [KNOWLEDGE.md](KNOWLEDGE.md)
**Video:** <https://youtu.be/kcpVI3IjUUM>

---

## Projects in this lesson

| Directory | Toolchain / kernel | Target |
|---|---|---|
| `tm4c123-qxk-keil/` | KEIL MDK + QXK | EK-TM4C123GXL LaunchPad |
| `qpc/`, `CMSIS/`, `ek-tm4c123gxl/` | — | framework and support code |

Start from a copy of the Lesson 27 project.

> A **logic analyser** is close to essential here — the failures are timing
> failures. Without one, follow the recorded observations.

**The plan:** create a race deliberately, then fix it four different ways and
compare — critical section, semaphore, selective scheduler lock, and mutex.

---

## Part A — Create a race between two threads

### 1. Add toggle operations that share a register

Reuse the sharing from Lesson 20 — the combined `GPIOF->DATA` register:

```c
void BSP_ledBlueToggle(void) {
    GPIOF->DATA = GPIOF->DATA ^ LED_BLUE;   /* == GPIOF->DATA ^= LED_BLUE; */
}
void BSP_ledGreenToggle(void) {
    GPIOF->DATA ^= LED_GREEN;
}
```

> **The critical difference:** `ledOn`/`ledOff` use **different** registers (the
> `DATA_Bits` array), while the **toggle** operations use the **same**
> `GPIOF->DATA` register — which becomes a **shared resource** between any
> threads that call them.

Add the prototypes to `bsp.h` and use the toggles instead of on/off in the blinky
threads.

### 2. Re-tune the loop count

One toggle per iteration is faster than an on+off pair, so raise the iteration
count to keep CPU utilization comparable — **about 1900**.

Also, consider the LED's state after the loop: it is unchanged after an **even**
number of toggles and flipped after an **odd** number. You want it to flip, so
use an explicitly odd count:

```c
for (uint32_t i = 1900U + 1U; i != 0U; --i) { ... }
```

> `1900U + 1U` is folded at compile time to 1901, but the explicit `+ 1`
> **documents the intent** of using an odd number.

### 3. Set up the analyser

Four traces, highest priority at the top:

| Trace | What |
|---|---|
| **SW1** | the switch — falling edge triggers the GPIO ISR, which signals blinky2's semaphore |
| **T1** | blinky1 (highest) — toggles the **green** LED |
| **T2** | blinky2 — toggles the **blue** LED |
| **IDL** | idle (lowest) — switches the **red** LED |

Trigger on the **falling edge of SW1** (active low). Nothing is captured until
you press the switch.

### 4. Find the corrupted toggle

After pressing SW1, blinky2 runs but is preempted every 2 ms by blinky1.

Watch the green LED: it changes state after **every** period of activity — **but
sometimes it doesn't**, and only when blinky2 is also running.

Zoom in on one such point:

- blinky1 **did** change the green LED as expected;
- when blinky2 **resumed after being preempted**, the green LED changed **again**;
- zoom to the **nanosecond** level: green and blue always change
  **simultaneously**.

> Simultaneous change = **both LEDs altered by the same CPU instruction** — the
> race-condition signature from Lesson 20.

**What happened:** blinky2 was preempted by SysTick **after reading**
`GPIOF->DATA` but **before writing it back**. SysTick scheduled blinky1, which
toggled green. When blinky2 resumed it wrote its **outdated** value, switching
**both** LEDs.

> Races happen not only between main code and interrupts, but **between any two
> concurrent threads** in a preemptive kernel — and there, even more so.

---

## Part B — Fix 1: critical section

### 5. Use the kernel's critical section, not the naive one

The unconditional disable/enable from Lesson 20 has **two drawbacks**:

1. **It conflicts with the kernel's policy.** QXK disables interrupts
   **selectively** up to a priority level ("zero latency"); a blanket disable
   penalizes interrupts that should never be disabled.
2. **It doesn't nest.** Hide a critical section in a function, call that function
   from inside another critical section, and the inner exit **re-enables
   interrupts prematurely** — creating a race in code meant to be protected.

QXK's version does both correctly:

```c
void BSP_ledGreenToggle(void) {
    QF_CRIT_STAT_TYPE istatus;
    QF_CRIT_ENTRY(istatus);        /* SAVE the status, then disable */
    GPIOF->DATA ^= LED_GREEN;
    QF_CRIT_EXIT(istatus);         /* RESTORE the saved status      */
}
```

Because it **restores** rather than unconditionally enables, it **nests**.

Delete any redundant naive critical sections, and add the same protection to the
blue toggle.

### 6. Verify

Build, load, and capture again:

- blinky1 now **uses more CPU** — the critical-section overhead inside the toggle
  functions.
- **The green LED always switches state**, as expected. No matter how many times
  you try, you cannot find an incorrect toggle.

> Testing can't prove the absence of bugs, but the race appears eliminated.

> **Scope of applicability:** the critical section is so powerful that it is only
> suitable for **very short** code — more than a few microseconds (a handful of
> instructions) and you are increasing interrupt latency system-wide.

---

## Part C — A resource held far too long for a critical section

### 7. Implement Morse code

| Element | Duration |
|---|---|
| **dot** | the base unit — ~**40 µs** here |
| **dash** | 3 dot-times |
| space within a letter | 1 dot-time |
| space between letters | 3 dot-times |
| space between words | 7 dot-times |

Encode a message as a **bitmask**, one bit per dot-time. E.g. SOS and TEST with
their correct intra- and inter-letter pauses, written in hex (each nibble = 4
bits, Lesson 1).

```c
void BSP_sendMorseCode(uint32_t bitmask) {
    for (; bitmask != 0U; bitmask <<= 1) {
        if ((bitmask & 0x80000000U) != 0U) { /* green LED on  */ }
        else                                { /* green LED off */ }
        /* busy-wait one dot-time */
    }
    /* green LED off, then a 7-dot pause after the word */
}
```

> A **40 µs** delay is **far too short for the blocking RTOS delay**, which only
> works in multiples of the system clock tick. **The only option for microsecond
> delays is a busy-wait loop.**

Add the prototype to `bsp.h`.

### 8. Share it between two threads

- **blinky1** (high priority, 5) sends the urgent **"SOS"**, then delays 1 tick →
  deadline: once every 2 ms.
- **blinky3** (low priority, **1**) — unused until now — sends **"TEST"** twice,
  then delays 5 ticks.

Start blinky3 at priority 1. **Deliberately leave `BSP_sendMorseCode()`
unprotected** — you need to see the failure first.

### 9. Watch the messages collide

Capture (trigger still on SW1) and zoom into the green LED:

- You can read the `... --- ...` **SOS**, arriving every 2 ms.
- But some patterns are wrong. Decode one: `-` (T), `·` (E), `···` (S) — then
  instead of the final `-` (T) you see `-··`, which is **D** — followed by
  `---` (O), `···` (S), then the 7-dot pause.

**`TESDOS`** — neither TEST nor SOS. Later patterns are worse.

> Both messages are damaged, and **SOS misses its hard real-time deadline**.

The resource is held for nearly a **millisecond** — a thousand times too long for
a critical section.

---

## Part D — Fix 2: semaphore (and its catastrophic failure)

### 10. Protect with a semaphore

```c
static QXSemaphore Morse_sema;              /* static: only used in bsp.c */
...
QXSemaphore_init(&Morse_sema, 1U, 1U);      /* initial count 1 = AVAILABLE */
...
QXSemaphore_wait(&Morse_sema, QXTHREAD_NO_TIMEOUT);   /* like waiting at a green light */
    BSP_sendMorseCode(...);
QXSemaphore_signal(&Morse_sema);
```

Binary → **only one thread past the wait at a time** — exactly the mutual
exclusion needed.

### 11. It works…

Capture: SOS is clearly `··· --- ···`, and TEST is `- · ··· -`, **intact**. When
TEST sneaks in, the next SOS is delayed but **still meets its deadline**.

Try many times — everything looks fine.

### 12. …until it doesn't

Eventually you catch a trace where **the green LED stops blinking entirely** for
over **6 ms**, while blinky2 runs undisturbed. The **highest-priority** blinky1
is being prevented from running by a **lower-priority** thread.

Reconstruct it:

1. The button press arrived **while TEST was being sent** — so low-priority
   **blinky3 owns the semaphore**.
2. blinky2 (higher than blinky3) **preempts blinky3** — fine so far.
3. The next tick unblocks **blinky1 (highest)** — which **immediately blocks on
   the semaphore**, since blinky3 holds it.
4. **blinky2 runs for as long as it wants.** blinky1 **misses its deadline.**

> This is **unbounded priority inversion** — a **catastrophic failure** in a hard
> real-time system.

**Root cause:** the semaphore is **unaware of thread priorities**.

> Unsurprising: semaphores come from the **time-sharing** era, where priorities
> weren't used. Without priorities, priority inversion can't happen — it became a
> problem only when semaphores met priority-based kernels.
>
> **The classic semaphore is fine for synchronization (Lesson 27) but is NOT a
> good mutual-exclusion mechanism in priority-based systems.**

---

## Part E — Fix 3: selective scheduler locking

### 13. Swap the mechanism

Comment out the semaphore code (keep it, with a note, so you can experiment with
all four options later) and remove its initialization. Then:

```c
QSchedStatus sstat = QXK_schedLock(5U);   /* ceiling = blinky1's priority */
    BSP_sendMorseCode(...);
QXK_schedUnlock(sstat);
```

Understand each part:

- The parameter is the **priority ceiling**: threads **at or below** it are **not
  scheduled**; threads **above** it run as usual.
- **ISRs run above all threads and are completely unaffected — no impact on
  interrupt latency.**
- The ceiling must be **at least as high as the highest-priority thread using the
  resource** — blinky1, priority **5**.
- It may only be called **from a thread**, which then **owns** the lock.
- It **returns the previous lock status**, so — like the critical section — locks
  **nest**.
- It is **non-blocking**, like a critical section, but far less pervasive.

### 14. Verify both properties

**Collisions:** SOS and TEST are intact and don't run into each other. ✓

**Priority inversion:** catch the special case where the button press arrives
**during TEST**. It happens quickly:

- Even though the button was pressed, **blinky2 does not preempt blinky3** — it
  is **below the ceiling** and therefore not scheduled.
- blinky3 finishes TEST undisturbed, then **blinky1 sends SOS**.
- Only then does blinky2 get to run, until blinky1 preempts it again.

**Unbounded priority inversion is prevented.** ✓

Compare the two timing diagrams:

| Time | Semaphore | Scheduler lock |
|---|---|---|
| **A** (button) | blinky2 scheduled (higher than blinky3) | blinky2 **not** scheduled — below the ceiling |
| **B** (tick) | blinky1 scheduled but **blocks** on the semaphore → blinky2 runs unbounded | blinky1 **not** scheduled either — not above the ceiling |
| after | — | blinky3 finishes, **releases the lock**, scheduler picks the highest ready thread: **blinky1** |

### 15. Know the limitation

> Many RTOSes offer only **crude scheduler locking of all threads** — too
> pervasive. Modern kernels including QXK offer **selective** locking up to a
> ceiling, so higher-priority threads that don't share the resource run
> **completely undisturbed**.

**The one restriction: the owner must not block while holding the lock.**

> Blocking while accessing a shared resource (a blocking delay, a semaphore wait)
> is **bad practice and should be avoided** — but it happens in real projects.
> **QXK will assert** if a thread tries to block while owning a scheduler lock.

If you need to block, you need a mutex.

---

## Part F — Fix 4: priority-ceiling mutex

### 16. Set it up

> **MUTEX** = **MUT**ual **EX**clusion. Sometimes called a "mutual-exclusion
> semaphore" — but **don't think of it as a kind of semaphore.** Think of it as
> an RTOS object **designed for the general case where threads might block while
> accessing the resource.**

```c
static QXMutex Morse_mutex;
...
QXMutex_init(&Morse_mutex, 6U);     /* priority ceiling */
```

Three constraints on the ceiling:

1. **At least as high** as the highest-priority thread using the resource
   (blinky1 = 5);
2. **unique in QXK** — it cannot equal a priority already used by a thread;
3. hence **6**, one level above blinky1.

Because a ceiling mutex is "a bit like a thread", it **must be initialized after
`QF_init()`**. The initialization happens in `BSP_init()`, so **reverse the order
of calls in `main()`** accordingly.

### 17. Apply it

```c
QXMutex_lock(&Morse_mutex, QXTHREAD_NO_TIMEOUT);   /* thread becomes the OWNER */
    BSP_sendMorseCode(...);
QXMutex_unlock(&Morse_mutex);                       /* relinquishes ownership   */
```

> The **lock()/unlock()** naming was chosen deliberately to resemble **scheduler
> locking** and to differ from semaphores' **wait()/signal()**.

### 18. Verify

- **Collisions:** SOS and TEST are intact. ✓
- **Priority inversion:** catch a press during TEST — TEST still comes out intact
  and **blinky2 does not preempt blinky3**. ✓

**How it achieves that:** on `lock()`, the low-priority blinky3 is **promoted to
the ceiling priority**, protecting it from preemption by blinky2 **and even
blinky1**. On `unlock()`, its priority **drops back** and the scheduler picks the
highest ready thread — blinky1.

> With no blocking in the protected code, the timing diagrams for **scheduler
> locking** and **priority-ceiling mutex** are **identical**.

### 19. Compare the two mutex protocols

QXK implements the **Priority-Ceiling Protocol**; **most RTOSes implement
Priority Inheritance** instead. (QXK doesn't support inheritance, so this is
theory — but worth knowing.)

| | Priority Ceiling | Priority Inheritance |
|---|---|---|
| Initialization | needs a ceiling | **none** — adjusts automatically |
| At lock (time A) | owner **immediately promoted** | **no change** → a medium-priority thread **can preempt** |
| Promotion trigger | unconditional | only when a higher-priority thread **contends**; the owner **inherits** its priority |
| Context switches | **2** | **4** |
| High-priority thread finishes | earlier | **later** → higher risk of missing its deadline |
| Implementation | simpler | **much harder to get right** |
| Timing analysis | **simpler** | harder |

> For all these reasons **priority ceiling is preferred**, and is the only
> protocol supported in QXK.

---

## Perspective

> **Shared-state concurrency** — the traditional preemptive-RTOS model — has
> dominated since the 1980s, but:
>
> - **first-order** effects of sharing are **races and collisions**, which
>   unprotected lead directly to failure;
> - **second-order** effects are the mutual-exclusion mechanisms themselves, each
>   with its own costs — mostly to **real-time performance**, leading again to
>   missed deadlines.
>
> The 1990s brought architectures that **avoid sharing altogether**. Those are
> the active-object lessons later in the course.

---

## Exercises

1. **Measure the overhead.** Compare blinky1's CPU utilization with and without
   the critical section in the toggle.
2. **Break the nesting.** Replace `QF_CRIT_EXIT` with an unconditional enable and
   construct a case where it creates a race.
3. **Lower the ceiling.** Set the scheduler-lock ceiling to 2 and find the
   failure it lets through.
4. **Block under a lock.** Call `QXThread_delay()` while holding a scheduler lock
   and confirm QXK asserts.
5. **Duplicate ceiling.** Initialize the mutex with ceiling 5 and see what QXK
   says.
6. **Wrong order.** Call `BSP_init()` before `QF_init()` and observe the mutex
   initialization failing.
7. **Count switches.** Instrument the context switches for one lock/unlock cycle
   and compare with the 2-vs-4 figure for the two protocols.
8. **Eliminate the sharing.** Redesign so the two threads never share the green
   LED at all. What would that require? (This is the direction of the
   active-object lessons.)

---

## Self-check

- [ ] I created a genuine inter-thread race and identified it by simultaneous
      LED changes
- [ ] I fixed it with a **nesting** critical section and know why nesting matters
- [ ] I can state the size limit on critical sections and why
- [ ] I reproduced scrambled Morse messages from an unprotected shared function
- [ ] I reproduced **unbounded priority inversion** with a semaphore and can
      narrate the four steps
- [ ] I used `QXK_schedLock()` with a correct ceiling and verified both properties
- [ ] I used a priority-ceiling mutex, with a unique ceiling, initialized after
      `QF_init()`
- [ ] I can explain why priority ceiling is preferred over priority inheritance
