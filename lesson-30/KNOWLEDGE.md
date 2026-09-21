# Lesson 30 — Knowledge: OOP Part 2 — Inheritance in C and C++

**Video:** <https://youtu.be/oS3a7wn9P_s> · **Transcript:** <https://www.state-machine.com/course/lesson-30.txt>
**Hands-on companion:** [PRACTICE.md](PRACTICE.md)

---

## The big idea

> **Inheritance is the ability to define new classes based on existing classes,
> in order to reuse the code and organization.**

In C it is achieved by **embedding an instance of the base class as the very
first attribute** — and the C standard guarantees that this works.

---

## 1. The motivation

A real LCD needs not nebulous `Shape`s but concrete **Rectangles, Circles and
Triangles**. Writing each from scratch, you would notice they **all** contain a
position and operations to move and measure distance — exactly what `Shape`
already has.

## 2. Inheritance in C

```c
/* rectangle.h */
#include "shape.h"

typedef struct {
    Shape super;        /* <== INHERITS Shape -- must be the FIRST attribute */

    uint16_t width;     /* attributes added by Rectangle */
    uint16_t height;
} Rectangle;

void     Rectangle_ctor(Rectangle * const me, int16_t x0, int16_t y0,
                        uint16_t w0, uint16_t h0);
void     Rectangle_draw(Rectangle const * const me);
uint32_t Rectangle_area(Rectangle const * const me);
```

The conventional name for the embedded base instance is **`super`**.

The constructor takes parameters for **all** attributes — inherited *and* added —
and calls the base-class constructor first:

```c
void Rectangle_ctor(Rectangle * const me, int16_t x0, int16_t y0,
                    uint16_t w0, uint16_t h0)
{
    Shape_ctor(&me->super, x0, y0);   /* initialize the INHERITED attributes */
    me->width  = w0;                   /* then the added ones                */
    me->height = h0;
}
```

## 3. Why embedding first is enough — the C standard guarantees it

The memory layout puts `super` at the **very beginning** of the `Rectangle`
structure, so a `Rectangle*` and a `Shape*` have the **same address**. That is
not a coincidence — **it is a rule enshrined in the C language**. ISO/IEC C,
*Structure and union specifiers*:

> *"A pointer to a structure object, suitably converted, points to its initial
> member… There may be unnamed padding within a structure object, but not at its
> beginning."*

> A complicated way of saying: **a pointer to a structure can always safely be
> cast to a pointer to its initial member.**

**The consequence:** any `Rectangle*` may be passed to a function expecting a
`Shape*`, so **all Shape operations apply to Rectangles as well**:

```c
Shape_moveBy((Shape *)&r1, 2, -4);              /* C needs the explicit cast */
Shape_distanceFrom((Shape *)&r1, (Shape *)&s1);
```

> **In this sense the Rectangle class *inherits* all operations from Shape.**

## 4. Upcasting

C requires the cast to be **explicit**. Truly object-oriented languages **know
that this cast is always safe** and perform it **automatically**.

The name comes from the class diagram, drawn traditionally with the **base class
on top**:

```
        ┌─────────┐
        │  Shape  │   <- base / superclass / parent class
        └────△────┘
             │       <- inheritance: arrow with a big TRIANGULAR end
        ┌────┴─────┐
        │ Rectangle│   <- derived / subclass / child class
        └──────────┘
```

Casting derived → base goes **up** the diagram: **upcasting**.

> **The arrow points to the base class** — often the exact opposite of what
> people first expect. It points in the direction of **generalization**, from the
> more specialized class to the more general one. Hence inheritance is also
> called **generalization**.

| Base class | Derived class |
|---|---|
| superclass | subclass |
| parent class | child class |

Inheritance is **not limited to one level**: `Rectangle` can itself be
specialized into `FilledRectangle`, and so on — producing whole family trees of
increasingly specialized classes.

## 5. How to *think* about inheritance

> **The family-tree analogy is incorrect. Do not think about inheritance that
> way.**

Every instance of the derived class **literally contains the whole instance of
the base class** — so any derived instance **can be treated as** an instance of
the base class. Inheritance establishes the **"IS A…"** relationship:

> "a Rectangle **IS A** Shape" ✓

The family tree doesn't work like that: Donald does not contain an instance of
his parent Mary Anne, and "Donald IS Mary Anne" is meaningless.

### A better analogy: biological classification

> A House Cat **IS A** Felis (cat-like animal). A House Cat **IS A** Carnivore
> (predominantly meat-eating). A House Cat **IS A** Mammal (feeds young with
> milk) — all at the same time.

And **behaviours introduced at higher levels make sense in the lower ones**: the
`Mammal` class introduces *lactate*, which makes sense for Carnivores, Felis and
House Cats alike.

## 6. Inheritance vs. composition

You implemented inheritance by **embedding** — which is literally
**composition**: the subclass is the superclass plus some added attributes and
operations. The two are similar and can accomplish similar goals, but:

