# Lesson 3 — Knowledge: Variables and Pointers

**Video:** <https://youtu.be/o9WpXYBqdPU> · **Transcript:** <https://www.state-machine.com/course/lesson-03.txt>
**Hands-on companion:** [PRACTICE.md](PRACTICE.md)

---

## The big idea

Memory addresses are fundamental to the CPU — every memory access needs one. C
exposes addresses directly through **pointers**: variables that hold addresses.
This is the mechanism that later lets you reach hardware registers from C.

---

## 1. Where a variable lives depends on where it is defined

| Definition | Storage | Visible in debugger as |
|---|---|---|
| Inside a function (`int counter;`) | **Local** — usually kept in a CPU register | Locals view |
| Outside any function (`static int counter;`) | **RAM** | Watch view / Memory view |

On the TM4C123, RAM begins at address **`0x20000000`**, so a variable whose
address starts with `0x2...` lives in RAM. A local variable that the compiler
keeps in a register has no memory address at all while it is in a register.

## 2. The RISC load/store pattern

ARM is a **RISC** (Reduced Instruction Set Computer) architecture. To modify a
variable in memory, the CPU must:

1. **`LDR`** — *load* the value from memory into a register
2. operate on it in the register (e.g. **`ADDS`**)
3. **`STR`** — *store* the register value back to memory

Data manipulation happens **only in registers**. This is the whole shape of ARM
code, and it is why register pressure and memory traffic matter.

> Contrast: **CISC** architectures such as x86 have complex instructions whose
> operands may remain in memory.

The address itself must first be loaded into a register too — typically via a
PC-relative `LDR` from a **literal pool** placed near the code (the `??main2`
style labels you see in the disassembly hold the constant addresses).

## 3. Pointers

A **pointer** is a variable that holds an address.

```c
int *p_int;
```

Read C declarations **backwards**: `p_int` is a *pointer* (that's the `*`) *to*
`int`. It can hold addresses of integer variables.

### The two operators

| Operator | Name | Meaning |
|---|---|---|
| `&x` | address-of | yields the address of `x` |
| `*p` | dereference | yields the value stored *at* the address in `p` |

```c
static int counter = 0;
int *p_int;

p_int = &counter;   /* p_int now holds the address of counter */
*p_int = 5;         /* writes 5 into counter                  */
++(*p_int);         /* counter becomes 6                      */
```

`*p_int` and `counter` become **aliases** for the same storage — use either.

> The `*` is doing double duty: in a *declaration* it means "pointer to"; in an
> *expression* it means "value at". They are different uses of the same symbol.

### Pointers can make code faster

Introducing `p_int` can *simplify* the generated machine code: the address is
loaded into a register once at the top instead of being reloaded before every
access. Fewer `LDR` instructions, same result.

## 4. Fabricated addresses and type casting

A pointer can hold **almost any address**, not just the address of a variable
you declared. That is how embedded software reaches memory-mapped hardware.

The compiler will not let you assign a bare number to a pointer:

```c
p_int = 0x20000002U;       /* ERROR: the compiler rejects this */
```

You must force the type with a **cast** — the type name in parentheses:

```c
p_int = (int *)0x20000002U;   /* accepted */
*p_int = (int)0xDEADBEEF;     /* poke a value at that address */
```

This is a blunt instrument: a cast tells the compiler "trust me", and it stops
checking. Used deliberately, this is *the* technique for accessing peripheral
registers — the subject of Lesson 4.

## 5. Alignment — why the DEADBEEF hack is scary

Writing a 32-bit value to address `0x20000002` (not a multiple of 4) is a
**misaligned access**. The write lands partly on one variable and partly on the
next word in memory, silently corrupting both.

- **Cortex-M4** tolerates misaligned data accesses for ordinary loads/stores.
- **Cortex-M0/M0+** does **not** — the same code raises a **HardFault**.

Portability across Cortex-M cores therefore depends on keeping accesses aligned.

---

## Key takeaways

1. Locals tend to live in registers; file-scope variables live in RAM.
2. ARM is load/store: memory → register → operate → register → memory.
3. `&` takes an address, `*` dereferences one; `*p` is an alias for the pointee.
4. A cast lets a pointer name an arbitrary address — the foundation of
   memory-mapped I/O, and a loaded gun.
5. Misaligned accesses corrupt neighbours on M4 and fault outright on M0/M0+.

---

## Glossary

| Term | Meaning |
|---|---|
| Pointer | Variable holding a memory address |
| Dereference | Access the value at the address held by a pointer |
| `LDR` / `STR` | ARM load-from-memory / store-to-memory instructions |
| RISC / CISC | Load-store architecture vs. memory-operand architecture |
| Literal pool | Constants (including addresses) stored next to the code |
| Type cast | `(type)expr` — forces the compiler to accept a type |
| Alignment | Requirement that an N-byte access sit on an N-byte boundary |

---

## Pitfalls to remember

- **A cast silences the compiler, not the hardware.** Wrong address, wrong
  alignment, or wrong type will still break at run time.
- **Uninitialized pointers hold garbage addresses.** Dereferencing one writes
  somewhere arbitrary.
- **`*` in a declaration ≠ `*` in an expression.** Read declarations backwards.
- **Misalignment is silent on M4.** Code that "works" on the TivaC LaunchPad can
  HardFault on a Cortex-M0+ board such as the NUCLEO-C031C6.

---

**Next:** Lesson 4 uses exactly this cast-a-pointer-at-an-address technique to
reach GPIO registers and blink an LED.
