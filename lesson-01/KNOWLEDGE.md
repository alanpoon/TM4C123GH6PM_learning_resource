# Lesson 1 — Knowledge: How Computers Count

**Video:** <https://youtu.be/gQOv8o5lS2k> · **Transcript:** <https://www.state-machine.com/course/lesson-01.txt>
**Hands-on companion:** [PRACTICE.md](PRACTICE.md)

---

## The big idea

Everything inside a computer — data *and* code — is just numbers in memory. This
lesson makes that concrete by watching a single `counter` variable increment in
a debugger, and by discovering that the counter does *not* grow forever: it
wraps around according to the **two's complement** representation.

---

## 1. Variables

A **variable** is a named location in computer memory that holds a value. In C
a variable must be *defined* before it can be used:

```c
int counter = 0;   /* type, name, optional initial value */
```

The type (`int`) tells the compiler how many bytes the value occupies and how to
interpret the bit pattern stored there.

## 2. Memory and addresses

Memory is one big table of **bytes**, numbered sequentially starting from zero.
Those sequential numbers are **addresses**. Every access to memory requires an
address — there is no other way to reach a byte.

## 3. Machine instructions are numbers too

The compiler translates C statements into **machine instructions**, which are
themselves numbers stored in memory. The debugger's disassembly view shows
those numbers next to human-readable **mnemonics** (`MOVS`, `ADDS`, …) that the
debugger adds purely for readability — the CPU never sees a mnemonic.

Most ARM Cortex-M instructions occupy **2 bytes** (the Thumb-2 instruction set),
which is why viewing memory in 2-byte units makes instructions easy to spot.

## 4. CPU registers

The ARM Cortex-M processor has 16 registers, **R0–R15**, each holding 32 bits.
They are the CPU's scratchpad — like the memory key on a pocket calculator, but
there are 16 of them.

- Machine instructions manipulate registers **directly**, typically in a single
  clock cycle. Memory access is much slower.
- **R15 is the Program Counter (PC)** — it always holds the address of the
  current instruction. Every instruction advances the PC as a *side effect*;
  there is no separate "increment PC" instruction.
- A local variable often lives entirely in a register and never touches RAM.

## 5. Hexadecimal notation

Programmers work in **hex** (base 16) because it maps perfectly onto binary:

| Decimal | Binary | Hex |
|--------:|:------:|:---:|
| 0–9     | 0000–1001 | 0–9 |
| 10      | 1010   | A   |
| 11      | 1011   | B   |
| 12      | 1100   | C   |
| 13      | 1101   | D   |
| 14      | 1110   | E   |
| 15      | 1111   | F   |

- One hex digit == one **nibble** == exactly 4 bits.
- Two hex digits == one **byte** (8 bits).
- Eight hex digits == one 32-bit word.

Example: the 32-bit string `0010 0110 0000 1111 0011 1110 0101 1010` is
`0x260F3E5A`. In C the `0x` prefix marks a hex literal.

Decimal has no such clean mapping (it needs two digits past 9), which is why the
debugger shows most values in hex.

## 6. Two's complement — how negative numbers work

`int` in C is a **signed** type: it holds both positive and negative values. The
representation is **two's complement**, best pictured as a circle where every
step clockwise is `+1`:

```
        0x00000000  (0)
             ↑
   0xFFFFFFFF (-1)   0x00000001 (1)
             ...
   0x80000000        0x7FFFFFFF
  (-2147483648) ←→  (+2147483647)
        ↑ overflow happens here ↑
```

- `0x7FFFFFFF` is the **largest positive** 32-bit signed value (all bits set
  except the most significant).
- Incrementing it overflows into the most significant bit, and the value becomes
  `0x80000000` — the **smallest negative** value, `-2147483648`.
- Keep incrementing and the value becomes *less* negative until it reaches
  `0xFFFFFFFF` == `-1`.
- Increment `-1` and you are back at `0`. The cycle repeats forever.

The **most significant bit acts as the sign bit** for signed types.

---

## Key takeaways

1. Code and data are both just numbers in memory; addresses are the numbers that
   identify bytes.
2. The PC register drives execution; instructions advance it implicitly.
3. Locals frequently live in registers, not RAM.
4. Hex is the natural notation for embedded work — 1 digit = 4 bits.
5. Signed integers wrap from the largest positive value to the smallest
   negative value. Integer overflow is silent; the hardware does not complain.

---

## Glossary

| Term | Meaning |
|---|---|
| Address | A number identifying a byte in memory |
| Mnemonic | Human-readable name of a machine instruction, added by the debugger |
| PC / R15 | Program Counter — address of the current instruction |
| Nibble | 4 bits = one hex digit |
| Two's complement | Binary representation of signed numbers used by virtually all CPUs |

---

## Pitfalls to remember

- **Signed overflow wraps silently.** A counter that "can't" go negative will,
  once it passes `0x7FFFFFFF`.
- **Decimal display hides bit patterns.** `-2147483648` and `0x80000000` are the
  same 32 bits; only the display differs.
- **A variable optimized into a register** may not appear in a memory view at
  all — that is not a bug.

---

**Next:** Lesson 2 replaces the repeated increments with a loop and explores how
the flow of control can be changed.
