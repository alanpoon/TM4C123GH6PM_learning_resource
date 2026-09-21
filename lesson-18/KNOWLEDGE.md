# Lesson 18 — Knowledge: Interrupts Part 3 — How ARM Cortex-M Does It

**Video:** <https://youtu.be/O0Z1D6p7J5A> · **Transcript:** <https://www.state-machine.com/course/lesson-18.txt>
**Hands-on companion:** [PRACTICE.md](PRACTICE.md)

---

## The big idea

Lesson 17 identified two reasons ISRs can't normally be plain C functions:
they must **save extra registers** and **return by a special instruction**.
Cortex-M solves both — elegantly — and the solutions are worth studying.

---

## 1. Triggering any interrupt on Cortex-M

The MSP430 trick (write the counter just below the limit) **does not work** for
SysTick: `STCURRENT` is **write-clear** — any write clears it without triggering
anything.

Cortex-M offers something better and more direct: **pend the interrupt manually**.

| Register | Bit | Effect |
|---|---|---|
| **ICSR** (Interrupt Control and State Register) | **26 — `PENDSTSET`** | sets SysTick to the **pending** state |

> **Cortex-M provides a pending bit for *every* interrupt source**, so you can
> trigger **any** interrupt in the system this way. Most of these pending bits
> live in the **NVIC** (Nested Vectored Interrupt Controller).

As in Lesson 17, you settle "did it preempt here?" with **two breakpoints** —
one on the next instruction, one in the ISR — and by **running free**, since
single-stepping disables interrupt checking.

## 2. The interrupt stack frame

On interrupt entry (FPU disabled) **SP drops by 8 words**. The datasheet's
**"Exception Entry and Return"** section shows the frame:

```
   (higher addresses)
       xPSR
       PC        <-- the return address / preemption point
       LR
       R12
       R3
       R2
       R1
       R0
  [ optional aligner word ]
   (lower addresses)  <-- SP
```

> Remember the datasheet draws memory high-to-low; flip it to match a debugger
> memory view.

### The punchline

Compare that list with the **AAPCS** (Lesson 9):

| AAPCS | Registers |
|---|---|
| **Caller-saved** (may be clobbered by a call) | **R0–R3, R12** + LR |
| **Callee-saved** (a function must preserve) | R4–R11 |

> **The interrupt stack frame saves precisely the registers that a C function is
> allowed to clobber.** The hardware saves the caller-saved set; the compiler's
> ordinary function prologue already saves the callee-saved set.

**Cortex-M interrupt entry complements the AAPCS — that is why a regular C
function can serve as an interrupt handler.**

This has a consequence worth stating:

> It **elevates the AAPCS** from a mere calling convention that could in
> principle differ per compiler, to a **hard rule that all compilers must
> implement identically**.

## 3. The special return value in LR

The handler returns in a completely standard way — **`BX LR`** — because it *is*
an ordinary function.

But the value in **LR is `0xFFFFFFF9`** — `-7` in two's complement, and **not a
valid code address**.

> **When such a special value is loaded into the PC, the Cortex-M hardware treats
> it as a return from interrupt.**

Executing `BX LR` therefore pops the stack frame, restores all registers to their
pre-interrupt state, and returns SP and PC to the exact preemption point.

### Data instead of a special instruction

> What other processors (e.g. MSP430) achieve with a **special instruction**
> (`RETI`), ARM Cortex-M achieves with **special data** — the LR contents.

The data-based solution is **more flexible and extensible**: several variants of
interrupt return exist, tabulated in the datasheet, selected by which special LR
value is used.

### The terminology in that table

| Term | Meaning |
|---|---|
| **Handler mode** | processor state while handling an exception (interrupt or fault) |
| **Thread mode** | executing regular code, e.g. your `while (1)` in `main()` |
| **Floating-point state** | FPU active; interrupts use the larger FPU stack frame |
| **MSP** / **PSP** | Main Stack Pointer / Process Stack Pointer |

**Cortex-M has two stack pointers**, `SP_main` and `SP_process`, but only one is
visible as `SP` depending on the CPU's internal state. This is **register
banking**, another ARM special — and it becomes important when you reach the
**RTOS** lessons.

## 4. The aligner word

The optional extra stack entry exists to **align the interrupt stack frame to an
8-byte boundary**.

**Why the hardware wants alignment:** it performs highly optimized **block
transfers** of registers to and from the stack. Interrupt entry and exit each
take only **12 clock cycles** — impressively fast for pushing or popping 8
registers.

If SP is misaligned when the interrupt hits, the hardware **skips one stack
entry** (inserting the aligner), making a **9-word** frame; on return the whole
9 words are removed and SP goes back to its original misaligned value.

> In practice **misalignment should never happen**: 8-byte stack alignment is an
> **AAPCS requirement**, and compilers ensure it. The concept still matters for
> the RTOS lessons.

## 5. The cost of the FPU

Re-enabling the FPU changes the picture substantially:

| | SP drop on entry | LR value |
|---|---|---|
| **without FPU** | 8 words | `0xFFFFFFF9` |
| **with FPU** | **26 words** | **`0xFFFFFFE9`** |

The floating-point stack frame is **more than four times bigger**.

> **Moral:** if you use the FPU you must size the stack **significantly** bigger,
> and you pay an additional price in **longer interrupt entry and exit time**.

---

## Key takeaways

1. Pend any interrupt manually via its pending bit — `ICSR.PENDSTSET` for
   SysTick, the NVIC for the rest.
2. The interrupt stack frame is exactly the **AAPCS caller-saved** set
   (R0–R3, R12, LR, PC, xPSR).
3. Hardware + compiler together save everything — hence plain C functions work as
   ISRs.
4. Cortex-M makes the AAPCS a hardware-enforced rule, not a per-compiler
   convention.
5. Return is a normal `BX LR`; the **magic is the special LR value**
   (`0xFFFFFFF9`), data rather than a special instruction.
6. Cortex-M has **two banked stack pointers**, MSP and PSP.
7. The **aligner word** keeps the frame 8-byte aligned for fast block transfers;
   entry and exit are ~12 cycles each.
8. **The FPU makes the interrupt frame 26 words** — size your stack accordingly.

---

## Glossary

| Term | Meaning |
|---|---|
| ICSR | Interrupt Control and State Register |
| `PENDSTSET` | ICSR bit that pends the SysTick interrupt |
| NVIC | Nested Vectored Interrupt Controller |
| Interrupt stack frame | R0–R3, R12, LR, PC, xPSR pushed by hardware |
| `EXC_RETURN` | The special LR value signalling interrupt return |
| Handler / Thread mode | Executing an exception / executing normal code |
| MSP / PSP | Main / Process Stack Pointer |
| Register banking | One register name, multiple physical registers |
| Aligner word | Padding keeping the frame 8-byte aligned |

---

## Pitfalls to remember

- **Trying the MSP430 counter trick on SysTick** — `STCURRENT` is write-clear.
- **Single-stepping to catch preemption** — interrupts are suppressed.
- **Undersizing the stack with the FPU enabled** — 26 words per interrupt, and
  nesting multiplies it.
- **Assuming R4–R11 are saved by hardware** — they are not; the compiler's
  prologue handles them.
- **Treating the LR value as an address** — `0xFFFFFFF9` is a signal, not a
  location.

---

**Next:** Lesson 19 switches toolchains to GNU-ARM and Eclipse; Lesson 20 covers
**race conditions**, which you must understand to work with interrupts safely.
