# Lesson 17 — Practice: Triggering an Interrupt at Will (MSP430)

**Concepts behind this exercise:** [KNOWLEDGE.md](KNOWLEDGE.md)
**Video:** <https://youtu.be/o_UaEcbfodo>

---

## Projects in this lesson

| Directory | Toolchain | Target |
|---|---|---|
| `tm4c123-iar/` | IAR EWARM | EK-TM4C123GXL LaunchPad |
| `msp430-iar/` | **IAR EW for MSP430** | **MSP-EXP430G2 LaunchPad** |
| `ek-tm4c123gxl/` | — | TM4C123 device support |

> **Start from the provided `lesson-17` project, not a copy of lesson 16.** It
> uses a different TM4C header that is more compatible with the current CMSIS.
> Everything else is identical to the end of Lesson 16.

> **This lesson needs a second board.** The MSP-EXP430G2 LaunchPad and the IAR
> toolset for MSP430 (a different product from EWARM; a free KickStart edition
> exists). Without them, follow along — the observations are all recorded below.

---

## Part A — An ISR that is just a C function (ARM)

### 1. Call the ISR directly

Add a direct call from `main()`:

```c
SysTick_Handler();     /* calling an interrupt handler like any function */
```

### 2. Give the interrupt something to preempt

An empty `while (1)` is a single branch-to-self — poor material for studying
preemption. Add a couple of instructions:

```c
while (1) {
    GPIOF_AHB->DATA_Bits[LED_GREEN] = LED_GREEN;   /* green on  */
    GPIOF_AHB->DATA_Bits[LED_GREEN] = 0;           /* green off */
}
```

Visually the green LED **glows at about half intensity** — it blinks far too
fast for the eye.

### 3. Observe both invocation paths

Breakpoints at the direct call and at the top of `SysTick_Handler`. Run.

- The direct call happens by an ordinary **`BL`**.
- The handler toggles the red LED correctly.
- It returns flawlessly by the standard **`BX LR`**.
- Continue: the same breakpoint is hit again — this time reached through
  **interrupt preemption**.

> **This is unusual.** No other processor lets an interrupt handler be a plain C
> function. The rest of this lesson shows why.

---

## Part B — The MSP430 comparison

### 4. Look at the MSP430 blinky

Structurally identical to the ARM version: configure LED pins, set up a periodic
timer interrupt, enable interrupts, and a `while (1)` rapidly toggling an LED.

The handler, however, reads:

```c
#pragma vector = TIMER0_A0_VECTOR
__interrupt void Timer0_Handler(void) {
    ...
}
```

- **`__interrupt`** tells the compiler to generate ISR entry/exit code.
- **`#pragma vector`** installs it in the vector table automatically — simpler
  than Cortex-M's explicit table.
- Both are **non-standard**, specific to IAR **and** MSP430. `Timer0_Handler`
  **cannot be called directly**.

### 5. Configure the debugger

In the MSP430 project, make sure the **FET debugger does not use software
breakpoints**, so it uses the MCU's **hardware breakpoints**. This MSP430
variant has **two** — exactly what the experiment below needs.

### 6. Sanity-check the program

Run: the red LED blinks once per second, the green LED glows. Break in: the
program is in the `while (1)` loop, and single-stepping visibly toggles green.
Breakpoint in `Timer0_Handler`: it is hit, and the LED toggles each time.

---

## Part C — Trigger the interrupt at a chosen instruction

### 7. Understand Timer0

| Register | Role |
|---|---|
| `TA0R` | the counter — an **up**-counter (unlike SysTick, which counts down) |
| `TA0CCR0` | the limit; on match, reset to 0 and raise the interrupt |
| control | clock source and divider — here **SMCLK ÷ 8** |

The ÷8 divider exists because half a second of ticks must fit in **16 bits**.

**The trick:** write `TA0R` a value **one less** than `TA0CCR0`. The next clock
cycle makes them match — firing the interrupt exactly where you want it.

### 8. Set up the two-path experiment

1. Run free, then break in — you land in the `while (1)` loop, stopped at a `BIS`
   instruction.
2. Open the Timer0 registers, click `TA0R`, and enter **`0xF422`** (one less than
   `TA0CCR0`).
3. Set **two** breakpoints:
   - on the **very next `BIC` instruction** — hit if **no** preemption occurs;
   - inside **`Timer0_Handler`** — hit only if the interrupt preempts **right
     here**.

### 9. Run free — don't single-step

> **Single-stepping disables the check for interrupts after each instruction.**
> You must let the program run free to settle the question.

