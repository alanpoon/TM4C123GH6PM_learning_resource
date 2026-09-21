# Lesson 11 — Practice: Fixed-Width Integers and Portability Traps

**Concepts behind this exercise:** [KNOWLEDGE.md](KNOWLEDGE.md)
**Video:** <https://youtu.be/9uvj6eugbJE>

---

## Projects in this lesson

| Directory | Toolchain | Target |
|---|---|---|
| `simulator-arm-keil/` | KEIL MDK | ARM Cortex-M simulator |
| `simulator-arm-iar/` | IAR EWARM | ARM Cortex-M simulator |
| `simulator-msp430-iar/` | **IAR for MSP430** | **MSP430** simulator |

> **This lesson is unusual: it needs two architectures.** The portability bugs
> are *invisible* on the 32-bit ARM and only appear on a 16-bit machine, which
> is why the MSP430 project exists. Run each experiment on **both**.
>
> Requires the IAR toolset for **MSP430** (a different product from IAR EWARM).
> Without it you can still follow along and reason out the results — the
> expected values are given below.

## Setup

Enable **C99** in the project options (`stdint.h` requires it). You can inspect
`stdint.h` itself — it ships inside your toolchain's installation directory.

---

## Part A — Sizes, layout and endianness

### 1. Declare one variable per type

The naming convention encodes the type: `u` = unsigned, `s` = signed.

```c
#include <stdint.h>

uint8_t  u8a, u8b;
uint16_t u16c, u16d;
uint32_t u32e, u32f;

int8_t  s8;
int16_t s16;
int32_t s32;
```

### 2. Verify the sizes

```c
u8a  = sizeof(u8a);        /* size of a VARIABLE */
u16c = sizeof(uint16_t);   /* size of a TYPE     */
u32e = sizeof(uint32_t);
```

Build (`F7`), debug in the **simulator**, and add the variables to **Watch 1**.

> Variables not yet used anywhere will be missing — the compiler eliminated
> them. They appear as soon as the code touches them.

Expected: **1**, **2**, **4**.

### 3. Note the memory layout

Look at the **addresses** in the Watch view. They are *not* in declaration
order: the 4-byte `u32e` sits at a **lower** address than the 2-byte `u16c`,
with the 1-byte `u8a` at the end.

> **Never assume the compiler preserves your declaration order.**

### 4. Watch sized loads and stores

Assign recognizable byte-patterned constants, then copy between variables:

```c
u8a  = 0xa1U;
u16c = 0xc1c2U;
u32e = 0xe1e2e3e4U;

u8b  = u8a;     /* LDRB / STRB */
u16d = u16c;    /* LDRH / STRH */
u32f = u32e;    /* LDR  / STR  */
```

Set the memory view to the start of RAM with **1×Units** (raw bytes), and
single-step the disassembly. Confirm each assignment uses the width-appropriate
instruction pair.

### 5. See little-endian with your own eyes

Compare `u32e` in the **register** (`0xE1E2E3E4`) with the same value in the
**memory view**. The byte order is **reversed**: `E4 E3 E2 E1`. The least
significant byte `E4` is at the **lowest** address.

That is **little-endian**. To picture a big-endian machine (e.g. PowerPC),
manually type the bytes in the opposite order in the memory view.

---

## Part B — The four portability traps

Run each on ARM **and** on MSP430.

### 6. Trap 1 — Promotion overflow

```c
u16c = 40000U;
u16d = 30000U;
u32e = u16c + u16d;              // NOT portable!
```

| Target | Result |
|---|---|
| ARM | 70000 ✓ |
| MSP430 | **4464** ✗ |

On MSP430, `int` is 16 bits, so the "promotion" widens nothing and the addition
**overflows**; 4464 is 70000 truncated to 16 bits (check with a calculator).

The destination `u32e` is 32 bits wide and *could* hold 70000 — irrelevant. The
left-hand side never affects the precision of the computation.

