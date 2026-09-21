# Lesson 6 — Knowledge: Bitwise Operators in C

**Video:** <https://youtu.be/7Iru_LM3qY0> · **Transcript:** <https://www.state-machine.com/course/lesson-06.txt>
**Hands-on companion:** [PRACTICE.md](PRACTICE.md)

---

## The big idea

All three LED bits live in **one register**. Writing a whole value to that
register clobbers the bits you didn't mean to touch. The bitwise operators let
you change **individual bits without disturbing their neighbours** — the single
most-used skill in embedded programming.

> These operators show up constantly in embedded job interviews, especially the
> logical-vs-arithmetic shift distinction.

---

## 1. The six bitwise operators

| Operator | Name | ARM instruction |
|---|---|---|
| `a \| b` | bitwise **OR** | `ORRS` |
| `a & b` | bitwise **AND** | `ANDS` |
| `a ^ b` | bitwise **exclusive OR** (XOR) | `EORS` |
| `~b` | bitwise **NOT** (one's complement), *unary* | `MVNS` |
| `a << n` | **left shift** | `LSLS` |
| `a >> n` | **right shift** | `LSRS` / `ASRS` |

Each applies the operation **bit by bit** between corresponding bits of the
operands — and all 32 bits are done in **one machine instruction**. Bitwise work
is extremely cheap.

### Truth tables

| a | b | a\|b | a&b | a^b |
|---|---|---|---|---|
| 0 | 0 | 0 | 0 | 0 |
| 0 | 1 | 1 | 0 | 1 |
| 1 | 0 | 1 | 0 | 1 |
| 1 | 1 | 1 | 1 | 0 |

## 2. Shifts

- **Left shift `a << n`** — moves bits toward the most significant end, shifting
  **zeros** into the low bits. Equivalent to multiplication by 2ⁿ.
  **Careful:** high-order bits can "fall off the left edge" — the result no
  longer fits in 32 bits, silently.
- **Right shift `a >> n`** — moves bits toward the least significant end.
  Equivalent to integer division by 2ⁿ.

### Logical vs. arithmetic right shift

This is the nuance that catches people out:

| Operand type | Shifts in at the top | Instruction |
|---|---|---|
| **unsigned** | always **0** | `LSRS` — Logical Shift Right |
| **signed** | a copy of the **sign bit** | `ASRS` — Arithmetic Shift Right |

For a signed negative value, **ones** are shifted into the most significant
bits. This is **sign extension** in the two's complement representation
(Lesson 1), and it is *necessary* to preserve the equivalence between
right-shifting and division by a power of 2:

```c
int x =  1024;  x >> 3  ==   128   /* 1024 / 8 */
int y = -1024;  y >> 3  == -128    /* -1024 / 8 — only correct with sign extension */
```

The compiler picks `LSRS` or `ASRS` based purely on the **type** of the operand.

## 3. Defining bit constants by shifting

```c
#define LED_RED   (1U << 1)
#define LED_BLUE  (1U << 2)
#define LED_GREEN (1U << 3)
```

These are **compile-time constants** — zero overhead compared to writing `0x08`.
The advantage is that the **bit number is visible as the shift amount**. For
low-order bits the benefit is modest; for bit 18, `(1U << 18)` is instantly
clear where `0x40000` is not.

> Recommended practice: it saves time counting bits and prevents a whole class of
> stupid bugs.

## 4. The two essential coding idioms

### Set a bit (without disturbing others)

```c
GPIO_PORTF_DATA_R = GPIO_PORTF_DATA_R | LED_RED;   /* long form */
GPIO_PORTF_DATA_R |= LED_RED;                      /* idiom     */
```

OR-ing preserves every bit where the mask is `0`, and forces to `1` every bit
where the mask is `1`.

### Clear a bit (without disturbing others)

```c
GPIO_PORTF_DATA_R = GPIO_PORTF_DATA_R & ~LED_RED;  /* long form */
GPIO_PORTF_DATA_R &= ~LED_RED;                     /* idiom     */
```

AND-ing with the **complemented** mask preserves every bit where `~mask` is `1`,
and forces to `0` the bits where `~mask` is `0`.

### Compound assignment

C's `op=` notation (`|=`, `&=`, `^=`, `<<=`, `>>=`) is an abbreviation for
assignments whose left-hand side is also the first right-hand operand. It means
exactly the same thing, written more succinctly.

> **Memorize both idioms.** They are the vocabulary of register manipulation.

### Multiple bits at once

Masks combine with OR:

```c
GPIO_PORTF_DIR_R |= (LED_RED | LED_BLUE | LED_GREEN);
GPIO_PORTF_DATA_R &= ~(LED_RED | LED_BLUE | LED_GREEN);   /* all off */
```

## 5. Read-modify-write requires a readable register

The set/clear idioms **read** the register, modify the value, and **write** it
back. That only works if the register is **Read/Write** — check the access type
(`R/W` vs `WO`) in the datasheet before applying them.

## 6. Idioms let the compiler understand your intent

Something remarkable happens with the clear idiom. The source says "AND with a
complemented mask", but the compiler emits a single **`BIC`** (Bit Clear)
instruction — it did *not* literally follow the source; it recognized the
**intent** and produced better code.

> **Lesson:** following established idioms lets the compiler optimize at the
> level of *what you mean*, not just *what you wrote*. Clever hand-rolled
> alternatives often defeat this.

---

## Key takeaways

1. Six bitwise operators, each one machine instruction over all 32 bits.
2. Right shift is **logical** for unsigned and **arithmetic** (sign-extending)
   for signed types — a favourite interview question.
3. Define bit masks as `(1U << n)` so the bit number is visible.
4. `reg |= MASK;` sets bits; `reg &= ~MASK;` clears bits. Learn them cold.
5. Read-modify-write needs a readable register.
6. Idiomatic code compiles better than clever code.

---

## Glossary

| Term | Meaning |
|---|---|
| Mask | A value whose set bits select the bits to act on |
| One's complement | `~x` — every bit inverted |
| Logical shift (`LSRS`) | Right shift filling with zeros |
| Arithmetic shift (`ASRS`) | Right shift filling with the sign bit |
| Read-modify-write | Load register, alter bits, store back |
| `BIC` | ARM Bit Clear instruction |

---

## Pitfalls to remember

- **`|` vs `||` and `&` vs `&&`.** Bitwise operators work on bits; logical
  operators produce 0/1 truth values and short-circuit.
- **Forgetting `~`** in the clear idiom — `reg &= MASK` clears *everything
  else* instead.
- **Shifting a signed value right** and expecting zeros at the top.
- **Overflow on left shift** — bits silently fall off the top.
- **Applying the RMW idiom to a write-only register** — the read returns garbage.
- **Read-modify-write is not atomic** — an interrupt in the middle loses updates.
  That problem is the subject of Lesson 7 (and properly of Lesson 20).

---

**Next:** Lesson 7 shows how the TM4C123 GPIO hardware lets you replace the
read-modify-write sequence with a single *atomic* write, using arrays and
pointer arithmetic.
