# Lesson 19 — Knowledge: GNU-ARM, Eclipse, and Code Portability

**Video:** <https://youtu.be/BBF3ZMi8WK4> · **Transcript:** <https://www.state-machine.com/course/lesson-19.txt>
**Hands-on companion:** [PRACTICE.md](PRACTICE.md)

---

## The big idea

Switching toolchains is a good way to find out what **code portability** really
means. Most of your code moves unchanged — thanks to **CMSIS**. What breaks is
exactly the part that used **compiler-specific extensions**: the startup code and
parts of the BSP.

---

## 1. Choosing a GNU-ARM toolset

Many GNU-ARM toolsets exist. The deciding factor is **support for your board's
debugger interface** — for the TivaC LaunchPad that is **Stellaris-ICDI**, which
points to TI's **Code Composer Studio (CCS)**.

CCS is free and unlimited **when used with the GNU-GCC toolset**. During
installation you must expand *32-bit ARM MCUs*, select **Tiva-C Series support**,
and explicitly select the **GCC ARM Compiler**.

> Avoid install directories with spaces or non-standard characters.

## 2. Eclipse concepts

| Concept | Meaning |
|---|---|
| **Workspace** | a grouping of related projects; common to all Eclipse-based IDEs |
| **Edit Perspective** | the screen layout for editing code |
| **Debug Perspective** | the layout with debugger views (disassembly, registers, …) |

> Recommendation: use a **dedicated workspace** for this course rather than one
> default workspace for everything.

### Eclipse's implicit project membership

**All source files in the project directory are automatically part of the
project** — you don't add them explicitly as in IAR.

Convenient, but it cuts both ways: copying in a second startup file gives you
**two** startup files in the build, and you must delete one.

Also note: Eclipse projects generally **cannot be opened by double-clicking**.
You launch the IDE and then **File → Import → Existing Projects into Workspace**.

## 3. What is wrong with typical vendor startup code

The startup code CCS generates is typical of silicon vendors, and shows all the
shortcomings covered in Lessons 13–15:

1. **Proprietary exception names**, not CMSIS-compliant.
2. **The vector table must be edited** every time you start or stop using a
   handler — plus a prototype added at the top of the file.
3. **Exception handlers are endless loops** that tie up the CPU. If one ever
   executes, the system freezes: **denial of service**, unacceptable in
   production code.

### And with the typical linker script

The generated `.lds` linker script allocates the **stack as the last section in
RAM**.

> That is a **mistake**. The stack grows toward **lower** addresses on ARM, so an
> overflow silently **damages the RAM sections above it**.
>
> This is the likely cause of failure in the infamous **Toyota unintended
> acceleration** cases — see the article
> *"Are we shooting ourselves in the foot with stack overflow?"*.

**The better arrangement: put the stack as the *first* section in RAM.** Then:

- an overflow cannot damage other RAM sections, and
- overflowing into **unmapped memory below RAM** raises a **HardFault**
  automatically — so you find out about it.

## 4. IAR extension → GNU equivalent

This is the portability table worth memorizing:

| Purpose | IAR | GNU-ARM |
|---|---|---|
| Place a symbol in a section | `@ ".intvec"` | `__attribute__((section(".isr_vector")))` |
| Function uses no stack | `__stackless` | `__attribute__((naked))` |
| Overridable default symbol | `#pragma weak` alias | `__attribute__((weak, alias("Default_Handler")))` |
| Enable interrupts | `__enable_interrupt()` | `__enable_irq()` |
| Disable interrupts | `__disable_interrupt()` | `__disable_irq()` |

### `weak` and `alias`

- **`weak`** — the definition **can be overridden** by another; the linker
  quietly discards the weak one instead of reporting a duplicate symbol.
- **`alias`** — if the symbol isn't defined, use the alias symbol instead.

So defining `SysTick_Handler` in your application makes the linker take **your**
(non-weak) definition; omitting it makes the linker take **`Default_Handler`** —
with no errors either way, and **no editing of the vector table**.

> Not every handler is aliased: the **standard fault handlers are defined in the
> startup code itself**, so your application needn't define them.

## 5. What CMSIS buys you

Most of the application code — `main.c`, `bsp.c`, `bsp.h`, the device header —
moves **unchanged**, because it is written against **CMSIS**.

CMSIS is **not bundled with CCS** the way it is with IAR, so you supply it
yourself: a `CMSIS/Include` directory, placed alongside your lessons so all
projects can share it.

### Absolute vs. relative include paths

Adding the path by browsing produces an **absolute** path that works only on your
machine. Eclipse supports **relative** paths via build variables:

```
${PROJECT_LOC}/../CMSIS/Include
```

> Prefer **forward slashes** — Windows accepts both, and forward slashes are
> more universal.

## 6. Production-quality startup for GNU

The replacement startup code and its **matching linker script** (they must match
closely) are:

- compliant with **CMSIS**;
- vector table placed via `section(".isr_vector")`, which the linker script makes
  the **first section in ROM**, hence at address **0**;
- initial SP taken from **`&__stack_end__`**, a **linker-provided symbol**
  declared `extern` in the startup file (the same technique as `CSTACK$$Limit`
  in Lesson 15);
- **stack as the first section in RAM**, with its size set by a `STACK_SIZE`
  symbol at the top of the linker script;
- every CMSIS-named handler present and weak-aliased to `Default_Handler`;
- **fault handlers using inline assembly** that carefully avoids the stack (which
  may be corrupted), storing fault information in **r0 and r1** and branching to
  **`assert_handler`** for damage control.

> This startup code and linker script work with **any GNU-ARM-based toolset**,
> not just CCS, and are easily adapted to any Cortex-M microcontroller.

---

## Key takeaways

1. Portable code is CMSIS-based code; compiler extensions are what break.
2. Eclipse auto-includes every source file in the project directory.
3. Vendor startup code is typically non-CMSIS, hand-edited, and freezes on fault.
4. **Put the stack first in RAM** so overflow faults instead of corrupting data.
5. `weak` + `alias` give you a vector table you never have to edit.
6. Know the IAR↔GNU extension mapping.
7. Use **relative** include paths so projects build on any machine.

---

## Glossary

| Term | Meaning |
|---|---|
| GNU-ARM / GCC | Free, unlimited ARM compiler toolchain |
| CCS | TI's Eclipse-based Code Composer Studio |
| Workspace / Perspective | Eclipse project grouping / screen layout |
| `.lds` | GNU linker script |
| `__attribute__((section(...)))` | GNU extension placing a symbol in a section |
| `__attribute__((naked))` | GNU extension: no compiler-generated prologue/epilogue |
| `weak` / `alias` | Overridable definition / fallback symbol |
| `__enable_irq()` / `__disable_irq()` | CMSIS intrinsics for the PRIMASK bit |
| `__stack_end__` | Linker-provided symbol for the top of stack |

---

## Pitfalls to remember

- **Two startup files** in an Eclipse project after copying files in.
- **Absolute include paths** that break on another machine.
- **Stack last in RAM** — silent corruption instead of a clean fault.
- **Editing the vector table by hand** instead of using weak aliases.
- **Endless-loop fault handlers** inherited from vendor code.
- **Mismatched startup code and linker script** — they are a pair.

---

## Further reading

- *Building Bare Metal ARM Systems with GNU* — a 10-part article; written for
  ARM7/ARM9 but much still applies to Cortex-M.
- *Are we shooting ourselves in the foot with stack overflow?*

---

**Next:** Lesson 20 covers **race conditions** — essential for working safely
with interrupts.