**Fix and re-run:**

```c
u32e = (uint32_t)u16c + u16d;    // portable
```

Single-step the MSP430 **disassembly** and watch the extra work the 16-bit CPU
does to widen both operands — including the familiar partial value **4464**
before it is extended to 32 bits. Final result: **70000**.

> Copy the fixed version back into the ARM project — the un-cast version left
> there is not portable. On ARM the cast costs no additional cycles; verify the
> disassembly is unchanged.

### 7. Trap 2 — Signed/unsigned subtraction

```c
u16c = 100U;
s32  = 10 - u16c;                // NOT portable!
```

| Target | Result |
|---|---|
| ARM | −90 ✓ |
| MSP430 | **65446** ✗ |

Mixing signed and unsigned makes the result `unsigned int`. On ARM that is 32
bits, filling `s32` and re-reading as negative two's complement. On MSP430 it is
16 bits, filling only the lower half of `s32` — and unsigned values are **not
sign-extended**, so it reads as a large positive number.

**Fix:**

```c
s32 = 10 - (int32_t)u16c;        // portable: -90 on both
```

Note also the tempting but wrong variant — `10 - (int16_t)u16c` — which the
source file flags as *"INCORRECT: unintended sign extension"*. Work out why
before moving on.

### 8. Trap 3 — The pointless comparison

```c
if (u32e > -1) {
    /* try to set a breakpoint HERE */
}
else {
    /* and here */
}
```

- The compiler **warns**: pointless integer comparison, always false.
- You **cannot set a breakpoint** in the `if` branch — the compiler removed it.
- Running it hits the **`else`** branch.

Intuition says an unsigned value is always greater than −1. But `-1` is
converted to `unsigned int` = `0xFFFFFFFF`, the largest possible value. Nothing
is greater.

**Fix** with an explicit cast from unsigned to signed.

### 9. Trap 4 — Complement of a byte

```c
u8a = 0xFF;
if (~u8a == 0) {                 /* ALWAYS false — and NO warning */
    ...
}
```

This time there is **no warning**, yet you cannot breakpoint the `if` — or even
the line itself. The compiler eliminated it entirely.

`u8a` is promoted to `int` first (upper bytes zero); complementing makes those
upper bytes all-ones, so the result can never be zero.

**Fix** — revert the promotion:

```c
if ((uint8_t)~u8a == 0U) { ... }
```

> This pattern shows up in real code when verifying a byte-wide **checksum**.

---

## Exercises

1. **Size table.** Print `sizeof` for every type in `stdint.h` on both targets
   and tabulate them.
2. **Endianness test.** Write a function that reports at run time whether the
   machine is little- or big-endian, using a `uint32_t` and a `uint8_t*`.
3. **char signedness.** Determine whether plain `char` is signed or unsigned in
   your compiler, then find the option that changes it.
4. **Interview question.** Predict the result of
   `(uint8_t)200 + (uint8_t)100` on ARM and on MSP430. Then check.
5. **Comparison drill.** For `int i = -1; unsigned u = 1;`, predict `i < u` and
   explain the result to someone else in one sentence.
6. **Checksum.** Implement an 8-bit checksum over an array, verify it with the
   complement test, and confirm it works for all values 0–255.
7. **Audit.** Go back through lessons 4–10 and convert every `int`/`unsigned` to
   a `stdint.h` type. Does anything change in the generated code?

---

## Self-check

- [ ] My variables use `stdint.h` types throughout
- [ ] I verified sizes with `sizeof` and saw the compiler reorder my variables
- [ ] I matched `LDRB`/`LDRH`/`LDR` to the width being accessed
- [ ] I saw the byte-reversal that makes ARM little-endian
- [ ] I reproduced 4464 instead of 70000 (or can explain exactly why it happens)
- [ ] I can state the rule for mixing signed and unsigned operands
- [ ] I can explain why `u32e > -1` is always false
