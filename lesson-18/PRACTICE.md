# Lesson 18 — Practice: Dissecting Cortex-M Interrupt Entry and Return

**Concepts behind this exercise:** [KNOWLEDGE.md](KNOWLEDGE.md)
**Video:** <https://youtu.be/O0Z1D6p7J5A>

---

## Projects in this lesson

| Directory | Toolchain | Target |
|---|---|---|
| `tm4c123-iar/` | IAR EWARM (`workspace.eww`) | EK-TM4C123GXL LaunchPad |

A board is required — you will be reading CPU registers and the live stack.

---

## Part A — Trigger SysTick at will

### 1. Why the MSP430 trick won't work

On MSP430 you wrote the counter one below its limit. **`STCURRENT` on Cortex-M is
write-clear** — any write clears it without triggering anything.

### 2. Use the pending bit instead

The datasheet describes a more direct route: set the interrupt's **pending bit**.

| Register | Bit | Effect |
|---|---|---|
| **ICSR** | **26 — `PENDSTSET`** | pends the SysTick interrupt |

> Cortex-M has a pending bit for **every** interrupt source, so this technique
> triggers **any** interrupt. Most pending bits live in the **NVIC**.

### 3. Set up the two-path experiment

1. Flash the code, breakpoint at the **top of the `while (1)` loop**, run.
2. When hit, **move** that breakpoint to the **very next `LDR.N`** instruction,
   and set another breakpoint in **`SysTick_Handler`**.
3. In the **Register** panel, select the **System Control Block**, expand
   **ICSR**, find **`PENDSTSET`** and set it to **1**.

You are stopped at a `MOVS`, with the interrupt line about to go high, and
breakpoints on both possible paths:

- next `LDR.N` → hit if **no** preemption;
- inside the handler → hit if the interrupt preempts **right here**.

### 4. Run free

> **Do not single-step** — that disables the interrupt check after each
> instruction.

**Result: the SysTick interrupt fires.** You now have a verified way to trigger
an interrupt at any machine instruction you choose.

---

## Part B — The interrupt stack frame

### 5. Turn the FPU off first

**Project Options → General Options → Floating Point Unit → None.**

The FPU speeds up floating-point maths but complicates interrupt processing.
Rebuild without it. (You turn it back on in Part D.)

### 6. Watch the stack

Set the breakpoints and trigger SysTick exactly as before, but also open a
**memory view on the stack**. The ARM stack grows **down**, so scroll so the
current **SP sits at the bottom** of the view.

### 7. Identify the eight words

When you hit the breakpoint inside the handler, **SP has dropped by 8 entries**.

Find the datasheet's **"Exception Entry and Return"** section and its picture of
the interrupt stack frame without FPU. Flip it (datasheets run high-to-low) and
match it to your memory view:

```
   xPSR
   PC        <-- the return address
   LR
   R12
   R3
   R2
   R1
   R0        <-- SP
```

Confirm the saved **PC** by scrolling the disassembly: it is the **`LDR.N`**
instruction inside your `while (1)` loop — the preemption point.

### 8. The recognition moment

Mark the registers saved in the frame, then recall the **AAPCS** from Lesson 9:

| AAPCS | Registers |
|---|---|
| may be clobbered by a call (**caller-saved**) | **R0–R3, R12** |
| must be preserved by the callee | R4–R11 |

> **The frame saves exactly the registers a C function is allowed to clobber.**
> The hardware covers the caller-saved set; the compiler's ordinary prologue
> already covers R4–R11.

**That is why a plain C function can be an interrupt handler on Cortex-M.**

Note the consequence: this promotes the AAPCS from a per-compiler convention to a
**hard rule every compiler must implement identically**.

---

## Part C — The return

### 9. Step to the return

The handler returns with an entirely standard **`BX LR`** — it *is* a normal C
function.

### 10. Look at LR

**LR holds `0xFFFFFFF9`** — `−7` in two's complement, and **not a valid code
address**.

> When such a special value is loaded into the PC, the Cortex-M hardware treats
> it as a **return from interrupt**.

