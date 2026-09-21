# Lesson 7 — Practice: Atomic GPIO Writes via Array Indexing

**Concepts behind this exercise:** [KNOWLEDGE.md](KNOWLEDGE.md)
**Video:** <https://youtu.be/pQs8vp7JOSk>

---

## Projects in this lesson

| Directory | Toolchain | Target |
|---|---|---|
| `tm4c123-keil/` | KEIL MDK (`lesson.uvprojx`) | EK-TM4C123GXL LaunchPad |
| `tm4c123-iar/` | IAR EWARM (`workspace.eww`) | EK-TM4C123GXL LaunchPad |
| `stm32c031-keil/` | KEIL MDK (`lesson.uvprojx`) | STM32 NUCLEO-C031C6 |

> The bit-masked GPIO DATA registers are a **TivaC/Stellaris hardware feature**.
> The STM32 project achieves the same atomicity differently (its `BSRR` register);
> the C concepts — arrays and pointer arithmetic — are identical.

## Where you end up

```c
#include "tm4c.h"

#define LED_RED   (1U << 1)
#define LED_BLUE  (1U << 2)
#define LED_GREEN (1U << 3)

int main(void) {
    SYSCTL_GPIOHBCTL_R |= (1U << 5);  /* enable AHB for GPIOF   */
    SYSCTL_RCGCGPIO_R  |= (1U << 5);  /* enable clock for GPIOF */

    GPIO_PORTF_AHB_DIR_R |= (LED_RED | LED_BLUE | LED_GREEN);
    GPIO_PORTF_AHB_DEN_R |= (LED_RED | LED_BLUE | LED_GREEN);

    GPIO_PORTF_AHB_DATA_BITS_R[LED_RED | LED_BLUE | LED_GREEN] = 0;  /* all off */
    GPIO_PORTF_AHB_DATA_BITS_R[LED_BLUE] = LED_BLUE;                 /* blue on */

    while (1) {
        GPIO_PORTF_AHB_DATA_BITS_R[LED_RED] = LED_RED;   /* red on, atomic  */
        int volatile counter = 0;
        while (counter < 1000000) { ++counter; }

        GPIO_PORTF_AHB_DATA_BITS_R[LED_RED] = 0;         /* red off, atomic */
        counter = 0;
        while (counter < 1000000) { ++counter; }
    }
}
```

---

## Step-by-step

### 1. See the problem in the disassembly

Start from the Lesson 6 program. Debug and step to `GPIO_PORTF_DATA_R |= LED_RED;`.
Identify the three instructions: **`LDR` → `ORRS` → `STR`**.

Now imagine an interrupt landing between the `LDR` and the `STR`, in an ISR that
also touches Port F. Write down, in one sentence, what gets lost. That is the
motivation for everything below.

### 2. Brute force first: synthesize the address by hand

Compute the address of the register that isolates **only** bit 1:

```
base            = 0x40025000
offset          = LED_RED << 2    /* shift by 2: addresses must be /4 */
address         = 0x40025000 + (LED_RED << 2)   = 0x40025008
```

```c
*((unsigned long volatile *)(0x40025000 + (LED_RED << 2))) = LED_RED;
```

Build (`F7`) and run on the board. In the disassembly the read-modify-write has
collapsed to **a single `STR`** to the base+8 address. Step it: the red LED
lights and the other LEDs are **unchanged**.

> Because this address isolates one bit, only that bit's data value matters —
> whatever you write to the other bit positions is ignored. Writing `LED_RED`
> (i.e. `1` in that position) is the clear way to express it.

### 3. Learn arrays with a toy example

```c
int volatile counter[2] = { 0, 0 };   /* array initializer */
counter[0] = 1;
counter[1] = 2;
```

Build and confirm the compiler accepts it. In the debugger, find both elements
in the memory view and confirm they are **adjacent**.

### 4. Convert indexing to pointer arithmetic

