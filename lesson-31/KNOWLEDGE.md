# Lesson 31 — Knowledge: OOP Part 3 — Polymorphism in C++

**Video:** <https://youtu.be/xHMje9fL1Bk> · **Transcript:** <https://www.state-machine.com/course/lesson-31.txt>
**Hands-on companion:** [PRACTICE.md](PRACTICE.md)

---

## The big idea

> Unlike encapsulation and inheritance, **polymorphism is a uniquely
> object-oriented concept with no direct analogue in a traditional procedural
> language like C.**

So the order is reversed from Lessons 29–30: **learn it in C++ first**, then
emulate it in C (Lesson 32).

**Polymorphism** — from the Greek *poly* (many) and *morphe* (form) — means the
**same operation can take multiple forms**, selected by the **type of the
object**, not the type of the pointer.

---

## 1. Motivation: interface at the base class

`draw()` and `area()` were added to `Rectangle` — but they make sense for **any**
Shape: any shape can be drawn and has a surface area. The problem is that
`Shape` is **too generic to know *how***.

> OOP is all about separating the **interface** — "what can be done" — from the
> **implementation** — "how it is done". So `Shape` can provide the
> **interface** to `draw()` and `area()` and worry about implementation later.

## 2. `virtual` turns on polymorphism

Move the signatures up into `Shape` with dummy bodies, then upcast and call:

```c++
Shape *ps = &r1;     /* automatic upcast */
ps->draw();
ps->area();
```

| Declaration in `Shape` | Which implementation runs |
|---|---|
| plain | **`Shape::draw()`** — chosen by the **type of the pointer** |
| **`virtual`** | **`Rectangle::draw()`** — chosen by the **type of the object** |

> With `virtual`, the call suddenly selects the implementation based on the
> **type of the object** (Rectangle), not the **type of the pointer** (Shape).
> **That is polymorphism in action.**

## 3. Methods and overriding

The compiler warns that `Rectangle`'s `draw()`/`area()` "implicitly inherit
virtual". The warning is teaching you something:

> With polymorphism, `Rectangle::draw()` is **not a new operation**. It is
> merely a different **form** — a different **method** — of the `draw()`
> **operation** already specified in `Shape` and inherited from it.

**Terminology:**

- **Methods** describe the different **forms of the same operation**.
- A method in a subclass is said to **override** the method inherited from the
  base class.

Marking them **`virtual`** in the subclass too silences the warnings and states
the intent.

## 4. What polymorphism buys: generic code

> Polymorphism lets you **write generic code at a higher level of abstraction**
> than you could without it.

```c++
void drawGraph(Shape const *graph[]) {
    for (uint8_t i = 0U; graph[i] != nullptr; ++i) {
        graph[i]->draw();     /* <== THE polymorphic call */
    }
}
```

A "graph" is just an array of `Shape*`. The pointers may point to **different
types**, but since all inherit `Shape`, **all can be safely upcast**. The
`graph[i]->draw()` call then selects the right method per object.

### The extensibility demonstration

Add a brand-new `Circle` subclass (radius instead of width/height; `draw()` calls
`drawEllipse()`; `area()` = π·r², approximating π as 3 with integer math), put a
Circle into the graph, and rebuild.

> **Note which files are recompiled: only `circle.cpp` and `main.cpp`.
> `shape.cpp` — containing `drawGraph()` — is NOT recompiled.** The final image
> uses the `drawGraph()` compiled **before the Circle class even existed**.

And yet it calls `Circle::draw()` correctly.

> **The algorithm automatically adapted to a class that didn't exist when it was
> written.** That is the power and the **extensibility** of polymorphism — and a
> sign that the virtual call must work quite differently from a regular call.

## 5. Early vs. late binding

> The connection between a function call and a function body is called **call
> binding**.

| | When resolved | Called |
|---|---|---|
| **Early binding** | compile and link time | the only kind in procedural languages like C |
| **Late binding** (a.k.a. dynamic or real-time binding) | **run time** | used for a `virtual` operation invoked **through a pointer** |

> **An operation invoked on a full object — like `r1.draw()` — is always a
> regular, non-virtual call, even if the operation is declared `virtual`**,
> because the exact type is known at compile time.

Late binding applies only when the exact type **cannot** be known at compile
time, because the pointer may have been upcast from a derived class.

## 6. Reverse-engineering the virtual call

### Early binding

```
MOV r0, this          ; the 'this' pointer
BL  Rectangle::draw   ; target address HARD-CODED in the instruction
```

### Late binding

```
LDR r0, [r6]          ; fetch a 32-bit word FROM the object -> it is a POINTER
LDR r1, [r0]          ; use it as a base address: fetch the function address
MOV r0, r6            ; 'this' as the first parameter
BLX r1                ; Branch with Link and eXchange -- call the address in r1
```