### 11. Execute it and verify

After `BX LR`:

- **SP returns** to its pre-interrupt value (e.g. `0x3F8`);
- **all registers are restored** to their pre-interrupt state;
- **PC lands back in `while (1)`**, exactly at the preemption point.

> What MSP430 does with a **special instruction** (`RETI`), Cortex-M does with
> **special data**. The data approach is more flexible — the datasheet tabulates
> several interrupt-return variants selected by the LR value.

### 12. Read the return-variant table

Skim it, and learn the terminology:

| Term | Meaning |
|---|---|
| **Handler mode** | handling an exception (interrupt or fault) |
| **Thread mode** | running regular code, e.g. `main()`'s loop |
| **Floating-point state** | FPU active → larger interrupt stack frame |
| **MSP / PSP** | Main / Process Stack Pointer |

Cortex-M has **two stack pointers**; only one is visible as `SP` at a time —
**register banking**. This matters in the RTOS lessons.

---

## Part D — Two refinements

### 13. Force the aligner word to appear

The optional aligner exists so the frame is **8-byte aligned**, which lets the
hardware do **optimized block transfers** — entry and exit are only **12 cycles
each**, for 8 registers.

To see it, misalign SP deliberately:

1. Hit the breakpoint in `while (1)` and pend SysTick as before.
2. **Pre-fill the unused stack above SP with `0xDEADBEEF`**, so changes are
   obvious.
3. **Subtract 4 from SP.** Check that the result (e.g. `0x3F4`) is **not
   divisible by 8**.
4. Run free into the handler.

Observe: the 8-register frame is pushed, but **one entry is skipped** (e.g. at
`0x3F0`) — that is the **aligner word**. On return, all **9** words are removed
and SP goes back to the original misaligned `0x3F4`.

> In practice this never happens: 8-byte alignment is an **AAPCS requirement** and
> compilers guarantee it. The concept returns in the RTOS lessons.

### 14. Measure the cost of the FPU

Re-enable the FPU in the project options, rebuild, and repeat the entry
experiment.

| | SP before | SP after | LR |
|---|---|---|---|
| without FPU | `0x3F8` | −8 words | `0xFFFFFFF9` |
| **with FPU** | `0x3F8` | **`0x390` — 26 words** | **`0xFFFFFFE9`** |

The floating-point frame is **more than four times bigger**, and the special LR
value differs to select that frame format.

> **Moral:** using the FPU means sizing the stack **significantly** bigger, and
> paying longer interrupt entry/exit times.

---

## Exercises

1. **Pend a different interrupt.** Find another interrupt's pending bit in the
   NVIC and trigger it. Does its handler run?
2. **Count the cycles.** Using a GPIO toggle and a logic analyser, measure the
   real interrupt entry latency and compare with the quoted 12 cycles.
3. **Clobber test.** Have `SysTick_Handler` deliberately modify R0–R3 and verify
   that the preempted code is unaffected. Then try R4 without saving it.
4. **Read the frame in C.** Write a handler that takes the stacked PC from the
   frame and stores it — the beginning of a crash dump.
5. **Stack budget.** With the FPU on and interrupts able to nest one level,
   compute the worst-case stack requirement.
6. **Alignment check.** Verify in the disassembly that the compiler keeps SP
   8-byte aligned at every function boundary.
7. **Compare to MSP430.** Tabulate side by side: what the hardware saves, how the
   return works, and what the compiler must add — for both processors.

---

## Self-check

- [ ] I triggered SysTick via `ICSR.PENDSTSET` and confirmed preemption at a
      chosen instruction
- [ ] I identified all 8 words of the interrupt stack frame in memory
- [ ] I can explain how the frame complements the AAPCS caller-saved set
- [ ] I found `0xFFFFFFF9` in LR and know what it signals
- [ ] I forced the aligner word to appear by misaligning SP
- [ ] I measured the FPU's effect on frame size (26 words) and LR (`0xFFFFFFE9`)
- [ ] I can state why the FPU forces a bigger stack
