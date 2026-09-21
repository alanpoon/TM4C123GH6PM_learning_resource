# Lesson 16 — Knowledge: Interrupts Part 1 — What They Are and How They Work

**Video:** <https://youtu.be/jP1JymlHUtc> · **Transcript:** <https://www.state-machine.com/course/lesson-16.txt>
**Hands-on companion:** [PRACTICE.md](PRACTICE.md)

---

## The big idea

> Polling in a delay loop is like staying up all night counting clock ticks so
> you don't oversleep. Most people **set an alarm** instead — and then do
> something else.

Interrupts are the processor's alarm clock. They need **hardware on both ends**:
a peripheral that raises the alarm, and CPU hardware that listens for it.

---

## 1. Polling and busy-waiting

**Polling** is repeatedly checking for a condition in order to react to it. The
`delay()` function **busy-waits**: the CPU does nothing but check whether a
counter has reached zero.

Some polling is clever. Busy-wait polling over long periods is **one of the most
brain-dead** forms, because it ties up the CPU completely and makes it
unavailable for any other work.

## 2. What an interrupt is

An **interrupt** disrupts an ongoing activity and forces the processor to start
doing something else in response — exactly as the word suggests.

Interrupts aren't only for timeouts. Typical sources:

- a timer expiring
- a user pressing a button
- data arriving on a communication interface
- an analog-to-digital conversion completing

### Hardware is required on both sides

The alarm-clock analogy is precise:

| Alarm clock | Microcontroller |
|---|---|
| clock with alarm hardware to make noise | a **peripheral** that raises an interrupt |
| ears and auditory nerves to hear it | **CPU hardware** that samples the interrupt line |

> Software alone — although also necessary — is **not enough**.

## 3. The SysTick timer

The TM4C123 has many timers; **SysTick** (the System Timer) is the simplest. It
is a **hardware peripheral** — a separate block on the silicon — clocked by the
CPU clock, with three registers:

| Datasheet name | CMSIS name | Role |
|---|---|---|
| `STCURRENT` | `SysTick->VAL` | 24-bit **down-counter**, decrements once per CPU clock cycle |
| `STRELOAD` | `SysTick->LOAD` | **reload** value — sets the interval between interrupts |
| `STCTRL` | `SysTick->CTRL` | **control**: clock source, interrupt enable, counter enable |

How it runs autonomously:

1. `VAL` decrements every CPU clock cycle.
2. When it reaches **zero**, it can raise an **interrupt**.
3. It **automatically reloads** from `LOAD` and counts down again.

> The CMSIS register names differ from the datasheet names — annoying, but with
> three registers easy to match up.

### Configuring it

- **`CTRL`**: set bits **2, 1, 0** — clock source, interrupt enable, counter
  enable.
- **`VAL`**: *clear-on-write* — it clears whatever you write, so write `0`.
- **`LOAD`**: the interesting one.

### Computing the reload value

You need the CPU clock rate. The TM4C123's default is **16 MHz**, from the
on-board crystal (`Y2`) — you can read it off the crystal if your eyesight is
good. (The MCU can run much faster with different clock configuration.)

```c
#define SYS_CLOCK_HZ  16000000U

SysTick->LOAD = (SYS_CLOCK_HZ / 2U) - 1U;   /* half a second */
```

Two details:

- **The `-1`** accounts for SysTick counting **through zero** — without it you
  would count one clock too many.
- **Check the 24-bit range.** For long timeouts, verify the value fits in 24
  bits. 8,000,000-1 = `0x7A11FF` — it just fits.

> The CPU clock frequency is an important constant; `#define` it in one place.

## 4. How the CPU listens: preemption

Special CPU hardware **samples the interrupt line after every instruction**:

| Interrupt line | CPU does |
|---|---|
| low | fetch the next instruction in the pipeline |
| high | execute the **Interrupt Entry** instruction — this is **preemption** |

### Interrupts are asynchronous

Instructions are strictly synchronized to the CPU clock. **The interrupt line is
not.** It can change at any time, typically in the middle of an instruction —
**completely asynchronously** to program execution. Hence: *interrupts are
asynchronous to the executing program.*

### Interrupt entry is expensive

