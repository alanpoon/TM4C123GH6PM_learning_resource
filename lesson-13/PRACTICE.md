# Lesson 13 — Practice: Exploring the World Before `main()`

**Concepts behind this exercise:** [KNOWLEDGE.md](KNOWLEDGE.md)
**Video:** <https://youtu.be/zFAnW7Tzu4U>

---

## Projects in this lesson

| Directory | Toolchain | Target |
|---|---|---|
| `tm4c123-keil/` | KEIL MDK (`lesson.uvprojx`) | EK-TM4C123GXL LaunchPad |
| `tm4c123-iar/` | IAR EWARM (`workspace.eww`) | EK-TM4C123GXL LaunchPad |
| `stm32c031-keil/` | KEIL MDK (`lesson.uvprojx`) | STM32 NUCLEO-C031C6 |

> **A real board is required** for this lesson — the simulator will not show you
> the reset behaviour convincingly.

## Required project settings

1. **Debugger → un-check "Run to main"** — this is the whole point.
2. **Debugger → TI Stellaris / Stellaris ICDI**, with **Use flash loader** checked.
3. **General Options → Device**: revert from the Lesson-12 Cortex-M0 experiment
   back to the actual **TM4C** device. Note it has a hardware **FPU (FPv4)**.
4. **Linker → List → check "Generate linker map file"**.

---

## Part A — Walk the startup code

### 1. Land somewhere unexpected

Download and start debugging. You do **not** stop at `main()`. The first label
you hit is **`__iar_program_start`**.

Look at the registers: nearly all are **zero** — except **SP**, which already
holds a sensible RAM address. Where did that come from? Part C answers it.

### 2. Step through the call chain

Identify each `BL` (Branch with Link — a function call):

| Call | Purpose |
|---|---|
| `__iar_init_vfp` | initializes the hardware **FPU**, early, in case `main()` uses it |
| `?main` | an **illegal C name** — a reminder that you are in the world *before* C |
| `__low_level_init` | hook for very early custom hardware init |
| `__iar_data_init3` | zeroes `.bss`, copies `.data` |
| `main` | finally |

### 3. Note how `__low_level_init` decides

Watch the test of **R0** after `__low_level_init` returns. Its **return value**
selects between "go straight to `main()`" and "do the data initialization".
The library version returns non-zero, so `__iar_data_init3` runs.

> If you ever need to raise the CPU clock, do it in `__low_level_init` — the
> rest of the startup then runs faster.

Skip over `__iar_data_init3` for now; your program has no interesting data
sections yet.

---

## Part B — Sections and the map file

### 4. Read the map file

Rebuild (`F7`) and open the map file from the project's **Output** folder.

**Module Summary** — find, per object module (`main.o`, `delay.o`, …):

- read-only **code**
- read-only **data**
- read-write **data**

and the totals at the bottom (in the video: 470 / 18 / 1060 bytes). Note the
largest read-write contributor is the **1024-byte stack**.

**Placement Summary** — find each section and its address range:

| Section | Where | What |
|---|---|---|
| `.intvec` (`0`–`0x40`) | ROM | vector table |
| `.text` | ROM | code |
| `.rodata` | ROM | read-only data |
| `.bss` | RAM | uninitialized data (zeroed at startup) |
| `CSTACK` | RAM | the stack (left uninitialized) |

**Notice what's missing: there is no `.data` section yet**, because nothing in
your program is explicitly initialized.

### 5. Add initialized variables

```c
int16_t  x = -1;
uint32_t y = (LED_RED | LED_GREEN);

int16_t sqr[4] = { 1, 4, 9, 16 };

Point  p1 = { 1U, 2U };
Window w  = { { 1U, 2U }, { 3U, 4U } };   /* nested initializers */
```

Experiment with array initializers and note each behaviour:

| Written | Result |
|---|---|
| `int16_t sqr[] = {1,4,9,16};` | size **inferred** = 4 |
| `int16_t sqr[4] = {1,4};` | missing elements become **zero** |
| `int16_t sqr[2] = {1,4,9};` | **compilation error** — too many |

### 6. Watch the map file change

Rebuild and compare:

1. A new **"Initializer bytes"** section appears in the **ROM** range.
2. Two new **`.data`** sections appear in **RAM**, and two `.bss` sections
   disappear.
3. The combined `.data` size (`0xC`) exactly matches the new Initializer-bytes
   size.

That is the whole mechanism: initial values live in ROM, the variables live in
RAM, and startup copies one to the other.

---

## Part C — Watch initialization actually happen

### 7. Prime the RAM so changes are visible

Start a debug session, set the memory view to the RAM region, and **fill it with
`0xFF`**. Now any byte that becomes 0 — or anything else — is obvious.

### 8. Step **into** `__iar_data_init3`

