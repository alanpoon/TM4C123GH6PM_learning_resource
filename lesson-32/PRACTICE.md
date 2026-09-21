# Lesson 32 — Practice: Hand-Building the VPTR/VTABLE in C

**Concepts behind this exercise:** [KNOWLEDGE.md](KNOWLEDGE.md)
**Video:** <https://youtu.be/2v_qM5SJDlY>

---

## Projects in this lesson

| Directory | Toolchain | Language |
|---|---|---|
| `simulator-keil/` | KEIL MDK (`lesson.uvprojx`) | **C** |

Start from a copy of the **Lesson 30 C** project (not the `_cpp` one).

> **Before you start:** encapsulation and single inheritance were essentially
> free in C — **polymorphism is not**. If you plan to use it extensively, you are
> probably better off in C++. But when you **build or use a library** (such as
> QP/C), the complexity can be **confined to the library and hidden from
> application developers**. Either way, the point here is to see how it really
> works.

---

## Part A — Declare the machinery

### 1. Add the VPTR to `Shape`

```c
typedef struct {
    struct ShapeVtable const *vptr;   /* <== the FIRST attribute */
    int16_t x;
    int16_t y;
} Shape;
```

- **`const`** lets the VTABLE live in **ROM**.
- `ShapeVtable` isn't declared yet — and that's fine. The compiler only needs
  **the size of a pointer**, which it knows, not the size of the table.

### 2. Declare the VTABLE

> Although called a "table", a VTABLE is typically **not an array** but a
> **structure of pointers** to all the virtual functions.

Build each member from the function's signature, in three steps:

```c
void draw(Shape const * const me);        /* 1. the signature                     */
void *draw(Shape const * const me);       /* 2. WRONG -- '*' binds to the return   */
void (*draw)(Shape const * const me);     /* 3. right -- parenthesize the pointer  */
```

> A pointer to a function holds an **address** plus **type information** — and
> for functions, that type information is the **full signature**.

```c
struct ShapeVtable {
    void     (*draw)(Shape const * const me);
    uint32_t (*area)(Shape const * const me);
};
```

---

## Part B — Implement the virtual call, three ways

### 3. Option (a): member functions

```c
/* shape.c */
void Shape_draw_vcall(Shape const * const me) {
    (*me->vptr->draw)(me);
}
uint32_t Shape_area_vcall(Shape const * const me) {
    return (*me->vptr->area)(me);       /* note the 'return' */
}
```

Both syntaxes compile — `me->vptr->draw(me)` works because the parameter list
tells the compiler this is a call through a function pointer.

> **Prefer the explicit `(*...)` form**: the plain syntax **fails to show that
> you are dereferencing a pointer.**

**The key observation:**

> **The `me` pointer is used twice** — once to find the **VPTR within the
> object** (so the call is specific to the **object**, not to the type of the
> pointer), and once as the usual **first parameter** of a member function.

**Drawback:** an extra function call for every virtual call.

### 4. Option (b): `static inline` — the preferred way

C99 added `inline` for exactly this. Move the **whole definitions into the
header** and prefix them:

```c
/* shape.h */
static inline void Shape_draw_vcall(Shape const * const me) {
    (*me->vptr->draw)(me);
}
```

The build fails — the compiler is still in **C89** mode. Fix it: **Project
Options → C/C++ tab → tick C99 Mode**. This triggers a full rebuild, which then
succeeds.

### 5. Option (c): macros — C89 only

```c
#define Shape_draw_vcall(me_) (*(me_)->vptr->draw)((me_))
```

Macros are **purely textual substitution**, so strip the type information and
**always parenthesize the parameters** — otherwise callers passing expressions
get surprises.

> Macros are **not nearly as good as inline functions**, but they are the only
> low-overhead option for older **C89** compilers, which remain in extensive use.

---

## Part C — Define the VTABLE and set the VPTR

### 6. In the base-class constructor

As you reverse-engineered in Lesson 31, this all happens in the **constructor**:

```c
/* shape.c */
static void     Shape_draw_(Shape const * const me);   /* static: local to module */
static uint32_t Shape_area_(Shape const * const me);

void Shape_ctor(Shape * const me, int16_t x0, int16_t y0) {
    static struct ShapeVtable const vtable = {   /* static + const ==> ROM */
        &Shape_draw_,
        &Shape_area_
    };
    me->vptr = &vtable;
    me->x = x0;
    me->y = y0;
}
```