The **Interrupt Entry** instruction is often one of the **longest in the
instruction set**: at least **12 cycles** on ARM Cortex-M, versus 1 cycle for a
simple `MOV` or `ADD`.

## 5. The `PRIMASK` bit

Peripherals connecting "directly to the CPU's interrupt line" is a
simplification — there is more in between, and in particular **every CPU can
block interrupts in software**.

On ARM Cortex-M that is the **`PRIMASK`** bit, which must be **cleared** for
interrupts to reach the CPU:

```c
__enable_interrupt();   /* IAR intrinsic — clears PRIMASK */
```

> A missing `__enable_interrupt()` is one of the most common reasons a correctly
> configured interrupt never fires.

## 6. Toggling a bit — the XOR idiom

Restructuring the blinky loop to a single delay requires toggling rather than
setting/clearing:

```c
GPIOF_AHB->DATA_Bits[LED_RED] ^= LED_RED;   /* toggle */
```

From the XOR truth table: `0 ^ 1 = 1` and `1 ^ 1 = 0` — so XOR-ing with a mask
**flips** exactly those bits. This joins `|=` (set) and `&= ~` (clear) from
Lesson 6 as the third essential bit idiom.

## 7. Structure of an interrupt-driven program

```c
int main(void) {
    /* configure hardware, configure SysTick, __enable_interrupt() */
    while (1) {
        /* nothing — for now */
    }
}

void SysTick_Handler(void) {     /* the ISR, installed in the vector table */
    GPIOF_AHB->DATA_Bits[LED_RED] ^= LED_RED;
}
```

Two important points:

- **The ISR only toggles the LED.** The delay happens **outside the CPU**,
  handled autonomously by the SysTick peripheral. That is the entire win.
- **You cannot delete the `while (1)` loop**, even though it does nothing. **The
  CPU must spend its time somewhere** between interrupts. Later that loop will do
  useful work, or put the CPU to sleep (Lesson 52).

### Where shared definitions belong

LED pin masks and the clock frequency are needed by both `main.c` and `bsp.c`, so
they belong in **`bsp.h`** — the BSP's header. (The obsolete `delay.c`/`delay.h`
module gets retired at this point.)

---

## Key takeaways

1. Busy-wait polling wastes the entire CPU.
2. Interrupts need hardware at both ends — a peripheral and the CPU's sampling
   logic.
3. SysTick is a 24-bit autonomous down-counter with reload; `LOAD = ticks - 1`.
4. The CPU samples the interrupt line **after every instruction**; taking the
   interrupt is **preemption**.
5. Interrupts are **asynchronous** to program execution.
6. Interrupt entry is expensive — ≥12 cycles on Cortex-M.
7. `PRIMASK` must be cleared (`__enable_interrupt()`) or nothing happens.
8. `^=` toggles bits; the ISR does the work, the peripheral does the waiting.
9. Keep the `while (1)` loop — the CPU must be somewhere.

---

## Glossary

| Term | Meaning |
|---|---|
| Polling / busy-wait | Repeatedly checking a condition, doing nothing else |
| Interrupt | Hardware-forced change in the flow of control |
| ISR | Interrupt Service Routine — the handler |
| SysTick | Cortex-M system timer peripheral |
| Preemption | CPU hardware forcing Interrupt Entry after an instruction |
| Asynchronous | Not synchronized to instruction execution |
| `PRIMASK` | Cortex-M bit that blocks all interrupts when set |
| Clear-on-write | Register cleared by any write, whatever the value |

---

## Pitfalls to remember

- **Forgetting `__enable_interrupt()`** — PRIMASK blocks everything.
- **Forgetting the `-1`** in the reload value.
- **Overflowing SysTick's 24-bit range** on long intervals.
- **Deleting the empty `while (1)`** — the CPU needs somewhere to be.
- **Putting a long delay inside an ISR** — it must be short; the whole point is
  to free the CPU.
- **Defining board constants in `main.c`** where the ISR in `bsp.c` can't see them.

---

**Next:** Lesson 17 steps into the machine-level detail of preemption, and shows
how interrupts work on the MSP430 — which highlights what makes Cortex-M special.
