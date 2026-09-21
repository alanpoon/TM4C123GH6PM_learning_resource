# Lesson 11 — Knowledge: Standard Integers and Mixing Integer Types

**Video:** <https://youtu.be/9uvj6eugbJE> · **Transcript:** <https://www.state-machine.com/course/lesson-11.txt>
**Hands-on companion:** [PRACTICE.md](PRACTICE.md)

---

## The big idea

C's built-in integer types have **no fixed size**. `stdint.h` fixes that. What
`stdint.h` cannot fix is C's **implicit conversion rules**, which are the
trickiest part of the language and a rich source of subtle, non-portable bugs.

> This lesson answers several classic embedded **job-interview** questions.

---

## 1. Built-in integer types have unspecified sizes

The C standard does **not** prescribe the size of `int`, `short`, `long` or
`char`. It only requires:

```
sizeof(short)  <=  sizeof(int)  <=  sizeof(long)
```

| Type | On ARM (32-bit) | On MSP430 / AVR (16/8-bit) |
|---|---|---|
| `int`, `unsigned` | 32 bits | **16 bits** |

`char` is typically one byte but may be **signed or unsigned** depending on
compiler options.

These ambiguities are **intentional** — they give compiler vendors flexibility.
But in embedded work you often must know the exact size, signedness and dynamic
range so that your code behaves identically on every target.

## 2. `stdint.h` — the C99 solution

The historical workaround was for every project or OS to `typedef` its own
names (µC/OS-II uses `CPU_INT32U`, etc.):

```c
typedef unsigned int CPU_INT32U;   /* read declarations backwards */
```

Such hand-made headers are **not universal**: the names are non-standard, and
the definitions are correct only for one processor and one compiler.

C99 standardized this with **`stdint.h`** — for an embedded programmer, one of
the most valuable features of C99. The six types that matter:

| Type | Meaning |
|---|---|
| `int8_t` / `uint8_t` | signed / unsigned 8-bit |
| `int16_t` / `uint16_t` | signed / unsigned 16-bit |
| `int32_t` / `uint32_t` | signed / unsigned 32-bit |

The value is twofold: **standard names**, and the **compiler vendor** — not you
— is responsible for the correct definitions on your CPU.

> With an ISO standard available, inventing your own integer type names is
> counter-productive. If your compiler is not C99 compliant (e.g. older MSVC),
> **supply a `stdint.h` with the standard names** rather than inventing others.

## 3. `sizeof`

```c
sizeof(u8a)        /* size of a variable, in bytes */
sizeof(uint16_t)   /* size of a type */
```

## 4. The compiler reorders your variables

Variables are **not** laid out in the order you declared them. The compiler
groups by size — typically 4-byte variables at lower addresses, then 2-byte,
then 1-byte.

> **Never assume declaration order is preserved in memory.**

## 5. Sized load/store instructions

ARM has width-specific memory instructions:

| Width | Load | Store |
|---|---|---|
| byte (8-bit) | `LDRB` | `STRB` |
| half-word (16-bit) | `LDRH` | `STRH` |
| word (32-bit) | `LDR` | `STR` |

## 6. Endianness

**Little-endian** means the **low-order byte of a register is stored at the
lowest address**.

Storing `0xE1E2E3E4` from a register to memory on ARM produces the byte sequence
`E4 E3 E2 E1` — reversed relative to how you read the number.

> The ARM core has *configurable* endianness, but virtually all silicon vendors
> (TI included) choose **little-endian**. PowerPC — e.g. in pre-Intel Macs — is
> an example of a **big-endian** machine.

## 7. Implicit conversions — the tricky part

Mixing types is unavoidable, and C converts automatically. The rules are
**complicated, counter-intuitive, and a frequent source of subtle bugs**.

### Rule 1 — Integer promotion

> **Any integer smaller than `int` is promoted to `int`/`unsigned int` before
> any computation.**

### Rule 2 — Usual arithmetic conversions

> The computation is performed at the **largest precision** among the operands.
> If one operand is 32-bit, the other is promoted to 32-bit.

### Rule 3 — Signed/unsigned mixing

> When a **signed** and an **unsigned** operand of the same rank are mixed,
> **both become `unsigned int`**, and so does the result.

### Rule 4 — The left-hand side is irrelevant

> **The precision of the computation does NOT depend on the type being assigned
> to.** The expression is evaluated first, then converted.

---

## 8. Four classic traps

### Trap 1 — Overflow during promotion

