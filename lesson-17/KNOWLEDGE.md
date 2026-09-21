# Lesson 17 — Knowledge: Interrupts Part 2 — How Most CPUs Handle Them

**Video:** <https://youtu.be/o_UaEcbfodo> · **Transcript:** <https://www.state-machine.com/course/lesson-17.txt>
**Hands-on companion:** [PRACTICE.md](PRACTICE.md)

---

## The big idea

Your `SysTick_Handler` turned out to be an **ordinary C function** — you can even
call it directly. That is a **unique feature of ARM Cortex-M**. To appreciate
why, you have to see how interrupts work on a *typical* processor: the MSP430.

---

## 1. On Cortex-M, an ISR is just a C function

`SysTick_Handler` can be invoked two completely different ways:

| Invocation | Mechanism |
|---|---|
| `SysTick_Handler();` from `main()` | ordinary `BL` call, ordinary `BX LR` return |
| by the timer expiring | **interrupt preemption** through the vector table |

Both work, and the handler behaves identically.

> **No other processor allows interrupt handlers to be regular C functions.**

## 2. What ISRs normally require

On most processors an ISR differs from a regular function in **two** ways:

1. **A special return instruction** — a "return from interrupt" (`RETI` on
   MSP430), because it must pop different registers than a normal return.
2. **Saving more CPU registers** than a regular function.

They also often need special **entry code**. Hence they cannot be standard C
functions.

## 3. MSP430 syntax — and why it isn't C

```c
#pragma vector = TIMER0_A0_VECTOR     /* assigns the ISR to a vector table slot */
__interrupt void Timer0_Handler(void) {
    ...
}
```

- **`__interrupt`** tells the IAR compiler this is not an ordinary function but
  an **ISR** — generating the special entry/exit code.
- **`#pragma vector`** automatically installs it in the MSP430 vector table — a
  much simpler mechanism than Cortex-M's explicit table.

> Both go **beyond standard C** and are specific to **both the IAR toolset and
> the MSP430 processor**. `Timer0_Handler` is a **non-standard C function that
> you cannot call directly**.

## 4. The MSP430 Timer0 — and how to fake its expiry

Three registers, similar in role to SysTick but only **16 bits** wide:

| Register | Role |
|---|---|
| `TA0R` | the counter — an **up-counter** |
| `TA0CCR0` | the limit; on match, reset to 0 and raise the interrupt |
| control | clock source, divider, mode |

The blinky example clocks it from **SMCLK ÷ 8** — a large divisor needed to fit
half a second's worth of ticks into 16 bits. (Half a second is an awfully long
time for an MCU, even one running at 1 MHz.)

**The trick:** write into `TA0R` a value **one less** than `TA0CCR0`. The next
clock cycle makes them match and **triggers the interrupt** at an instruction of
your choosing.

> Note the contrast with SysTick, which **counts down**; Timer0 **counts up**.

### Triggering interrupts at will

> This technique — like **fault injection** in Lesson 15 — is **invaluable**. It
> lets you track down the most elusive, intermittent problems, because instead of
> waiting forever for a rare event you can **make it happen at will, any number
> of times**.

### Single-stepping hides interrupts

**You cannot single-step to observe preemption** — single-stepping *disables*
the check for interrupts after each instruction. You must let the program **run
free**, with breakpoints on both possible paths:

- the **next instruction** (hit if no preemption occurs), and
- inside the **ISR** (hit only if the interrupt preempted at that exact point).

## 5. The MSP430 interrupt stack frame

On interrupt entry the SP drops by **4 bytes** — two 16-bit entries. The
datasheet's *Interrupt Processing* section confirms the hardware pushes:

| Pushed | Meaning |
|---|---|
| **PC** | the return address — the preemption point |
| **SR** (Status Register) | the CPU flags |

That is **how the interrupt knows where to return to**.

Additionally, on entry the **SR is cleared**, which among other things clears the
**GIE (Global Interrupt Enable)** bit — **disabling further interrupts**.

**`RETI`** does the exact opposite: it restores **SR and PC**, returning to the
precise preemption point and re-enabling interrupts via the restored GIE.

## 6. Why ISRs must save more registers

Give the ISR more work — e.g. calling a regular function — and compare it
against an ordinary function with an *identical body*:

| Function | Generated code |
|---|---|
| `Timer0_Function()` (regular) | **one instruction**: branch to `LED_toggle()` |
| `Timer0_Handler()` (ISR) | `PUSH R12–R15` → `CALL LED_toggle` → `POP R12–R15` → **`RETI`** |

Identical source, completely different code. Why?

> **An interrupt can preempt asynchronously between *any* two instructions.** The
> compiler cannot tolerate an interrupt having the side effect of clobbering
> registers, so the ISR must preserve everything a call might clobber.

Contrast with an ordinary call: it is **synchronous**. The compiler emits the
`CALL` itself, so it *knows* at that exact point that certain registers may be
clobbered, and has planned around it.

This is the crux, and it is exactly what Lesson 18 shows Cortex-M solving in
hardware.

## 7. Debugger note: hardware breakpoints

For this experiment the MSP430 FET debugger must be configured **not to use
software breakpoints**, so it uses the MCU's built-in **hardware breakpoints**.
The MSP430 variant used has **two** — exactly what the two-path experiment needs.

---

## Key takeaways

1. On Cortex-M an ISR is an ordinary C function, callable directly — unique
   among processors.
2. Typical ISRs need special entry code, a special return instruction, and extra
   register saving.
3. MSP430 uses `__interrupt` and `#pragma vector` — non-standard extensions.
4. MSP430 interrupt entry pushes **PC and SR**; `RETI` restores them.
5. Interrupt entry **clears GIE**, disabling further interrupts.
6. ISRs save more registers because preemption is **asynchronous**; regular calls
   are synchronous and planned for.
7. Triggering interrupts at will is a first-class debugging technique.
8. **Single-stepping suppresses interrupts** — run free to observe preemption.

---

## Glossary

| Term | Meaning |
|---|---|
| ISR | Interrupt Service Routine |
| `__interrupt` | IAR keyword marking a function as an ISR (non-standard) |
| `#pragma vector` | MSP430 directive installing an ISR in the vector table |
| `RETI` | Return-from-interrupt instruction |
| SR | MSP430 Status Register |
| GIE | Global Interrupt Enable bit |
| Interrupt stack frame | Registers the hardware pushes on interrupt entry |
| Synchronous / asynchronous | Occurring at a known point / at any point |

---

## Pitfalls to remember

- **Single-stepping to observe an interrupt** — it won't fire.
- **Calling an `__interrupt` function directly** on a non-Cortex-M processor.
- **Assuming interrupts stay enabled inside an ISR** — on MSP430 GIE is cleared.
- **Forgetting ISRs cost more than regular functions** — the register saving is
  real.
- **Software breakpoints** interfering with timing-sensitive experiments.

---

**Next:** Lesson 18 shows how Cortex-M solves both problems — extra register
saving and the special return — so that plain C functions can serve as ISRs.
