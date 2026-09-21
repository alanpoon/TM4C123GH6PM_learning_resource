# Lesson 2 — Knowledge: Flow of Control

**Video:** <https://youtu.be/cZj284kfuE8> · **Transcript:** <https://www.state-machine.com/course/lesson-02.txt>
**Hands-on companion:** [PRACTICE.md](PRACTICE.md)

---

## The big idea

Straight-line execution is *hardwired into the instructions themselves* — every
instruction bumps the PC as a side effect. To loop or to make decisions, a
program must deliberately **change the PC**, and the instruction that does it is
the **branch**.

---

## 1. Linear control flow is the default

In a program with no loops or conditionals, control flows top to bottom. There
is no machinery making that happen: each instruction simply advances the PC to
the next one. Linear flow is free.

## 2. The `while` loop

```c
while (condition) {
    /* body */
}
```

Semantics: test the condition; if true, execute the body and go back to the
test. Exit only when the condition is false.

## 3. The `if` / `else` statement

```c
if (condition) {
    /* executed when condition is true */
}
else {
    /* executed when condition is false */
}
```

The `else` branch is optional. Control-flow statements **nest** — an `if` inside
a `while`, a `while` inside an `if`, and so on.

### Testing for odd numbers

```c
if ((counter & 1) != 0) { /* counter is odd */ }
```

- `&` is the **bitwise AND** operator: it ANDs each bit of the left operand with
  the corresponding bit of the right operand.
- ANDing with `1` isolates the **least significant bit**, which is `0` for even
  numbers and `1` for odd numbers.
- `!=` means "not equal".

## 4. What the CPU actually does

Three instructions do the work of a `while` loop on ARM Cortex-M:

| Instruction | Effect |
|---|---|
| `B` (Branch) | Unconditionally modifies the PC — jumps |
| `CMP` | Compares two values by computing their difference, and **sets flags in the APSR** as a side effect |
| `B<cond>` (e.g. `BLT`) | Modifies the PC **only if** the tested flag condition holds; otherwise falls through |

**APSR** = Application Program Status Register. `CMP R0, #21` computes `R0 - 21`
and, when the result is negative, sets the **N (negative) flag**. `BLT` ("branch
if less than") then tests that flag.

### How a branch knows where to jump

The jump distance is **encoded inside the instruction itself** as a signed
**offset** added to the PC. Worked example from the ARM Architecture Reference
Manual:

- Instruction `0xDBFC`
- First nibble `0xD` → encoding **T1** of the conditional branch
- Second nibble `0xB` → condition **LT**
- Low byte `0xFC` → the offset; as a two's complement signed byte this is **−4**
- Current PC `0x7E` + (−4) = **`0x7A`** → the jump target, i.e. backwards, which
  is exactly what makes it a loop

This is the same two's complement representation from Lesson 1, now used for
jump distances.

## 5. The compiler restructures your loop

Source-level `while` tests *first*, then runs the body. The generated code
usually does the opposite: an unconditional `B` to the test at the bottom, then
body, then `CMP` + conditional branch back up.

The two flows are **equivalent**, but the generated one is faster: it executes
only **one conditional branch per iteration** instead of a test plus a jump.

Two lessons follow from this:

1. A single C statement can produce **multiple machine instructions**, and they
   need not even be contiguous.
2. **The compiler knows the processor better than you do.** Don't fight it.

## 6. Why branches cost time

### Loop overhead
Each iteration executes extra instructions (the compare and the branch) that do
no useful work.

### Pipeline stalls
All modern processors, Cortex-M included, use an **instruction pipeline**: like
an assembly line, several instructions are in flight at different stages
(fetch → decode → execute), one stage per clock cycle. Throughput is highest
when instructions execute in order.

A taken branch breaks that order. The partially processed instructions already
in the pipeline must be **discarded** and the pipeline **refilled** from the new
address — costing several clock cycles.

### Loop unrolling
When speed genuinely matters, **unroll** the loop: put more work in each pass so
the test and branch execute less often, for the same total work.

> **Perspective:** this matters for *time-critical code* such as interrupt
> handlers. For most code it is irrelevant. Do not avoid loops as a rule.

---

## Key takeaways

1. Branch instructions are the only way to change the flow of control; they work
   by modifying the PC.
2. `CMP` sets status flags in the APSR; conditional branches read those flags.
3. Branch targets are PC-relative signed offsets encoded in the instruction.
4. The compiler legitimately reorders your loop into a faster but equivalent form.
5. Branches cost loop overhead plus pipeline stalls — relevant only in hot code.

---

## Glossary

| Term | Meaning |
|---|---|
| APSR | Application Program Status Register, holds N/Z/C/V condition flags |
| `B` / `BLT` | Unconditional / conditional branch instruction |
| Offset | Signed PC-relative jump distance encoded in the branch |
| Pipeline | Overlapped fetch/decode/execute stages of instruction processing |
| Pipeline stall | Wasted cycles after a taken branch flushes the pipeline |
| Loop unrolling | Reducing branch overhead by doing more work per iteration |

---

## Pitfalls to remember

- **Don't expect the disassembly to mirror your source structure.** Equivalent
  ≠ identical.
- **`&` (bitwise AND) is not `&&` (logical AND).** `(counter & 1)` isolates a
  bit; `(counter && 1)` asks a truth question and gives a different result.
- **Micro-optimizing branches is usually wasted effort.** Measure first; unroll
  only where it demonstrably matters.

---

**Next:** Lesson 3 moves the variable out of a register and into RAM, and
introduces pointers.
