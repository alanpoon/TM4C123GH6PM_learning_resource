# Lesson 9 — Practice: Split the Program, Write a Recursive Factorial

**Concepts behind this exercise:** [KNOWLEDGE.md](KNOWLEDGE.md)
**Video:** <https://youtu.be/_I-SeeC07Jo>

---

## Projects in this lesson

| Directory | Toolchain | Target |
|---|---|---|
| `tm4c123-keil/` | KEIL MDK (`lesson.uvprojx`) | EK-TM4C123GXL LaunchPad |
| `tm4c123-iar/` | IAR EWARM (`workspace.eww`) | EK-TM4C123GXL LaunchPad |
| `stm32c031-keil/` | KEIL MDK (`lesson.uvprojx`) | STM32 NUCLEO-C031C6 |

The board is convenient but not required — the simulator works for everything
here.

## Files you will end up with

```
main.c      -- #include "delay.h", the blinky loop, fact() experiments
delay.c     -- the delay() definition
delay.h     -- the delay() prototype, with include guards
tm4c.h      -- vendor register header
```

---

## Part A — Modularize

### 1. Move `delay()` to its own file

1. **New document** → cut-and-paste the `delay()` **definition** into it.
2. Save it as **`delay.c`** in the project directory.
3. The file now exists on disk but is **not part of the project**.
   Right-click the project → **Add → Add `delay.c`**.

### 2. Hit the prototype error — and resist the lousy fix

`F7` fails: *`delay()` defined without a prototype.*

The tempting fix is to copy the prototype into `delay.c`. **Don't.** Two copies
of the same declaration can drift apart — DRY violated again.

### 3. Create the header

1. **New document** → cut-and-paste the **prototype** into it.
2. Save as **`delay.h`**.
3. Add `#include "delay.h"` to **both** `main.c` **and** `delay.c`.

Including it in `delay.c` is what lets the compiler verify the definition
matches the declared interface.

### 4. Add include guards

Copy the pattern used by the vendor header (`tm4c.h` uses `__TM4C_H__`):

```c
#ifndef __DELAY_H__
#define __DELAY_H__

void delay(int iter);

#endif
```

Don't forget the closing `#endif`.

Build and confirm the LED still blinks exactly as before.

---

## Part B — Return values and the linker

### 5. Design top-down

Write the prototype and the *use cases* before any implementation:

```c
unsigned fact(unsigned n);

unsigned volatile x;   /* volatile, so the compiler can't optimize it away */

x = fact(0U);              /* assign the return value       */
x = fact(3U) + fact(2U);   /* use it inside an expression   */
(void)fact(5U);            /* explicitly discard the result */
```

Note the `U` suffixes — the parameter is unsigned, and `0U` is the smallest
value for which factorial is defined.

### 6. Meet the linker

Build now. The error is **not** a compiler error — scroll up and see that
**linking** failed: `fact()` not found.

> The compiler cannot detect this: it has no way of knowing which file might
> contain the definition. Only the linker, which sees all compilation units, can.

Learn to read the build log well enough to tell the two stages apart.

### 7. Implement it recursively

Write the mathematical definition as a comment first, then translate:

```c
/* 0! = 1
   n! = n * (n-1)!   for n > 0  */
unsigned fact(unsigned n) {
    if (n == 0U) {
        return 1U;
    }
    else {
        return n * fact(n - 1U);
    }
}
```

Compilation **and** linking now succeed. That is your first recursive function.

---

## Part C — Watch the stack build and unwind

### 8. Prepare the view

Point a **Memory view at the address in SP** and use a single column.
Keep the **Registers** and **Disassembly** views visible too.

### 9. Step into the call

The call is **two** instructions: move the argument into **R0**, then **`BL`**.

Inside `fact()`, the first instruction is `PUSH {R4, LR}`:

- **LR** must be saved because `fact()` is **not a leaf** — it calls itself, and
  `BL` clobbers LR.
- **R4** must be saved because the function is about to use it, and the AAPCS
  makes R4–R11 the callee's responsibility.

The very next instruction shows *why* R4 is needed: R0 carries the argument, but
R0 is also reused for the return value **and** for the nested call's argument.
So the compiler copies `n` into **R4**, where it will survive the recursion.

### 10. Watch the return path

The last instruction is `POP {R4, PC}` — it kills two birds:

- restores R4 (reversing the entry PUSH exactly), and
- loads the saved LR straight into the **PC**, which *is* the return.

Notice the value popped into PC is **odd** (`0x49`) while the PC becomes **even**
(`0x48`). Popping into PC behaves like `BX`: the LSB is the Thumb-state bit.

### 11. Recurse deeply and watch RAM fill

Step into `fact(5U)`. The key observation:

> The next call to `fact()` happens **before** the previous one returns and pops
> — so each activation nests **on top of** the previous one's stack space.

Step through all **6 levels** and watch the memory view build a clearly
repeating pattern of **decrementing `n` values and return addresses**.

Then step through the return sequence and watch the stack **unwind**, one level
at a time. After the multiplication chain completes, R0 holds `0x78` = **120**,
which is 5!.

### 12. Find the AAPCS in action

Look back over what you just watched and name each convention:

| Observation | AAPCS rule |
|---|---|
| Argument loaded into R0 before `BL` | R0–R3 pass arguments |
| Result read from R0 after the call | R0 returns the value |
| `PUSH {R4, ...}` at function entry | R4–R11 are **callee-saved** |
| Return address in LR | LR holds the return address |

---

## Exercises

1. **Iterative factorial.** Rewrite `fact()` with a loop. Compare the stack
   usage against the recursive version — this is the version you should ship.
2. **Lookup table.** Replace it with a table of precomputed factorials. How many
   entries fit in `unsigned` before overflow?
3. **Overflow hunt.** Find the largest `n` for which `fact(n)` is correct for a
   32-bit unsigned result. What does `fact(13U)` return, and why?
4. **Measure the frame.** How many bytes of stack does one `fact()` activation
   consume? Multiply by the depth for `fact(20)`.
5. **Guard removal.** Delete the include guards from `delay.h` and include it
   twice. What error appears, and from which build stage?
6. **Forget to add the file.** Remove `delay.c` from the project (leaving it on
   disk) and rebuild. Predict the error before you see it.
7. **Break the AAPCS.** In `fact()`, use R4 without pushing it (via inline
   assembly or a hand-edit) and observe the corruption.

---

## Self-check

- [ ] `delay()` lives in its own `.c`/`.h` pair with include guards
- [ ] The header is included in both `main.c` and `delay.c`
- [ ] I produced a linker error on purpose and recognized it as such
- [ ] My recursive `fact(5U)` returns 120
- [ ] I watched 6 stack frames build up and unwind
- [ ] I can state which registers a function may clobber and which it must preserve
- [ ] I can explain why recursion is discouraged in embedded code