**Result: the interrupt fires.** You are inside `Timer0_Handler`.

### 10. Step through the ISR

- The `XOR` instruction toggles the red LED.
- The next instruction is **`RETI`** — the special **return from interrupt**.
- Execution returns to the **`BIC`** instruction, where your other breakpoint
  still sits.

> You have created **interrupt preemption at will**. Like fault injection
> (Lesson 15), this technique finds elusive, intermittent bugs — instead of
> waiting for a rare event, you cause it on demand, repeatedly.

---

## Part D — How does the interrupt know where to return?

### 11. Watch the stack

Reset and set the experiment up again, but this time also watch:

- the **CPU registers**, especially **SP**;
- the **memory view** at the stack, in **2-byte chunks** (MSP430 is 16-bit).
  Highlight the current SP.

### 12. Identify the stack frame

On entry, **SP drops from `0x3FE` to `0x3FA`** — 4 bytes, two entries.

The MSP430 datasheet's **Interrupt Processing** section says entry pushes **PC**
and **SR**. So in the memory view:

| Value | Is |
|---|---|
| `0x000D` | the saved **SR** |
| `0xC038` | the saved **PC** — the return address |

Scroll the disassembly to `0xC038` and confirm it is the **`BIC`** instruction
inside `while (1)`.

### 13. Note the disabled interrupts

After being saved, the **SR is cleared** on entry — which clears the **GIE
(Global Interrupt Enable)** bit, **disabling further interrupts**.

`RETI` does the exact opposite: it restores **SR and PC**, returning to the
preemption point *and* re-enabling interrupts via the restored GIE.

---

## Part E — Why an ISR saves more registers

### 14. Build the comparison

Restructure the code so an ISR and a regular function have **identical bodies**:

1. Move the ISR's body into a regular function `LED_toggle()`.
2. Call `LED_toggle()` from `Timer0_Handler` instead of toggling directly.
3. Make **another copy** of the handler as a **regular** function
   `Timer0_Function()` — the control sample.
4. Call `Timer0_Function()` from `main()`, or the linker will eliminate it.
5. Put all the new prototypes in `bsp.h`.

### 15. Compare the generated code

Breakpoints in both, and look at the disassembly:

| Function | Code |
|---|---|
| `Timer0_Function()` (regular) | **one instruction** — branch to `LED_toggle()` |
| `Timer0_Handler()` (ISR) | `PUSH R13/R12/R15`, `PUSH R14` → `CALL LED_toggle` → `POP` in exact reverse order → **`RETI`** |

Identical source. Completely different code.

### 16. Work out why

A call from inside the ISR can clobber **R12–R15**, so the ISR must preserve
them — otherwise interrupt preemption would have the side effect of corrupting
registers.

> **An interrupt can preempt asynchronously between any two instructions**, so
> the compiler cannot tolerate clobbering. A regular call is **synchronous** —
> the compiler emits it and knows exactly which registers are at risk there.

That is the problem Cortex-M had to solve to make plain C functions usable as
ISRs. Lesson 18 shows how.

---

## Exercises

1. **Pick your instruction.** Trigger the interrupt at three different points in
   the loop and confirm the saved PC changes accordingly.
2. **Prove single-stepping hides it.** Set `TA0R` as in step 8 but single-step
   instead of running. Which breakpoint is hit?
3. **Nested interrupts?** With GIE cleared on entry, what happens if the timer
   expires again while the ISR is running? Predict, then test.
4. **Grow the ISR.** Add local variables to `Timer0_Handler` and see how the
   push/pop list changes.
5. **Call the ISR directly** on MSP430. What does the compiler say?
6. **ARM contrast.** Do step 15's comparison on the ARM project — an ISR and a
   regular function with the same body. What differs? (Predict; Lesson 18
   confirms.)
7. **Frame arithmetic.** How many bytes of stack does one MSP430 interrupt cost,
   including the ISR's own pushes?

---

## Self-check

- [ ] I called `SysTick_Handler()` directly on ARM and it worked
- [ ] I can explain what `__interrupt` and `#pragma vector` do, and why they are
      not standard C
- [ ] I triggered the MSP430 timer interrupt at an instruction of my choosing
- [ ] I know why single-stepping cannot be used to observe preemption
- [ ] I identified the saved PC and SR on the MSP430 stack
- [ ] I know that entry clears GIE and `RETI` restores it
- [ ] I compared an ISR against a regular function with the same body and can
      explain the extra pushes
