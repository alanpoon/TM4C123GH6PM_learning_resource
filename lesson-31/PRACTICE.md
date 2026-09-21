# Lesson 31 — Practice: Virtual Functions and the VPTR/VTABLE

**Concepts behind this exercise:** [KNOWLEDGE.md](KNOWLEDGE.md)
**Video:** <https://youtu.be/xHMje9fL1Bk>

---

## Projects in this lesson

| Directory | Toolchain | Language |
|---|---|---|
| `simulator-keil-cpp/` | KEIL MDK (`lesson.uvprojx`) | **C++ only** |

> **This lesson is C++ only** — polymorphism has no direct analogue in
> procedural C, so you learn it in a language that supports it directly, and then
> emulate it in C in Lesson 32.

Start from a copy of the Lesson 30 C++ project.

---

## Part A — Discover polymorphism

### 1. Move the interface up to `Shape`

`draw()` and `area()` were added to `Rectangle`, but they make sense for **any**
shape. `Shape` is just too generic to know **how**.

> OOP separates the **interface** ("what can be done") from the
> **implementation** ("how it is done"). So let `Shape` provide the interface and
> worry about implementation later.

Copy the `area()` and `draw()` signatures from `Rectangle` up into `Shape`, and
copy their implementations too — adapted, with **dummy code**, since at this
level you don't yet know whether you're dealing with Rectangles, Circles,
Triangles or Lines.

### 2. Call through an upcast pointer

```c++
Shape *ps = &r1;     /* automatic upcast -- lesson 30 */
ps->draw();
ps->area();
```

Builds cleanly. Step into the call in the debugger:

**`Shape::draw()` runs.** Unremarkable — the compiler chose the function matching
the **type of the pointer**, not the type of the object.

### 3. Add `virtual` and watch it change

Put **`virtual`** in front of `draw()` and `area()` in the **`Shape`**
declaration. (Some warnings appear — ignore them for now.) Debug again.

**This time `Rectangle::draw()` runs.** Same for `area()`.

> The call now selects the implementation by the **type of the object**
> (Rectangle), not the **type of the pointer** (Shape). **That is polymorphism.**
>
> From Greek *poly* (many) + *morphe* (form): the same operation can take
> multiple forms, depending on what the pointer happens to point at.

### 4. Understand the warnings

The compiler said `Rectangle`'s `draw()`/`area()` "implicitly inherit virtual".
That is teaching you the terminology:

> With polymorphism, `Rectangle::draw()` is **not a new operation**. It is a
> different **form** — a different **method** — of the `draw()` **operation**
> already declared in `Shape` and inherited from it.
>
> - **Methods** = the different forms of the same operation.
> - A subclass method **overrides** the inherited one.

Add **`virtual`** to the declarations in `Rectangle` too: the warnings disappear
and the intent is explicit.

---

## Part B — What polymorphism is for

### 5. Write a generic algorithm

> Polymorphism lets you write generic code at a **higher level of abstraction**.

Declare `drawGraph()` at the **Shape** level and implement it in `shape.cpp`:

```c++
void drawGraph(Shape const *graph[]) {
    for (uint8_t i = 0U; graph[i] != nullptr; ++i) {
        graph[i]->draw();          /* <== the pivotal polymorphic call */
    }
}
```

A "graph" is just an array of `Shape*`. The pointers may point to **different**
types, but since all inherit `Shape`, **all can be safely upcast**.

### 6. Test it

```c++
static Shape const *graph[] = {
    &r1,        /* a Rectangle       */
    ps3,        /* a dynamic Shape   */
    nullptr     /* terminator        */
};
drawGraph(graph);
```

Breakpoint in `drawGraph()` and step through:

- the first `graph[i]->draw()` invokes **`Rectangle::draw()`** with `r1` as
  `this`;
- the second invokes **`Shape::draw()`** with `ps3` as `this`;
- the loop finds the null pointer and stops.

Simple and intuitive — and it does exactly what it should.

