# Lesson 21 — Knowledge: The Foreground/Background ("Superloop") Architecture

**Video:** <https://youtu.be/AoLLKbvEY8Q> · **Transcript:** <https://www.state-machine.com/course/lesson-21.txt>
**Hands-on companion:** [PRACTICE.md](PRACTICE.md)

---

## The big idea

Foreground/background — also called **superloop** or **main+ISRs** — is the
**starting point for all embedded software architectures** and the stepping
stone to understanding an RTOS. This lesson also draws the distinction that runs
through the rest of the course: **sequential/blocking** versus **event-driven/
non-blocking** code.

> This lesson begins a new group of lessons on **architecture and design**, and
> is a prerequisite for the RTOS lessons that follow.

---

## 1. The architecture

| Part | What it is |
|---|---|
| **Background** | the endless loop inside `main()` |
| **Foreground** | the interrupt handlers (`SysTick_Handler()` and others) |

Interrupts in the foreground **preempt** the background loop, but **always return
to the point of preemption**.

### Communication between the two

The two parts communicate through **shared variables**. To avoid race conditions
(Lesson 20) those variables must be:

- declared **`volatile`** — they change without any visible instruction doing it;
- accessed from the background only inside a **critical section** (interrupts
  briefly disabled).

### The fundamental limitation

> The timing of functions called from the background loop is **not well defined**:
> it depends on time spent in the loop, which varies from pass to pass due to
> conditional branching and interrupt activity.

Therefore **anything with strict timing constraints must be pushed up to the
interrupt level**. But that makes interrupts longer, and they begin to interfere
with the background loop and with each other. This tension is what eventually
motivates an RTOS.

### Where you'll find it

Despite its limits, foreground/background is **very popular in high-volume
embedded applications**: consumer electronics, home appliances, toys, remote
controls.

It is also **exactly the architecture of Arduino**, hidden inside the library:

```c
int main(void) {
    init();
    setup();
    for (;;) {          /* `for (;;)` == `while (1)` — the C idiom for "forever" */
        loop();
    }
}
```

Arduino has a foreground too — a system clock-tick ISR incrementing counters,
and the matching polling `delay()`.

## 2. A better delay: `BSP_delay()`

Unlike Lesson 8's crude counting loop, this one is based on the **SysTick
interrupt**, giving **timing independent of compiler-generated code speed**.

```c
#define BSP_TICKS_PER_SEC 100U     /* in bsp.h */

static uint32_t volatile l_tickCtr;      /* static AND volatile */

void SysTick_Handler(void) {
    ++l_tickCtr;
}

uint32_t BSP_tickCtr(void) {
    uint32_t tickCtr;
    __disable_irq();                     /* critical section! */
    tickCtr = l_tickCtr;
    __enable_irq();
    return tickCtr;
}

void BSP_delay(uint32_t ticks) {
    uint32_t start = BSP_tickCtr();
    while ((BSP_tickCtr() - start) < ticks) {
    }
}
```

Points worth noting:

- **`volatile`** because the ISR modifies it; **`static`** to limit it to the
  module.
- The read **must** be in a critical section to avoid a race with the ISR.
- **Two's complement arithmetic handles the rollover correctly** when the tick
  counter wraps from all-Fs to 0 — the subtraction still yields the right
  difference.
- **But it is still polling.** It wastes every CPU cycle until the delay expires.

## 3. Taking the BSP to the next level

> The background code should specify **WHAT** needs to be done; the BSP should
> specify **HOW** to do it.

Move everything board-specific into the BSP:

- `BSP_init()` — all hardware initialization;
- `BSP_ledGreenOn()` / `BSP_ledGreenOff()` / … — LED control;
- the MCU header, the LED pin masks, the clock constant.

Prototypes go in `bsp.h`; `main.c` then needs **none** of the hardware details.

### Why separating WHAT from HOW pays

1. **The main code is smaller and self-explanatory** — it barely needs comments.
2. **Portability**: the same application code runs on a different board or with a
   different toolchain — you only reimplement `bsp.c`. It can even run on a
   desktop PC, which is not an embedded board at all.

## 4. Sequential/blocking vs. event-driven/non-blocking

### Sequential and blocking

```c
while (1) {
    BSP_ledGreenOn();
    BSP_delay(BSP_TICKS_PER_SEC / 4U);
    BSP_ledGreenOff();
    BSP_delay(3U * BSP_TICKS_PER_SEC / 4U);
}
```

- **Blocking** — it waits for an event (a timeout) **in-line** and makes no
  progress until the event arrives. When it does arrive, the code proceeds
  naturally, because **the code downstream of the blocking call provides the
  right context** for that event.
- **Sequential** — the **sequence of expected events is hard-coded in the
  sequence of instructions**.

In a flowchart, the polling loops show up as **arrows going backwards** — and
that is where the program spends essentially all its time. Break in at random
and you will find it inside `BSP_tickCtr()`, called from `BSP_delay()`, called
from `main()`. **You have almost no chance of catching it doing anything else.**

### Event-driven and non-blocking

The same behaviour can be written without any polling loop — as a **polling state
machine** — so every path through the loop runs to completion.

- In the flowchart, **no arrow goes backwards**; the program spends its time in
  the **main loop itself**.
- The loop spins **hundreds of thousands of times per second** instead of once
  per second, so it can **handle events as soon as they arrive, in the order they
  arrive**. That is why it is called **event-driven**.

### The trade-off

> The price of that flexibility and timeliness is **apparent higher complexity**:
> the sequence of acceptable events is **no longer hard-coded in the sequence of
> instructions**.

And a warning about real code:

> Structuring non-blocking code as a **state machine** is unfortunately **not the
> norm**. Most real projects use convoluted, deeply nested IF-THEN-ELSE branching
> over many global variables — "spaghetti code" or a "big ball of mud".

State machines are the subject of Lessons 35–42.

---

## Key takeaways

1. Foreground/background = background `while(1)` loop + foreground ISRs.
2. Shared variables must be `volatile` and accessed in critical sections.
3. Background-loop timing is not deterministic; strict timing must go into ISRs —
   at the cost of longer ISRs.
4. `BSP_delay()` is more precise than a counting loop, but still pure polling.
5. Put **WHAT** in the application and **HOW** in the BSP — it buys clarity and
   portability.
6. **Sequential/blocking** hard-codes the event sequence and spends its life
   waiting.
7. **Event-driven/non-blocking** reacts to events in arrival order, at the cost of
   complexity — best managed with **state machines**.

---

## Glossary

| Term | Meaning |
|---|---|
| Foreground/background | Superloop + ISRs; a.k.a. main+ISRs |
| Background loop | The endless loop in `main()` |
| Foreground | The interrupt handlers |
| BSP | Board Support Package |
| `BSP_TICKS_PER_SEC` | System clock-tick rate (100 Hz here) |
| Blocking | Waiting in-line until an event arrives |
| Sequential | Event order hard-coded in instruction order |
| Event-driven / non-blocking | Reacting to events as they arrive, never waiting in-line |
| `for (;;)` | C idiom for "forever", equivalent to `while (1)` |

---

## Pitfalls to remember

- **Reading a shared counter without a critical section.**
- **Forgetting `volatile`** on a variable modified by an ISR.
- **Relying on background-loop timing** for anything time-critical.
- **Letting ISRs grow long** to compensate — they start interfering with
  everything.
- **Board details leaking into `main.c`** — they belong in the BSP.
- **Non-blocking code written as nested `if`s** instead of a state machine.

---

**Next:** Lesson 22 begins the RTOS series — running **multiple background
loops** on one CPU.
