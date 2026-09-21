# Lesson 13 — Knowledge: Startup Code Part 1 — From Reset to `main()`

**Video:** <https://youtu.be/zFAnW7Tzu4U> · **Transcript:** <https://www.state-machine.com/course/lesson-13.txt>
**Hands-on companion:** [PRACTICE.md](PRACTICE.md)

---

## The big idea

`main()` is not the beginning. Before it runs, **startup code** must set up the
stack, initialize your variables, and satisfy the C standard's guarantees. And
before *that*, the hardware itself must get the CPU running — via the **vector
table** at address 0.

---

## 1. Program sections

> For the linker, a **section** is simply a contiguous chunk of memory with a
> symbolic name.

| Section | Contents | Located in |
|---|---|---|
| `.intvec` | the **vector table** (addresses `0`–`0x40`) | ROM |
| `.text` | executable **code** | ROM |
| `.rodata` | **read-only data** (constants) | ROM |
| **Initializer bytes** | initial values for `.data`, copied at startup | ROM |
| `.data` | **initialized** variables | RAM |
| `.bss` | **uninitialized** variables, zeroed at startup | RAM |
| `CSTACK` | the **stack**, left uninitialized | RAM |

The names are historical, inherited from ancient assemblers — `.bss` once stood
for "Block Started by Symbol", which means something entirely different today.

### The `.data` / Initializer-bytes pairing

Adding an initialized variable creates **two** sections of equal size:

- a `.data` section in **RAM** (where the variable lives), and
- a matching **"Initializer bytes"** section in **ROM** (holding the values).

Startup copies ROM → RAM.

> Crucially, the startup code does **not** initialize variables one at a time as
> written in your source. The linker **reorders** the variables so that all
> initialized data can be copied in a **single block copy**.

## 2. The linker map file

The map file looks like machine-level gibberish, but it is a **treasure trove**.

> Generate the map file for **all** your projects, and learn to read it.

Two sections to know:

| Map file section | Tells you |
|---|---|
| **Module Summary** | size of read-only code, read-only data and read-write data, **per object module**, plus totals |
| **Placement Summary** | every program section and the address range it occupies |

This is the **only reliable way to know how big your program is** in ROM and RAM.
(Note in the example that the largest single contributor to read-write data is
the **1024-byte stack**.)

## 3. Variable initializers

```c
int16_t  x = -1;
uint32_t y = (LED_RED | LED_GREEN);

int16_t sqr[4] = { 1, 4, 9, 16 };   /* braces + commas for arrays */
int16_t sqr[]  = { 1, 4, 9, 16 };   /* size INFERRED from the initializer */
int16_t sqr[4] = { 1, 4 };          /* missing elements become ZERO */
int16_t sqr[2] = { 1, 4, 9 };       /* ERROR: too many initializers */

Point  p1 = { 1U, 2U };                   /* structures work the same way */
Window w  = { { 1U, 2U }, { 3U, 4U } };   /* nested initializers */
```

## 4. The C startup sequence (IAR library version)

Un-checking "Run to main" reveals the world before `main()`:

```
__iar_program_start
  └─ BL __iar_init_vfp      ; initialize the hardware Floating Point Unit
  └─ BL ?main               ; note: an ILLEGAL C name — we are before C here
       └─ BL __low_level_init
       └─ BL __iar_data_init3    ; if __low_level_init returned non-zero
            ├─ zero-init the .bss sections
            └─ copy Initializer bytes (ROM) -> .data (RAM)
       └─ BL main
```

### `__low_level_init`

A hook for **customized hardware initialization that must happen very early**, or
that speeds up startup — for example raising the CPU clock, so the rest of the
startup runs faster. If you don't define one, an empty library version is used.
Its **return value** decides whether to perform data initialization or jump
straight to `main()`.

### The initialization loops

Both loops use a **post-increment addressing mode**:

```
STR R3, [R2], #4     ; store, THEN advance R2 by 4 — ideal for tight loops
```

> Don't confuse this with `STR R3, [R2, #4]`, where the offset is applied
> *temporarily* to compute the address and the base register is unchanged.

- **Zeroing `.bss`:** `R3` = 0, `R2` = start of `.bss`; loop stores words.
- **Copying `.data`:** an `LDR`/`STR` pair — `R2` walks the Initializer bytes in
  ROM, `R3` walks `.data` in RAM.

