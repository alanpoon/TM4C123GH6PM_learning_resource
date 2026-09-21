# Lesson 5 — Practice: Readable Blinky, and the Optimization Trap

**Concepts behind this exercise:** [KNOWLEDGE.md](KNOWLEDGE.md)
**Video:** <https://youtu.be/5MzilJ2-MGY>

---

## Projects in this lesson

| Directory | Toolchain | Target |
|---|---|---|
| `tm4c123-keil/` | KEIL MDK (`lesson.uvprojx`) | EK-TM4C123GXL LaunchPad |
| `tm4c123-iar/` | IAR EWARM (`workspace.eww`) | EK-TM4C123GXL LaunchPad |
| `stm32c031-keil/` | KEIL MDK (`lesson.uvprojx`) | STM32 NUCLEO-C031C6 |

Note the new file in the project: **`tm4c.h`** — the vendor-supplied register
header (named `lm4f120h5qr.h` in the original video).

## Where you end up

```c
#include "tm4c.h"

int main(void) {
    SYSCTL_RCGCGPIO_R = 0x20U; // enable clock for GPIOF
    GPIO_PORTF_DIR_R  = 0x0EU; // set pins 1,2,3 as outputs
    GPIO_PORTF_DEN_R  = 0x0EU; // enable digital function on pins 1,2,3

    while (1) {
        GPIO_PORTF_DATA_R = 0x02U; // turn the red LED on
        int volatile counter = 0;
        while (counter < 1000000) { ++counter; }  // delay loop

        GPIO_PORTF_DATA_R = 0x00U; // turn the red LED off
        counter = 0;
        while (counter < 1000000) { ++counter; }  // delay loop
    }
}
```

Compare this to Lesson 4's wall of hex. Same machine code, vastly better code.

---

## Step-by-step

### 1. Define your first macro

Start from the Lesson 4 program. Replace the clock-gating write with a macro
named after the datasheet's own name for the register (*Run-Mode Clock Gating
Control Register for GPIO*):

```c
#define RCGC_GPIO   (*((unsigned int *)0x400FE608U))
...
RCGC_GPIO = 0x20U;
```

Press `F7`. It should build cleanly.

### 2. Prove the preprocessor is just text substitution

Three quick experiments:

1. Define a macro to something syntactically meaningless and **don't use it**.
   It still compiles — the compiler never sees it.
2. Define `#define FOO (unsigned int *)0x400FE608U` — a *fragment*, not a
   complete C construct — and use it inside a larger expression. It compiles,
   because the pasted text makes sense in that context.
3. Remove the parentheses from `RCGC_GPIO` and use it in a larger expression.
   Observe how its meaning shifts with context. **Put them back.**

### 3. Build macros from macros

Replace the remaining literals with a base plus offsets, exactly as the
datasheet documents them:

```c
#define GPIOF_BASE  0x40025000U
#define GPIOF_DIR   (*((unsigned int *)(GPIOF_BASE + 0x400U)))
#define GPIOF_DEN   (*((unsigned int *)(GPIOF_BASE + 0x51CU)))
#define GPIOF_DATA  (*((unsigned int *)(GPIOF_BASE + 0x3FCU)))
```

### 4. Comment the code

Add both comment styles:

```c
/* traditional C comment */
GPIOF_DATA = 0x02U;   // turn the red LED on
```

### 5. Verify there is no run-time cost

Run on the board and confirm the LED still blinks. Then **single-step the
disassembly** at the `GPIOF_DATA` write:

- The `LDR.N` loads the **complete address `0x400253FC`** in one instruction.
- There is **no addition** of base and offset at run time.

That is **constant folding**. Your readable macros cost nothing.

While you are there, identify the single instruction that actually changes the
outside world: it is the **`STR`**. Talking to hardware is one store.

### 6. Reproduce the optimization bug

This is the centrepiece of the lesson. With `int counter` (no `volatile`):

1. **Project → Options → C/C++ Compiler → Optimizations**, set level to **High**.
2. Rebuild and run on the board.
3. **The LED lights up and stays on.**
4. Single-step: the LED on/off instructions are still there, but **the delay
   loops are gone** — the compiler deleted them, correctly, because their result
   is never used.

### 7. Fix it with `volatile`

```c
int volatile counter = 0;
```

Rebuild at **High** optimization and run. The LED blinks again, and the delay
loop is visible when you single-step.

> From now on you can write code that behaves identically at **any** optimization
> level. That is a real milestone.

### 8. Switch to the vendor header

1. Add `tm4c.h` to the project (**right-click project → Add → Add Files**).
2. Open it and browse the macros — they use the datasheet's register names.
3. Put it side by side with `main.c` and replace your macros one at a time:

   | Yours | Vendor's |
   |---|---|
   | `RCGC_GPIO` | `SYSCTL_RCGCGPIO_R` |
   | `GPIOF_DIR` | `GPIO_PORTF_DIR_R` |
   | `GPIOF_DEN` | `GPIO_PORTF_DEN_R` |
   | `GPIOF_DATA` | `GPIO_PORTF_DATA_R` |

4. Add `#include "tm4c.h"` at the top and **delete your own definitions**.
5. Rebuild and confirm the LED still blinks.

### 9. Compare the two macro styles

Copy one macro out of `tm4c.h` into `main.c` and diff it against yours:

- `unsigned long` vs `unsigned int` — equivalent on a 32-bit ARM, both 32 bits.
- **`volatile`** — present in the vendor macro, absent in yours. This is the
  meaningful difference, and it is the right call: I/O registers change on their
  own (two Port F bits are wired to the user switches).

---

## Exercises

1. **Verify a macro's address.** Pick any macro from `tm4c.h`, look up its
   address in the debugger, and confirm it against the datasheet.
2. **Break it on purpose.** Remove `volatile` from your own `GPIOF_DATA` macro,
   compile at High optimization, and see whether the writes survive.
3. **Precedence trap.** Define `#define HALF 1/2` (no parentheses) and evaluate
   `4 * HALF`. Explain the result, then fix the macro.
4. **Optimization sweep.** Build at none / low / medium / high with and without
   `volatile` on `counter`, and tabulate which combinations blink.
5. **Header hygiene.** What happens if you `#include "tm4c.h"` twice? Look up
   *include guards* and check whether `tm4c.h` has them.
6. **Port it.** Repeat the exercise in `stm32c031-keil/` and note which register
   names and addresses differ — and which concepts do not.

---

## Self-check

- [ ] My blinky uses named macros, no bare hex addresses
- [ ] I confirmed via disassembly that base+offset costs nothing at run time
- [ ] I reproduced the disappearing delay loop at High optimization
- [ ] I fixed it with `int volatile counter`
- [ ] I can explain in one sentence why I/O registers must be `volatile`
- [ ] My program now blinks correctly at every optimization level
