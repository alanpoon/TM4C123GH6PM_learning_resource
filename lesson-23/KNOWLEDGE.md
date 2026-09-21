# Lesson 23 — Knowledge: RTOS Part 2 — Automating the Context Switch

**Video:** <https://youtu.be/PKml9ki3178> · **Transcript:** <https://www.state-machine.com/course/lesson-23.txt>
**Hands-on companion:** [PRACTICE.md](PRACTICE.md)

---

## The big idea

Turn Lesson 22's manual procedure into code — the start of **MiROS**, your
**MI**nimal **R**eal-time **O**perating **S**ystem.

> Building a minimal but functional kernel from scratch teaches more than
> reverse-engineering a real product like FreeRTOS, where the extra complexity
> makes it **too easy to lose the big picture in the minutia**.

---

## 1. Representing a thread

```c
typedef struct {
    void *sp;        /* stack pointer -- extended in later lessons */
} OSThread;
```

In standard RTOS terminology this structure is the **Thread Control Block
(TCB)**.

### The `OS` prefix

Many RTOSes use such a prefix for two reasons:

1. It marks clearly which elements belong to the **operating system**.
2. It **reduces name collisions** in larger projects where someone else may pick
   the same name for something entirely different.

## 2. `OSThread_start()`

```c
typedef void (*OSThreadHandler)(void);   /* pointer to a function taking no
                                            arguments and returning void */

void OSThread_start(
    OSThread *me,                 /* "me" = pointer to the TCB */
    OSThreadHandler threadHandler,
    void *stkSto, uint32_t stkSize);
```

> The **`me`** naming convention comes from object-oriented programming in C
> (Lesson 29).

Its job is what you did by hand in Lesson 22:

1. **Start from the end of the stack memory** — the ARM stack grows high → low.
2. **Round the end address down to an 8-byte boundary**. The caller may not know
   about the alignment requirement, so the kernel must not assume it. Integer
   division by 8 followed by multiplication by 8 does it.
3. **Fabricate the interrupt stack frame** (xPSR with the THUMB bit, PC = the
   thread handler, …).
4. **Store the resulting top of stack** into the TCB's `sp` member.
5. Optionally **pre-fill the remaining stack** with a known pattern such as
   `0xDEADBEEF` — this makes the stack visible in a memory view and lets you
   determine **worst-case stack usage**.

## 3. Why PendSV

The context switch must happen **during the return from an interrupt**. Coding it
inside `SysTick_Handler` would mean adding it to **every ISR in the system** —
repetitious, and it would defeat one of Cortex-M's main benefits, that ISRs can
be **pure C functions**. (The switch itself **cannot be written in standard C**;
it needs CPU-specific assembly to build stack frames and manipulate SP.)

Cortex-M's solution: code the switch in **one** exception and trigger it from
anywhere.

> **PendSV** exists for exactly this purpose, and **virtually all Cortex-M RTOSes
> use it for context switching.** Note, though, that PendSV isn't magic — in
> principle any asynchronous exception would do.

| | |
|---|---|
| **ICSR** (Interrupt Control and State Register) | `0xE000ED04` |
| Bit to pend PendSV | **28** (`0x10000000`) |

## 4. Interrupt priorities

Triggering PendSV from inside SysTick initially makes **PendSV preempt the still
active SysTick** — the wrong order. You want SysTick to finish, *then* PendSV to
switch context.

| Register | Address |
|---|---|
| **SYSPRI3** | `0xE000ED20` — holds SysTick and PendSV priorities |

> **Cortex-M priorities work "backwards": a higher priority *number* means lower
> priority for preemption.**

So give **SysTick a high priority (0)** and **PendSV the lowest**.

### Only the high bits are implemented

Write `0xFF` into PendSV's priority byte and it reads back as **`0xE0`** on the
TM4C — Cortex-M implements priority only in the **highest-order bits**:

| MCU | Priority bits | `0xFF` reads back as |
|---|---|---|
| TivaC (TM4C) | 3 | `0xE0` |
| STM32 | 4 | `0xF0` |

> **Rule to remember: PendSV must have the lowest priority of all exceptions and
> interrupts. Writing `0xFF` achieves that on every Cortex-M variant.**
>
> Correspondingly, **application interrupts should avoid the lowest priority**,
> which is reserved for PendSV — hence SysTick is raised to 0.
>
> Further reading: *"Cutting Through the Confusion with Arm Cortex-M Interrupt
> Priorities"*.

### A portability note

Inside the kernel, the raw address of SYSPRI3 is used rather than the CMSIS
interface, so the code doesn't commit to a specific core (M0/M3/M4/M7) — **the
PendSV priority byte is at the same address in all of them**.

## 5. `OS_sched()` — trigger the switch

Two `volatile` pointers track the threads:

```c
OSThread * volatile OS_curr;   /* currently running */
OSThread * volatile OS_next;   /* to run next       */
```

> **Note the placement of `volatile`: *after* the asterisk.** That makes **the
> pointer** volatile. Before the asterisk would give a non-volatile pointer to a
> volatile struct — not what you want.

