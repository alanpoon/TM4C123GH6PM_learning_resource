# Lesson 10 — Practice: Breaking the Stack on Purpose

**Concepts behind this exercise:** [KNOWLEDGE.md](KNOWLEDGE.md)
**Video:** <https://youtu.be/jmzvued3w3Y>

---

## Projects in this lesson

| Directory | Toolchain | Target |
|---|---|---|
| `tm4c123-keil/` | KEIL MDK (`lesson.uvprojx`) | EK-TM4C123GXL LaunchPad |
| `tm4c123-iar/` | IAR EWARM (`workspace.eww`) | EK-TM4C123GXL LaunchPad |
| `stm32c031-keil/` | KEIL MDK (`lesson.uvprojx`) | STM32 NUCLEO-C031C6 |

### Source files — note there are three `main`s

| File | Demonstrates |
|---|---|
| `main.c` | the stack-corrupting/overflowing `fact()` — **deliberately broken** |
| `main_fact.c` | the factorial variant |
| `main_swap.c` | the `swap()` / pointer-argument segment |
| `delay.c` / `delay.h` | the module from Lesson 9 |

> To run the `swap()` part of the lesson, **copy `main_swap.c` over `main.c`**
> (the header comment in `main.c` says the same).

> **Board note:** the Tiva C Series LaunchPad replaced the older Stellaris
> LaunchPad. For this course they are interchangeable — the same code runs on
> both; only the branding and header-file names changed.

---

## Part A — The stack is full of garbage

### 1. Hack `fact()` to consume stack

Add a local array to the recursive factorial:

```c
unsigned fact(unsigned n) {
    unsigned foo[10];
    foo[n] = n;                        /* use it, or it gets optimized away */
    if (n == 0U) { return 1U; }
    else { return foo[n] * fact(n - 1U); }
}
```

Without the assignment the compiler warns that `foo` is unreferenced and removes
it.

### 2. Open the dedicated stack views

- **Raw memory view** positioned at SP (as in Lesson 9).
- **View → Stack → Stack 1** — IAR's dedicated stack view.
- **View → Call Stack** — shows the nest of active calls.

> In the Call Stack view at the top of `main()`, note that `main` was itself
> called from **`__call_main`** — part of the **startup code** (Lessons 13–15).

### 3. Watch instances pile up

Put a breakpoint on the recursive call inside `fact()` and run.

- First hit: `foo` appears in the Stack view — it lives on the stack.
- After the recursive call: **a second instance of `foo`** appears on top.
- Again: **a third**. Each activation gets its own copy.

### 4. Look at what's inside `foo`

Each instance already contains **values you never wrote**. That data is left
over from previous uses of the RAM — in this case it looks like a flash-memory
image, probably left by the flash loader that programmed your code.

> **The content of the stack is garbage.** Initialize every automatic variable
> explicitly. The stack of dishes is a stack of *dirty* dishes.

---

## Part B — Overflow the stack

### 5. Hammer harder

Increase the array by an order of magnitude:

```c
unsigned foo[100];
```

Keep the breakpoint inside `fact()` and run.

### 6. Watch it die

- Each recursion consumes a much bigger frame; the stack grows fast.
- By about **5 levels** of nesting, SP reaches the **start of RAM**
  (`0x20000000`) — there is nowhere lower to grow.
- Continue: the program **freezes** and stops hitting the breakpoint.

### 7. Diagnose it

Break in manually and check two things:

1. **SP is below valid RAM.**
2. The program is spinning in an endless loop in **`BusFault_Handler`** — the
   default exception handler from the toolchain's startup code.

> Form the habit now: **program hanging in a hardware exception → check SP.**

Note that overflow does not always fault. It can instead quietly corrupt other
data, which is far harder to find.

### 8. Resize the stack

**Options → Linker → Config → Override default → Edit → Stack/Heap Sizes**

- Observe the default **2 KB** stack (shown in hex; decimal is accepted).
- Set it to **1 KB**, which is plenty for this stage of the course — after you
  remove the `fact()` hack.
- Set the **heap to 0**. The heap serves `malloc`/`free`, and in real-time
  embedded work it causes more harm than good.
- Save, and choose a location for the now project-specific linker script
  (`project.icf`). It must travel with the project.

---

## Part C — The stack-corruption mystery

### 9. Set up the crime scene

```c
unsigned foo[6];       /* six elements: valid indices 0..5 */
...
x = fact(7U);          /* set a breakpoint here */
```

Index 7 writes **two slots past the end**.

### 10. Watch the crime

Run to the breakpoint, then single-step the disassembly:

| Instruction | What it does |
|---|---|
| `PUSH {R4, LR}` | familiar from Lesson 9 |
| `SUB SP, SP, #0x18` | allocates `foo` (6 × 4 bytes) — **makes room, doesn't clean it** |
| `ADD R1, SP, #0` | address of `foo` = current top of stack |
| `STR R0, [R1, R0, LSL #2]` | `foo[n] = n`; the `LSL #2` scales the index by the 4-byte element size |

**Watch the `STR` carefully.** With `n == 7`, it lands two words past the end of
`foo` — exactly on the **saved LR**. The return address is now corrupted.

> C let you do this with no complaint. It does not check indices; it trusts you.

### 11. Follow the story, with strategic breakpoints

Don't single-step thousands of instructions. Put a breakpoint at **the return
from `fact()`** — you know the return address is the problem.

At that breakpoint:

- All recursive calls are nested; this is peak stack usage. Verify there is
  **no** overflow — this failure is different.
- Continue repeatedly and watch the stack **unwind** one frame at a time.

At the last return:

