# Lesson 22 — Knowledge: RTOS Part 1 — What Is a Real-Time Operating System?

**Video:** <https://youtu.be/TEq3-p0GWGI> · **Transcript:** <https://www.state-machine.com/course/lesson-22.txt>
**Hands-on companion:** [PRACTICE.md](PRACTICE.md)

---

## The big idea

> **An RTOS kernel is software that extends the foreground/background
> architecture by letting you run multiple background loops — called threads or
> tasks — on a single CPU.**

The mechanism is to **exploit the interrupt hardware already in your processor**
to switch the CPU between those loops.

> **Terminology:** "RTOS" here means the **real-time kernel** — the component
> responsible for **multitasking**. Not hardware abstraction layers, device
> drivers, file systems or networking, which are sometimes lumped under the name.

> Lesson 18 (interrupts on Cortex-M) is a hard prerequisite. Re-watch it first.

---

## 1. The problem sequential code can't solve

Blinking a second LED **independently** by copy-pasting the code fails: the LEDs
blink **in sequence**, not simultaneously. That is the nature of sequential
code — you merely extended the hard-coded event sequence.

> To blink the LEDs truly independently **while preserving the simple sequential
> structure**, you need **two background loops running simultaneously**.

## 2. Definitions

| Term | Definition |
|---|---|
| **RTOS kernel** | Software that extends foreground/background by allowing multiple background loops (**threads** / **tasks**) on one CPU |
| **Multithreading / multitasking** | Switching the CPU context frequently from one thread to another to create the **illusion that each thread has the whole CPU to itself** |

> **Threads are essentially the background loops** of the foreground/background
> architecture.

## 3. First attempt: hack the return address

Inside an ISR, the **7th stack entry from the top** of the Cortex-M interrupt
frame holds the **PC** — the address the CPU returns to. Overwrite it with the
address of a different background loop, and on `BX LR` the CPU **returns
somewhere else entirely**.

That demonstrates three things:

1. **Switching the CPU between multiple background loops is possible.**
2. The general mechanism is to **exploit the interrupt processing hardware
   already in the processor**.
3. It illustrates **multitasking on a single CPU**.

### But it is illegal

The interrupt saved **blinky1's registers** and restores **blinky1's registers** —
yet returns to **blinky2**. The wrong thread gets the wrong register set.

> It happens to work for dead-simple blinky threads, but **will break down for
> more complex threads that use more registers**.

**The fix:** keep register sets separate → **each thread needs its own private
stack**.

## 4. Per-thread stacks

A stack is nothing more than **an area of RAM plus a pointer to its current top**:

```c
uint32_t stack_blinky1[40];               /* the RAM area  */
uint32_t *sp_blinky1 = &stack_blinky1[40];/* one word past the end */
```

The pointer starts **one word beyond the end**, because on ARM the stack grows
**down** — from the end of the array toward its beginning.

### Fabricating an initial stack frame

Instead of *calling* the thread functions, you **pre-fill each thread's stack
with a fabricated Cortex-M interrupt stack frame**, making it look as though the
thread had been preempted by an interrupt **just before** its first instruction.

Use the datasheet's exception-frame layout as the template, and note:

- **Start from the high-memory end** — the stack grows high → low.
- **8-byte alignment is required** (Lesson 18). A 40-word array ends on an 8-byte
  boundary, so **no aligner word is needed**.
- ARM uses a **full stack**: SP points at the **last used** entry, not the first
  free one. So to push: **decrement first, then dereference and write**.

| Entry | Value |
|---|---|
| **xPSR** | only **bit 24** set — the **THUMB state** bit. Cortex-M can't be in any other state, but historically the bit must be set. |
| **PC** | the **address of the thread function**, cast to `uint32_t` |
| LR, R12, R3–R0 | don't matter (a thread never returns) — but setting them to recognizable numbers makes the frame easy to spot in the debugger |

> Taking a function's address uses the same **`&`** operator as for a variable,
> yielding a **pointer to function** (Lesson 15; more in the state-machine
> lessons).

## 5. The context switch

With private stacks, switching threads becomes simple and **no longer touches
stack contents**:

