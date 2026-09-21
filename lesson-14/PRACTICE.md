# Lesson 14 — Practice: Dissecting the Build and Claiming the Vector Table

**Concepts behind this exercise:** [KNOWLEDGE.md](KNOWLEDGE.md)
**Video:** <https://youtu.be/DfiwWxTRIZE>

---

## Projects in this lesson

| Directory | Toolchain | Target |
|---|---|---|
| `tm4c123-keil/` | KEIL MDK (`lesson.uvprojx`) | EK-TM4C123GXL LaunchPad |
| `tm4c123-iar/` | IAR EWARM (`workspace.eww`) | EK-TM4C123GXL LaunchPad |
| `stm32c031-keil/` | KEIL MDK (`lesson.uvprojx`) | STM32 NUCLEO-C031C6 |
| `CMSIS/` | — | CMSIS headers |

**Goal:** add `startup_tm4c.c` defining your own `__vector_table`, placed in
ROM at address 0, so the library's generic table is never linked in.

> ⚠️ **Safety note:** an incorrectly initialized vector table can hang your
> debugger and prevent you from re-programming the board. Follow step 12.

---

## Part A — Look inside an object file

### 1. Find the objects

Browse to `Debug\Obj\` and locate `delay.o` and `main.o`.

### 2. Open one as text

It is mostly binary garbage, but two things are visible:

- distinct **sections**;
- the first ASCII characters spell **`ELF`** — Executable and Linkable Format.

### 3. Dump it in human-readable form

From a command prompt, run the IAR dump utility with no arguments to see its
help, then dump everything:

```
ielfdumparm --all main.o > main.txt
```

> `objdump` from the GNU toolchain works too — ELF is a standard format.

Open `main.txt` in the IDE. Find the familiar sections — `.text`, `.data`,
`.bss` — plus symbol tables for the linker and a lot of **debug information**.

> That debug information is why you must **never judge code size from object-file
> size**. Use the linker map file (Lesson 13).

### 4. Dump the final image too

`c.out` also begins with `ELF`:

```
ielfdumparm --all c.out > c.txt
```

> This is one of the quickest ways to see the generated disassembly without
> loading anything onto the target.

---

## Part B — See relocation with your own eyes

### 5. Compare the call instruction

Put `main.txt` and `c.txt` side by side and scroll the final image to the
`.text16` section containing `main`.

Most instructions match exactly. But find the 32-bit `BL` where `main` calls
`delay`:

| File | Encoding |
|---|---|
| `main.o` | `0xF7FF 0xFFFE` |
| `c.out` | `0xF000 0xF820` |

`BL` is **PC-relative** — the branch distance is a signed offset inside the
instruction. The object file can't know where `delay()` will land, so it carries
a placeholder offset (`0x7FFFFFE`). **The linker patches it** once it decides the
layout.

### 6. Compare the constant pool

Look at the section `??main_0` immediately after `main`'s code — the **constant
pool** holding the addresses of `p1`, `w`, `t`, `p2`, `w2`.

In `main.o` they are all **zero**. In `c.out` they are real addresses.

> That is what "relocatable code" means. And it is why a linker must be
> target-specific: it patches **binary opcodes**. An x86 linker cannot link ARM
> code, ELF or not.

---

## Part C — Trace symbol resolution through the map file

### 7. Follow the linker's two lists

Walk through the process on paper for your own project:

| Step | Exported | Undefined |
|---|---|---|
| start | — | `__vector_table` |
| after `main.o` | `p1, w, t, main, …` | `__vector_table`, `delay` |
| after `delay.o` | + `delay` | `__vector_table` |
| → libraries | | |

> Objects **listed in the project** are always linked (order irrelevant).
> Objects **inside a library** are linked only if they export a currently
> undefined symbol.

### 8. Find the library objects in the map file

1. Search the map file for **`__vector_table`**. It appears in the **Entry List**
   section, provided by **`vector_table_M.o`**.
2. Search for **`vector_table_M.o`**. It appears in **Module Summary** under
   **`rt7M_tl.a`** — the `.a` extension marks a library (archive).
3. That object imports `__iar_program_start`, `BusFault_handler`, … so the linker
   keeps searching the same library, finding **`cstartup_M.o`**.
4. `cstartup_M.o` imports `__iar_data_init3`, `__iar_zero_init3`,
   `__low_level_init`, **and `main`** — the last resolves instantly from **your**
   `main.o`.

### 9. Note the granularity

Observe that library objects contain **one function or one variable each**. That
is deliberate: it ensures only what is needed gets linked.

> If you ever build your own library, keep objects small — ideally one function
> or one global per module. Otherwise you bloat every image that uses it.

### 10. Cause a linker error on purpose

Remove `delay.o` from the project and rebuild. Read the error: it lists the
unresolved references still in the undefined list.

Typical fixes: add the missing object or library; **reorder** libraries;
occasionally list a library **twice** to break circular dependencies. See
["Library order in static linking"](http://eli.thegreenplace.net/2013/07/09/library-order-in-static-linking).

---

## Part D — Replace the vector table

### 11. Create `startup_tm4c.c`

Add a new C file to the project and define the global array `__vector_table`.

> **Why can this be C at all?** At startup there is no stack set up, no `.data`
> copied, no `.bss` cleared — on most processors startup *must* be assembly.
> Cortex-M was specifically designed to reduce the need for assembly. You still
> need non-standard extensions and you must not assume any initialization has
> happened.

For now, just get it to compile with a couple of placeholder zeros — placement
is the real challenge.

### 12. Get the placement right

Find where the library table went: the map file shows **`.intvec` at address 0
in ROM**.

Understand how that section is defined — open the linker script **`project.icf`**.
The IDE-managed region is editable via **Project Options → Linker Configuration
File editor**, whose tabs control:

1. the location of the **`.intvec`** section,
2. **ROM/RAM** start and end,
3. **stack and heap** sizes.

> Try nudging the stack size in the editor and watch the corresponding line in
> `project.icf` update — it's the same file.

At the bottom of the script, the `place` command puts the read-only `.intvec`
section at `ICFEDIT_intvec_start`.

Now place *your* array there. Standard C has no syntax for this; IAR's extension
is `@` followed by the section name:

```c
uint32_t __vector_table[] @ ".intvec" = { 0, 0 };
```

Keep the map file visible and press `F7`.

### 13. Hit the surprise — and fix it with `const`

The compiler accepts the file, but the map file shows **`.intvec` pushed down
into RAM**, not at address 0.

**Why?** A non-`const` variable can change, so the compiler cannot put it in
read-only memory.

```c
uint32_t const __vector_table[] @ ".intvec" = { ... };
```

> `const` behaves syntactically like `volatile` (Lesson 5) and may go before or
> after the type. **After the type is recommended.**

Rebuild and check the map file:

- `.intvec` is back in **ROM at address 0**, and
- the module providing it is **your `startup_tm4c.o`** — not the library's.

That is the objective achieved: your object is linked directly, so
`__vector_table` is never undefined when the linker reaches the library.

### 14. Initialize it safely before flashing

⚠️ The table is placed correctly but **not yet initialized correctly**, so the
program will not run — and a bad vector table **can hang the debugger and
prevent you from re-programming the board**.

Initialize it with safe values: entry 0 = the top-of-stack address, entry 1 = a
valid code address (e.g. `__iar_program_start` or your own `Reset_Handler`).
Lesson 15 fills in the rest properly.

---

## Exercises

1. **Size comparison.** Compare `sizeof` the object file against the code size
   the map file reports for the same module. What accounts for the difference?
2. **Two dumps.** Diff `main.txt` and `c.txt` systematically. List every
   instruction the linker had to patch.
3. **Symbol hunt.** Pick a standard-library function you call and trace, via the
   map file, which library object supplied it and what *it* imported.
4. **Order matters.** Construct two small libraries with a circular dependency
   and find the library ordering that links successfully.
5. **Section games.** Place an ordinary variable in a named section of your own
   and add a `place` rule for it in `project.icf`.
6. **`const` drill.** Declare a `const` array *without* a section attribute.
   Which section does it land in, and why?
7. **Fill the table.** Before watching Lesson 15, try to write the first 16
   entries of a correct vector table from the datasheet.

---

## Self-check

- [ ] I dumped an object file and recognized the ELF signature and sections
- [ ] I found one instruction whose encoding differs between `.o` and the image,
      and can explain why
- [ ] I traced `__vector_table` from the Entry List to a library archive
- [ ] I can state the two different linking rules for objects vs. libraries
- [ ] I created `startup_tm4c.c` and placed `__vector_table` in `.intvec`
- [ ] I saw the table land in RAM without `const`, and in ROM at address 0 with it
- [ ] The map file shows my object — not the library — providing `.intvec`