### 7. Prove the extensibility

Create a **`Circle`** subclass by copying `rectangle.h`/`rectangle.cpp` to
`circle.h`/`circle.cpp`, adding them to the project, and adapting:

| Change | To |
|---|---|
| include guards, class name | `Circle` |
| attributes | `radius` instead of `width`/`height` |
| constructor | takes `r0` for the radius |
| virtual declarations | unchanged — it provides **its own methods** for the inherited operations |
| `draw()` | calls a primitive `drawEllipse()` |
| `area()` | π·r², using integer maths with π ≈ 3 for simplicity |

In `main.cpp`, include `circle.h`, instantiate `c1`, and **add `&c1` to the
graph array**.

### 8. Notice what was NOT recompiled

Build and read the compiler output carefully:

> **Only `circle.cpp` and `main.cpp` are recompiled. `shape.cpp` — which contains
> `drawGraph()` — is NOT.** The final image therefore uses the `drawGraph()`
> compiled **before the Circle class existed**.

Run and step into `drawGraph()`: it calls **`Circle::draw()`** correctly for
`c1`, and the right methods for `r1` and `ps3`.

> **The algorithm automatically adapted to a class that did not exist when it was
> written and compiled.** That is the power and the **extensibility** of
> polymorphism — and it means the virtual call must work very differently from a
> regular one.

---

## Part C — Early vs. late binding

### 9. Put the two side by side

Add a **regular** call immediately before the virtual one:

```c++
r1.draw();        /* EARLY binding -- on a full object */
ps->draw();       /* LATE binding  -- through a pointer */
```

> **An operation invoked on a full object is always a regular, non-virtual call —
> even if the operation is declared `virtual`** — because the exact type is known
> at compile time.
>
> The connection between a call and a function body is called **call binding**.
> Established at compile/link time it is **early binding** — the only kind in
> procedural languages like C. A virtual operation invoked **through a pointer**
> uses **late binding** (also *dynamic* or *real-time* binding), because the
> pointer may have been upcast from a derived class.

### 10. Read the early-binding code

Breakpoint on the early-binding call; look at the disassembly:

```
MOV r0, <this>            ; the 'this' pointer
BL  Rectangle::draw       ; target HARD-CODED in the instruction
```

Step into the `BL`: you land in `Rectangle::draw()`. **Note its address in ROM**
— e.g. `0x250C`.

### 11. Read the late-binding code

Return to `main.cpp`. The late-binding code is **significantly different**:

```
LDR r0, [r6]        ; fetch a 32-bit word FROM the object...
LDR r1, [r0]        ; ...use it as a BASE ADDRESS -> so it's a POINTER
MOV r0, r6          ; 'this' as the first parameter
BLX r1              ; Branch with Link and eXchange -> call the address in r1
```

> But wait — **you never added any pointer attribute** to `Shape` or `Rectangle`.

---

## Part D — Reverse-engineer the VPTR and VTABLE

### 12. Find the hidden pointer

Inspect the `this` object in RAM. You can recognize the 16-bit `x`, `y`
(inherited) and `width`, `height` (added) — **preceded by a pointer**.

> The C++ compiler **added it when you introduced virtual functions in `Shape`**.
> The Rectangle object is now **larger by one pointer**.

### 13. Follow it into ROM

The added pointer looks like a **ROM** address. View that location as **32-bit
words**:

| Word | Value | Which is |
|---|---|---|
| 1st | suspiciously like `0x250C` — in fact **`0x250C + 1`** | the address of **`Rectangle::draw()`** |
| 2nd | e.g. `0x2503` | subtract 1 → `0x2502`, found in the disassembly as **`Rectangle::area()`** |

> The **+1** is the familiar Thumb-state bit: program addresses on Cortex-M are
> stored **odd** (Lesson 8).

### 14. Name what you found

> The compiler added a pointer at the beginning of the class attributes, pointing
> to a **table in ROM containing the addresses of all virtual functions**.
>
> - the table is the **VTABLE**;
> - the pointer inside the object is the **VPTR**.

