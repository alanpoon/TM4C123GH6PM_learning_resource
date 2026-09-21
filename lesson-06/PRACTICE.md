# Lesson 6 — Practice: Blinking Red While Blue Stays On

**Concepts behind this exercise:** [KNOWLEDGE.md](KNOWLEDGE.md)
**Video:** <https://youtu.be/7Iru_LM3qY0>

---

## Projects in this lesson

| Directory | Toolchain | Target |
|---|---|---|
| `simulator-iar/` | IAR EWARM | Simulator — for the operator experiments |
| `tm4c123-keil/` | KEIL MDK (`lesson.uvprojx`) | EK-TM4C123GXL LaunchPad |
| `tm4c123-iar/` | IAR EWARM (`workspace.eww`) | EK-TM4C123GXL LaunchPad |
| `stm32c031-keil/` | KEIL MDK (`lesson.uvprojx`) | STM32 NUCLEO-C031C6 |

## The problem to solve

Turn the **blue** LED on and keep it on, while the **red** LED blinks. The naive
approach fails immediately:

```c
GPIO_PORTF_DATA_R = LED_BLUE;   /* blue on  */
while (1) {
    GPIO_PORTF_DATA_R = LED_RED;   /* red on — and blue just went OFF */
    ...
}
```

All the LED bits share one register, so a plain assignment clobbers the others.

---

## Part A — Explore the operators in the simulator

The project keeps this experiment in a commented-out block at the top of
`main.c`. Uncomment it to work through Part A.

### 1. Set up

- Optimization level → **None**
- Debugger → **Simulator** (no board needed for this part)

```c
unsigned int a = 0x5A5A5A5A;
unsigned int b = 0xDEADBEEF;
unsigned int c;

c = a | b;   // OR
c = a & b;   // AND
c = a ^ b;   // XOR
c = ~b;      // NOT
c = a << 1;  // left shift
c = a << 2;
c = b >> 1;  // right shift
c = b >> 3;
```

### 2. Watch in binary

Build, debug, step over the initializations, then **switch the Locals view to
binary format**. This is the whole point — you must *see* the bits.

Step over each expression and verify the result against the truth tables. For
each one, check the disassembly and note that **a single instruction** does all
32 bits:

| Expression | Instruction |
|---|---|
| `a \| b` | `ORRS` |
| `a & b` | `ANDS` |
| `a ^ b` | `EORS` |
| `~b` | `MVNS` (move-negative) |
| `a << n` | `LSLS` |
| `b >> n` | `LSRS` |

### 3. Observations to confirm yourself

- `LSRS` shifts **zeros** into the most significant bits.
- `LSLS` shifts **zeros** into the least significant bit.
- `b >> 1` equals `b / 2`; `a << 3` equals `a * 8` — verify with a calculator.
- With a large left operand, left-shifting makes high bits **fall off the left
  edge**: the result no longer fits in 32 bits.

### 4. The signed/unsigned shift experiment

```c
int x = 1024;
int y = -1024;
int z;

z = x >> 3;   /* positive: zeros shifted in */
z = y >> 3;   /* negative: ONES shifted in  */
```

Step through and confirm:

- `x >> 3` == `128` == `1024 / 8` — zeros at the top.
- `y >> 3` == `-128` == `-1024 / 8` — **ones** at the top.
- In the disassembly, the signed shifts use **`ASRS`** (arithmetic) while the
  unsigned ones use **`LSRS`** (logical).

That sign extension is exactly what keeps right-shift equivalent to division by
a power of two for negative numbers.

---

## Part B — Apply it to blinky

### 5. Define the LED bit masks

```c
#define LED_RED   (1U << 1)
#define LED_BLUE  (1U << 2)
#define LED_GREEN (1U << 3)
```

Replace the hex literals throughout. The code becomes self-explanatory — the
comments explaining which bit is which become redundant and can go.

### 6. Rewrite every register write as a set-bit idiom

Every line in your initialization is really "set some bits", so code it that way
(after confirming in the datasheet that these registers are **R/W**):

```c
SYSCTL_RCGCGPIO_R |= (1U << 5);                        // clock for GPIOF
GPIO_PORTF_DIR_R  |= (LED_RED | LED_BLUE | LED_GREEN);
GPIO_PORTF_DEN_R  |= (LED_RED | LED_BLUE | LED_GREEN);
```

### 7. Start from a known state, then solve the problem

```c
GPIO_PORTF_DATA_R &= ~(LED_RED | LED_BLUE | LED_GREEN);  // all off
GPIO_PORTF_DATA_R |= LED_BLUE;                            // blue on, stays on

while (1) {
    GPIO_PORTF_DATA_R |= LED_RED;    // red on — blue untouched
    int volatile counter = 0;
    while (counter < 1000000) { ++counter; }

    GPIO_PORTF_DATA_R &= ~LED_RED;   // red off — blue untouched
    counter = 0;
    while (counter < 1000000) { ++counter; }
}
```

### 8. Run it on the board

Set optimization back to **High** and the debugger to **TI Stellaris** /
Stellaris ICDI. Load and run.

Expected: **blue on continuously, fainter red blinking on top of it.**

### 9. Inspect what the compiler produced

Break in and set breakpoints on the set and the clear.

- **Setting** the bit: a load-modify-store sequence using **`ORRS`**.
- **Clearing** the bit: a load-modify-store using a single **`BIC`**
  (bit-clear) instruction — the compiler did *not* literally emit
  "complement then AND". It recognized the idiom and produced better code.

> This is worth pausing on: idiomatic code communicates *intent*, and the
> compiler optimizes intent.

---

## Exercises

1. **Toggle.** Replace the set/clear pair with a single XOR toggle
   (`GPIO_PORTF_DATA_R ^= LED_RED;`). Does the blink still work? What does the
   disassembly look like?
2. **Cycle the colours.** Blink red, then blue, then green in sequence, with the
   others off each time.
3. **Purple.** Keep red and blue on together while green blinks.
4. **Interview drill.** Without running it, predict `(-1) >> 1` and
   `((unsigned)-1) >> 1`. Then check.
5. **Mask arithmetic.** Write a macro `BIT(n)` and redefine the LED masks with
   it. Confirm the generated code is identical.
6. **Clobber test.** Deliberately use `=` instead of `|=` for the red LED and
   watch the blue LED die. Explain in one sentence why.
7. **Race preview.** The read-modify-write sequence takes three instructions.
   Imagine an interrupt landing between the load and the store — what would be
   lost? (Lesson 7 solves this; Lesson 20 names it.)

---

## Self-check

- [ ] I watched all six operators in the Locals view in binary format
- [ ] I can explain `LSRS` vs `ASRS` and when each is generated
- [ ] My LED masks are defined as `(1U << n)`
- [ ] Blue stays lit while red blinks
- [ ] I found the `BIC` instruction the compiler generated for the clear idiom
- [ ] I can write both bit idioms from memory
