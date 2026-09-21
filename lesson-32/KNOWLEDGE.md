# Lesson 32 — Knowledge: OOP Part 4 — Polymorphism in C

**Video:** <https://youtu.be/2v_qM5SJDlY> · **Transcript:** <https://www.state-machine.com/course/lesson-32.txt>
**Hands-on companion:** [PRACTICE.md](PRACTICE.md)

---

## The big idea

Implement the VPTR–VTABLE design **by hand in portable, standard-compliant C**,
and learn the far more important lesson: **when polymorphism is the right tool,
and when it is a mistake.**

> **Encapsulation and single inheritance were essentially free in C. Polymorphism
> is not** — it adds coding complexity and overhead.
>
> **If you intend to use polymorphism extensively, you are probably better off
> switching to C++.**
>
> But if you **build or use software libraries** (such as the QP/C real-time
> framework), the complexity can be **confined to the library and effectively
> hidden from application developers**.

---

## 1. Pointers to functions

C lets you take a pointer to a **function** just as to a variable. In both cases
the pointer holds an **address** plus **type information** — and for a function,
that type information is its **full signature**.

To declare one, start from the function's signature and turn the name into a
pointer:

```c
void draw(Shape const * const me);        /* 1. the signature       */
void *draw(Shape const * const me);       /* 2. WRONG: '*' binds to the return type */
void (*draw)(Shape const * const me);     /* 3. right: parenthesize the pointer     */
```

## 2. The VPTR and VTABLE in C

```c
/* shape.h */
typedef struct {
    struct ShapeVtable const *vptr;   /* <== VPTR: the FIRST attribute */
    int16_t x;
    int16_t y;
} Shape;
```

- **`const`** allows the VTABLE to reside in **ROM**.
- The `ShapeVtable` struct isn't declared yet — and that's fine: the compiler
  only needs the **size of a pointer**, which it knows, not the size of the
  table.

```c
struct ShapeVtable {
    void     (*draw)(Shape const * const me);
    uint32_t (*area)(Shape const * const me);
};
```

> Though called a "table", a VTABLE is typically **not an array** but a
> **structure of pointers** to all the virtual functions.

## 3. Three ways to implement the virtual call

### (a) Member functions

```c
/* shape.c */
void Shape_draw_vcall(Shape const * const me) {
    (*me->vptr->draw)(me);
}
uint32_t Shape_area_vcall(Shape const * const me) {
    return (*me->vptr->area)(me);
}
```

Two syntaxes work: `me->vptr->draw(me)` — the parameter list alone tells the
compiler this is a call through a function pointer — or the explicit
**`(*me->vptr->draw)(me)`**.

> The explicit form is preferred because the plain syntax **fails to show that
> you are dereferencing a pointer**.

> **Note the `me` pointer is used twice:** once to find the **VPTR within the
> object** (so the call is specific to the **object**, not to the type of the
> pointer), and once as the usual **first parameter** of a member function.

**Drawback:** an extra function-call overhead for the `vcall` wrapper.

### (b) `inline` functions — preferred

C99 introduced **`inline`** exactly for this. Move the whole definitions into
the **header** and add **`static inline`**:

```c
static inline void Shape_draw_vcall(Shape const * const me) {
    (*me->vptr->draw)(me);
}
```

This requires **C99 mode** to be enabled in the project options (older compilers
default to C89 and reject `inline`).

### (c) Preprocessor macros — for C89 only

```c
#define Shape_draw_vcall(me_) (*(me_)->vptr->draw)((me_))
```

Macros are purely **textual substitution**, so **always parenthesize the
parameters** to avoid surprises when callers pass expressions.

> Macros are **not nearly as good as inline functions**, but they are the only
> low-overhead option for older **C89** compilers, which remain in extensive use.

## 4. Defining the VTABLE and setting the VPTR

Both happen in the **constructor**, exactly as C++ does it:

```c
/* shape.c */
static void     Shape_draw_(Shape const * const me);   /* static: local to this module */
static uint32_t Shape_area_(Shape const * const me);

void Shape_ctor(Shape * const me, int16_t x0, int16_t y0) {
    static struct ShapeVtable const vtable = {   /* static + const  ==>  in ROM */
        &Shape_draw_,                             /* '&' leaves no doubt these  */
        &Shape_area_                              /*  are ADDRESSES             */
    };
    me->vptr = &vtable;       /* set the VPTR */
    me->x = x0;
    me->y = y0;
}
```

- **`static` + `const`** → the table lives in ROM and must be **initialized at
  the point of creation** (a `const` object cannot be changed later).
- Writing the function names **without parentheses** also works — the absence of
  `()` means "address of", not "call" — but the explicit **`&`** is clearer.
- At the `Shape` level the methods can't do anything meaningful, so they are
  empty; **cast unused parameters to `void`** to silence warnings.

### In a subclass

`Rectangle` inherits the VPTR and the virtual-call mechanism, but needs **its own
VTABLE** and **its own methods**:

```c
void Rectangle_ctor(Rectangle * const me, ...) {
    static struct ShapeVtable const vtable = {
        (void (*)(Shape const * const))&Rectangle_draw_,        /* CAST required */
        (uint32_t (*)(Shape const * const))&Rectangle_area_
    };
    Shape_ctor(&me->super, x0, y0);        /* FIRST: base ctor sets vptr -> Shape  */
    me->super.vptr = &vtable;              /* THEN: override -> Rectangle's VTABLE */
    ...
}
```

Two things to get right:

1. **The cast on the function pointers.** `Rectangle_draw_()` takes a
   `Rectangle*`, but the VTABLE slot expects a `Shape*`. **The C compiler does
   not know about the inheritance relationship and will not upcast
   automatically.**