### What that 32-bit word is

Inspect the object in RAM and you see the 16-bit `x`, `y` (inherited) and
`width`, `height` (added) — **preceded by a pointer you never declared**. The
C++ compiler **added it when you introduced virtual functions in `Shape`**, so
the object grew by one pointer.

That pointer holds a **ROM** address. Viewing that ROM location as 32-bit words:

| Word | Value | Is |
|---|---|---|
| 1st | `Rectangle::draw` address **+ 1** | the first virtual function |
| 2nd | `Rectangle::area` address **+ 1** | the second virtual function |

(The **+1** is the familiar Thumb-state bit — program addresses on Cortex-M are
stored odd, Lesson 8.)

### The names

> The compiler added a pointer at the beginning of the class attributes, pointing
> to a table in ROM containing the addresses of all virtual functions.
>
> - The table is called the **VTABLE**.
> - The pointer stored in the object is called the **VPTR**.

### The cost

| Overhead of late binding | Where |
|---|---|
| one **VPTR per object** | **RAM** |
| one **VTABLE per class** | **ROM** |
| **two extra instructions** per call | code |

> Not a large overhead for what you get, but **non-negligible** — which is why
> **C++ applies it only to functions explicitly declared `virtual`**, to avoid
> paying for functions that don't need it. Other OO languages — **Java**, for
> example — treat polymorphism as so fundamental that **all functions are virtual
> by default**.

> The VPTR–VTABLE double indirection is only **one possible implementation**; the
> C++ standard leaves it entirely to compiler designers. That said, **essentially
> all existing C++ implementations across a wide range of CPUs use it.**

## 7. Who sets the VPTR?

The VTABLE is a ROM constant per class, easy to emit. But the **VPTR must be set
in every instance**, many of which are allocated at run time.

> **The ideal place to establish the VPTR is the class constructor** — and the
> C++ compiler **secretly synthesizes** the code to do it.

Stepping through a `Rectangle` construction shows:

1. On entry to the Rectangle constructor, the **vptr is not yet initialized**.
2. Stepping into the **`Shape` constructor** (invoked from Rectangle's
   initializer list), `this` changes type to Shape, and two machine instructions
   **set the VPTR** — to the **Shape VTABLE** (verified by checking that its
   second entry is `Shape::area`).
3. Returning to the **Rectangle** constructor, similar instructions
   **re-initialize the VPTR** — now to the **Rectangle VTABLE** (second entry:
   `Rectangle::area`).

> **The VPTR is initialized and re-initialized in every constructor to point to
> that class's VTABLE.** Because constructors run **base class first**, the VPTR
> ends up pointing to the **most derived** class — which is correct.

> Consequence worth noting: during a base-class constructor, the object is still
> treated as an instance of that base class.

---

## Key takeaways

1. Polymorphism = the same operation taking **multiple forms**, chosen by the
   **object's** type.
2. `virtual` is what enables it; without it the **pointer's** type decides.
3. A subclass provides a **method** that **overrides** the inherited one.
4. Polymorphism enables **generic, extensible** code — algorithms work with
   classes that did not exist when they were compiled.
5. **Early binding** is resolved at compile/link time; **late binding** at run
   time.
6. A virtual call **through an object** is still early-bound.
7. Late binding is implemented with a **VPTR** in each object and a **VTABLE**
   per class in ROM.
8. Cost: one pointer per object, one table per class, two instructions per call.
9. **Constructors set the VPTR**, base class first, so the most derived VTABLE
   wins.

---

## Glossary

| Term | Meaning |
|---|---|
| Polymorphism | "Many forms" — object-determined behaviour |
| `virtual` | Marks an operation as polymorphic |
| Operation vs. method | The interface vs. one concrete form of it |
| Override | Supply a subclass's own method for an inherited operation |
| Call binding | Connecting a call to a function body |
| Early / late binding | Resolved at compile-link time / at run time |
| VPTR | Pointer to the VTABLE, stored in each object |
| VTABLE | Per-class table of virtual function addresses, in ROM |
| `BLX` | Branch with Link and eXchange — indirect call |

---

## Pitfalls to remember

- **Forgetting `virtual`** — you silently get the base-class behaviour.
- **Expecting a virtual call on a full object** to be late-bound. It isn't.
- **Ignoring the RAM cost** of the VPTR in objects you create by the thousand.
- **Calling a virtual operation from a base-class constructor** — the VPTR still
  points at the base VTABLE.
- **Assuming the VPTR–VTABLE layout is standardized.** It isn't, though it is
  universal in practice.

---

**Next:** Lesson 32 implements exactly this VPTR–VTABLE design **by hand in
portable C**, and gives guidelines on when *not* to use polymorphism.