```c
uint16_t u16c = 40000U, u16d = 30000U;
uint32_t u32e = u16c + u16d;     /* NOT portable! */
```

| Target | Result | Why |
|---|---|---|
| ARM (32-bit `int`) | **70000** ✓ | promotion to 32-bit `int` |
| MSP430 (16-bit `int`) | **4464** ✗ | "promotion" to 16-bit `int` → overflow, truncated |

The trap: the *destination* is 32 bits wide and could hold 70000 — which throws
many developers off. But by Rule 4 the destination does not matter.

**Fix** — force at least one operand to 32 bits so Rule 2 applies:

```c
uint32_t u32e = (uint32_t)u16c + u16d;   /* portable */
```

On a 32-bit machine this cast costs **zero** extra cycles.

### Trap 2 — Signed/unsigned subtraction

```c
uint16_t u16c = 100U;
int32_t s32 = 10 - u16c;         /* NOT portable! */
```

| Target | Result |
|---|---|
| ARM | **−90** ✓ |
| MSP430 | **65446** ✗ |

Rule 3 makes the result `unsigned int`. On ARM that unsigned 32-bit value fills
`s32` completely and is re-interpreted as negative in two's complement. On
MSP430 the 16-bit unsigned value fills only the lower half of `s32` and — being
unsigned — is **not sign-extended**, so it reads as a large positive number.

**Fix** — don't mix signedness; cast explicitly:

```c
int32_t s32 = 10 - (int32_t)u16c;
```

Here the problem was *signedness*, not width, so `(int16_t)` would suffice —
but beware: `10 - (int16_t)u16c` re-introduces a width issue in general, so
prefer casting to the width you actually need.

### Trap 3 — The "pointless comparison"

```c
uint32_t u32e;
if (u32e > -1) { ... } else { ... }
```

The compiler warns: *pointless integer comparison — always false.*

Intuition says an unsigned value is always ≥ 0 and therefore always > −1, so the
comparison should always be **true**. The compiler says **always false** — and
the compiler is right. By Rule 3, `-1` is converted to `unsigned int` =
**`0xFFFFFFFF`**, the largest 32-bit unsigned value. Nothing can exceed it.

You cannot even set a breakpoint in the `if` branch; the compiler removed it.

**Fix** — cast explicitly from unsigned to signed.

### Trap 4 — Complementing a small integer

```c
uint8_t u8a = 0xFF;
if (~u8a == 0) { ... }     /* ALWAYS false — and no warning! */
```

Plausible use: verifying a byte-wide checksum. But `u8a` is **promoted to
`int`** first, so its upper bytes are zero; complementing sets those upper bytes
to all-ones, and the result can never be zero. The compiler silently eliminates
the whole `if` — you cannot even breakpoint on it.

**Fix** — undo the promotion by casting the result back to a byte:

```c
if ((uint8_t)~u8a == 0U) { ... }
```

---

## Key takeaways

1. Built-in integer sizes are target-dependent; `stdint.h` types are not.
2. Use `uint8_t` … `int32_t` everywhere; don't invent your own names.
3. The compiler may reorder your variables in memory.
4. ARM is little-endian: low byte at the low address.
5. Everything smaller than `int` gets promoted to `int` before any computation.
6. Mixing signed and unsigned makes the whole expression unsigned.
7. **The assignment target never affects how an expression is computed.**
8. When in doubt, cast **explicitly** — it documents intent and costs nothing on
   a 32-bit machine.

---

## Glossary

| Term | Meaning |
|---|---|
| `stdint.h` | C99 header defining fixed-width integer types |
| `typedef` | Defines a new name for an existing type (read backwards) |
| `sizeof` | Operator yielding the size in bytes of a type or variable |
| Integer promotion | Automatic widening of small types to `int` |
| Usual arithmetic conversions | Rules choosing the common type of an expression |
| Little-endian | Least significant byte stored at the lowest address |
| `LDRB`/`LDRH`/`LDR` | Byte / half-word / word load instructions |

---

## Pitfalls to remember

- **Assuming `int` is 32 bits.** It isn't, on 8- and 16-bit targets.
- **Believing a wide destination widens the computation.** It does not.
- **Mixing signed and unsigned** in arithmetic or comparisons.
- **`~` on a small integer** without casting back.
- **Trusting declaration order** for memory layout.
- **Ignoring "pointless comparison" warnings** — they are almost always real bugs.

---

**Next:** Lesson 12 introduces C structures and CMSIS, the standard way to
access Cortex-M hardware.
