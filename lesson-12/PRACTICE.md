# Lesson 12 — Practice: From Macros to CMSIS Structures

**Concepts behind this exercise:** [KNOWLEDGE.md](KNOWLEDGE.md)
**Video:** <https://youtu.be/A0r3O2TxtiU>

---

## Projects in this lesson

| Directory | Toolchain | Target |
|---|---|---|
| `tm4c123-keil/` | KEIL MDK (`lesson.uvprojx`) | EK-TM4C123GXL LaunchPad |
| `tm4c123-iar/` | IAR EWARM (`workspace.eww`) | EK-TM4C123GXL LaunchPad |
| `stm32c031-keil/` | KEIL MDK (`lesson.uvprojx`) | STM32 NUCLEO-C031C6 |

### Support directories (new from this lesson on)

| Directory | Contents |
|---|---|
| `CMSIS/` | The CMSIS standard headers (`core_cm4.h`, etc.) |
| `ek-tm4c123gxl/` | TM4C123 device header (`TM4C123GH6PM.h`) and system files |
| `nucleo-c031c6/` | STM32C031 device header and system files |

## Goal

Replace every hard-coded register macro with CMSIS structure access:

```c
/* before */                        /* after */
SYSCTL_RCGCGPIO_R |= (1U << 5);     SYSCTL->RCGCGPIO |= (1U << 5);
GPIO_PORTF_AHB_DIR_R |= LED_RED;    GPIOF_AHB->DIR   |= LED_RED;
```

---

## Part A — Structures from scratch

Use the **simulator** for this part (the board works too).

### 1. Try every declaration form

Work through each form and press `F7` after each — all of them compile:

```c
struct Point { uint16_t x; uint8_t y; } pa, pb;   /* tag + variables    */
struct       { uint16_t x; uint8_t y; } pc, pd;   /* tag omitted        */
struct Point { uint16_t x; uint8_t y; };          /* no storage reserved */
struct Point p1, p2;                              /* declared later      */
```

> Note the trailing `;` after the closing brace — the only place in C where a
> closing brace *must* be followed by a semicolon.

### 2. Simplify with `typedef`

```c
typedef struct Point Point;   /* tag and typedef may share a name...   */
                              /* ...they live in different namespaces  */
```

Then the form to actually use:

```c
typedef struct {
    uint16_t x;
    uint8_t  y;
} Point;

Point p1, p2;      /* no 'struct' keyword needed */
```

Confirm the compiler no longer recognizes `struct Point` — and that it doesn't
matter.

### 3. Access members

```c
p1.x = sizeof(Point);
p1.y = p1.x - 3U;      /* '.' has higher precedence than '-' */
```

### 4. Discover the padding

Debug, watch `p1`, and set the memory view around its address.

Single-step and observe:

- `p1.x` is stored with **`STRH`** (half-word) — an ordinary 16-bit variable.
- `p1.y` is stored with **`STRB`** (byte).
- **`sizeof(Point)` is 4, not 3.** In memory: `x` (2 bytes), `y` (1 byte), then
  **one padding byte**.

### 5. Prove the compiler honours member order

Swap the declaration order (`y` first, then `x`) and rerun.

The order in memory swaps too: `y` at the lower address, then **one unused
byte**, then `x` — still 4 bytes total.

> Two conclusions: member **order** is preserved exactly as you typed it
> (unlike plain variables), but the compiler **may insert padding**.

### 6. Force packing and see the cost

Add IAR's extension before `struct`:

```c
__packed struct { ... }
```

