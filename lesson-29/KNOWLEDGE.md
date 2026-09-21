# Lesson 29 — Knowledge: OOP Part 1 — Encapsulation (Classes) in C and C++

**Video:** <https://youtu.be/dSLodtKuung> · **Transcript:** <https://www.state-machine.com/course/lesson-29.txt>
**Hands-on companion:** [PRACTICE.md](PRACTICE.md)

---

## The big idea

> **OOP is not the use of any specific language, but a way of software design**
> based on three fundamental concepts: **encapsulation**, **inheritance** and
> **polymorphism**.

All three can be implemented in **standard, portable C**. This lesson does
encapsulation in C, then translates it to C++ and compares the generated machine
code.

> This lesson lays the foundation for almost all remaining lessons in the course.

---

## 1. Historical context

The RTOS went mainstream in the **1980s**, and the RTOS-based **shared-state
concurrency** model with blocking continues essentially unchanged to this day.
But the 1980s also brought **object-oriented programming** and **event-driven
programming** into the mainstream.

> The most important and complex parts of an RTOS turned out **not** to be
> context switching and scheduling, but the numerous mechanisms for blocking,
> synchronization and mutual exclusion needed to support shared-state
> concurrency.

## 2. You have already used abstraction and information hiding

The BSP separates **what** (`bsp.h`) from **how** (`bsp.c`):

- **Abstraction** — all the details of handling the LEDs are reduced to three
  operations: on, off, toggle.
- **Information hiding** — how those operations are performed is hidden from
  users such as the blinky threads, which need only `bsp.h`.

### The limitation

> This design handles only a **fixed** number of LEDs and **cannot easily be
> extended to an open-ended number** of things.

Imagine an LCD needing rectangles, circles and triangles. You can't know upfront
how many, and hiding them inside a module would require **dynamic memory
allocation — generally a BAD idea in real-time and embedded systems**.

**The solution:** let application programmers allocate as many as they like,
however they like — statically, automatically inside functions, or dynamically
on the heap if they choose.

## 3. Emulating a class in C

```c
/* shape.h */
typedef struct {
    int16_t x;     /* signed: allows off-screen positions */
    int16_t y;     /* 16-bit: adequate range for a small LCD */
} Shape;

void     Shape_ctor       (Shape * const me, int16_t x0, int16_t y0);
void     Shape_moveBy     (Shape * const me, int16_t dx, int16_t dy);
uint16_t Shape_distanceFrom(Shape const * const me, Shape const * const other);
```

### The two coding conventions that make it a class

1. **Every associated function's name starts with the name of the structure.**
2. **Every function takes the `me` pointer as its first parameter**, specifying
   which instance it operates on.

> **By convention you disallow accessing the struct members directly** — that is
> the encapsulation.

### `const` placement matters

| Declaration | Meaning |
|---|---|
| `Shape * const me` | **the pointer** cannot change; the Shape **can** — used in the constructor and `moveBy` |
| `Shape const * const me` | neither the pointer **nor** the Shape can change — used in `distanceFrom` |

Try calling `moveBy()` through a `Shape const *` and the **compiler refuses**.

> That is the guiding principle for class interfaces: **easy to use correctly,
> hard to use incorrectly.**

### `me` in other languages

| Language | Name |
|---|---|
| C (this course) | **`me`** (explicit) |
| C++ | **`this`** (implicit) |
| Python | **`self`** |

## 4. Class, object, attributes, operations

> A **class** combines **data** — called **attributes** — and **functions** —
> called **operations** — into one entity.

- A **class diagram** shows a class as a box: name on top, then attributes, then
  operations (and later, relationships between classes).
- A class realizes **encapsulation**: it presents only the outer shell of
  operations, while internal data and implementation stay inside.
- Think of a class as a **cookie-cutter** from which you create any number of
  **objects** (instances).

### Three ways to instantiate

```c
static Shape s1;                            /* static     */
Shape s2;                                   /* automatic, inside a function */
Shape *ps3 = (Shape *)malloc(sizeof(Shape));/* dynamic, on the heap */
...
free(ps3);                                  /* always pair it, to avoid a leak */
```

> Dynamic allocation is typically a **rather bad idea** in real-time embedded
> software; it is shown only for completeness. (Using it requires a **non-zero
> heap size**, set in the startup code's assembly symbols via the project
> options.)

### Constructors must be called explicitly in C

> Objects must be **initialized before any other operation** is performed on
> them. Object-oriented languages call constructors **automatically**; **in C you
> must call the `_ctor()` functions explicitly.**

You have already seen this pattern with the `QXThread` objects in QP/C
(Lesson 27) — now you know what it was.

## 5. What it costs at the machine level

- `sizeof(Shape)` is **4 bytes** — two 16-bit integers, exactly as expected.
- A class operation places the **`me` pointer in `r0`**, per the **AAPCS**
  (Lesson 9). This pattern repeats for **all** class operations.
- Inside an operation, attribute access uses the **offset-from-register**
  addressing mode off `r0` — **very efficient** (the same mode as Lesson 12).
- In the debugger, the `me` pointer sits **at the top of the Locals window**, so
  you conveniently see all the object's attributes.

> The `me` pointer helps not only in understanding the code but in **debugging** —
> and it yields excellent performance.

## 6. The same design in C++

### C++ as "a better C"