Continue stepping: the address of `Rectangle::draw()` is loaded from the VTABLE
into `r1`, `this` is placed in `r0`, and **`BLX r1`** performs the call. Step
into it — you are in `Rectangle::draw()`, and examining `this` you can see the
**vptr is the first attribute of the inherited Shape slice**.

### 15. Account for the cost

| Overhead of late binding | Where |
|---|---|
| a **VPTR per object** | RAM |
| a **VTABLE per class** | ROM |
| **two extra instructions** per call | code |

> Not large for what you get, but **non-negligible** — which is why **C++ applies
> it only to explicitly `virtual` functions**. Other OO languages like **Java**
> treat polymorphism as so fundamental that **all functions are virtual by
> default**.
>
> Note too that VPTR–VTABLE is only **one possible implementation**; the C++
> standard leaves it open. In practice **essentially all C++ implementations use
> it**.

---

## Part E — Who sets the VPTR?

### 16. The question

The per-class VTABLE is a ROM constant, easy to emit. But the **VPTR must be set
in every instance**, many allocated at run time (e.g. automatic objects).

You wrote no code for it — **so the compiler must have synthesized some**. As an
embedded engineer, go and look.

### 17. Step through construction

Breakpoint in the **`Rectangle` constructor** and start the debugger. It is hit
immediately (a static Rectangle is constructed before `main`).

1. Expand `this` — **the vptr is not yet initialized**.
2. **Step in** → you are in the **`Shape` constructor**, invoked from Rectangle's
   initializer list. `this` changes type to `Shape`; the vptr is **still**
   uninitialized.
3. In the disassembly, spot **two instructions that set the VPTR**.
4. **Which VTABLE?** Inspect it in memory, take the **second entry**, subtract 1,
   and find it in the disassembly → **`Shape::area()`**. So it is the **Shape**
   VTABLE.

> At this stage of initialization, the Rectangle instance is **still treated as
> its Shape base class**.

5. Step through the Shape attribute initialization and return to the **Rectangle
   constructor**. Similar instructions **re-initialize the VPTR**.
6. Check that VTABLE's second entry the same way → **`Rectangle::area()`**. It is
   now the **Rectangle** VTABLE.

### 18. The rule

> **The VPTR is initialized and re-initialized in every constructor to point to
> that class's VTABLE.** Because constructors run **base class first**, the VPTR
> ends up pointing to the **most derived** class — which is correct.

---

## Exercises

1. **Third subclass.** Add `Triangle`, put it in the graph, and confirm
   `drawGraph()` still isn't recompiled.
2. **Measure the growth.** Compare `sizeof(Rectangle)` before and after adding
   `virtual`. Account for every byte.
3. **Count the instructions.** Compare cycle counts for an early-bound and a
   late-bound call.
4. **Constructor trap.** Call a virtual operation from inside `Shape`'s
   constructor. Which method runs, and why?
5. **Read the VTABLE.** For a three-level hierarchy, dump all three VTABLEs and
   map every entry to a function.
6. **Non-virtual override.** Remove `virtual` from `Shape::draw()` but keep
   `Rectangle::draw()`. What does `drawGraph()` do now?
7. **Java comparison.** Explain in two sentences the trade-off C++ makes by
   requiring `virtual` explicitly.

---

## Self-check

- [ ] I saw the same call select `Shape::draw()` then `Rectangle::draw()`, before
      and after adding `virtual`
- [ ] I can explain *operation* vs. *method* and what *override* means
- [ ] `drawGraph()` works on classes compiled after it
- [ ] I can distinguish early from late binding and predict which a call uses
- [ ] I found the hidden pointer in the object and followed it into ROM
- [ ] I identified both VTABLE entries as the two virtual functions (+1 for Thumb)
- [ ] I watched the VPTR get set in the base constructor and re-set in the derived
- [ ] I can state the three components of late-binding overhead