(Change the value assigned to `p1.y` so it isn't 0 when the size becomes 3.)

- `p1.x` is now at an **odd address**.
- On **Cortex-M4** the code is still efficient — one `STRH`, even misaligned.

Now change the core:

1. **Options → General Options → Core → Cortex-M0** (instead of Cortex-M4F).
2. **Debugger → TI Stellaris**, with **Use flash loader** checked.
3. Download to the **Cortex-M4** LaunchPad anyway.

> This works because Cortex-M cores are **binary compatible**: M4 ⊇ M3 ⊇ M0/M1,
> like nested dolls. Run the program — the LED still blinks.

Compare disassembly side by side:

| Core | Code for `p1.x = ...` |
|---|---|
| Cortex-M4 | one **`STRH`** |
| Cortex-M0 | two **`STRB`** + a shift |

Cortex-M0 *has* `STRH` — but its version cannot reach a half-word at an odd
address. **This is why the compiler pads your structures**: it prefers wasting a
byte over wasting cycles.

### 7. Nesting, arrays, and assignment

```c
typedef struct { Point top_left, bottom_right; } Window;
typedef struct { Point corner[3]; }              Triangle;

Window   w;    /* 'w' is an INSTANCE of Window   */
Triangle t;

w.top_left.x  = 10U;
t.corner[0].y = 2U;

p2 = p1;       /* whole-structure assignment */
w2 = w1;       /* copies a sizable chunk of memory! */
```

Let the IDE's autocomplete show you the available members as you type.

### 8. Pointers to structures

```c
Point  *pp = &p1;
Window *wp = &w2;

(*pp).x = 5U;           /* parentheses REQUIRED — '.' binds tighter than '*' */
pp->x   = 5U;           /* the arrow operator: equivalent, and preferred      */
wp->top_left = *pp;     /* another whole-structure assignment                */
```

### 9. Find the base+offset addressing mode

Remove `__packed` (to avoid muddying the picture) but keep the **Cortex-M0**
setting. Step to the `Window` accesses with `w` expanded in the Watch view.

In the disassembly note the **square brackets**:

```
STRH R0, [R1, #2]     ; store at (address in R1) + offset 2  -> top_left.x
STRB R0, [R1, #4]     ;                         + offset 4  -> bottom_right.y
```

Confirm those offsets against `w`'s address in the memory view.

> `STRH` *is* available on Cortex-M0 — as long as the member is aligned. And
> base-plus-offset addressing exists more or less for this purpose: **a struct
> is just a bunch of offsets.**

---

## Part B — CMSIS

### 10. Read the datasheet's register map

Open the [TM4C123GH6PM datasheet](../resources/TM4C123GH6PM_Datasheet.pdf), go to
the GPIO chapter and find **"Register Map"**. Every register is listed as an
**offset from the block's base address** — the blueprint for a C structure.

### 11. Compare it to the CMSIS header

Open the device header (`ek-tm4c123gxl/TM4C123GH6PM.h`, called
`tm4c_cmsis.h` in the video) and find the `SYSCTL` and `GPIO` structure
typedefs. Match them against the datasheet line by line:

| Datasheet | C structure |
|---|---|
| `GPIODATA`, then a `0x400` gap | `DATA_Bits[255]` + the special `DATA` member (Lesson 7!) |
| gap after `GPIOAFSEL` | a `RESERVED1[]` array, so later members land at the right offsets |
| all other registers | one-to-one members |
| access column: R/W, RO, WO | `__IO`, `__I`, `__O` |

### 12. Find the CMSIS macro definitions

The device header includes **`core_cm4.h`** — part of the CMSIS industry
standard, shipped with the toolchain (`arm/CMSIS/Include/`, or this repo's
`CMSIS/` directory). Open it and find:

```c
#define __I   volatile const   /* read-only  */
#define __O   volatile         /* write-only */
#define __IO  volatile         /* read-write */
```

> `volatile` can qualify **individual structure members**. `const` on `__I`
> means the member cannot be written.

While you are there, find the **`NVIC`** structure — the Nested Vectored
Interrupt Controller, present in every Cortex-M core, which you will use in the
interrupt lessons.

### 13. See how structures get placed at hardware addresses

At the **end** of the device header, find:

- `#define`d **base addresses** for each hardware block, and
- `#define`d **pointers** casting those addresses to the right structure type:

```c
#define SYSCTL     ((SYSCTL_Type *)SYSCTL_BASE)
#define GPIOF_AHB  ((GPIO_Type *)GPIO_PORTF_AHB_BASE)
```

Exactly the technique from Lesson 4 — a hard-coded pointer — now pointing at a
whole structure instead of a single register.

### 14. Convert your blinky program

1. Change the include from the old macro header to the CMSIS device header.
2. Remove the old header from the project.
3. Replace each register access, one at a time, letting autocomplete list the
   members (consult the datasheet and the struct definition when unsure):

```c
SYSCTL->RCGCGPIO |= (1U << 5);
GPIOF_AHB->DIR   |= (LED_RED | LED_BLUE | LED_GREEN);
GPIOF_AHB->DEN   |= (LED_RED | LED_BLUE | LED_GREEN);
GPIOF_AHB->DATA_Bits[LED_RED] = LED_RED;    /* still an array of 255 */
```

4. Build cleanly, then verify the **LED still blinks exactly as before**.

---

## Exercises

1. **Predict the size.** For a struct of `uint8_t, uint32_t, uint8_t`, predict
   `sizeof` before checking. Then reorder the members for the smallest size.
2. **Offset check.** Verify a member's offset with
   `(char *)&s.member - (char *)&s`, and match it to the disassembly.
3. **Delete a RESERVED.** Remove a `RESERVED` array from a copy of the GPIO
   struct and work out which registers you would now be writing to.
4. **Const-correctness.** Try to assign to an `__I` member. Which build stage
   complains?
5. **Drop volatile.** Copy the GPIO struct without `__IO`, build at High
   optimization, and see what breaks.
6. **Write your own.** Hand-write a struct for a different peripheral (e.g.
   `UART0`) straight from the datasheet's register map, and check it against the
   vendor header.
7. **Pass by pointer.** Write `void moveBy(Point *p, int dx, int dy)` and compare
   the generated code against a by-value version.

---

## Self-check

- [ ] I can write the `typedef struct { ... } Name;` form from memory
- [ ] I observed padding and explained it in terms of alignment
- [ ] I saw the Cortex-M0 vs. M4 difference for a misaligned half-word
- [ ] I used both `(*pp).x` and `pp->x` and know why the parentheses are needed
- [ ] I found base+offset addressing in the disassembly for a struct member
- [ ] I matched the CMSIS GPIO struct against the datasheet's register map
- [ ] My blinky program now uses `SYSCTL->` and `GPIOF_AHB->` and still blinks
