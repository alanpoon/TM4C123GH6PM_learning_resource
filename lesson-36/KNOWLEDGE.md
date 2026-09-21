# Lesson 36 — Knowledge: State Machines Part 2 — Guard Conditions

**Video:** <https://youtu.be/RGQRllvrMyY> · **Transcript:** <https://www.state-machine.com/course/lesson-36.txt>
**Hands-on companion:** [PRACTICE.md](PRACTICE.md)

---

## The big idea

A state machine's structure is **fixed at design time**. **Guard conditions** and
**choice pseudostates** add the **run-time flexibility** needed when behavior
depends on information not known until the program runs.

> But they are **IFs and ELSEs — exactly what the state machine was designed to
> eliminate**. Use them **judiciously**.

---

## 1. Recap: why states work

> A state machine is **the most optimal solution to the very common problem where
> a system must react to events based not only on the event type, but also on the
> history of past events.**

In BlinkyButton the **same** `TIMEOUT` event must produce **different** reactions:

| Current state | TIMEOUT causes |
|---|---|
| `off` | LED **on**, transition to `on` — the transition *is* how the change is remembered |
| `on` | LED **off**, transition to `off` |

> The reaction depends on **both** the event signal **and** the current state.

## 2. The limitation

> The whole structure of states and transitions is **fixed at design time**.
>
> **But what if the behavior — the reactions to future events — depends on user
> input, which is not known until run time?** Then for certain transitions **you
> cannot know at design time which state to go to.**

## 3. Choice pseudostates and guards

The UML answer **combines a flowchart decision point with the transition
notation**:

```
            ┌──────────┐
   TIMEOUT / decrement │
   ───────────▶  ◇  ───┼──[ blink_ctr > 0 ]/ actions ──▶  blink
                 │     │
                 └─────┴──[ else ]/ actions ───────────▶  boom
```

| Element | Notation | Meaning |
|---|---|---|
| **Choice pseudostate** | a **diamond** | a run-time decision point |
| **Guard condition** | a boolean expression in **square brackets** | if **true**, that transition segment is taken; otherwise the transition is **disabled** (not taken) |
| **`else` guard** | `[else]` | the special complementary guard — **true only if all other guards from the same choice pseudostate are false** |

Guards are **simply boolean expressions evaluated at run time**.

## 4. Worked example: the Time Bomb

Required behavior:

> After reset, light the **green** LED and wait for **SW1**. When pressed, turn
> green off and start timing out: blink the **red** LED on and off **3 times**,
> then **explode** — emulated by turning **all three LED colors on**.
>
> In short: green light; then **blink-pause, blink-pause, blink-pause, boom!**

### The design, step by step

| State | Handles | Actions / transition |
|---|---|---|
| *(initial)* | — | green LED on → `wait4button` |
| **`wait4button`** | `BUTTON_PRESSED` | green off, red on, arm the time event for ½ s, **initialize `blink_ctr` = 3** → `blink` |
| **`blink`** | `TIMEOUT` | red off, arm ½ s → `pause` |
| **`pause`** | `TIMEOUT` | **decrement `blink_ctr`**, then a **choice** (below) |
| **`boom`** | *(nothing)* | final state |

### The crux — where does `pause` go?

> You **cannot naively go back to `blink`**, because that would create an endless
> loop of blink and pause.

**The brute-force solution** — three groups of blink/pause states — is
**brain-dead**:

> not only because there are so many states **mindlessly repeating essentially
> the same behavior**, but also because **the number of blinks is likely to
> change**. Indeed, the next feature could easily be making the number of blinks
> **user-adjustable** — so **you simply cannot know it at design time.**

**The smart solution:** add a **`blink_ctr`** to the state machine, decrement it
each time through `pause`, and use a **choice pseudostate**:

- `[blink_ctr > 0]` → prepare for another blink (red on, arm ½ s) → **`blink`**
- `[else]` → explode (all LEDs on) → **`boom`**

> **Don't forget the actions on each transition segment** — each must prepare for
> the state it enters.

And **initialize the counter** somewhere **upstream** of `blink` — `wait4button`
is a natural place.

## 5. Implementation

> **Choice pseudostates and guard conditions turn out to be simply IFs and ELSEs
> in your C code.**

```c
case PAUSE: {
    switch (e->sig) {
        case TIMEOUT_SIG: {
            --me->blink_ctr;                     /* the first transition segment */

            if (me->blink_ctr > 0U) {            /* [blink_ctr > 0]              */
                BSP_ledRedOn();
                TimeEvent_arm(&me->te, HALF_SEC, 0U);
                me->state = BLINK;
            }
            else {                               /* [else]                       */
                BSP_ledRedOn();
                BSP_ledGreenOn();
                BSP_ledBlueOn();
                me->state = BOOM;
            }
            break;
        }
    }
    break;
}
case BOOM: {                                      /* handles no events           */
    break;
}
```

Note the mapping:

| Diagram | Code |
|---|---|
| first transition segment (before the diamond) | statements before the `if` |
| guard condition | the **`if`** test |
| `else` guard | the **`else`** branch |
| actions on a segment | statements in that branch |
| target state | assignment to the state variable |
| a state handling no events | an **empty** `case` |

## 6. The warning — use guards judiciously

> **These are exactly the same IFs and ELSEs that the state machine was designed
> to eliminate in the first place** — just recall the spaghetti code of the
> Visual Basic calculator.
>
> **The moral: guard conditions should be used very judiciously.** Too many of
> them and **you'll find yourself back at square one (spaghetti)**, where the
> guards effectively take over handling of all the relevant conditions in the
> system.
>
> On the other hand, **guards are sometimes unavoidable**, because they add the
> necessary flexibility.

> **One of the main challenges in becoming an effective state machine designer is
> to develop a sense for which parts of the behavior should be captured in states
> and transitions at design time, and which parts require the run-time
> flexibility of guard conditions.**
>
> **Generally, challenge yourself to use as few guard conditions as possible.**

---

## Key takeaways

1. A state machine's structure is fixed at design time; guards add run-time
   flexibility.
2. A **choice pseudostate** (diamond) is a run-time decision point on a
   transition.
3. A **guard** is a boolean expression in square brackets; false ⇒ the transition
   is **disabled**.
4. **`[else]`** is true only when all sibling guards are false.
5. A counter plus a guard replaces a brute-force explosion of near-identical
   states.
6. In code: guards become **`if`/`else`**; each segment carries its own actions.
7. **Guards are the very IFs state machines exist to eliminate — use as few as
   possible.**

---

## Glossary

| Term | Meaning |
|---|---|
| Choice pseudostate | Diamond-shaped run-time decision point |
| Guard condition | Boolean expression in `[...]` gating a transition segment |
| `[else]` guard | Complementary guard, true when all others are false |
| Disabled transition | One whose guard evaluated false — not taken |
| Transition segment | One leg of a transition split by a choice pseudostate |

---

## Pitfalls to remember

- **Forgetting the `[else]` branch** — the event is silently ignored.
- **Omitting actions on a transition segment** — the target state isn't prepared
  (e.g. the time event never armed).
- **Initializing a counter in the wrong place** — it must be upstream of the loop.
- **Creating an endless loop** between two states with no exit condition.
- **Too many guards** — you have re-created spaghetti inside a state machine.
- **Brute-force state duplication** instead of a counter plus a guard.

---

**Next:** Lesson 37 examines **input-driven (polled) state machines** and how
they compare with the event-driven kind.