Replace `counter[1]` with `*(counter + 1)`, rebuild, and confirm identical
behaviour. The array name *is* a pointer to the first element.

### 5. Index the GPIO registers as an array

The vendor header already declares the pointer you need:

```c
GPIO_PORTF_DATA_BITS_R     /* pointer to volatile unsigned long */
```

Write the same operation three ways and keep all three temporarily:

```c
*((unsigned long volatile *)(0x40025000 + (LED_RED << 2))) = LED_RED;  /* 1 */
*(GPIO_PORTF_DATA_BITS_R + LED_RED) = LED_RED;                         /* 2 */
GPIO_PORTF_DATA_BITS_R[LED_RED] = LED_RED;                             /* 3 */
```

### 6. Prove they are identical

Step through all three in the debugger. **All three write to the same address**
(held in the same register, e.g. `R4`) and generate the same machine code.

Now note *why* option 1 needs `<< 2` and options 2–3 do not:

- Option 1 is **address arithmetic** on a raw integer — you scale by the 4-byte
  register size yourself, then cast.
- Options 2–3 are **pointer arithmetic** — the compiler scales by
  `sizeof(unsigned long)` automatically.

Keep option 3 (array indexing — the cleanest) and comment the others out.

### 7. Use the idiom consistently

- Clear the red LED: `GPIO_PORTF_DATA_BITS_R[LED_RED] = 0;`
- Turn blue on: `GPIO_PORTF_DATA_BITS_R[LED_BLUE] = LED_BLUE;`
- All LEDs off in one atomic write:
  `GPIO_PORTF_DATA_BITS_R[LED_RED | LED_BLUE | LED_GREEN] = 0;`

Run it. Behaviour is unchanged from Lesson 6, but every LED write is now a
single atomic store. Break in and confirm: clearing the red bit is **one `STR`**.

### 8. Switch to the faster AHB bus

1. In the datasheet's system-control section find **`GPIOHBCTL`**; note Port F is
   **bit 5**.
2. In `tm4c.h`, find the `SYSCTL_GPIOHBCTL_R` macro and set bit 5:
   `SYSCTL_GPIOHBCTL_R |= (1U << 5);`
3. Search the header for `GPIO_PORTF` and find the registers with the **`_AHB`**
   suffix.
4. Add `_AHB` to **every** Port F register in your program — `DIR`, `DEN`, and
   `DATA_BITS`.

Rebuild and run. Same behaviour, faster bus.

> Do the rename in one pass. A half-converted program writes some registers
> through the APB aperture and some through AHB, which is confusing to debug.

---

## Exercises

1. **One write, three effects.** Using a single indexed write, turn red on, blue
   off and green on simultaneously. Which index and which data value?
2. **Decode an address.** Given the address `0x40025038`, which GPIO bits does a
   write there affect? Work it out from the offset.
3. **Double-scale bug.** Deliberately write `GPIO_PORTF_DATA_BITS_R[LED_RED << 2]`
   and explain what address it hits and why the LED misbehaves.
4. **Why `0x3FC`?** Explain, from the addressing scheme, why the plain
   `GPIO_PORTF_DATA_R` lives at offset `0x3FC`.
5. **Array/pointer equivalence.** Verify that `2[counter]` compiles and works.
   Why is that legal C? What does it tell you about `a[i]`?
6. **Bounds.** Index the toy array out of range and find what memory you hit.
   Note that nothing warned you.
7. **Port it.** In `stm32c031-keil/`, find how the STM32 achieves atomic pin
   writes (look for `BSRR`) and compare the two hardware approaches.

---

## Self-check

- [ ] I can explain exactly what an interrupt breaks in a read-modify-write
- [ ] I computed a bit-masked GPIO address by hand and it worked
- [ ] I verified the three spellings generate identical machine code
- [ ] I can state when scaling by element size is automatic and when it is not
- [ ] My program uses array indexing and runs on the AHB aperture
