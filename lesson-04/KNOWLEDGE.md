# Lesson 4 — Knowledge: Talking to the Outside World (GPIO)

**Video:** <https://youtu.be/1Kjh0CAgnl4> · **Transcript:** <https://www.state-machine.com/course/lesson-04.txt>
**Hands-on companion:** [PRACTICE.md](PRACTICE.md)

---

## The big idea

From the CPU's point of view, **controlling hardware is just writing numbers to
specific addresses**. The pointer-cast "hack" from Lesson 3 is the whole
mechanism. What remains is knowing *which* addresses — and that knowledge lives
in the board manual and the MCU datasheet.

---

## 1. Memory-mapped I/O

The ARM Cortex-M address space is a **single, flat, linear 32-bit space** — no
segments, no memory banks, no paging (a real relief compared to older 8-bit
architectures). Different "islands" in that space are mapped to different
things:

| Address range (TM4C123GH6PM) | Contents |
|---|---|
| `0x00000000` – `0x0003FFFF` | On-chip **Flash** (256 KB) — your code |
| `0x20000000` – `0x20007FFF` | On-chip **SRAM** (32 KB) — your variables |
| `0x40000000` – … | **Peripherals**, including the GPIO ports |
| gaps between them | unmapped — the debugger shows holes |

**Peripheral registers are not memory** — they are hardware control points that
happen to be reachable at an address. Writing to one changes a voltage on a pin.

## 2. The datasheet is the map

The **datasheet** describes the memory map and every register in excruciating
detail. The TM4C123GH6PM datasheet is over 1200 pages — and that's *short* as
datasheets go.

> These documents are not meant to be read cover to cover. A large part of being
> an embedded engineer is knowing **how to search** a datasheet efficiently.

Practical search terms: `"memory map"`, `"clock gating"`, the register name.

The companion **board user manual** tells you how components are wired to the
MCU pins, and contains the **schematics**.

## 3. How a register is documented

Datasheet register descriptions follow a standard format:

- A picture of the register as a block of **bits, always numbered from 0**.
- The **access type** of each bit: `RO` (read-only), `R/W` (read-write),
  `WO` (write-only).
- Below the picture, descriptions of logically related bit groups, listed from
  the **most significant bit** downward.
- A **base address** plus an **offset** — the full register address is
  `base + offset`.

## 4. Clock gating — why a peripheral looks "empty"

If you point the debugger at a peripheral's advertised address range and see
nothing, the block is most likely **switched off**.

**Clock gating** is the standard power-saving technique of withholding the clock
signal from parts of the chip. A gated block is dormant: its registers do not
respond. Modern MCUs gate nearly every peripheral by default.

On the TM4C123, the register that ungates GPIO clocks is the **Run-Mode Clock
Gating Control Register for GPIO (RCGCGPIO)**:

```
address = 0x400FE000 (base) + 0x608 (offset) = 0x400FE608
bit 5 → enables the clock to GPIO Port F
```

> **Rule of thumb: enable the peripheral's clock before touching any of its
> registers.** This is the single most common cause of "my peripheral code does
> nothing".

## 5. Configuring a GPIO pin on the TM4C123

The RGB user LED on the EK-TM4C123GXL is driven by transistors controlled by
**GPIO Port F pins 1, 2 and 3**:

| Pin | Colour |
|---|---|
| PF1 | Red |
| PF2 | Blue |
| PF3 | Green |

Bits 1, 2, 3 together are `0b1110` = **`0x0E`**.

Three registers get you from "clock on" to "LED lit":

| Register | Address | Purpose |
|---|---|---|
| `RCGCGPIO` | `0x400FE608` | ungate the Port F clock (bit 5 = `0x20`) |
| `GPIO_PORTF_DIR` | `0x40025400` | pin **direction**: 1 = output |
| `GPIO_PORTF_DEN` | `0x4002551C` | **digital enable** for the pins |
| `GPIO_PORTF_DATA` | `0x400253FC` | the **data** register — read inputs, write outputs |

Writing `0x02` to the DATA register lights the red LED; `0x04` gives blue,
`0x08` green, `0x00` turns everything off.

> Note the DATA register address `...3FC` is not a typo — the TM4C123 GPIO data
> register uses an address-based bit-masking scheme, covered in Lesson 6.

## 6. The endless loop

```c
while (1) {
    /* ... */
}
```

`1` is always true, so the loop never exits. Embedded programs typically never
terminate — there is no operating system to return to. The compiler will warn
that any `return` after such a loop is **unreachable code**, which is correct.

## 7. Delay loops — and why you need them

A CPU running at tens of MHz toggles an LED far faster than an eye can see; the
LED appears continuously lit. Slowing down needs a **delay loop** — a counting
`while` that burns CPU cycles:

```c
int counter = 0;
while (counter < 1000000) {
    ++counter;
}
```

This is wasteful of cycles (every one of them), and the delay length depends on
clock speed and optimization level. It is a beginner's tool, not a real
technique — but it works, and it is honest about what it costs.

> A delay is needed **after turning the LED on *and* after turning it off** —
> otherwise the off-phase is invisible.

---

## Key takeaways

1. Hardware control = writing values to addresses. Nothing more exotic.
2. The datasheet's memory map tells you the addresses; the board manual tells
   you the wiring.
3. Register address = base + offset; bits are numbered from 0.
4. **Ungate the clock first**, or the peripheral is invisible.
5. GPIO needs direction + digital-enable configured before the data register
   does anything.
6. A single `STR` instruction is what actually changes the outside world.

---

## Glossary

| Term | Meaning |
|---|---|
| GPIO | General Purpose Input-Output |
| Memory-mapped I/O | Peripheral registers reachable as ordinary addresses |
| Clock gating | Withholding a clock from a block to save power |
| RCGCGPIO | Run-Mode Clock Gating Control register for GPIO |
| DIR / DEN / DATA | GPIO direction / digital-enable / data registers |
| Base + offset | How datasheets specify register addresses |

---

## Pitfalls to remember

- **Forgetting the clock gate.** The peripheral address range reads as empty and
  writes do nothing.
- **Configuring DIR but not DEN** (or vice versa). Both are required.
- **Expecting to see blinking at full speed.** Without a delay it looks solid-on.
- **Delay loops get optimized away** at higher optimization levels — the fix is
  `volatile`, in Lesson 5.
- **Magic numbers everywhere.** This lesson's code is deliberately unreadable;
  Lesson 5 cleans it up with macros.

---

**Next:** Lesson 5 replaces the cryptic addresses with preprocessor macros and
explains why the delay loops need `volatile`.