- **`static` + `const`** puts it in ROM — and a `const` object **must be
  initialized at the point of creation**.
- Bare function names (no parentheses) would also work — the absence of `()`
  means "address of", not "call" — but **the explicit `&` leaves no doubt**.

### 7. Implement the base methods

At the `Shape` level you **cannot provide meaningful implementations** — Shape is
too abstract. Leave them empty, and **cast the unused parameter to `void`** to
avoid compiler warnings.

### 8. Write the generic algorithm

```c
void drawGraph(Shape const *graph[]) {
    for (uint8_t i = 0U; graph[i] != (Shape *)0; ++i) {
        Shape_draw_vcall(graph[i]);     /* <== polymorphism in C */
    }
}
```

Structurally identical to the C++ version from Lesson 31 — the only difference is
the explicit `_vcall` inline function. Add its prototype to `shape.h`.

### 9. In the subclass constructor

`Rectangle` inherits the VPTR and the call mechanism, but needs **its own
VTABLE** and **its own methods**:

```c
void Rectangle_ctor(Rectangle * const me, int16_t x0, int16_t y0,
                    uint16_t w0, uint16_t h0)
{
    static struct ShapeVtable const vtable = {
        (void (*)(Shape const * const))&Rectangle_draw_,
        (uint32_t (*)(Shape const * const))&Rectangle_area_
    };

    Shape_ctor(&me->super, x0, y0);   /* FIRST -- sets vptr to the SHAPE vtable  */
    me->super.vptr = &vtable;          /* THEN override -> the RECTANGLE vtable   */
    me->width  = w0;
    me->height = h0;
}
```

Two things you must get right:

1. **The casts.** `Rectangle_draw_()` takes a `Rectangle*`, but the VTABLE slot
   expects a `Shape*`. **The C compiler doesn't know about the inheritance and
   will not upcast automatically**, so you cast the whole function signature.
2. **The order.** Override the VPTR **after** the base constructor — exactly as
   in C++, the base constructor sets it to the **base** VTABLE first.

> This assumes Rectangle adds **no new** virtual functions, so it can reuse the
> `ShapeVtable` struct. Otherwise you would have to apply inheritance to the
> VTABLEs too.

### 10. Wire up `main.c`

Copy the graph snippet from the Lesson 31 C++ code, adapting:

- there is no `Circle` class in C — use the `s1` **Shape** object instead;
- allocate a **Rectangle** dynamically instead of a Shape, and call the right
  constructor on it;
- add the **explicit casts** the C compiler requires, in the Rectangle
  constructor call and in the `graph[]` initializer.

---

## Part D — Verify on the board

### 11. Check the constructors

Connect the TivaC LaunchPad and step into the **`Rectangle` constructor**:

1. The first code is the call to the **`Shape` constructor**. Step in.
2. The **VPTR is initially unset**, then gets initialized to the **Shape
   VTABLE**, containing the addresses of `Shape_draw_` and `Shape_area_`.
3. **Note the VTABLE address starts with zeros — it really is in ROM.**
4. Keep stepping back into the Rectangle constructor: the **VPTR is overridden**
   to the **Rectangle VTABLE**, with `Rectangle_draw_` and `Rectangle_area_`.

> Your C constructors now work **exactly like the C++ constructors** from Lesson
> 31 — except that in C you wrote the VTABLE and the VPTR assignment by hand,
> while C++ synthesized them automatically.

### 12. Check the virtual calls

Note the graph order first: `s1` (a Shape), then `r1` (a Rectangle), then another
Rectangle via `ps3`.

Step into `drawGraph()`, reach `Shape_draw_vcall()`, and look at the
disassembly. Compare it with the C++ output from Lesson 31: **identical.**

Step one instruction at a time:

| Instruction | Does |
|---|---|
| `LDR` | fetches the **VPTR** from `me` in **r6** into **r0** |
| `LDR` | fetches the **first virtual function** from the VTABLE in r0 into **r1** |
| `MOV` | copies `me` from r6 to **r0** — the first parameter |
| `BLX r1` | **calls** the address in r1 |

