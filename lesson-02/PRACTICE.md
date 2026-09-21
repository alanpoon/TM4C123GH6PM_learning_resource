# Lesson 2 — Practice: Loops, Branches and the APSR

**Concepts behind this exercise:** [KNOWLEDGE.md](KNOWLEDGE.md)
**Video:** <https://youtu.be/cZj284kfuE8>

---

## Projects in this lesson

| Directory | Toolchain | Target |
|---|---|---|
| `simulator-iar/` | IAR EWARM (`workspace.eww`) | Instruction-set simulator |
| `simulator-keil/` | KEIL MDK (`lesson.uvprojx`) | Instruction-set simulator |

## The code

```c
int main(void) {
    int counter = 0;
    while (counter < 21) {
        ++counter;
        if ((counter & 1) != 0) {
            /* do something when the counter is odd */
        }
        else {
            /* do something when the counter is even */
        }
    }
    return 0;
}
```

The 21 hand-written increments of Lesson 1 have collapsed into one loop that
performs the same 21 increments.

---

## Working habit to adopt now

> **Copy the previous lesson's project before changing it, and keep the code
> working at all times by making small incremental changes.**

When you break something, it is almost always faster to back up to the last
working version than to debug a large broken change. This habit pays for itself
repeatedly through the rest of the course.

---

## Step-by-step

### 1. Baseline: see linear flow

Open the project, build (`F7`), and start the debugger. Step one **machine
instruction** at a time (step in the disassembly window, not the source window)
through straight-line code and watch **PC** in the Registers view.

Notice: you are executing only increment instructions — nothing explicitly
touches the PC, yet it advances every step. Linear flow is a side effect.

### 2. Run the loop and identify the instructions

Rebuild with the `while` loop and step through the disassembly. Find and label:

- `MOVS Rn, #0` — initializes `counter` (it lives in a register, e.g. `R0`)
- `B` — an **unconditional** branch that skips forward over the body
- `CMP R0, #21` — the comparison; note `21` appears in the instruction encoding
  as hex `0x15`
- `BLT` — the **conditional** branch back to the top of the body

### 3. Watch the APSR

Step the `CMP` instruction and watch the **APSR** in the Registers view: the
**N (negative)** flag sets, because `R0 - 21` is negative while the loop is
still running. Step until `counter` reaches 21 and watch N clear — that is the
moment the loop will exit.

### 4. Decode the branch offset by hand

Before executing the `BLT`, compute where it will jump:

1. Read the raw instruction encoding (e.g. `0xDBFC`).
2. First nibble `0xD` → T1 encoding of a conditional branch.
3. Second nibble → the condition code (`0xB` = LT).
4. Low byte `0xFC` → two's complement offset = **−4**.
5. Predicted target = current PC + offset (e.g. `0x7E − 4 = 0x7A`).

Now execute the `BLT` and confirm the PC lands where you predicted.

> This one exercise is worth doing carefully — it is a direct window into how
> the CPU works. You will not need to decode instructions by hand again.

### 5. Compare source flow vs. generated flow

Sketch the flow of control in the disassembly and compare it to the `while`
semantics:

| Source `while` | Generated code |
|---|---|
| test condition | unconditional branch to the test |
| body if true | body |
| back to test | test + conditional branch back |

Convince yourself the two are equivalent, and that the generated version
executes **one** conditional branch per iteration instead of a test *and* a jump.

### 6. Unroll the loop

Modify the loop to do more increments per pass, adjusting the bound so the total
is still 21 increments. For example, 3 increments per pass with `counter < 21`
stepping by 3.

Rebuild, run, and observe that testing and branching now happen **less
frequently** for the same amount of work.

Then press `Ctrl+Z` a few times to revert.

### 7. Make a decision at run time

With the `if ((counter & 1) != 0)` test in place, single-step and watch which
branch is taken on each iteration. Put a distinguishing statement in each branch
(for example, assign a different value to a scratch variable) so you can see
odd vs. even iterations in the Locals view.

---

## Exercises

1. **Bitwise vs. logical.** Change `(counter & 1)` to `(counter && 1)`, rebuild,
   and explain in one sentence why the behaviour changes.
2. **Every third value.** Change the test so the "special" branch runs when
   `counter` is a multiple of 3. Which operator do you need, and why can't you
   use `&`?
3. **Nesting.** Nest an `if` inside the odd branch that also checks
   `counter > 10`. Verify in the debugger that both conditions must hold.
4. **Count the cost.** Using the disassembly, count how many instructions one
   loop iteration executes. Compare against your unrolled version from step 6.
5. **`for` vs. `while`.** Rewrite the loop as a `for` loop, rebuild, and diff the
   disassembly against the `while` version. Are they the same?

---

## Self-check

- [ ] I can point to the `CMP` and conditional-branch instructions in the
      disassembly
- [ ] I watched the N flag in the APSR change as the comparison result changed
- [ ] I decoded a branch offset by hand and correctly predicted the new PC
- [ ] I can explain why the compiler's loop shape differs from my source
- [ ] I unrolled the loop and observed fewer branches for the same work