| | Relationship | Requires |
|---|---|---|
| **Inheritance** | **"IS A…"** | the superclass embedded as the **very first** attribute, so upcasting is legitimate |
| **Composition** | **"HAS A…"** | the component anywhere — "Rectangle **has a** Shape" |

> Move the superclass instance to a different position and you still have
> composition, but **you can no longer legitimately upcast**. You would have to
> explicitly reference the component attribute, **which would break
> encapsulation**.

## 7. Inheritance in C++

```c++
class Rectangle : public Shape {     /* <== the inheritance syntax */
private:
    uint16_t width;
    uint16_t height;
public:
    Rectangle(int16_t x0, int16_t y0, uint16_t w0, uint16_t h0);
    void draw(void) const;
    uint32_t area(void) const;
};
```

- **`: public Shape`** means Rectangle **publicly inherits** Shape. (Private
  inheritance also exists — an advanced concept you needn't worry about yet.)
- The rest is as in Lesson 29: drop the class-name prefix, drop `me`, keep the
  trailing `const`.

### Calling the base constructor

C++ provides special syntax, based on the **constructor initializer list**:

```c++
Rectangle::Rectangle(int16_t x0, int16_t y0, uint16_t w0, uint16_t h0)
  : Shape(x0, y0),      /* <== base-class constructor */
    width(w0), height(h0)
{}
```

### `protected` — the third access level

`Rectangle::draw()` needs the inherited `x` and `y` (written `me->super.x` in C).
But `Shape` declared them **`private`**, which restricts access to that class
alone.

> For derived classes to have access, the base class must grant **`protected`**
> access: **derived classes at any level can access such attributes directly,
> but they remain protected against public access.**

Then the indirection disappears — **C++ lets you access inherited attributes
directly, as though they were declared in the derived class**.

### Automatic upcasting

```c++
r1.moveBy(2, -4);       /* no cast at all -- the compiler upcasts automatically */
```

## 8. What the debugger shows

Set a breakpoint in the `Rectangle` constructor and view memory at the start of
RAM as **signed shorts** (both classes' attributes are 16-bit).

1. The breakpoint is hit **before `main()`** — C++ static constructors again
   (Lesson 29).
2. Step from the Rectangle constructor into the **Shape constructor**: the
   **`me` / `this` pointer value is the same**, even though its **type changed**
   from `Rectangle` to `Shape`.
3. The Shape constructor fills memory **at the beginning** of the Rectangle
   structure; back in the Rectangle constructor, the memory **after** the Shape
   portion is filled.
4. **The whole Rectangle object is aligned with its inherited Shape part.**
5. **The C++ memory layout is identical to the C emulation.**
6. In the **Locals** view, `this` shows the object as the inherited Shape part
   followed by the attributes added by Rectangle.
7. When an **inherited** operation like `Shape::moveBy()` is entered, `this`
   still points at `r1` but has been **automatically upcast** to `Shape` — and
   **only the Shape portion is visible** in Locals.

---

## Key takeaways

1. Inheritance reuses attributes and operations from an existing class.
2. In C: **embed the base instance as the first attribute**, conventionally named
   `super`.
3. The C standard guarantees a pointer to a struct converts to a pointer to its
   **initial member** — that is what makes upcasting safe.
4. All base-class operations therefore apply to derived objects.
5. C requires **explicit** upcasts; C++ does them **automatically**.
6. In a class diagram the inheritance arrow points **to the base class** —
   toward generalization.
7. Think **"IS A…"** (biological classification), **not** family trees.
8. Embedding first gives **inheritance**; embedding elsewhere gives only
   **composition** ("HAS A…") and no legitimate upcasting.
9. C++ uses `: public Base`, the constructor initializer list, and **`protected`**
   for derived-class access.
10. The C++ memory layout is **identical** to the C emulation.

---

## Glossary

| Term | Meaning |
|---|---|
| Inheritance / generalization | Defining a class based on an existing class |
| `super` | Conventional name for the embedded base instance in C |
| Base / super / parent class | The more general class |
| Derived / sub / child class | The more specialized class |
| Upcasting | Converting a derived pointer to a base pointer |
| "IS A…" / "HAS A…" | Inheritance / composition relationships |
| `: public Base` | C++ public inheritance syntax |
| `protected` | Accessible to derived classes, not to the public |
| Constructor initializer list | C++ syntax that also invokes the base constructor |

---

## Pitfalls to remember

- **Not placing `super` first** — upcasting becomes illegal and you're left with
  composition.
- **Forgetting to call the base constructor** — inherited attributes stay
  uninitialized.
- **Thinking in family trees** instead of "IS A…".
- **Misreading the class-diagram arrow** — it points *to* the base.
- **Leaving base attributes `private`** when derived classes need them — use
  `protected`.
- **Reaching into `me->super.x` from outside** — that breaks encapsulation.

---

**Next:** Lesson 31 adds **polymorphism**, which is what turns inheritance into
real extensibility.