## 5. The C standard's guarantee — and who breaks it

> By the time `main()` is called, the C standard requires **all initialized
> variables to hold their initial values** and **all uninitialized variables to
> be zero**.

Startup code from some vendors does **not** comply — notably, some TI DSP
startup code routinely fails to clear `.bss`.

**Recommendation: test your startup code** using the technique in the practice
file (fill RAM with `0xFF` and watch). If `.bss` is not cleared, you must
explicitly initialize those variables to zero — but note the cost: you convert
`.bss` into `.data`, which then needs a matching Initializer-bytes section in
ROM. **You spend ROM storing a block of zeros.**

## 6. The reset sequence and the vector table

Two questions remain: how did **SP** get a sensible value, and how did **PC**
reach `__iar_program_start`? The answers sit at **address 0**:

| Address | Contents |
|---|---|
| `0x00000000` | initial **stack pointer** (`CSTACK$$LIMIT`) |
| `0x00000004` | initial **program counter** (`__iar_program_start`) |

These are **not instructions** — they are plain words in memory. The ARM
Cortex-M is **hardwired** so that after reset it:

1. copies the word at address `0` into **SP**, and
2. copies the word at address `4` into **PC**, forcing the **LSB to 0**.

The stored value is **odd** (e.g. `0x219` → PC becomes `0x218`) because the LSB
signals **Thumb mode** — the only mode Cortex-M supports (Lesson 8).

This structure at address 0 is the **vector table**. It is described in the
microcontroller datasheet and holds far more than these two entries: **every
exception and interrupt vector the processor can handle**.

> The datasheet draws the vector table "upside down" — address 0 at the
> **bottom**, high addresses at the top — the reverse of a disassembly view.

### Why the library vector table is not enough

The IAR library's vector table is **generic**: it has only the standard
exception vectors common to **all** Cortex-M microcontrollers, and **none** of
the device-specific interrupt vectors (`IRQ0`, `IRQ1`, …). It therefore
**cannot handle any interrupts** on your particular chip.

Replacing it with a table matching your datasheet is the subject of Lesson 14.

## 7. Default exception handlers

Every exception vector in the library table appears to point at
`BusFault_Handler`. In fact the IAR startup defines **all** the handlers —
BusFault, DebugMonitor, HardFault, MemManage, NMI — but they all point at the
**same address**. The disassembler, seeing only that address, shows the
alphabetically first name.

The code there is a **single branch instruction that jumps to itself** — a tight
endless loop.

| | |
|---|---|
| **Good for debugging** | break in and you find yourself inside an exception handler |
| **Unacceptable in production** | the device appears completely locked and unresponsive — a **denial of service** |

---

## Key takeaways

1. `main()` runs only after startup code has prepared the C environment.
2. Sections: `.text`/`.rodata`/Initializer bytes in ROM; `.data`/`.bss`/`CSTACK`
   in RAM.
3. The **linker map file** is the only reliable source of code and data sizes.
4. Startup zeroes `.bss` and block-copies `.data` from ROM; the linker reorders
   variables to make one block copy possible.
5. Not all vendors' startup code complies with the standard — **verify yours**.
6. After reset the hardware loads **SP from address 0** and **PC from address 4**.
7. The generic library vector table cannot service device-specific interrupts.
8. Default exception handlers are endless loops — fine for debug, not for
   production.

---

## Glossary

| Term | Meaning |
|---|---|
| Startup code | Code running between reset and `main()` |
| Section | Named contiguous chunk of memory |
| `.data` / `.bss` | Initialized / uninitialized variables in RAM |
| Initializer bytes | ROM copy of `.data`'s initial values |
| `CSTACK` | The stack section |
| Linker map file | Report of sizes and section placement |
| `__low_level_init` | Early hardware-initialization hook |
| Vector table | Table at address 0 holding SP, PC and all exception vectors |
| Denial of service | System locked and unresponsive |

---

## Pitfalls to remember

- **Assuming `.bss` is cleared** without verifying your toolchain does it.
- **Zero-initializing everything explicitly** — it wastes ROM on a block of zeros.
- **Judging code size by object-file size.** Use the map file.
- **Shipping endless-loop exception handlers** in production code.
- **Expecting interrupts to work** with the library's generic vector table.

---

**Next:** Lesson 14 digs into the embedded build process and replaces the
generic vector table with your own.
