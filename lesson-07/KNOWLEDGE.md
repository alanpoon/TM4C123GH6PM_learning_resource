# Lesson 7 — Knowledge: Arrays and Pointer Arithmetic

**Video:** <https://youtu.be/pQs8vp7JOSk> · **Transcript:** <https://www.state-machine.com/course/lesson-07.txt>
**Hands-on companion:** [PRACTICE.md](PRACTICE.md)

---

## The big idea

The read-modify-write idiom from Lesson 6 has a flaw: it is **not atomic**. The
TM4C123 GPIO hardware offers a way out — bit-masked addressing — and reaching it
cleanly from C means understanding **arrays and pointer arithmetic**.

---

## 1. Why read-modify-write is dangerous

`GPIO_PORTF_DATA_R |= LED_RED;` compiles to three steps:

```
LDR   ; read the register
ORRS  ; modify the value in a register
STR   ; write it back
```

An **interrupt** is a hardware mechanism that abruptly changes the flow of
control: special hardware loads a new value into the PC, the CPU runs a short
**Interrupt Service Routine (ISR)**, and then resumes the original code as if
nothing had happened. (Interrupts get their own lessons, 16–18.)

Now suppose an ISR also changes GPIO bits, and the interrupt lands **between the
LDR and the STR**:

1. Main code reads the register — a snapshot of the old value.
2. ISR runs and changes some bits.
3. Main code writes back its **stale** value — **the ISR's changes are lost.**

> This is the inherent problem with read-modify-write, and it is not about
> speed. It is about being able to manipulate bits **truly independently** from
> any part of the program, including from interrupts.

## 2. The hardware solution: bit-masked addressing

The GPIO bits connect to the CPU through a **bus**. In the TM4C123 design, each
GPIO bit has both a dedicated **data line** *and* a dedicated **address line**.

> **A bit changes only when its address line is 1.** Otherwise it is unaffected,
> whatever the data line says.

So the *address you write to* selects **which bits may change**, and the *data
you write* determines **what those bits become** — in one **single, atomic
store**.

### Why 256 registers

To give every possible combination of 8 GPIO bits its own address, the hardware
provides **256 32-bit DATA registers** starting at `0x40025000` (Port F, APB).
Address bits `A0`/`A1` are unused, because all addresses must be divisible by 4 —
so the mask is shifted left by 2 in the address.

| Address offset | Meaning |
|---|---|
| `LED_RED << 2` | only bit 1 may change |
| `0x3FC` (`0b1111111100`) | **all 8 bits** may change — the plain `GPIO_PORTF_DATA_R` you have used so far |

That explains the odd `...3FC` address from Lesson 4: it is the register that
masks *nothing*.

Example: light red, extinguish blue, light green — **all in one write**.

## 3. Arrays

An **array** is a group of variables of the same type occupying **consecutive
memory locations** — exactly the shape of those 256 GPIO registers.

```c
int volatile counter[2] = { 0, 0 };   /* declaration + initializer */
counter[0] = 5;                        /* index; first element is ALWAYS 0 */
```

The number in brackets is the **index**; C arrays are **zero-based**.

## 4. Arrays and pointers are two views of one thing

The compiler treats an array as a **pointer to its first element**. To get a
pointer to element `i`, add `i` to the array pointer:

```c
counter[1]   ==   *(counter + 1)
```

That addition is **pointer arithmetic**.

The correspondence works **both ways** — any pointer can be indexed like an
array. The vendor header exploits this:

```c
GPIO_PORTF_DATA_BITS_R          /* a pointer to volatile unsigned long */
GPIO_PORTF_DATA_BITS_R[LED_RED] /* indexes into all 256 DATA registers */
```

### Three equivalent spellings

```c
/* 1. raw address arithmetic + cast */
*((unsigned long volatile *)(0x40025000 + (LED_RED << 2))) = LED_RED;

/* 2. pointer arithmetic */
*(GPIO_PORTF_DATA_BITS_R + LED_RED) = LED_RED;

/* 3. array indexing  -- cleanest */
GPIO_PORTF_DATA_BITS_R[LED_RED] = LED_RED;
```

All three generate **exactly the same machine code**: one `STR` to one address.

## 5. Address arithmetic vs. pointer arithmetic — the crucial difference

| | Scaling |
|---|---|
| **Address arithmetic** (option 1) | You must scale manually: `LED_RED << 2`, because a GPIO register is 4 bytes wide. Arithmetic happens *before* the cast, on a raw integer. |
| **Pointer arithmetic** (options 2 & 3) | Scaling is **automatic** — the compiler multiplies by `sizeof(*ptr)` for you. |

The automatic scaling *must* be so, precisely because pointer arithmetic and
array indexing have to be equivalent.

> Forgetting this asymmetry — scaling twice, or not at all — is a classic bug.

## 6. APB vs. AHB

The TM4C123 has **two peripheral buses**, and the GPIO ports are connected to
both:

| Bus | Character |
|---|---|
| **APB** — Advanced Peripheral Bus | the **default**; older and slower, kept for backwards compatibility |
| **AHB** — Advanced High-performance Bus | newer, faster |

Switching Port F to AHB takes two things:

1. Set the port's bit in the **`GPIOHBCTL`** system-control register
   (bit 5 for Port F).
2. Use the registers from the **AHB aperture** instead of the APB aperture — in
   the vendor header these carry an **`_AHB`** suffix
   (`GPIO_PORTF_AHB_DATA_BITS_R`, `GPIO_PORTF_AHB_DIR_R`, …).

"Aperture" is the datasheet's word for an address window onto the same
peripheral.

---

## Key takeaways

1. Read-modify-write can lose updates when an interrupt intervenes — it is not
   atomic.
2. TM4C123 GPIO gives every *combination* of bits its own address, so one store
   changes exactly the bits you name, atomically.
3. An array is consecutive same-type storage, indexed from 0.
4. `a[i]` is *defined* as `*(a + i)` — arrays and pointers are interchangeable.
5. Pointer arithmetic scales by element size automatically; raw address
   arithmetic does not.
6. Prefer the AHB aperture on the TM4C123; enable it via `GPIOHBCTL`.

---

## Glossary

| Term | Meaning |
|---|---|
| Atomic | Completes indivisibly; cannot be interrupted part-way |
| ISR | Interrupt Service Routine |
| Array | Consecutive storage of same-type elements, indexed from 0 |
| Pointer arithmetic | Adding an integer to a pointer, scaled by element size |
| Bit-masked addressing | Address selects which bits a write may alter |
| Aperture | An address window onto a peripheral (APB vs. AHB) |
| APB / AHB | Advanced Peripheral Bus / Advanced High-performance Bus |

---

## Pitfalls to remember

- **Double-scaling.** Writing `GPIO_PORTF_DATA_BITS_R[LED_RED << 2]` shifts
  twice — the compiler already scales.
- **Off-by-one from 1-based thinking.** C indexes from 0.
- **No bounds checking.** `a[999]` on a 2-element array compiles and corrupts
  memory.
- **Switching apertures halfway.** Enable `GPIOHBCTL` *and* change every register
  to the `_AHB` names — mixing them silently uses the wrong addresses.
- **Assuming RMW is safe because it's fast.** Speed is irrelevant; interruption
  is the hazard.

---

**Next:** Lesson 8 introduces functions and the call stack.
