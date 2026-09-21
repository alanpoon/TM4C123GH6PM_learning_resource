# Lesson 20 — Knowledge: Race Conditions

**Video:** <https://youtu.be/3ha72Y8pyD4> · **Transcript:** <https://www.state-machine.com/course/lesson-20.txt>
**Hands-on companion:** [PRACTICE.md](PRACTICE.md)

---

## The big idea

> **Race conditions are the worst kind of bug you will ever deal with. They are
> the direct consequence — and the huge price — of using interrupts.**

Every individual piece of code is correct. The bug exists only in the *combination*
and in *when* preemption happens, which you do not control.

---

## 1. Definition

> A **race condition** occurs when two or more pieces of code that can preempt
> each other access a **shared resource** in such a way that the **result depends
> on the sequence of execution** of those pieces.

## 2. The worked example

Main loop clears the green LED using the shared GPIO `DATA` register:

```c
GPIOF->DATA = GPIOF->DATA & ~LED_GREEN;   /* == GPIOF->DATA &= ~LED_GREEN; */
```

The short `&=` form performs **exactly the same read-modify-write sequence**.
In machine code that is three steps:

```
LDR   r2, [r3]      ; READ the DATA register
BIC   r2, r2, #8    ; MODIFY: clear the green bit
STR   r2, [r3]      ; WRITE back
```

Meanwhile `SysTick_Handler` sets the blue LED in the **same** register.

If the interrupt lands **after the read but before the write**:

1. Main reads `DATA` — a snapshot without blue.
2. The ISR runs and **turns the blue LED on**.
3. Main writes back its **stale** value — **extinguishing both green and blue**.

The main code was supposed to change **only** the green bit. The ISR's update is
silently lost.

**Symptom on the board:** the blue LED's blinking is no longer regular —
occasionally it pauses for a second or more. Yet stepping through each piece of
code individually shows nothing wrong.

## 3. Why this matters

In blinky it's cosmetic. Substitute something real:

> Suppose the SysTick interrupt switches on the **cooling system of a nuclear
> reactor**. The interrupt turns the cooling on; a fraction of a microsecond
> later the main loop turns it off again. The cooling stays off and the reactor
> melts down.

### Why race conditions are so nasty

- They **seem to defy logic** — each piece of code is individually correct.
- The failure needs a **narrow timing window** you cannot control.
- Resulting bugs are **intermittent, hard to reproduce, and hard to isolate**.
- You may test for **hours or weeks** without seeing a problem — and still ship a
  catastrophic race to the field.

## 4. Strategy 1 — Mutual exclusion

> Ensure that only one piece of concurrent code can execute while accessing a
> shared resource.

For a single-CPU system with interrupts, the simplest mechanism is to **disable
interrupts** around the access:

```c
__disable_irq();                  /* enter critical section */
GPIOF->DATA |= LED_GREEN;
__enable_irq();                   /* exit critical section  */
```

> The section of code between disabling and enabling interrupts is called a
> **critical section**.

### The cost is tiny

| Intrinsic | Instruction | Cycles |
|---|---|---|
| `__disable_irq()` | `CPSID i` | 1 |
| `__enable_irq()` | `CPSIE i` | 1 |

**One machine instruction each, with no function-call overhead** — the beauty of
intrinsic functions.

### What it achieves

The critical section **serializes** access to the shared resource and makes it
**atomic** (indivisible). The three pieces of code — the ISR and the two critical
sections — can now run **before or after each other, but never in the middle**.
That is what mutually exclusive access means.

### The interrupt is not lost

Pending the interrupt inside a critical section doesn't discard it. **It fires as
soon as interrupts are re-enabled** — merely *delayed*, not dropped.

## 5. Strategy 2 — Don't share the resource at all (better)

> **Better than mutual exclusion is to avoid race conditions by not sharing any
> resources in the first place.**

This is exactly what the TM4C123 `DATA_Bits` array (Lesson 7) is for:

```c
GPIOF_AHB->DATA_Bits[LED_GREEN] = LED_GREEN;   /* one atomic STR */
GPIOF_AHB->DATA_Bits[LED_GREEN] = 0;           /* one atomic STR */
```

Every combination of the 8 GPIO bits has its **own register**, so
`DATA_Bits[LED_GREEN]` and `DATA_Bits[LED_BLUE]` are **different registers**.
Nothing is shared — and there is **no read-modify-write sequence** at all, just a
single atomic write.

> **This is why TI's hardware engineers designed the GPIO registers in that
> peculiar, complex way**: to separate the GPIO bits, avoid sharing, and thereby
> eliminate potential race conditions in software. They did the heavy lifting in
> hardware so the software designer's life is easier.

---

## Key takeaways

1. A race condition = preemptible code sharing a resource, where the outcome
   depends on timing.
2. Read-modify-write is the classic vulnerable pattern: the update can be lost.
3. Races are intermittent and may survive weeks of testing — then fail in the
   field.
4. **Mutual exclusion** via a critical section (`__disable_irq()` /
   `__enable_irq()`) costs one cycle each and makes the access atomic.
5. Interrupts blocked during a critical section are **delayed, not lost**.
6. **Best of all: eliminate the sharing.** Hardware that gives each bit its own
   address makes the operation atomic by construction.

---

## Glossary

| Term | Meaning |
|---|---|
| Race condition | Outcome depends on the order of preempting accesses |
| Shared resource | Data or register accessed by more than one context |
| Read-modify-write | Load, alter, store — interruptible in the middle |
| Critical section | Code between disabling and re-enabling interrupts |
| Mutual exclusion | Only one context at a time may access the resource |
| Atomic | Indivisible; cannot be interrupted part-way |
| `CPSID i` / `CPSIE i` | Cortex-M instructions disabling/enabling interrupts |
| Intrinsic function | Compiler built-in mapping to a single instruction |

---

## Pitfalls to remember

- **Assuming `|=` and `&=` are atomic.** They are read-modify-write sequences.
- **"It works when I step through it."** Single-stepping suppresses interrupts —
  exactly the conditions under which races don't happen.
- **Testing is not proof.** A narrow window may not open for weeks.
- **Long critical sections** — they delay *every* interrupt and hurt real-time
  responsiveness.
- **Forgetting to re-enable interrupts** on an early return from a critical
  section.
- **Nesting critical sections naively** — an inner `__enable_irq()` re-enables
  interrupts the outer section wanted blocked.

---

**Next:** Lesson 21 covers the **foreground/background ("superloop")
architecture**, interrupt prioritization, and other ways of disabling interrupts
on Cortex-M.