Compiling the C code as C++ produces only a handful of errors, all of the same
kind: **C++ will not implicitly convert `void *`** (e.g. from `malloc`) to
another pointer type, while C will.

> The C++ compiler **accepts the C implementation just fine — it is only stricter
> about type checking.** You can move into C++ from C quite easily and treat it
> at first as **a better, more strict version of C**.

### Converting to a real C++ class

| C emulation | C++ |
|---|---|
| `typedef struct { ... } Shape;` | `class Shape { ... };` — the name is a type by itself, no typedef |
| operations declared outside | operations **inside** the class; the closing brace comes after the last one |
| name prefix `Shape_` | not needed — but the compiler does **name mangling** behind the scenes |
| convention that members are private | the **`private:`** keyword enforces it |
| — | **`public:`** for the operations |
| `Shape_ctor()` | a **constructor**: same name as the class, **no return value**, called **automatically** |
| explicit `me` parameter | the implicit **`this`** parameter |
| `Shape const * const me` | move the **`const` after the operation**: `uint16_t distanceFrom(...) const;` |
| `Shape_moveBy(&s1, ...)` | `s1.moveBy(...)` — the **dot** operator |
| `Shape_moveBy(ps3, ...)` | `ps3->moveBy(...)` — the **arrow** operator |
| `malloc` / `free` | **`new`** / **`delete`** |
| `Shape_ctor(&s1, 1, 2);` after allocation | **initialize at the point of allocation** |
| `Shape_ctor` body assignments | the **constructor initializer list** — the truly C++ way |
| `::` | the **scope resolution operator** binding a definition to its class |

> In C you *could* have named the pointer `this` and the C compiler would accept
> it — but that code would **not compile as C++**, where `this` is a keyword.
> **Avoid using keywords of related languages as identifiers.**

> Note that in both languages **the information supplied for each operation is
> identical** — only the syntax differs.

## 7. What the debugger reveals about C++

1. **Static objects' constructors run *before* `main()`.** A breakpoint in the
   constructor is hit before `main` is entered, and the call stack shows it
   called from **"cpp_initialization" code**.
   → **C++ requires an extra step in the startup code** to invoke static
   constructors.
2. **`new` calls `malloc()` internally** — single-step and you arrive there, with
   **4** in `r0`: the same object size as in C.
3. **`new` also calls the constructor** for the dynamically allocated object.
   So C++ ensures the constructor runs **in every case** after allocation.
4. **The generated machine code is identical to the C emulation** — for the
   constructor initializer list, for the call to `moveBy()`, and for `moveBy()`'s
   implementation.
5. In the Locals window, **`this` appears at the top**, exactly as `me` did.

> **The C++ compiler generates the same code as your C emulation, for both the
> invocation and the implementation of operations.**

## 8. Encapsulation does not solve concurrency

Share a `Shape` object between two RTOS threads — modify it in blinky1, assert
about it in blinky2 — and the assertion **eventually fails**. Most of the time it
passes thousands of times; then it doesn't.

The cause is a **race condition** around the shared object (Lesson 20).

> **Even though you applied encapsulation, it did not prevent a race condition.**
>
> Encapsulation in C++ works exactly as you emulated it in C, and **boils down to
> simple function calls**. That may hide the internal implementation, but it does
> **nothing** to prevent races.

> **To achieve true encapsulation for concurrency you need the *active class*,
> also known as the *active object* design pattern** — a fusion of concurrent,
> object-oriented and event-driven programming. That is the subject of later
> lessons (33–34, 43–44).

---

## Key takeaways

1. OOP is a design approach, not a language — C can do all three pillars.
2. A **class** = attributes + operations; an **object** is an instance.
3. In C: name functions `Class_operation()` and pass `me` first, by convention.
4. `const` placement encodes what may change — design interfaces that are hard to
   misuse.
5. Constructors must be **called explicitly in C**; C++ calls them automatically.
6. The `me` pointer travels in `r0` per AAPCS; attribute access is offset-based
   and efficient.
7. C++ compiles the C emulation almost unchanged and generates **identical
   machine code**.
8. C++ static constructors run **before `main()`**; `new` = `malloc` +
   constructor.
9. **Encapsulation does not prevent race conditions.**

---

## Glossary

| Term | Meaning |
|---|---|
| Encapsulation | Bundling data with the operations that act on it |
| Abstraction / information hiding | Simplifying to essentials / hiding the implementation |
| Class / object / instance | The cookie-cutter / a thing made from it |
| Attribute / operation | OOP names for data member / member function |
| Constructor (`ctor`) | Initializes a newly allocated object |
| `me` / `this` / `self` | Pointer to the instance being operated on |
| Class diagram | Box with name, attributes, operations |
| Name mangling | Compiler-generated unique names for class operations |
| `::` | C++ scope resolution operator |
| `new` / `delete` | C++ dynamic allocation with constructor/destructor |
| Active object | Class that also encapsulates concurrency (later lessons) |

---

## Pitfalls to remember

- **Accessing struct members directly** — it breaks the encapsulation convention.
- **Forgetting to call the constructor** in C.
- **`const` on the wrong side of the `*`.**
- **Dynamic allocation in real-time code** — and forgetting `free`/`delete`.
- **A heap size of 0** when using `malloc`/`new`.
- **Assuming encapsulation gives thread safety.** It does not.
- **Using `this` as an identifier in C** — it won't survive a move to C++.

---

**Next:** Lesson 30 adds **inheritance**, in both C and C++.
