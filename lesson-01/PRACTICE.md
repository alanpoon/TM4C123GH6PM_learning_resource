# Lesson 1 — Practice: Counting in the Debugger

**Concepts behind this exercise:** [KNOWLEDGE.md](KNOWLEDGE.md)
**Video:** <https://youtu.be/gQOv8o5lS2k>

---

## Projects in this lesson

| Directory | Toolchain | Target |
|---|---|---|
| `simulator-iar/` | IAR EWARM (`workspace.eww`) | Instruction-set simulator |
| `simulator-keil/` | KEIL MDK (`lesson.uvprojx`) | Instruction-set simulator |

No board is required for this lesson — the simulator is enough. The optional
last step runs the same code on the TivaC LaunchPad.

## The code

`main.c` is deliberately trivial:

```c
int main(void) {
    int volatile counter = 0;
    ++counter;   /* ...repeated 21 times... */
    return 0;
}
```

> `volatile` keeps the compiler from optimizing the increments away so you can
> watch every one of them. It is explained properly in Lesson 5.

---

## Step-by-step

### 1. Open and build

- **KEIL:** open `simulator-keil/lesson.uvprojx`, then **Project → Build**
  (or `F7`).
- **IAR:** open `simulator-iar/workspace.eww`, then **Project → Make** (`F7`).

Build must report **0 errors, 0 warnings**.

> **Toolchain settings that matter here**
> - Device: `LM4F120H5QR` (TM4C123 family).
> - C dialect: **C99** — used throughout this course.
> - Optimization: **low / none**. The first few lessons do not survive high
>   optimization levels.
> - Editor: indent **4 spaces, no tabs** (tabs render differently everywhere).

### 2. Compile early, compile often

Type something illegal on purpose, press `F7`, and double-click the error — the
IDE jumps to the offending line. Undo it and rebuild. Treat the compiler as a
reviewer watching over your shoulder; give it frequent chances to help.

### 3. Start the debugger

Use **Download and Debug** (IAR) or **Start/Stop Debug Session** (KEIL, `Ctrl+F5`).
Arrange these four views so all are visible at once:

- **Disassembly** — the machine instructions
- **Memory**
- **Registers**
- **Locals**

### 4. Read the disassembly

- The highlighted instruction is where the CPU is stopped — the start of `main`.
- The left column holds the **addresses** of the instructions.
- The instruction numbers themselves are the code; the mnemonics are decoration.

### 5. Find the instructions in the memory view

Set the memory view to display **2×Units** (2-byte groups), because most
Cortex-M instructions are 2 bytes wide. You should now be able to match numbers
in the memory view against the disassembly view at the same addresses.

### 6. Single-step and watch

Step one line at a time (**Step Into**) and observe:

- `counter` changes in the **Locals** view.
- **PC** advances in the **Registers** view.
- The Locals view names the register holding `counter` (e.g. `R1`); confirm the
  same value appears in that register.

### 7. Watch decimal and hex diverge

Keep stepping until `counter` reaches **10**. Locals shows `10` (decimal) while
the register shows `A` (hex). Same bits, two notations.

### 8. Force an overflow

In the debugger, click the `counter` value and type a new one — you can change
any variable at runtime:

1. Set `counter` to `0x7FFFFFFF`, press Enter. Note the large positive decimal.
2. Step once. `counter` becomes a large **negative** number and the register
   reads `0x80000000`. That is two's complement overflow.
3. Set `counter` to `-1`, note the register reads `0xFFFFFFFF`.
4. Step once — back to `0`. The cycle is closed.

### 9. Exit

Stop the debug session.

---

## Homework

> **Study counting of unsigned integers.**
> Change the type of `counter` from `int` to `unsigned int`, rebuild, and repeat
> steps 6–8.
>
> Questions to answer for yourself:
> - What is the largest value `unsigned int` reaches before wrapping?
> - At `0x7FFFFFFF + 1`, does the value still go negative? Why not?
> - Which bit pattern does `-1` correspond to for a signed vs. an unsigned type?

### Extra credit

- Change the initial value and predict the register contents *before* stepping.
- Switch the memory view between 1×, 2× and 4× units and explain what changes.

---

## Optional: run it on the board

1. **Project → Options → Debugger**, select **TI-Stellaris** (IAR) or the
   **Stellaris ICDI** driver (KEIL).
2. Under the **Download** tab, check **Use flash loader** and **Verify download**.
3. Connect the EK-TM4C123GXL LaunchPad via USB. On first connection allow a
   minute or two for the USB debugger driver to install.
4. Download and debug exactly as in the simulator.

> **Note:** the board ships with a demo program that blinks the LEDs.
> Flashing this lesson replaces it permanently — the board will count, not
> blink. Blinking comes back in Lesson 4.

---

## Self-check

- [ ] Project builds with 0 errors / 0 warnings
- [ ] I can point to an instruction in the disassembly view and find the same
      number in the memory view
- [ ] I can say which register holds `counter` and read its value in hex
- [ ] I reproduced the positive → negative overflow at `0x7FFFFFFF`
- [ ] I repeated the experiment with `unsigned int` and can explain the difference
