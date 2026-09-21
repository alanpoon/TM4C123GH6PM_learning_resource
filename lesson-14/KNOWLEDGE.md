# Lesson 14 — Knowledge: Startup Code Part 2 — The Build Process

**Video:** <https://youtu.be/DfiwWxTRIZE> · **Transcript:** <https://www.state-machine.com/course/lesson-14.txt>
**Hands-on companion:** [PRACTICE.md](PRACTICE.md)

---

## The big idea

To replace the library's generic vector table with your own, you have to
understand **how the linker chooses what goes into the final image**. That means
understanding object files, relocation, symbol resolution, library linking
rules, and the linker script.

---

## 1. Cross-development

Every build step runs on the **host machine** (your PC) even though the program
is for a completely different **target machine** (the LaunchPad). This is
**cross-development**, and it is characteristic of embedded systems — it makes
no sense to run a compiler on a small embedded target.

Contrast with desktop **native development**, where host and target are the same
machine.

## 2. The build pipeline

```
main.c  ──compiler──▶ main.o   ┐
delay.c ──compiler──▶ delay.o  ├──linker──▶ final image (a.out / c.out)
          libraries (.a)       │
          linker script (.icf) ┘
```

- The **compiler** turns each source file into an **object file**.
- The **linker** combines all object files, libraries and the linker script into
  the final program.

## 3. Object files and the ELF format

Object files contain **relocatable** machine code — not directly executable,
because it is **not yet committed to specific addresses**. The linker combines
the objects, resolves cross-module references, and **fixes the addresses**.

Object files live in the project's `Debug\Obj` directory. Viewed as text they
are mostly binary garbage, but:

- distinct **sections** are visible, and
- the first bytes spell **`ELF`** — **Executable and Linkable Format** (also
  "Extensible Linking Format"), one of the most popular object-file formats.

### Tools for inspecting ELF

| Tool | From |
|---|---|
| `ielfdumparm.exe` | the IAR toolchain |
| `objdump` | the GNU compiler collection — works on IAR's ELF files too, since ELF is standard |

An ELF dump contains `.text`, `.data`, `.bss` — plus symbol information for the
linker and a great deal of **debug information**.

> **Never judge code size from the size of an object file.** Machine code is a
> small part of it. The **linker map file** is the only reliable source.

The final image (`c.out`) is *also* an ELF file, so the same tools dump it. That
makes an ELF dump one of the quickest ways to see the generated disassembly
without loading anything onto the target.

## 4. What relocation actually means

Comparing the dump of `main.o` against the final image shows most instructions
are identical — but some **encodings differ**:

### Fixing a call

`BL` is a **PC-relative** instruction: the branch target is a signed offset
**encoded in the instruction itself**.

| | Encoding of `BL delay` |
|---|---|
| in `main.o` | `0xF7FF 0xFFFE` — a placeholder offset (`0x7FFFFFE`) |
| in `c.out` | `0xF000 0xF820` — the real offset |

The object file cannot know where `delay()` will end up; the **linker patches it**
once it decides the layout.

### Fixing data addresses

Addresses of variables aren't known at compile time either. In the **constant
pool** following `main`'s code, all variable addresses are **zero** in the object
file; the linker fills them in after placing the variables.

> Because the linker must understand and patch **binary opcodes**, the linker is
> **target-specific**. An x86 linker cannot link ARM programs, even though both
> use ELF. You need a compiler **and** a linker for the same processor.

## 5. Symbol resolution

Every object file has:

- **exported symbols** — defined here, usable by others
  (`main.o` exports `p1`, `w`, `t`, `main`, …)
- **imported symbols** — needed here but defined elsewhere
  (`main.o` imports `delay`)

The linker processes one object at a time, maintaining two lists: **exported**
and **undefined**.

### Worked example

| Step | Exported list | Undefined list |
|---|---|---|
| start | *(empty)* | `__vector_table` *(IAR-specific starting symbol)* |
| after `main.o` | `p1, w, t, main, …` | `__vector_table`, `delay` |
| after `delay.o` | … `delay` | `__vector_table` |
| → search libraries | | |

### Two different linking rules

| Input | Rule |
|---|---|
| **Object files listed in the project** | **always** linked into the image — order doesn't matter |
| **Objects inside a library** | linked in **only if** they export a symbol currently in the undefined list |

A **library** is simply a bundled collection of object files (`.a` = *archive*).

Continuing the example: `__vector_table` is exported by `vector_table_M.o`
inside IAR's `rt7M_tl.a`. Pulling that object in adds *its* imports
(`__iar_program_start`, `BusFault_handler`, …) to the undefined list, so the
linker keeps searching the same library — finding `cstartup_M.o`, which in turn
imports `__iar_data_init3`, `__low_level_init`, and `main`. `main` resolves
immediately, from your own `main.o`.