```c
void OS_sched(void) {
    /* ...choose OS_next (manual for now; round-robin in lesson 24)... */
    if (OS_next != OS_curr) {
        *(uint32_t volatile *)0xE000ED04 = (1U << 28);  /* pend PendSV */
    }
}
```

Only trigger when the next thread actually **differs** from the current one.

### Race conditions — the hardest part of writing an RTOS

> Guarding against race conditions is **the most difficult aspect of building an
> RTOS**. The `OS_curr`/`OS_next` pointers offer plenty of opportunities for
> them.

Two options:

| Option | Assessment |
|---|---|
| Disable interrupts **inside** `OS_sched()` | simple, but problematic |
| **Require the caller to already be in a critical section** | **preferred** — the scheduler often needs calling when interrupts are already disabled, so disabling/re-enabling again inside would be problematic |

So callers wrap it:

```c
__disable_irq();
OS_sched();
__enable_irq();
```

## 6. `PendSV_Handler` — the context switch itself

Written in assembly, but with a clever shortcut: **write it in C first, then
steal the compiler's disassembly as a starting point.**

The C sketch:

```
disable interrupts
if (OS_curr != (OSThread *)0) {       /* zero on the very first switch */
    push r4-r11 onto the current stack    <- can't be written in C
    OS_curr->sp = sp;                     <- fake 'sp' local; replace with real SP
}
sp = OS_next->sp;                         <- same trick
OS_curr = OS_next;
pop r4-r11 from the new stack             <- can't be written in C
enable interrupts
return
```

The first-time check matters: **`OS_curr` is zero out of reset**, because no
thread is running yet.

### Turning it into assembly

- Mark the function as assembly — KEIL Compiler 5 supports the **`__asm`**
  keyword applied to a function. (Non-standard, but most embedded compilers offer
  an equivalent.)
- In the disassembly, each line is *address*, *opcode*, *mnemonic*, *operands* —
  keep only the last two.
- `PUSH {r4-r11}` and `POP {r4-r11}` replace the C comments.
- Replace the compiler's fake `sp` register with the **real SP**.
- Add an assembly label (e.g. `PendSV_restore`) as the target of the `CBZ`
  branch.
- Symbols like `OS_curr`/`OS_next` need explicit **`IMPORT`** directives, or the
  assembler won't recognize them.

## 7. Tail-chaining — why the double interrupt is cheap

Exiting SysTick and entering PendSV *looks* expensive: an interrupt exit
normally pops 8 registers and an entry pushes 8.

> For **back-to-back interrupt processing**, the Cortex-M core **skips the
> popping and pushing** — a hardware optimization called **tail-chaining**. The
> overhead is comparable to a simple function call.

## 8. A real bug worth remembering

The first run ends in **`HardFault_Handler`** instead of the thread. Debugging
it:

1. LR is `0xFFFFFFF9` — correct (Lesson 18).
2. So the fault must be the **stacked PC** — and it starts with `0x2`, a **RAM**
   address, not ROM. Suspicious.
3. Look at the line producing it in `OSThread_start()`:

> **The bug:** `threadHandler` is **already a pointer to function**, so taking its
> address with `&` is wrong.

Remove the `&` and the thread starts correctly.

> The general debugging method here is worth more than the bug: work backwards
> from the symptom through the small number of things that could produce it.

---

## Key takeaways

1. A thread is represented by a **TCB** holding at minimum its stack pointer.
2. `OSThread_start()` aligns the stack, fabricates the frame, and records SP.
3. Pre-filling stacks with a pattern reveals **worst-case stack usage**.
4. Do the context switch in **PendSV** so no other ISR needs to know about it.
5. **PendSV must have the lowest priority**; write `0xFF` for portability.
6. Cortex-M priorities are **inverted**, and only the **high bits** are
   implemented.
7. Make the scheduler's pointers `volatile` — **after** the asterisk.
8. Require callers to hold a critical section rather than nesting one inside.
9. **Tail-chaining** makes the SysTick→PendSV hand-off nearly free.
10. `&function` on something that is already a function pointer is a bug.

---

## Glossary

| Term | Meaning |
|---|---|
| MiROS | The minimal RTOS built in this course |
| TCB | Thread Control Block |
| PendSV | Cortex-M exception intended for context switching |
| ICSR / SYSPRI3 | Interrupt Control and State / system priority register |
| Tail-chaining | Hardware skipping of stack traffic between back-to-back exceptions |
| `__asm` function | Compiler extension for a fully assembly-coded function |
| `IMPORT` | Assembler directive making a C symbol visible |
| Callback | Function declared by the RTOS but defined by the application |

---

## Pitfalls to remember

- **`volatile` before the asterisk** — qualifies the pointee, not the pointer.
- **Forgetting the `OS_curr == 0` check** on the very first switch.
- **Giving PendSV anything but the lowest priority.**
- **Assuming an 8-byte-aligned stack** from the caller.
- **Nesting critical sections** inside the scheduler.
- **`&threadHandler`** when `threadHandler` is already a function pointer.
- **Missing `IMPORT`** directives in hand-written assembly.

---

**Next:** Lesson 24 automates the *scheduling* decision with a **round-robin**
policy, turning MiROS into a working time-sharing system.
