# Lesson 29 — Practice: Building a `Shape` Class in C, then in C++

**Concepts behind this exercise:** [KNOWLEDGE.md](KNOWLEDGE.md)
**Video:** <https://youtu.be/dSLodtKuung>

---

## Projects in this lesson

| Directory | Toolchain | Language |
|---|---|---|
| `simulator-keil/` | KEIL MDK (`lesson.uvprojx`) | **C** |
| `simulator-keil-cpp/` | KEIL MDK (`lesson.uvprojx`) | **C++** |

> Two parallel projects — the same design in both languages, so you can compare
> them directly, including the generated machine code. The **simulator** is
> enough; no board needed.

## Files you create

```
shape.h    -- the Shape class interface (attributes + operations)
shape.c    -- the implementation
main.c     -- instantiates and uses Shape objects
qassert.h  -- assertion macros (from QP/C)
```

---

## Part A — The Shape class in C

### 1. Understand why the BSP pattern isn't enough

`bsp.h`/`bsp.c` already use **abstraction** (LEDs reduced to on/off/toggle) and
**information hiding** (how it's done is hidden from the blinky threads).

But it handles only a **fixed** set of LEDs. Imagine an LCD needing rectangles,
circles and triangles — you can't know how many upfront, and hiding them in a
module would force **dynamic memory allocation, generally a bad idea in
real-time embedded systems**.

**So instead: let the application allocate as many as it likes, however it
likes.**

### 2. Declare the attributes

```c
/* shape.h -- with the usual include guards */
typedef struct {
    int16_t x;
    int16_t y;
} Shape;
```

- **16-bit** — adequate range for a small LCD.
- **signed** — so shapes can be positioned off screen.

So far this is just Lesson 12. **The encapsulation comes next.**

### 3. Declare the operations

```c
void     Shape_ctor       (Shape * const me, int16_t x0, int16_t y0);
void     Shape_moveBy     (Shape * const me, int16_t dx, int16_t dy);
uint16_t Shape_distanceFrom(Shape const * const me, Shape const * const other);
```

**By coding convention you disallow accessing the struct members directly.**
The association between data and functions rests on two rules:

1. **every function's name starts with the structure's name**;
2. **every function takes `me` as its first parameter**, naming the instance.

> `me` corresponds to C++'s implicit **`this`** and Python's **`self`**.

### 4. Get the `const` placement right

| Written | Meaning |
|---|---|
| `Shape * const me` | **the pointer** can't change, the Shape **can** — constructor, `moveBy` |
| `Shape const * const me` | **neither** can change — `distanceFrom` |
| `Shape const * const other` | the other shape needn't change |

Note the rule: **`const` after the `*`** protects the pointer; **before the `*`**
protects the pointee.

### 5. Implement `shape.c`

```c
void Shape_ctor(Shape * const me, int16_t x0, int16_t y0) {
    me->x = x0;
    me->y = y0;
}

void Shape_moveBy(Shape * const me, int16_t dx, int16_t dy) {
    me->x += dx;
    me->y += dy;
}

uint16_t Shape_distanceFrom(Shape const * const me, Shape const * const other) {
    /* "taxicab" geometry: adequate for shapes, and needs no square root */
    ...
}
```

**What you just created is a `Shape` class** — a fundamental OOP concept
combining **attributes** (data) and **operations** (functions) into one entity.

> Draw its **class diagram**: a box with the class name on top, then attributes,
> then operations. Later, such diagrams also show relationships between classes.

### 6. Instantiate objects three ways

A class is a **cookie-cutter**; instances are **objects**.

```c
#include "shape.h"
#include <stdlib.h>

static Shape s1;                                  /* static     */

int main(void) {
    Shape s2;                                     /* automatic  */
    Shape *ps3 = (Shape *)malloc(sizeof(Shape));  /* dynamic    */
    ...
    free(ps3);                                    /* pair it immediately */
}
```

> Dynamic allocation is **typically a bad idea** in real-time embedded software —
> shown only for completeness. To use it, raise the **heap size**: Project
> Options → **Asm** tab, where the C-stack and heap symbols are defined (the
> sizes are set in the assembly startup code). **1 KB** is plenty.

### 7. Call the constructors explicitly

> Objects must be **initialized before any other operation**. OOP languages call
> constructors automatically; **in C you must call `_ctor()` yourself.**

```c
Shape_ctor(&s1, 1, 2);
Shape_ctor(&s2, 3, 4);
Shape_ctor(ps3, 5, 6);
```

> You have seen this pattern already — the **`QXThread` constructors** in QP/C
> (Lesson 27). Now you know what it was.

### 8. Use the operations

```c
Shape_moveBy(&s1, 7, 8);
Shape_moveBy(&s2, 9, 10);
Shape_moveBy(ps3, -1, -2);

/* assert the mathematical properties of a distance metric */
Q_ASSERT(Shape_distanceFrom(&s1, &s1) == 0U);
Q_ASSERT(Shape_distanceFrom(&s1, &s2) == Shape_distanceFrom(&s2, &s1));
Q_ASSERT(Shape_distanceFrom(&s1, &s2)
         <= Shape_distanceFrom(&s1, ps3) + Shape_distanceFrom(ps3, &s2));
```

### 9. Prove the interface resists misuse

```c
Shape const *ps1 = &s1;
Shape_moveBy(ps1, -3, -4);      /* the compiler REFUSES */
```

> **The guiding principle for class interfaces: easy to use correctly, hard to
> use incorrectly.**

### 10. Look inside

Step through in the debugger and confirm:

- `malloc` is called with **`sizeof(Shape)` = 4 bytes** — two 16-bit integers.
- Calling any class operation places the **`me` pointer in `r0`**, per the
  **AAPCS** (Lesson 9). **This repeats for every class operation.**
- Inside an operation, attribute access uses the **offset-from-register**
  addressing mode off `r0` — very efficient (Lesson 12).
- The **`me` pointer sits at the top of the Locals window**, so you see the
  object's attributes at a glance.

> The `me` pointer helps with understanding *and* debugging — and costs nothing
> in performance.

---

## Part B — The same design in C++

### 11. Compile the C code as C++ first

Copy the directory, rename the `.c` files to `.cpp`, drop the project on the
uVision IDE, remove the now-missing `.c` files from the project and add the
`.cpp` ones. Rebuild.

The only errors are of one kind: **C++ won't implicitly convert `void *`** (from
`malloc`, or QP/C's `QEvt` pointers) to another pointer type. C will; C++ demands
an exact match.

> **The C++ compiler accepts your C implementation just fine — it is only
> stricter about types.** You can move from C to C++ easily, treating C++ at
> first as **a better, more strict C**.

### 12. Convert `shape.h` to a real class

| Change | From → To |
|---|---|
| keyword | `typedef struct {` → **`class Shape {`** (no typedef needed — the name is a type) |
| closing brace | now goes **after the last operation** |
| access control | **`private:`** for attributes (a language rule, not a convention), **`public:`** for operations |
| operation names | drop the `Shape_` prefix — though the compiler still does **name mangling** behind the scenes |
| `me` parameter | **remove it** — C++ supplies the implicit **`this`** |
| constructor | name = **class name**, **no return type**, called **automatically** |
| `Shape const * const me` | move the `const` **after** the operation: `uint16_t distanceFrom(Shape const &other) const;` |

> In C you *could* have called the pointer `this`, and the C compiler would
> accept it — but that code would **not compile as C++**, where `this` is a
> keyword. **Avoid using a related language's keywords as identifiers.**

### 13. Convert `shape.cpp`

- Replace the `Shape_` underscore with the **`::` scope resolution operator**,
  telling the compiler the operation belongs to the class.
- Remove the `me` parameter.
- In `moveBy()`, **write `this->x` explicitly once** to prove the implicit
  pointer really exists and is used for attribute access.
- In `distanceFrom()`, **just drop `me->`** — the compiler recognizes attribute
  names and inserts `this->` implicitly. Mind the trailing `const`.
- For the constructor, the plain assignments compile — but the **truly C++ way**
  is the **constructor initializer list**:

```c++
Shape::Shape(int16_t x0, int16_t y0) : x(x0), y(y0) {}
```

Compile just `shape.cpp`: no errors, no warnings.

### 14. Convert `main.cpp`

| C | C++ |
|---|---|
| `static Shape s1; ... Shape_ctor(&s1,1,2);` | `static Shape s1(1, 2);` — **initialize at the point of allocation** |
| `Shape s2; Shape_ctor(&s2,3,4);` | `Shape s2(3, 4);` |
| `malloc` + `ctor` | **`Shape *ps3 = new Shape(5, 6);`** |
| `free(ps3)` | **`delete ps3;`** |
| `Shape_moveBy(&s1, 7, 8)` | `s1.moveBy(7, 8)` — the **dot** operator |
| `Shape_moveBy(ps3, -1, -2)` | `ps3->moveBy(-1, -2)` — the **arrow** operator |

> In C the dot and arrow accessed only **data members**; in C++ they access
> **both attributes and operations**. The syntax differs, but **the information
> supplied for each operation is identical**.

---

## Part C — Compare the machine code

### 15. Discover when static constructors run

**Before** launching the debugger, set a breakpoint in the **`Shape`
constructor**. Then start debugging.

**The breakpoint is hit before `main()` is even called.** Check the call stack:
the constructor is called not from `main` but from **"cpp_initialization" code**.

Two lessons:

1. **In C++, constructors of static objects run before `main()`.**
2. **C++ requires an extra step in the startup code** to invoke them.

Yet the machine code generated from the very different **constructor initializer
list** syntax turns out **identical** to the C implementation.

### 16. Watch `new` in action

Continue: the constructor runs a **second** time, now called from `main` — that
is the automatic `s2`.

Keep stepping to the **`new`** operator, then single-step into it… you arrive at
a call to **`malloc()`**.

> **`new` calls `malloc()` internally.**

The parameter in `r0` is **4** — the same object size as in C. Continue and the
constructor is invoked a **third** time.

> **`new` also calls the constructor.** C++ guarantees the constructor runs in
> every case after allocation.

### 17. Compare an operation call

Move the breakpoint to the first `moveBy()` call. Compare with the C version:
**identical machine code**.

Step inside: the **Locals window shows `this` at the top** — exactly as `me` was
in C. The implementation of `moveBy()` is **again exactly the same code**.

> **The C++ compiler generates the same code as your C emulation, for both the
> invocations and the implementations.**

---

## Part D — Encapsulation is not thread safety

### 18. Share an object between threads

Move the static `s1` to file scope, then:

- call **`moveBy()` on `s1` from the blinky1 thread**;
- copy the **`distanceFrom()` assertion on the same `s1` into blinky2**.

Note that `Q_ASSERT(distanceFrom(s1, s1) == 0)` should **always** be true, and
that in blinky2 it runs **downstream of the semaphore wait** — so only after you
press SW1.

### 19. Catch the failure

Before running, set a breakpoint in **`Q_onAssert()`**, so you know immediately
if an assertion fails.

Run free and press SW1 repeatedly. Most of the time nothing happens — the
assertion evaluates successfully **thousands of times**.

Keep trying, and… **the assertion fires**, inside blinky2.

### 20. Understand why

The cause is a **race condition** around the shared `s1` object: modified in
blinky1 while being used in blinky2 (Lesson 20).

> **Even though you applied encapsulation, it did not prevent the race.**
>
> Encapsulation in C++ works exactly as you emulated it in C, and **boils down
> to simple function calls**. It hides the implementation; it does **nothing**
> about concurrency.

> **True encapsulation for concurrency requires the *active class* / *active
> object* pattern** — a fusion of concurrent, object-oriented and event-driven
> programming. Lessons 33–34 and 43–44.

---

## Exercises

1. **Add an operation.** Implement `Shape_isAt(me, x, y)` and decide the right
   `const` qualification for `me`.
2. **Break encapsulation.** Access `s1.x` directly from `main.c`. Does it
   compile? What does that tell you about C's enforcement versus C++'s?
3. **Metric properties.** Replace taxicab distance with Euclidean and check which
   of the three assertions still hold.
4. **Size check.** Add a `uint8_t` attribute and predict `sizeof(Shape)` before
   checking (recall padding, Lesson 12).
5. **Heap exhaustion.** Set the heap to 0 and call `malloc`. What happens, and
   when do you find out?
6. **Mangled names.** Find the mangled name of `Shape::moveBy` in the C++ map
   file.
7. **Fix the race.** Protect the shared `s1` with a mutex (Lesson 28), and then
   argue why that is a symptom of a design problem rather than a solution.

---

## Self-check

- [ ] `shape.h`/`shape.c` follow both naming conventions and encapsulate the data
- [ ] I can explain the two `const` positions and what each protects
- [ ] I instantiated objects statically, automatically and dynamically
- [ ] I called every constructor explicitly in C
- [ ] I verified `me` arrives in `r0` and attributes are accessed by offset
- [ ] The C++ version compiles and I can map every C construct to its C++ form
- [ ] I observed a static constructor running **before** `main()`
- [ ] I confirmed `new` = `malloc` + constructor, with the same 4-byte size
- [ ] I reproduced a race on an encapsulated shared object and can explain it
