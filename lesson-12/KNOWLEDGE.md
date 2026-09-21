# Lesson 12 — Knowledge: Structures in C and CMSIS

**Video:** <https://youtu.be/A0r3O2TxtiU> · **Transcript:** <https://www.state-machine.com/course/lesson-12.txt>
**Hands-on companion:** [PRACTICE.md](PRACTICE.md)

---

## The big idea

> **For the compiler, a structure is nothing but a bunch of offsets** — one per
> member, measured from the start of the structure.

A peripheral's **register map** in the datasheet is also a list of offsets from
a base address. The two things are the same shape — which is why **CMSIS**
models hardware blocks as C structures.

---

## 1. Structures

**Arrays** group variables of the **same** type. **Structures** group variables
of possibly **different** types, so a set of related variables can be treated as
a unit.

```c
struct Point {      /* 'struct' keyword + optional TAG */
    uint16_t x;     /* MEMBERS */
    uint8_t  y;
};                  /* the ONE place in C where a brace must be followed by ';' */
```

### Declaration forms

```c
struct Point { ... } pa, pb;    /* declaration + variable list         */
struct { ... } pa, pb;          /* tag omitted when variables follow   */
struct Point { ... };           /* no variables -> reserves NO storage */
struct Point p1, p2;            /* ...but the tag can declare them later */
```

Repeating `struct` before every tag is cumbersome (and unnecessary in C++), so
combine it with `typedef`:

```c
typedef struct {
    uint16_t x;
    uint8_t  y;
} Point;            /* preferred form — no tag at all */

Point p1, p2;
```

Tag names live in a **different namespace** from typedef names, variable names
and function names — so `typedef struct Point {...} Point;` is legal, and the
`typedef` may even precede the struct declaration.

> **Preferred style:** the untagged `typedef struct {...} Name;` form. The
> **MISRA-C:2012** safety standard agrees — advisory **rule 2.4** says a project
> should not contain unused tag declarations. Tags are almost never needed; the
> notable exception is **self-referential structures** (linked-list and tree
> nodes).

### Member access — the `.` operator

```c
p1.x = sizeof(Point);
p1.y = p1.x - 3U;
```

The `.` operator has **very high precedence** — higher than any arithmetic
operator — so member access rarely needs parentheses.

## 2. Structure layout: order is honoured, padding is added

Two rules, learned by experiment:

1. **The compiler honours the order of members exactly as you declared them.**
   (Unlike ordinary variables, Lesson 11.)
2. **The compiler may insert padding bytes between members.**

A `Point` with a `uint16_t` and a `uint8_t` is **4 bytes**, not 3.

### Why: alignment

The compiler **prefers to waste a byte rather than place a half-word at an odd
address**, because misaligned access costs CPU cycles.

### Packed structures

Standard C cannot suppress padding, but most embedded compilers offer an
extension — IAR's `__packed` keyword before `struct`.

The cost only becomes visible on smaller cores:

| Core | `p1.x = ...` at an odd address |
|---|---|
| Cortex-M4 | one **`STRH`** |
| Cortex-M0 | two **`STRB`** plus a shift |

Cortex-M0 *has* `STRH`, but its version is less capable and cannot reach a
half-word at an odd address.

> **Use packed structures judiciously** — only when you absolutely must avoid
> padding (e.g. wire protocols). Alignment is why padding exists.

### Cortex-M binary compatibility

Code compiled for Cortex-M0 runs unmodified on a Cortex-M4. The instruction sets
nest like Russian dolls: **M4 ⊇ M3 ⊇ M0/M1**.

## 3. Nesting, arrays and whole-structure assignment

```c
typedef struct { Point top_left, bottom_right; } Window;   /* struct in struct */
typedef struct { Point corner[3]; } Triangle;              /* array in struct  */

Window w;      /* 'w' is an INSTANCE of Window */
w.top_left.x = 10U;
t.corner[0].y = 2U;
```

Whole structures can be assigned:

```c
p2 = p1;
w2 = w1;    /* copies the entire structure */
```

> An innocent-looking structure assignment can mean copying a **sizable chunk of
> memory**. For large structures, prefer **pointers**.

## 4. Pointers to structures and the `->` operator

```c
Point  *pp = &p1;
Window *wp = &w2;

(*pp).x = 5U;    /* parentheses REQUIRED: '.' binds tighter than '*' */
pp->x   = 5U;    /* the 'arrow' operator — equivalent and preferred  */
wp->top_left = *pp;
```

### The addressing mode that makes structures cheap

