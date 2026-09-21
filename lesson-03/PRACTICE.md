# Lesson 3 — Practice: Watching a Variable Live in RAM

**Concepts behind this exercise:** [KNOWLEDGE.md](KNOWLEDGE.md)
**Video:** <https://youtu.be/o9WpXYBqdPU>

---

## Projects in this lesson

| Directory | Toolchain | Target |
|---|---|---|
| `simulator-iar/` | IAR EWARM (`workspace.eww`) | Simulator (board optional) |
| `simulator-keil/` | KEIL MDK (`lesson.uvprojx`) | Simulator (board optional) |

## The code

```c
static int counter = 0;          /* now at file scope -> lives in RAM */

int main(void) {
    int volatile *p_int;
    p_int = &counter;            /* pointer holds the address of counter */
    while (*p_int < 21) {
        ++(*p_int);              /* increment through the pointer */
    }

    p_int = (int *)0x20000002U;  /* fabricated, deliberately misaligned */
    *p_int = (int)0xDEADBEEF;    /* the "horrible hack" */

    return 0;
}
```

---

## Step-by-step

### 1. Baseline — a local in a register

Start from the Lesson 2 code with `counter` defined **inside** `main`. Build,
debug, and step: `counter` appears in the **Locals** view and lives in a
register (e.g. `R0`). The machine instructions manipulate it directly — no
memory traffic at all.

### 2. Move the variable out of `main`

Move the definition to file scope (`static int counter = 0;`), rebuild, and
restart the debugger.

`counter` **disappears from Locals** — it is no longer local. To see it:

- **View → Watch → Watch 1**, click the first line and type `counter`.

Its address now starts with `0x2...` — it lives in RAM.

### 3. See it in the memory view

Set the **Memory** view to address `0x20000000` and switch the display to
**4×Units** so 32-bit integers are readable. Find the word corresponding to
`counter`.

### 4. Trace the load/store pattern

Single-step through one iteration in the **disassembly** window and narrate each
instruction:

| Instruction | What it does |
|---|---|
| `LDR.N R0, ??main2` | loads the **address** of `counter` into `R0` (from the literal pool) |
| `LDR R0, [R0]` | loads the **value** at that address into `R0` |
| `ADDS R0, R0, #1` | increments the value in the register |
| `LDR.N R1, ??main2` | loads the address again, into `R1` |
| `STR R0, [R1]` | stores the register back to memory |

Watch the Watch1 view and the Memory view both update on the `STR` — and *only*
on the `STR`. Scroll to the literal-pool label to confirm the constant stored
there really is the address of `counter`.

### 5. Introduce the pointer

Add `int volatile *p_int;`, assign `p_int = &counter;`, and replace `counter`
with `*p_int` throughout the loop. Build and debug with **both** views open:

- **Watch 1** → `counter`
- **Locals** → `p_int`

Step through and confirm `*p_int` and `counter` change together — the pointer is
an alias.

### 6. Compare the generated code

Put the disassembly side by side with the pre-pointer version. The
address-loading `LDR` has moved to the **top**, and one duplicate `LDR` is gone
entirely. The pointer made the machine code *simpler*, not more complex.

### 7. Use a breakpoint instead of stepping

Set a breakpoint after the loop and press **Go** to run at full speed. Confirm
`counter == 21` on arrival — much faster than 21 single steps.

### 8. Negotiate with the compiler

Try each of these in turn, pressing `F7` after each:

```c
p_int = 0x20000002;      /* rejected */
p_int = 0x20000002U;     /* still rejected */
p_int = (int *)0x20000002U;   /* accepted — the cast forces it */
```

Then write a recognizable value:

```c
*p_int = (int)0xDEADBEEF;
```

### 9. Watch the corruption happen

Set a breakpoint on the `p_int` reassignment, run to it, then step **one machine
instruction at a time** with the Watch and Memory views visible:

1. `LDR` loads the fabricated address → confirm it in `p_int` (Locals) and in
   the register view.
2. `LDR` loads `0xDEADBEEF` into another register.
3. `STR` writes it to memory.

Because `0x20000002` is **misaligned** (not a multiple of 4), the value lands
half on `counter` and half on the next word. Observe both change.

> Run this step on the **board** if you have one, to prove the simulator isn't
> being lenient. Set the debugger to TI-Stellaris / Stellaris ICDI and enable
> **Use flash loader**. Cortex-M4 accepts this misaligned write; a Cortex-M0+
> would fault.

---

## Exercises

1. **Align it.** Change the fabricated address to `0x20000004` and compare the
   effect on memory. Which variables are touched now?
2. **Predict the addresses.** Add a second file-scope variable and predict its
   address before looking. Were they placed where you expected?
3. **`static` or not.** Remove `static` from `counter`. Does the address change?
   Does anything else? (Scope and linkage are covered properly in Lesson 9.)
4. **Pointer to pointer.** Declare `int **pp = &p_int;` and increment `counter`
   via `**pp`. Explain the extra `LDR` in the disassembly.
5. **Danger drill.** Declare an *uninitialized* pointer and dereference it in
   the simulator. Where did the write land? Why is this catastrophic on real
   hardware?

---

## Self-check

- [ ] I can explain why `counter` vanished from Locals when moved to file scope
- [ ] I found `counter` in the memory view at its RAM address
- [ ] I can name each instruction in the LDR → ADDS → STR sequence
- [ ] I know why a bare number cannot be assigned to a pointer without a cast
- [ ] I saw a misaligned 32-bit write straddle two words of memory