**Zeroing `.bss`:**

- Find the core `STR` instruction. `R3` holds **0**; `R2` holds the address of
  the **first `.bss` section**.
- Execute it and watch a 4-byte word turn from `FF FF FF FF` to zero.
- Note the addressing mode: **`STR R3, [R2], #4`** — the `#4` *after* the
  bracket **post-increments R2**. Watch R2 advance in the register view.

> Don't confuse this with `STR R3, [R2, #4]`, where the offset is applied
> temporarily and R2 is unchanged. The post-increment form is built for tight
> loops.

- Step around the loop and watch `.bss` clear word by word.

**Copying `.data`:**

- The copy is an **`LDR`/`STR` pair** using the same post-increment mode.
- `R2` (source) points at the start of **Initializer bytes in ROM**.
- `R3` (destination) points at the start of **`.data` in RAM**.
- Loop around and watch the whole `.data` section fill.

Notice the copy is a **single block move** — the linker deliberately reordered
your variables so this is possible. It is *not* "two bytes here, four bytes
there".

### 9. Confirm the standard's guarantee

Continue to `main()` and verify: every initialized variable holds its initial
value; every uninitialized variable is zero.

> **Test this on every new toolchain you use.** Some vendors' startup code does
> not clear `.bss` — TI's DSP startup routinely doesn't. If yours doesn't, you
> must initialize those variables explicitly, at the cost of ROM space for a
> block of zeros.

---

## Part D — The vector table

### 10. Answer the two questions

Start a fresh debug session and look at **address 0** in the disassembly window:

| Address | Symbol |
|---|---|
| `0x00000000` | `CSTACK$$LIMIT` — the initial **SP** |
| `0x00000004` | `__iar_program_start` — the initial **PC** |

These are **not instructions**, just words in memory; real code starts later.

The Cortex-M is **hardwired** to load SP from address 0 and PC from address 4
after reset — that is how SP was already valid at step 1.

Note the value at address 4 is **odd** (e.g. `0x219`) while PC becomes `0x218`:
the LSB is the **Thumb bit** (Lesson 8), forced out of the PC.

### 11. Compare against the datasheet

Find the **vector table** section in the
[TM4C123GH6PM datasheet](../resources/TM4C123GH6PM_Datasheet.pdf). It confirms
entries 0 and 1, then lists **all exception and interrupt vectors**.

The datasheet draws it with **address 0 at the bottom** — upside down relative
to your disassembly view. Mentally flip one of them and match them up:

- All exception vectors → `BusFault_Handler`
- "Reserved" vectors → zero
- **`IRQ0`, `IRQ1`, … are missing entirely from the IAR table**

> The library vector table is **generic**: standard Cortex-M exceptions only, no
> device-specific interrupts. It **cannot handle any interrupts** on your chip.
> Lesson 14 replaces it.

### 12. Inspect the default exception handler

The vector table points to `BusFault_Handler` at (say) `0x1DB`; the actual code
is at `0x1DA` — you know why by now.

Two things to notice:

1. IAR defines **all** the handlers — BusFault, DebugMonitor, HardFault,
   MemManage, NMI — but they all share **one address**. The disassembler can
   only show one name, and picks the alphabetically first.
2. The code is a **single branch instruction that jumps to itself**.

| Verdict | |
|---|---|
| For debugging | **useful** — a hang inside an exception handler is diagnosable |
| For production | **unacceptable** — the device locks up, unresponsive: denial of service |

---

## Exercises

1. **Budget your program.** From the map file, state your exact ROM and RAM
   usage. Which module is largest?
2. **Shrink the stack.** Change the stack size and find every place in the map
   file that changes.
3. **Zero-initialize on purpose.** Add `static int big[256] = {0};` and compare
   ROM usage against `static int big[256];`. Explain the difference.
4. **Verify `.bss`.** Fill RAM with `0xAA`, run to `main()`, and confirm every
   uninitialized variable is zero. Write down how you would check this on an
   unfamiliar toolchain.
5. **Use the hook.** Define your own `__low_level_init` that toggles an LED, and
   confirm it runs before `main()`.
6. **Map the table.** Write out the first 16 entries of the datasheet's vector
   table and mark which ones the IAR library table actually supplies.
7. **Trip a handler.** Cause a deliberate BusFault (Lesson 10) and confirm you
   land in the endless loop.

---

## Self-check

- [ ] I reached `__iar_program_start` instead of `main()`
- [ ] I can name each function in the startup call chain and what it does
- [ ] I can read Module Summary and Placement Summary in the map file
- [ ] I saw `.data` and Initializer-bytes sections appear together
- [ ] I watched `.bss` being zeroed and `.data` being block-copied
- [ ] I can explain how SP and PC get their values after reset
- [ ] I know why the library vector table can't handle my chip's interrupts