- `ADD SP, ...` removes `foo`.
- `POP {R4, PC}` restores the **corrupted** value **7** into the PC. It succeeds
  only because 7 is **odd** — an even value would fault immediately (Lesson 8).
- PC becomes **6**, inside the **exception/interrupt vector table**. That memory
  is *data*, but the CPU decodes it as 16-bit instructions — two disassembly
  steps per 32-bit vector. The disassembly view is misleading here.
- `main()` happens to follow the vector table in Flash, so the CPU falls into
  **`main()` through the back door** and pushes a fresh frame — even though the
  previous `main()` never returned.

### 12. Watch the slow leak

Remove the return-breakpoint, keep the one at `fact(7U)`, and press Continue
repeatedly. Each press runs a whole cycle: recurse → corrupt → re-enter `main()`
through the vector table. **The stack grows a little every cycle** because
`main()` never pops its frame.

Remove the last breakpoint and let it run. It ends where you now expect: stack
overflow → **BusFault**.

---

## Part D — Pointer arguments

*(Copy `main_swap.c` over `main.c` for this part.)*

### 13. Prove arguments are passed by value

Modify `delay()` to decrement its own argument:

```c
void delay(int iter) {
    int volatile counter = 0;   /* volatile, or the loop vanishes */
    while (iter > 0) { --iter; }
}
```

Update the prototype in `delay.h` — the signature changed.

Call it with a **variable** instead of a constant, and breakpoint before and
after the call:

- Before: `x == 1000000`.
- After: **`x` is still 1000000**, even though `delay()` decremented its copy
  all the way to 0.

> C passes arguments **by value**. The function gets a copy and can never change
> the caller's variable.

### 14. Write `swap()` — first the broken version

```c
void swap(int x, int y) {
    int tmp = x;
    x = y;
    y = tmp;
}
...
int x = 1, y = 2;
swap(x, y);     /* does nothing useful */
```

Confirm it doesn't work. Remember the prototype — "require prototypes" is on.

### 15. Fix it with pointers

The syntax helps you — just add the stars:

```c
void swap(int *x, int *y) {
    int tmp = *x;
    *x = *y;
    *y = tmp;
}
...
swap(&x, &y);   /* take the addresses */
```

Step through the disassembly and name each step:

- Addresses of `x` and `y` prepared in **R0 and R1** (AAPCS).
- Value of `x` copied to **R2** (the `tmp`).
- Value of `y` loaded into R3 and stored at the address of `x`.
- `tmp` stored at the address of `y`.

After the return, `x` and `y` are genuinely exchanged.

---

## Part E — Returning a pointer to a local

### 16. Write the broken version

Make `swap()` also return the original `(x, y)` pair as an array:

```c
int *swap(int *x, int *y) {
    int tmp[2];
    tmp[0] = *x;  tmp[1] = *y;
    *x = tmp[1];  *y = tmp[0];
    return tmp;          /* the compiler WARNS — ignore it for now */
}
```

Use the returned pair to keep swapping the LED on/off delay times.

### 17. Watch the data evaporate

Set up a raw memory view around SP (you need to see a bit more than the CSTACK
view shows) and breakpoint at the **return from `swap()`**:

- At the breakpoint: the stack holds `tmp`, plus `x` and `y`.
- After stepping out: the stack holds only `x` and `y`. `tmp` is still visible
  in the *raw* memory view, but it is now **above SP** — outside the stack.

Now breakpoint at the **second** `delay()` call:

- `iter` in R0 is **0**, not the expected 500,000.
- The raw memory view shows why: the first `delay()` call reused that stack
  space and destroyed the values.

> Returning a pointer to a local is always wrong. Locals **go out of scope** when
> the function returns — they no longer exist.

### 18. Fix it with `static`

```c
static int tmp[2];
```

- The warning disappears.
- At the return from `swap()`, `tmp` is **no longer on the stack** — it now lives
  in ordinary RAM near the start of the RAM region.
- The second `delay()` receives the correct **500,000**.
- Remove the breakpoints: the LED blinks with the alternating pattern.

### 19. Clean up the last warning

Comment out `return 0;` at the end of `main()`. The standard wants `main()` to
return `int` *and* wants every non-`void` function to return explicitly, but
`while (1)` makes the return unreachable — the two cannot both be satisfied.
This course opts for a clean build.

---

## Exercises

1. **Prove the dirt.** Declare an uninitialized local `int` and print/inspect its
   value across several calls. Is it ever the same twice?
2. **Find your limit.** With `foo[100]` and a 1 KB stack, calculate the maximum
   recursion depth before overflow. Then verify it empirically.
3. **Silent corruption.** Make the out-of-bounds write land on a *local
   variable* instead of the saved LR. Does anything fault? How would you have
   found this bug?
4. **Guard value.** Fill the bottom of the stack with a known pattern at startup
   and check it periodically — a poor man's stack-overflow detector. Where would
   you put the check?
5. **Heap off.** Set the heap to 0 and then call `malloc()`. What happens, and at
   which build stage do you find out?
6. **Scope drill.** Add a `static` counter inside `delay()` that counts
   invocations. Where does it live, and what is its initial value?

---

## Self-check

- [ ] I saw multiple instances of a local array stacked up during recursion
- [ ] I saw garbage in freshly allocated locals and can explain where it came from
- [ ] I overflowed the stack and diagnosed it from SP + `BusFault_Handler`
- [ ] I changed the stack size in the linker settings and set the heap to 0
- [ ] I corrupted the saved LR with an out-of-bounds write and followed the
      aftermath
- [ ] I demonstrated pass-by-value, then fixed `swap()` with pointers
- [ ] I saw returned stack data get destroyed, and fixed it with `static`