2. **The order.** The VPTR must be overridden **after** the base constructor,
   because — exactly as in C++ — the base constructor sets it to the **base**
   VTABLE.

> (This assumes the subclass adds **no new** virtual functions, so it can reuse
> the `ShapeVtable` struct. Otherwise you'd have to apply inheritance to the
> VTABLEs too.)

## 5. It compiles to the same machine code

Stepping through `drawGraph()` in C, the late-binding disassembly is
**identical** to what the C++ compiler generated:

```
LDR  ; fetch the VPTR from the 'me' pointer in r6 into r0
LDR  ; fetch the first virtual function from the VTABLE in r0 into r1
MOV  ; copy 'me' from r6 to r0 as the first parameter
BLX r1
```

> **The C implementation works exactly like the C++ original, down to the machine
> instructions for late binding.**

## 6. An alternative C implementation

The VPTR–VTABLE scheme is not the only option. The most common alternative
**removes the VPTR indirection and embeds the whole VTABLE inside every object**.

| | Advantage | Drawback |
|---|---|---|
| Embedded VTABLE | slightly simpler virtual call; nicer C++-like syntax (`obj.draw()` via the dot operator) | **VTABLE moves from ROM to RAM, and is repeated in every object** — with many objects you can easily **double or triple your RAM usage** |

> Presented in *"Design Patterns for Embedded Systems in C"* by Bruce Powel
> Douglass — which, incidentally, also uses the **`me` pointer convention**.

## 7. When to use polymorphism

### The code smell

Without late binding, `drawGraph()` would be written the traditional way: add a
**`kind`** attribute to `Shape`, an **enumeration** of all possible kinds
(`RECTANGLE`, `CIRCLE`, `TRIANGLE`, …), and a **`switch`** or if-then-else chain
wherever kind-specific behaviour is needed.

> Such code is **far less maintainable**: every time you add or remove a kind of
> Shape you must **find and change all those places** throughout the code.
>
> The virtual call is not only more efficient but **automatically extensible**:
> you can add and remove subclasses **without changing the calling code at all**,
> and **without even recompiling** `drawGraph()`.

**Guideline:**

> **Whenever you see or anticipate `switch` statements scattered throughout your
> project, consider polymorphism.**
>
> But remember: **the only valid reason to apply polymorphism is that the
> object-specific behaviour must be selected at run time.**

### When NOT to use it

> You don't need polymorphism when the selection based on object type **does not
> need to happen at run time**.

**The most frequent misuse in industry: managing product lines.**

A medical company making infusion pumps starts with one pump, then produces ever
more versions for various drugs and market segments. The software is never
written from scratch — a **single, ever-growing code base** is extended to serve
all pumps.

> **And herein lies madness.** Developers litter the code with conditional logic
> that quickly becomes **unmanageable and untestable**, making decisions at run
> time based on product type and version numbers — **while any given code set
> ends up in only one specific product.** The selection **does not need to happen
> at run time**; it can happen at **compile time and link time**.

Even though polymorphism *could* eliminate much of that IF-THEN-ELSE logic, the
**better way** is:

> Design a clean **BSP interface** and provide **different implementations** for
> different products — `bsp_ABC.c`, `bsp_XYZ.c`, … This is **abstraction**
> (Lesson 29).

### Physical design

> Effective management of product lines requires careful **physical design** —
> how you partition code into **directories and files**. That way you build the
> software for any product by **combining modules at link time**, rather than
> using run-time techniques like polymorphism.
>
> The art of good physical design is **very valuable in embedded systems** and
> unfortunately **not widely known or appreciated**. Tons of books cover logical
> design (such as OOP); very few cover physical design.
>
> A notable exception: **"Large Scale C++ Software Design" by John Lakos.**
> Highly recommended.

---

## Key takeaways

1. Polymorphism in C is **not free** — consider C++ if you need it extensively.
2. Library code can **hide** the complexity from application developers.
3. A function pointer needs **parentheses** around the `*`.
4. The **VPTR must be the first attribute**; `const` puts the VTABLE in ROM.
5. Implement virtual calls with **`static inline`** functions (C99); macros only
   for C89.
6. The `me` pointer is used **twice** — to find the VPTR and as the first
   argument.
7. VTABLEs are defined and VPTRs set **in constructors**, subclass **after** base.
8. **Cast subclass method pointers** — C doesn't upcast automatically.
9. The generated machine code is **identical to C++'s**.
10. Use polymorphism when behaviour must be chosen **at run time** — and only
    then.
11. For product lines, prefer **BSP abstraction and link-time composition** over
    run-time conditionals.

---

## Glossary

| Term | Meaning |
|---|---|
| Pointer to function | Holds a function's address plus its full signature |
| VPTR / VTABLE | Pointer in the object / per-class table of virtual functions |
| `static inline` | C99 function definable in a header with no call overhead |
| Method cast | Casting a subclass function pointer to the base signature |
| Code smell | A pattern suggesting a design problem |
| Physical design | Partitioning code into files and directories |
| Link-time composition | Selecting behaviour by which modules you link |

---

## Pitfalls to remember

- **Missing parentheses** in a function-pointer declaration.
- **VPTR not first** — the whole scheme collapses.
- **Setting the subclass VPTR before calling the base constructor** — it gets
  overwritten.
- **Omitting the function-pointer casts** in a subclass VTABLE.
- **A non-`const` VTABLE** — it lands in RAM.
- **`inline` without C99 mode enabled.**
- **Unparenthesized macro parameters.**
- **Using polymorphism for compile-time variability** — the classic product-line
  mistake.

---

**Next:** Lesson 33 begins the segment on **event-driven programming** — the last
big trend that went mainstream in the 1980s.