```
at the end of the ISR:
  1. save the CPU's SP into the CURRENT thread's stack-pointer variable
  2. load the NEXT thread's stack-pointer variable into the CPU's SP
  3. return from interrupt
```

Now registers never get mixed: blinky1's registers are stored on **blinky1's
stack** and restored from the same stack.

### What gets preserved

A resumed thread continues **precisely at its point of preemption** — not at the
start of its function. The whole call chain (`main_blinky1` → `BSP_delay` →
`BSP_tickCtr`) is preserved **on that thread's private stack**.

## 6. The remaining problem: R4–R11

The Cortex-M interrupt stack frame follows the **AAPCS** (Lesson 18): it saves
only the registers a **function call may clobber** — and **not R4–R11**, which a
function must preserve.

That is fine for an ordinary ISR, because **an ISR runs to completion and returns
to the code it preempted**. If the ISR uses R7 it saves and restores it itself.

**But a context switch does not return to the preempted code.** It returns to
*another thread*, which may also use R7. That thread is **only partly executed**
— a fragment, not a complete function — so it is **not obliged to honour the
AAPCS**, and may leave R7 changed. By the time blinky1 resumes, **R7 is
clobbered**. The same argument applies to all of **R4–R11**.

### The complete algorithm

> **Save R4–R11 onto the thread's stack at the end of the ISR, right before
> switching away; restore them from the thread's stack right before returning to
> that thread.**

Concretely:

1. Append the 8 extra registers to the **fabricated** stack frame of every thread.
2. When saving the current context: push **R11 down to R4** on top of the ISR
   frame, and **subtract `0x20`** from SP before storing it in the thread's stack
   pointer.
3. When restoring the next thread: **add `0x20`** to its stack pointer before
   writing it to the CPU's SP, and restore **R11 down to R4**.

That is a complete, precise context-switch algorithm — ready to be automated in
software, which is Lesson 23.

---

## Key takeaways

1. An RTOS kernel lets multiple background loops (threads) share one CPU.
2. Multitasking = switching context fast enough to create the illusion of a
   dedicated CPU per thread.
3. The mechanism reuses the **interrupt hardware**.
4. **Each thread needs its own private stack** — otherwise register sets get
   mixed.
5. A thread's stack is pre-filled with a **fabricated interrupt stack frame**
   (xPSR with the THUMB bit, PC = thread function address).
6. Context switch = save SP to the current thread, load SP from the next thread.
7. A resumed thread continues **at its point of preemption**, with its whole call
   chain intact.
8. The hardware frame omits **R4–R11**, so the kernel must save and restore them
   itself.

---

## Glossary

| Term | Meaning |
|---|---|
| RTOS kernel | The multitasking component of an RTOS |
| Thread / task | A background loop with its own stack and context |
| Context switch | Swapping the CPU's state from one thread to another |
| Private stack | Per-thread RAM area holding its context and call chain |
| Fabricated stack frame | A hand-built interrupt frame making a thread look preempted |
| xPSR bit 24 | The THUMB state bit, which must be set |
| Full stack | SP points at the last used entry (decrement before writing) |
| AAPCS | The calling convention the Cortex-M frame complements |

---

## Pitfalls to remember

- **Changing only the stacked PC** — the wrong register set goes with it.
- **Forgetting R4–R11** — works for trivial threads, breaks for real ones.
- **Misaligning a thread stack** — the frame must be 8-byte aligned.
- **Initializing SP to the last element** instead of one past the end.
- **Forgetting the THUMB bit** in the fabricated xPSR.
- **Undersizing thread stacks** — each holds a full call chain plus two frames.

### Project settings that matter here

- **Turn the FPU off** — it simplifies interrupt processing (Lesson 18) and makes
  the frames match the discussion.
- **Give the heap a non-zero size** — the KEIL debugger's *semihosting* feature
  wants some.
- Calling two never-returning functions in a row gets the second **eliminated as
  unreachable**; guard it with an `if` on a `volatile` variable.

---

**Next:** Lesson 23 automates this manual procedure — the beginning of your own
RTOS kernel.