In the disassembly you see `STRH R0, [R1, #2]` — store at *base register +
offset*. This base-plus-offset addressing mode fits structures perfectly, and
was probably designed for exactly this purpose. Each member is just a constant
offset from the base pointer.

## 5. CMSIS — structures for hardware

Until now you accessed registers through preprocessor macros, each hard-coding
one address:

```c
#define GPIO_PORTF_AHB_DIR_R (*((volatile unsigned long *)0x4005D400))
```

The CMSIS approach instead defines **one structure per hardware block**, whose
members correspond one-to-one to the block's registers:

```c
typedef struct {
    __IO uint32_t DATA_Bits[255];
    __IO uint32_t DATA;
    __IO uint32_t DIR;
    ...
    __IO uint32_t AFSEL;
    uint32_t RESERVED1[55];   /* fills a gap in the register map */
    ...
} GPIO_Type;
```

**CMSIS** = **Cortex Microcontroller Software Interface Standard**.

### Reading the datasheet as a blueprint

Each peripheral chapter has a **"Register Map"** section listing every register
as an **offset from the block's base address** — precisely the information a C
structure encodes.

Two details worth noting in the GPIO map:

- The first entry `GPIODATA` is followed by a **`0x400` gap**, because it is not
  one register but the **256 bit-masked DATA registers** from Lesson 7. The
  struct models them as `DATA_Bits[255]` plus the special final `DATA` member.
- Gaps elsewhere (e.g. after `AFSEL`) appear as **`RESERVED` arrays**, so the
  following members land at exactly the datasheet offsets.

### The `__I`, `__O`, `__IO` macros

These CMSIS macros map to the datasheet's access-type column:

| Macro | Meaning | Defined as |
|---|---|---|
| `__I` | Input — **Read-Only** | `volatile const` |
| `__O` | Output — **Write-Only** | `volatile` |
| `__IO` | Input/Output — **Read/Write** | `volatile` |

Two things follow: **`volatile` can qualify individual structure members**, and
`const` marks a member that cannot be modified.

The macros are defined in **`core_cm4.h`** — part of the CMSIS industry standard,
distributed with the toolchain (`arm/CMSIS/Include/`). That file also defines
core peripherals common to every Cortex-M, notably the **NVIC** (Nested Vectored
Interrupt Controller), used in the interrupt lessons.

### Placing a structure at a hardware address

The same trick as Lesson 4: a **pointer to the structure, hard-coded to the base
address from the datasheet**.

```c
#define SYSCTL_BASE  0x400FE000UL
#define SYSCTL       ((SYSCTL_Type *)SYSCTL_BASE)
#define GPIOF_AHB    ((GPIO_Type *)GPIO_PORTF_AHB_BASE)
```

Usage becomes readable and IDE-autocompletable:

```c
SYSCTL->RCGCGPIO  |= (1U << 5);
GPIOF_AHB->DIR    |= LED_RED;
GPIOF_AHB->DATA_Bits[LED_RED] = LED_RED;
```

---

## Key takeaways

1. A structure groups related variables of different types into one unit.
2. Prefer `typedef struct { ... } Name;` with no tag (MISRA rule 2.4).
3. The compiler preserves member **order** but may insert **padding** for
   alignment.
4. Packed structures avoid padding but can cost extra instructions on smaller
   cores.
5. Structure assignment copies memory — use pointers and `->` for large structs.
6. A struct is a set of offsets; a register map is a set of offsets. CMSIS joins
   the two.
7. `__I`/`__O`/`__IO` encode the datasheet's access type as `const`/`volatile`.

---

## Glossary

| Term | Meaning |
|---|---|
| Structure / member / tag | Grouped variables / a grouped variable / optional struct name |
| Instance | A variable of a structure type |
| Padding | Unused bytes inserted to keep members aligned |
| Packed | Compiler extension suppressing padding |
| `->` | Member access through a pointer |
| CMSIS | Cortex Microcontroller Software Interface Standard |
| `__I` / `__O` / `__IO` | CMSIS read-only / write-only / read-write qualifiers |
| NVIC | Nested Vectored Interrupt Controller |
| Register map | Datasheet table of register offsets from a base address |

---

## Pitfalls to remember

- **Assuming `sizeof(struct)` is the sum of its members.** Padding says
  otherwise.
- **Packing structures reflexively** — it can make access slower.
- **Copying large structures by assignment** without realizing the cost.
- **Forgetting parentheses** in `(*pp).x` — `.` binds tighter than `*`.
- **Omitting `RESERVED` gaps** in a hand-written register struct — every
  subsequent member lands at the wrong address.
- **Dropping `volatile`** from a hardware structure.

---

**Next:** Lesson 13 covers pointers to functions and the startup code.
