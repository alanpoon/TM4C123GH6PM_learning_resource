# Lesson 4 — Practice: Blink the LED

**Concepts behind this exercise:** [KNOWLEDGE.md](KNOWLEDGE.md)
**Video:** <https://youtu.be/1Kjh0CAgnl4>

---

## Projects in this lesson

| Directory | Toolchain | Target |
|---|---|---|
| `tm4c123-keil/` | KEIL MDK (`lesson.uvprojx`) | EK-TM4C123GXL LaunchPad |
| `tm4c123-iar/` | IAR EWARM (`workspace.eww`) | EK-TM4C123GXL LaunchPad |
| `stm32c031-keil/` | KEIL MDK (`lesson.uvprojx`) | STM32 NUCLEO-C031C6 |

A board is strongly recommended from this lesson on. Without one you can still
follow every step in the simulator — the debugger views differ slightly and,
naturally, no LED lights up.

## Documents to have open

- **Board user manual** — [`resources/Tiva-C_Launchpad_User_Manual.pdf`](../resources/Tiva-C_Launchpad_User_Manual.pdf)
- **MCU datasheet** — [`resources/TM4C123GH6PM_Datasheet.pdf`](../resources/TM4C123GH6PM_Datasheet.pdf)

## The code you will end up with

```c
int main(void) {
    *((unsigned int *)0x400FE608U) = 0x20U;  /* clock for GPIOF     */
    *((unsigned int *)0x40025400U) = 0x0EU;  /* PF1,2,3 as outputs  */
    *((unsigned int *)0x4002551CU) = 0x0EU;  /* digital enable      */

    while (1) {
        *((unsigned int *)0x400253FCU) = 0x02U;   /* red LED on  */
        int counter = 0;
        while (counter < 1000000) { ++counter; }  /* delay        */

        *((unsigned int *)0x400253FCU) = 0x00U;   /* red LED off */
        counter = 0;
        while (counter < 1000000) { ++counter; }  /* delay        */
    }
}
```

Note there is no separate pointer variable — the cast is dereferenced directly.
The pointer type is `unsigned int` because ARM registers are unsigned.

---

## Step-by-step

### 1. Read the hardware documentation first

In the **board user manual**:
- Find the RGB **user LED** at the right edge of the board.
- Confirm it is connected to **GPIO**.
- Go to the **schematics** at the end. On page 1, see that the R/G/B elements
  are driven by transistors controlled by `LED_R`, `LED_G`, `LED_B`.
- Trace those signals up to the MCU pins: **PF1, PF2, PF3** — the "F" is
  GPIO Port F.

### 2. Configure the toolchain for the board

- Debugger: **TI Stellaris** (IAR) / **Stellaris ICDI** (KEIL).
- Download tab: check **Use flash loader**.
- IAR only: **TI Stellaris menu → "Reset will do system reset"**, so every run
  starts from a clean reset.
- **Rebuild all** — otherwise the toolchain may pull in the previous lesson's
  `main.c`.

### 3. Explore the address space in the debugger

Before writing any code, browse memory:

- Around address `0`: machine instructions → this is **Flash**.
- At `0x20000000`: your variables → **RAM** starts here.
- At `0x20008000`: RAM ends. The island is `0x8000` bytes = **32 KB**.
- Between the islands: holes the debugger cannot read — unmapped addresses.

### 4. Find GPIO Port F in the datasheet

Search the datasheet for **`memory map`**. In the table, locate the peripherals
region and scroll to **GPIO Port F**. Copy its base address.

Paste that address into the **Memory** view of the debugger.

> **It will look empty.** That is the lesson — the block is clock-gated off.

### 5. Find the clock-gating register

Search the datasheet for **`clock gating`**, then within that section search for
`GPIO`. You want the **RCGCGPIO** register:

- Base `0x400FE000`, offset **`0x608`** → address **`0x400FE608`**
- Read the bit-field description: **bit 5 enables the clock to GPIO Port F**

### 6. Wake up the peripheral by hand

Open a second memory view (**Symbolic Memory** in IAR) at `0x400FE608`, keeping
the original memory view on the GPIO Port F base address.

Edit the RCGCGPIO value to set bit 5 — that is **`0x20`** in hex — and press
Enter.

**Watch the GPIO-F register block come alive** in the other view.

### 7. Configure the pins by hand

Still in the debugger, still typing values directly into memory:

| Address | Value | Effect |
|---|---|---|
| `0x40025400` | `0x0E` | PF1, PF2, PF3 set as **outputs** (`0b1110`) |
| `0x4002551C` | `0x0E` | **digital function** enabled on those pins |

### 8. Light the LED by hand

The Port F **data register** is at `0x400253FC`. Type values into it:

| Write | Result |
|---|---|
| `0x02` | **red** LED on (bit 1) |
| `0x00` | LED off |
| `0x04` | **blue** LED on (bit 2) |
| `0x08` | **green** LED on (bit 3) |

> You have now driven hardware without writing a single line of code. Everything
> that follows is just automating these four writes.

### 9. Now write it in C

Convert each hand-typed write into a dereferenced pointer cast, as in the code
listing above. Build with `F7` after each line.

### 10. Wrap it in an endless loop

Add `while (1) { ... }` around the on/off sequence. The compiler warns that the
trailing `return` is unreachable — it is right; comment it out.

### 11. Single-step, then run — and hit the bug

Single-step: the clock-gating write wakes GPIO-F, the LED turns on, then off.
Everything looks correct.

Press **Go** to run at full speed → **the LED stays on**.

Break in and single-step again → correct again.

**Why?** The loop runs far too fast for the eye. The LED really is blinking, at
a rate that averages out to "on".

### 12. Add the delay loops

Insert a counting `while` loop after the on-write **and** after the off-write.
Tune the limit (start around `1000000`) until the blink is comfortable.

---

## Exercises

1. **Other colours.** Blink blue and green instead of red by changing the value
   written to the data register.
2. **Mixed colours.** What does `0x06` do? `0x0E`? Predict before you try.
3. **Asymmetric blink.** Make the LED on-time much shorter than the off-time.
4. **Skip a step.** Comment out the `DEN` write and rebuild. What happens, and
   what would you have concluded if you hadn't known about `DEN`?
5. **Datasheet drill.** Without looking at this file, find the base address of
   **GPIO Port A** and the offset of its `DIR` register.
6. **Measure it.** Time 10 blinks with a stopwatch and estimate how many CPU
   cycles one delay-loop iteration costs.

---

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| Peripheral registers read as `0`/dashes | clock not ungated (step 6) |
| LED never lights | `DIR` or `DEN` not configured |
| LED lights but never blinks | delay loops missing or too short |
| Old code seems to run | project not fully rebuilt (step 2) |
| Board behaves oddly after reset | "Reset will do system reset" not enabled |

See also [`resources/AN_Troubleshooting_TivaC.pdf`](../resources/AN_Troubleshooting_TivaC.pdf).

---

## Self-check

- [ ] I located the LED pins in the board schematics myself
- [ ] I found the memory map and the clock-gating register in the datasheet
- [ ] I lit the LED by typing into memory, before writing any C
- [ ] My C version blinks on the board (or runs correctly in the simulator)
- [ ] I can explain why full-speed execution appeared to leave the LED solid on
