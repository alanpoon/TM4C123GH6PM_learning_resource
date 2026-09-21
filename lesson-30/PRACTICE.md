# Lesson 30 — Practice: Deriving `Rectangle` from `Shape`

**Concepts behind this exercise:** [KNOWLEDGE.md](KNOWLEDGE.md)
**Video:** <https://youtu.be/oS3a7wn9P_s>

---

## Projects in this lesson

| Directory | Toolchain | Language |
|---|---|---|
| `simulator-keil/` | KEIL MDK (`lesson.uvprojx`) | **C** |
| `simulator-keil-cpp/` | KEIL MDK (`lesson.uvprojx`) | **C++** |

Each adds `rectangle.h` / `rectangle.c` (`.cpp`) to the Lesson 29 project.

---

## Part A — Inheritance in C

### 1. Declare the derived class

Create `rectangle.h` with the usual include guards, and build the attribute
structure **starting from `Shape` rather than from scratch**:

```c
#include "shape.h"

typedef struct {
    Shape super;        /* <== INHERITS Shape -- must be the FIRST attribute */

    uint16_t width;     /* attributes added by Rectangle */
    uint16_t height;
} Rectangle;
```

The conventional name for the embedded base instance is **`super`**. (Comment it
as "inherits Shape" — Part A step 5 shows in what sense that's true.)

### 2. Declare the operations

```c
void     Rectangle_ctor(Rectangle * const me, int16_t x0, int16_t y0,
                        uint16_t w0, uint16_t h0);
void     Rectangle_draw(Rectangle const * const me);
uint32_t Rectangle_area(Rectangle const * const me);
```

- The constructor takes parameters for **all** attributes — `x0`/`y0` for the
  inherited ones, `w0`/`h0` for the added ones.
- Neither `draw()` nor `area()` needs to change the instance, so **`const` goes
  before the `*`** on `me`.

### 3. Implement `rectangle.c`

```c
void Rectangle_ctor(Rectangle * const me, int16_t x0, int16_t y0,
                    uint16_t w0, uint16_t h0)
{
    Shape_ctor(&me->super, x0, y0);   /* FIRST: initialize inherited attributes */
    me->width  = w0;                   /* then the ones added here               */
    me->height = h0;
}

uint32_t Rectangle_area(Rectangle const * const me) {
    return (uint32_t)me->width * (uint32_t)me->height;
}

void Rectangle_draw(Rectangle const * const me) {
    /* pseudocode: draw vertical and horizontal lines on the LCD */
}
```

> `draw()` stays pseudocode — all you need today is somewhere to set a breakpoint
> and confirm it was called.

### 4. Exercise the class

In `main.c`: instantiate a Rectangle, call its constructor, then call `draw()`
and `area()`. Build cleanly.

### 5. Discover the memory layout

Open the memory view at the start of RAM (`0x20000000`) and:

1. Run to the **Rectangle constructor** and step in. Note the **`me` pointer**
   value and find it in memory.
2. Step into the **Shape constructor** — **the `me` pointer value is the same**,
   even though its **type changed** from `Rectangle` to `Shape`.
3. Switch the memory view to **signed short** (both classes use 16-bit
   attributes).
4. Watch the Shape constructor fill the memory at the **beginning** of the
   Rectangle structure.
5. Watch control return and the Rectangle constructor fill the memory
   **after** the Shape portion.

**The whole Rectangle structure is aligned with its first member `super`.**

### 6. Learn why that is guaranteed

This is **not a coincidence** — it is a rule in the C language. From the ISO/IEC
C standard, *Structure and union specifiers*:

> *"A pointer to a structure object, suitably converted, points to its initial
> member… There may be unnamed padding within a structure object, but not at its
> beginning."*

> In plain terms: **a pointer to a structure can always safely be cast to a
> pointer to its initial member.**

### 7. Use the inherited operations

The consequence is that **any `Rectangle*` can be passed to a function expecting
a `Shape*`** — so all Shape operations apply to Rectangles:

```c
Shape_moveBy((Shape *)&r1, 2, -4);
Shape_distanceFrom((Shape *)&r1, (Shape *)&s1);
```

> **In this sense, Rectangle *inherits* all operations from Shape.**
>
> C requires the cast to be **explicit**. Object-oriented languages **know**
> upcasting is always safe and do it automatically (Part B).

### 8. Learn the terminology and the diagram

```
        ┌─────────┐
        │  Shape  │   base / superclass / parent class
        └────△────┘
             │       inheritance: arrow with a big TRIANGULAR end
        ┌────┴─────┐
        │ Rectangle│   derived / subclass / child class
        └──────────┘
```

- Casting derived → base goes **up** the diagram: **upcasting**.
- **The arrow points *to* the base class** — the opposite of most people's first
  guess. It points toward **generalization**; hence inheritance is also called
  generalization.
- Inheritance isn't limited to one level: `Rectangle` → `FilledRectangle` → …,
  producing whole family trees of increasingly specialized classes.

### 9. Fix your mental model

> **The family-tree analogy is wrong. Don't think about inheritance that way.**

Every derived instance **literally contains** the whole base instance, so it
**can be treated as** one. Inheritance is the **"IS A…"** relationship:

- "a Rectangle **IS A** Shape" ✓
- "Donald IS Mary Anne" ✗ — Donald does not contain an instance of his parent

**Use biological classification instead:** a House Cat **IS A** Felis, **IS A**
Carnivore, **IS A** Mammal — all simultaneously. And behaviours introduced higher
up make sense lower down: `Mammal::lactate` makes sense for Carnivores, Felis and
House Cats.

### 10. Contrast with composition

Try moving `super` **out of first position** and observe what breaks:

| | Relationship | Needs |
|---|---|---|
| **Inheritance** | **"IS A…"** | base embedded **first** → upcasting is legitimate |
| **Composition** | **"HAS A…"** | base anywhere — "Rectangle **has a** Shape" |

> Move it and you still have composition, but **you can no longer legitimately
> upcast**. You'd have to reference the component attribute explicitly, **which
> breaks encapsulation**.

Undo the change before moving on.

---

## Part B — Inheritance in C++

### 11. Set up

Copy the Lesson 29 C++ project, then copy `rectangle.h` and `rectangle.c` in from
the C version and rename the `.c` to **`.cpp`**. Open the project and **add both
files** to it.

### 12. Convert the header

```c++
class Rectangle : public Shape {     /* <== THE new element */
private:
    uint16_t width;
    uint16_t height;
public:
    Rectangle(int16_t x0, int16_t y0, uint16_t w0, uint16_t h0);
    void draw(void) const;
    uint32_t area(void) const;
};
```

- **`: public Shape`** = Rectangle **publicly inherits** Shape. (Private
  inheritance also exists — an advanced concept you needn't worry about now.)
- Everything else follows Lesson 29: drop the class-name prefix, drop `me`, keep
  the **trailing `const`**.

### 13. Convert the implementation — the constructor

Just as in C, the derived constructor must call the base constructor. C++ uses
the **constructor initializer list**:

```c++
Rectangle::Rectangle(int16_t x0, int16_t y0, uint16_t w0, uint16_t h0)
  : Shape(x0, y0),           /* <== base-class constructor */
    width(w0), height(h0)
{}
```

### 14. Meet `protected`

`draw()` needs the inherited `x` and `y` (in C: `me->super.x`). But `Shape`
declared them **`private`**, restricting access to that class only.

**Change them to `protected`** in `Shape`:

> **Derived classes at any level can then access those attributes directly, while
> they remain protected against public access.**

Now remove the `me->super.` indirection entirely — **C++ lets you access
inherited attributes directly, as if declared in the derived class**. Remove the
`me` pointer from all other parameters too; they go through the implicit `this`.

Do the same for `area()`. Compile: no errors, no warnings.

### 15. Convert `main.cpp`

Keep `main.c` open side by side for reference (it isn't part of the C++ project).

```c++
#include "rectangle.h"

static Rectangle r1(100, 50, 20, 10);   /* allocate AND initialize together */
...
r1.draw();
r1.area();

r1.moveBy(2, -4);                        /* INHERITED -- no cast needed!    */
r1.distanceFrom(s1);
```

> **Remove the explicit upcasts** — the C++ compiler performs upcasting
> automatically, knowing it is always safe.

---

## Part C — Compare the two implementations

### 16. Set up the views

Set a breakpoint in the **`Rectangle` constructor** *before* starting the
debugger. Then start it.

**The breakpoint is hit immediately** — C++ static constructors run before
`main()` (Lesson 29).

Open:

- **Locals** — `this` shows the object as the **inherited Shape part followed by
  the Rectangle attributes**;
- **Memory** at `0x20000000`, displayed as **signed shorts**.

### 17. Watch the layout being built

1. Step from the Rectangle constructor into the **Shape constructor** — the
   **`this` pointer value is unchanged**, though its **type** changed from
   `Rectangle` to `Shape`.
2. The Shape constructor fills the attributes **at the beginning** of the
   Rectangle memory.
3. Back in the Rectangle constructor, the memory **after** the Shape portion is
   filled.

**The C++ memory layout is identical to your C emulation:** the whole Rectangle
object is aligned with the inherited Shape object at its start.

### 18. Watch upcasting happen

- Step **over** the Shape operations from Lesson 29, and **into** the Rectangle
  ones (`Rectangle::draw()`, `Rectangle::area()`), using Locals and `this` to
  inspect the object.
- Now step into an **inherited** operation, `Shape::moveBy()`:

> In Locals, **`this` still points at `r1`**, but has been **automatically upcast
> to `Shape`** — and **only the Shape portion is visible**.

Watch `moveBy()` change the Shape attributes in **both** views — Locals and raw
memory. The same automatic upcast happens for `distanceFrom()`.

---

## Exercises

1. **Second level.** Derive `FilledRectangle` from `Rectangle` and verify the
   three-level memory layout.
2. **Break the rule.** Put `super` second in the C struct, then upcast anyway.
   What do you corrupt?
3. **Composition instead.** Rewrite Rectangle to *contain* a Shape as a named
   member. What does calling `moveBy` now look like, and what did you lose?
4. **Forget the base ctor.** Omit `Shape_ctor()` from `Rectangle_ctor()` and find
   out what the inherited attributes hold (recall Lesson 10).
5. **Access levels.** Try accessing a `protected` attribute from `main.cpp`.
   What does the compiler say?
6. **Sizes.** Compare `sizeof(Shape)` and `sizeof(Rectangle)` in both languages.
   Account for every byte.
7. **Draw it.** Draw the class diagram for Shape, Rectangle and one more derived
   class, getting the arrow direction right.

---

## Self-check

- [ ] `Rectangle` embeds `Shape super` as its **first** attribute
- [ ] `Rectangle_ctor()` calls `Shape_ctor()` before initializing its own
      attributes
- [ ] I verified in memory that the Shape part sits at the start of a Rectangle
- [ ] I can quote why the C standard makes upcasting safe
- [ ] I called inherited Shape operations on a Rectangle (with a cast in C)
- [ ] I can explain "IS A…" versus "HAS A…" and why the family-tree analogy fails
- [ ] The C++ version uses `: public Shape`, an initializer list, and `protected`
- [ ] I watched `this` get automatically upcast when entering an inherited
      operation