Eventually the undefined list empties and linking ends. If instead the linker
exhausts all objects and libraries with symbols still undefined, you get a
**linker error** listing the unresolved references.

> **Fixes:** add the missing object or library; sometimes **change the order** of
> libraries; occasionally list a library **more than once** to resolve circular
> dependencies among libraries.
> Further reading: ["Library order in static linking"](http://eli.thegreenplace.net/2013/07/09/library-order-in-static-linking).

### Design rule for your own libraries

Library objects typically contain **one function or one variable each**. That
fine granularity ensures only what is actually needed gets pulled in.

> If you build your own libraries, **keep objects small and nimble** — ideally
> one function or one global variable per module. Fat objects bloat the image.

## 6. The strategy for replacing the vector table

Now the plan is obvious:

> Define `__vector_table` in **your own object module**, linked **directly** into
> the project. Because directly-included objects are always linked, and library
> objects are only pulled in for *undefined* symbols, the symbol is already
> resolved by the time the linker reaches the library — so the library version is
> **never used**.

The new file: **`startup_tm4c.c`** — device-specific by design.

### Can startup code be written in C?

A fair objection: at startup the stack isn't set up, `.data` isn't copied, and
`.bss` isn't cleared. For **most processors** the startup code must be written in
**assembly**.

> **ARM Cortex-M was specifically designed to reduce the need for low-level
> assembly programming** — so C works here.

But it still requires **non-standard language extensions**, and you must not
assume any `.data` or `.bss` initialization has happened.

## 7. The linker script

The **linker script** (IAR: `project.icf`) tells the linker **where to place each
merged section in the address space**. It also holds the stack and heap sizes
(Lesson 10).

A region of the file is managed by the IDE's **Linker Configuration File
editor** (reachable from the project options), with tabs for:

1. the location of the **`.intvec`** section,
2. the start/end of **ROM** and **RAM**,
3. the sizes of **stack** and **heap**.

At the bottom of the script, a `place` command puts the read-only `.intvec`
section at the address given by `ICFEDIT_intvec_start`.

## 8. Placing a variable in a specific section

Standard C has **no syntax** for this. IAR's extension:

```c
uint32_t const __vector_table[] @ "​.intvec" = { ... };
```

— an `@` followed by the section name in double quotes.

## 9. Why `const` is essential here

Placing the array in `.intvec` without `const` produces a surprise: the map file
shows `.intvec` pushed down into **RAM**, not at address 0.

> A non-`const` variable **can change**, so the compiler cannot place it in
> read-only memory.

The fix is the **`const`** keyword:

```c
uint32_t const __vector_table[] @ ".intvec" = { ... };
```

Like `volatile` (Lesson 5), `const` may be written before or after the type;
**after the type is recommended** for consistency. With `const` applied, the map
file shows `.intvec` back in **ROM at address 0**, provided by **your**
`startup_tm4c.o`.

> ⚠️ At this stage the table is *placed* correctly but **not yet initialized
> correctly**, so the program will not run — and a bad vector table can hang the
> debugger and prevent re-programming the board. Initialize it with safe values
> (see the practice file). Lesson 15 fills it in properly.

---

## Key takeaways

1. Embedded builds are **cross-development**: host tools, target code.
2. Object files hold **relocatable** ELF code with placeholder addresses.
3. The linker **patches opcodes**, so it must be target-specific.
4. Directly-included objects are always linked; library objects only when they
   resolve an undefined symbol.
5. Defining a symbol in your own object **pre-empts** the library version — the
   trick that replaces the vector table.
6. The **linker script** decides where sections land.
7. **`const` puts data in ROM**; without it, your table lands in RAM.

---

## Glossary

| Term | Meaning |
|---|---|
| Host / target | Machine that builds / machine that runs the program |
| Cross-development | Building on one architecture for another |
| Object file | Compiler output with relocatable code |
| ELF | Executable and Linkable Format |
| Relocation | Patching addresses once the layout is fixed |
| Exported / imported symbol | Defined here / needed from elsewhere |
| Library (`.a`) | Archive of object files, linked selectively |
| Linker script (`.icf`) | File specifying section placement |
| `const` | Qualifier making an object unmodifiable — and ROM-able |

---

## Pitfalls to remember

- **Estimating code size from object-file size.**
- **Forgetting `const`** on a table meant for ROM.
- **Library order** — a real cause of unresolved symbols.
- **Fat library objects** that drag unused code into the image.
- **Flashing a malformed vector table** — it can lock the debugger out of the
  board.

---

**Next:** Lesson 15 fills in the vector table properly — stack pointer,
exception handlers, and all the device's interrupt vectors — and starts the
Board Support Package.