You land in **`Shape_draw_()`** — correct, the first graph entry was `s1`. Second
time round the loop, the **same** late-binding code invokes
**`Rectangle_draw_()`**.

> **The C implementation works exactly like the C++ original, down to the machine
> instructions for late binding.**

---

## Part E — An alternative, and when not to bother

### 13. Know the alternative implementation

The most common alternative **removes the VPTR indirection and embeds the whole
VTABLE inside every object**.

| | Advantage | Drawback |
|---|---|---|
| Embedded VTABLE | slightly simpler call; nicer C++-like syntax (`obj.draw()` via the dot operator) | **the VTABLE moves from ROM to RAM and is repeated in every object** — with many objects you can easily **double or triple RAM usage** |

> See *"Design Patterns for Embedded Systems in C"* by Bruce Powel Douglass —
> which also uses the **`me` pointer convention**.

### 14. Recognize the code smell

Imagine `drawGraph()` written traditionally, without late binding: a **`kind`**
attribute in `Shape`, an **enumeration** (`RECTANGLE`, `CIRCLE`, `TRIANGLE`, …),
and a **`switch`** wherever kind-specific behaviour is needed.

> Far less maintainable: **every time you add or remove a kind you must find and
> change all those places.** The virtual call is more efficient **and
> automatically extensible** — you never change the calling code, and never even
> recompile it.

**Guideline:**

> **Whenever you see or anticipate `switch` statements scattered throughout your
> project, consider polymorphism.**
>
> But: **the only valid reason to apply polymorphism is that the object-specific
> behaviour must be selected at RUN TIME.**

### 15. Recognize the misuse

> **You don't need polymorphism when the selection doesn't need to happen at run
> time.**

**The most frequent misuse in industry: managing product lines.** A medical
company making infusion pumps keeps producing versions for different drugs and
markets, extending **one ever-growing code base** to serve them all.

> **And herein lies madness.** The code fills with conditional logic that becomes
> **unmanageable and untestable**, deciding at run time based on product type and
> version numbers — **while any given code set ships in only one product.** That
> selection can happen at **compile time and link time**.

Even though polymorphism *could* remove much of that IF-THEN-ELSE logic, the
better answer is:

> Design a clean **BSP interface** and supply **different implementations** per
> product — `bsp_ABC.c`, `bsp_XYZ.c`, … That is **abstraction** (Lesson 29).

And beyond the BSP:

> Effective product-line management requires careful **physical design** — how
> you partition code into **directories and files** — so you can build any
> product by **combining modules at link time** instead of deciding at run time.
>
> This art is **very valuable in embedded systems** and **not widely
> appreciated**: tons of books cover logical design (OOP), very few cover
> physical design. A notable exception: **"Large Scale C++ Software Design" by
> John Lakos.** Highly recommended.

---

## Exercises

1. **Add a Circle.** Write `circle.c`/`circle.h` in C with its own VTABLE and
   confirm `drawGraph()` needs no change.
2. **Forget the order.** Set the subclass VPTR *before* calling the base
   constructor and observe which methods get called.
3. **Drop the casts.** Remove the function-pointer casts in the Rectangle VTABLE.
   What does the compiler say, and why is it right to complain?
4. **Non-const VTABLE.** Remove `const` and check the map file to see where the
   table landed.
5. **Compare the three call forms.** Measure code size and cycles for the
   function, inline and macro versions.
6. **Embedded VTABLE.** Implement the alternative (VTABLE inside each object) and
   compare RAM usage for 20 objects.
7. **Refactor a switch.** Find a `switch` on a "kind" field in any code you have
   and sketch the polymorphic redesign — then decide honestly whether the
   selection really needs to be made at run time.

---

## Self-check

- [ ] `vptr` is the **first** attribute of `Shape` and points to a `const` table
- [ ] I can write a pointer-to-function declaration correctly from a signature
- [ ] My virtual calls are `static inline` and C99 mode is enabled
- [ ] I can explain why `me` appears twice in a virtual call
- [ ] The VTABLE is `static const` and really lands in ROM
- [ ] The subclass overrides the VPTR **after** the base constructor, with casts
- [ ] The late-binding disassembly matches the C++ version instruction for
      instruction
- [ ] I can state when polymorphism is justified — and name the classic misuse
