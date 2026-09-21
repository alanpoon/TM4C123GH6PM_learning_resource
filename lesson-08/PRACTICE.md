# Lesson 8 — Practice: Factoring Out `delay()` and Watching the Stack

**Concepts behind this exercise:** [KNOWLEDGE.md](KNOWLEDGE.md)
**Video:** <https://youtu.be/Ju4KivZIL1g>

---

## Projects in this lesson

| Directory | Toolchain | Target |
|---|---|---|
| `tm4c123-keil/` | KEIL MDK (`lesson.uvprojx`) | EK-TM4C123GXL LaunchPad |
| `tm4c123-iar/` | IAR EWARM (`workspace.eww`) | EK-TM4C123GXL LaunchPad |
| `stm32c031-keil/` | KEIL MDK (`lesson.uvprojx`) | STM32 NUCLEO-C031C6 |

## Where you end up

```c
void delay(int iter);            /* prototype */

void delay(int iter) {           /* definition */
    int volatile counter = 0;
    while (counter < iter) {
        ++counter;
    }
}

int main(void) {
    /* ... GPIO setup as in lesson 7 ... */
    GPIO_PORTF_AHB_DATA_BITS_R[LED_BLUE] = LED_BLUE;
    while (1) {
        GPIO_PORTF_AHB_DATA_BITS_R[LED_RED] = LED_RED;
        delay(1000000);                        /* red on  — long  */
        GPIO_PORTF_AHB_DATA_BITS_R[LED_RED] = 0;
        delay(500000);                         /* red off — short */
    }
}
```

---

## Required project settings

Before you start, change two options:

1. **Optimization → Low.** At high optimization the compiler **inlines**
   `delay()` and there is no call left to study.
2. **Check "Require prototypes".** Strongly recommended whenever you work with
   functions.

---

## Step-by-step

### 1. Extract the repeated delay loop

The two identical delay loops violate **DRY**. Turn one into a function with no
arguments and no return value, then call it twice:

```c
void delay(void) {
    int volatile counter = 0;
    while (counter < 1000000) { ++counter; }
}
...
delay();   /* parentheses required even with no arguments */
```

Build with `F7`.

### 2. Meet the prototype requirement

With "Require prototypes" on, the build **fails**: *`delay()` has no prototype*.

Add one above the definition:

```c
void delay(void);
```

### 3. The `()` vs `(void)` trap

Now change the prototype to the old-style empty list:

```c
void delay();     /* try this */
```

It **no longer compiles**. For backwards compatibility `()` means "arguments
unspecified — could be anything", which the strict setting rejects.

> Take the habit away from this step: **always write `(void)`.**

### 4. Run it and find where the program lives

Load onto the board. The LED still blinks. Now press **Stop** at a random
moment — you will almost always land **inside `delay()`**, because the program
spends 99.999% of its time in the delay loop.

### 5. Watch the call happen

Set a breakpoint at the call and step into the disassembly.

- The call is a single **`BL`** instruction.
- Note its address and the address of the **next** instruction (e.g. `0x9C`).
- Observe that `BL` is **4 bytes** long, while neighbouring instructions are 2.

Step over the `BL` and check the registers:

- **PC** jumped to the start of `delay()`.
- **LR** holds `0x9D` — **odd**, one more than the return address.

### 6. Explain the odd LR

Thumb-2 instructions are always at even addresses, so an odd return address is
impossible. The LSB of LR is the **Thumb-state bit**, not part of the address.
It is consumed by the return instruction (step 8).

### 7. Find the stack

At the top of `delay()` you see `SUB SP, SP, #4` — the stack **grows by 4
bytes** to make room for the local `counter`.

Point a **Memory view at the address currently in SP**, and set the view to a
**single column** so the stack reads naturally. Remember: on ARM the stack grows
toward **lower** addresses (upward in the view).

Step through and watch `counter` — living at the top of the stack — get cleared
and incremented.

### 8. Watch the return

Breakpoint at the end of `delay()`:

1. The stack is shrunk by 4 — exactly reversing the entry adjustment. At the old
   top you can still see the final `counter` value `0xF4240` = 1,000,000.
2. The return is **`BX LR`** — Branch and eXchange. It sets PC from LR but
   **forces the PC's LSB to 0**.
3. Execute it: you land at `0x9C`, the instruction right after the `BL`.

### 9. Deliberately break it (instructive)

At the end of `delay()`, manually **clear the least significant bit of LR** and
execute the `BX`.

This asks the CPU to switch to the ARM instruction set, which Cortex-M does not
support → you land in a **BusFault exception handler**. That is how a processor
handles an impossible condition; an exception handler is just a function you can
define for your project (Lessons 15–18).

**Reset the board** to escape.

### 10. See `main()` become a non-leaf function

Reset puts you at the top of `main()`. Before you added the call, `main()` was a
**leaf function**. Now that it executes `BL`, LR gets clobbered — so `main()`
must save the old LR.

Find the **`PUSH`** at the top of `main()`. Execute it and watch SP decrement
and the saved value appear on the stack.

> `PUSH` saves a register list **and** decrements SP automatically and
> atomically.

**Summary of what the stack holds:** local variables and return addresses.

### 11. Add an argument

Give `delay()` an iteration count:

```c
void delay(int iter);
void delay(int iter) {
    int volatile counter = 0;
    while (counter < iter) { ++counter; }
}
```

Build **before** fixing the calls — the compiler flags **both** call sites as
not matching the prototype. That is the prototype earning its keep.

Then supply arguments: `delay(1000000);` and `delay(500000);` so the red LED is
on twice as long as it is off.

### 12. Watch the argument being passed

Run freely first: the red LED is visibly on about twice as long as it is off.

Then breakpoint each call and look at the instruction *before* the `BL`:

| Call | Constant loaded into R0 |
|---|---|
| `delay(1000000)` | `0xF4240` |
| `delay(500000)` | `0x7A120` |

The first argument travels in **R0**. Step into `delay()` and confirm `iter`
lives in R0 while `counter` sits at the top of the stack — its address equals SP.

---

## Exercises

1. **Inline it.** Rebuild at High optimization and look for the `BL`. Where did
   the function go?
2. **Stack arithmetic.** Add three more local variables to `delay()` and predict
   the new `SUB SP, SP, #n` value before checking.
3. **Two arguments.** Add a second parameter (say, which LED to blink). Where is
   the second argument passed? (Predict, then verify — Lesson 9 confirms it.)
4. **Return a value.** Make `delay()` return the final counter value and observe
   which register carries it back.
5. **Leaf test.** Write a small leaf function and confirm it does *not* push LR.
6. **Deliberate corruption.** In `delay()`, add 4 to SP without a matching
   subtraction and watch what the return does. (Reset afterwards.)

---

## Self-check

- [ ] `delay()` is a function with a prototype, called twice with different
      arguments
- [ ] I can explain why `void f();` and `void f(void);` differ
- [ ] I found the `BL`, noted it is 4 bytes, and saw LR take an odd value
- [ ] I watched SP move and located a local variable on the stack
- [ ] I triggered the BusFault by clearing LR's LSB and understand why
- [ ] I found the `PUSH` that makes `main()` a non-leaf function
- [ ] I saw the first argument passed in R0
