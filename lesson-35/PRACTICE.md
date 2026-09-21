# Lesson 35 — Practice: Your First State Machine

**Concepts behind this exercise:** [KNOWLEDGE.md](KNOWLEDGE.md)
**Video:** <https://youtu.be/EBSxZKjgBXI>

---

## Projects in this lesson

| Directory | Contents |
|---|---|
| `tm4c123-uc_ao-keil/` | KEIL + µC/AO — the BlinkyButton AO, now as a state machine |
| `demo_vbcalc/` | the **Visual Basic calculator** demo — the spaghetti-code cautionary tale |
| `ucos2/`, `CMSIS/`, `ek-tm4c123gxl/` | kernel and support code |

Start from a copy of the Lesson 34 project.

### Tooling

The state diagram is drawn with the freeware **QM** modeling tool from Quantum
Leaps (download from state-machine.com as part of the QP-bundle).

> **You do not need such a tool to apply state machines.** A piece of paper or a
> whiteboard is completely adequate, especially in the beginning. A drawing tool
> is just more convenient for a screencast — and it's interesting to see how one
> works.

---

## Part A — See why ad-hoc flags fail

### 1. Recall where the flag came from

In Lesson 34, blinking the green LED required inventing a boolean **`isLedOn`**
to remember the LED state **between TIMEOUT events**.

That flag was **not needed** in the original RTOS thread, because sequential code
**blocked** inside `OSTimeDly()`; on unblocking it **knew exactly which event it
got**.

| | Context management | Cost |
|---|---|---|
| Sequential / blocking | **automatic** | **a whole private stack** in precious RAM; event sequences hard-coded |
| Event-driven | **manual** — you invent flags | less stack, but you must preserve context yourself |

### 2. Watch manual context management collapse

Open the **`demo_vbcalc/`** four-operation calculator and try:

```
3   +   -   =   4   =
```

**It crashes** with "Run-time error 13".

> One might argue that is a meaningless sequence — but **no user actions should
> crash software like that.** The calculator has trouble responding to events in
> different **contexts**.

Now read its source. Even without knowing Visual Basic you can see:

- **a bunch of variables whose sole purpose is remembering context** —
  `DecimalFlag` (has a decimal point been entered?), `NumOps` (how many operands
  so far?), and more;
- **event handlers** per group of keys — `Operator_Click()`, handling `+ - × ÷`,
  is the most complex;
- **flags checked in nontrivial conditions, modified, and re-checked**, so it is
  **really hard to know which path will be taken**.

> A 4-operation calculator is **already complex enough to reach the limit** of
> this approach: **fixing one bug tends to create another.**
>
> **The real problem: the flags hold too much redundant, poorly organized
> information, which easily becomes internally inconsistent.**

### 3. Extract the correct intuition

Those flags reflect a **right instinct** — you don't need the *whole* history,
only its **relevant** part: **what influences the future behavior.**

Worked example: a **keyboard controller** produces lower- or upper-case codes
depending on **Shift**. It doesn't matter which other keys were pressed, or how
many. **The entire relevant history reduces to two classes: "normal" and
"shifted".**

> **A *state* is an equivalence class of past histories, all equivalent in that
> the future behavior given any of them is identical.**
>
> A **state machine** = all those states **plus the transitions** between them.

---

## Part B — Design the diagram

### 4. Create the model

Launch QM; in the **New Model** dialog pick **Blinky** as a starting point,
rename the model **BlinkyButton**, and place it in this lesson's directory. Then
**delete the supplied state machine** so you build yours from scratch.

### 5. Start with the initial transition

> **A good place to start is the default state and the initial transition**,
> because that establishes the initial condition of the system.

You may not know what to call the state yet — **don't worry, it becomes clear as
you flesh out the transition.**

Actions on the initial transition: **turn the green LED off** and **arm the time
event for the off-time**.

> QM gives two fields per action: a small one for **pseudocode** (a concise
> comment about what the action is for — this is what appears on the diagram,
> reducing clutter) and a larger one for **actual C code**, used for automatic
> code generation.

Those actions mean the default state represents the LED being off — so name it
**`off`**.

### 6. Add the second state

In `off`, a **TIMEOUT** event must turn the green LED **on** and arm the time
event for the on-time. But then you **can no longer stay in `off`** — so add an
**`on`** state and point the transition there.

In `on`, **TIMEOUT** transitions back to `off`, turning the LED off and arming
for the off-time.

**The blinking feature is now complete.**

### 7. Add the button events as internal transitions

This is Blinky**Button**, so handle `BUTTON_PRESSED` and `BUTTON_RELEASED` — as
**internal transitions**, which **execute actions but cause no change of state**
(drawn inside the state, with actions after a `/`):

- `BUTTON_PRESSED` → blue LED on, **halve** the green blink time;
- `BUTTON_RELEASED` → blue LED off.

**These must be repeated in the `on` state too.**

> Such repetition is no big deal here, but **in real projects repetitions like
> that can, and will, pile up. This is exactly the problem hierarchical state
> machines are designed to solve** (Lesson 40).

### 8. Learn the notation you just used

| Element | Notation |
|---|---|
| State | rounded rectangle, name at top |
| Regular transition | arrow labelled with the triggering event |
| Internal transition | the event written **inside** the state |
| Actions | after a **slash**, on either kind of transition |
| Initial transition | mandatory; points to the **default state**, active before any event is dispatched |

---

## Part C — Code it

### 9. Add the state variable

> The main job of a state machine is to **remember its current state** — which
> needs a **state variable**, most straightforwardly an **enumeration**:

```c
enum { OFF_STATE, ON_STATE } state;    /* private data of the AO */
```

**This replaces all the manual-context flags** — so **delete `isLedOn`**.

> In BlinkyButton that hardly looks like an improvement. But remember: in a
> machine like the calculator you would **still have only one state variable**,
> replacing many.

### 10. Put the machine in `dispatch()`

> The AO framework calls `dispatch` **only when there is an event**; otherwise
> the AO is completely dormant.
>
> **That is exactly how a state machine operates** — with no events it waits in
> its current state doing **absolutely nothing** (shown **blue** in QM), and only
> an arriving event activates it to process a transition (shown **red** in QM).

Keep the old code for reference for now; delete it at the end.

### 11. Code the initial transition

In µC/AO the initial transition runs when `dispatch` receives **`INIT_SIG`**:

```c
if (e->sig == INIT_SIG) {
    BSP_ledGreenOff();
    TimeEvent_arm(&me->te, OFF_TIME, 0U);
    me->state = OFF_STATE;      /* the TARGET of the initial transition */
    return;                     /* explicitly -- you're done            */
}
```

### 12. Code the states — the nested `switch`

Outer `switch` on the **state variable**, one `case` per state, plus a
**`default:` with an assertion** — reaching it means the state variable has
become **invalid**.

Inner `switch` on the **event signal**, one `case` per handled event.

```c
switch (me->state) {
    case OFF_STATE: {
        switch (e->sig) {
            case TIMEOUT_SIG: {
                BSP_ledGreenOn();
                TimeEvent_arm(&me->te, ON_TIME, 0U);
                me->state = ON_STATE;         /* REGULAR transition */
                break;
            }
            case BUTTON_PRESSED_SIG: {
                /* actions copied from the old code                    */
                /* INTERNAL transition -- do NOT touch me->state       */
                break;
            }
            case BUTTON_RELEASED_SIG: {
                /* likewise -- no state change                         */
                break;
            }
        }
        break;
    }
    case ON_STATE: {
        /* TIMEOUT -> OFF_STATE, plus the SAME two internal transitions */
        break;
    }
    default: {
        Q_ERROR();
        break;
    }
}
```

### 13. Note the mapping rules

| Diagram | Code |
|---|---|
| initial transition | the `INIT_SIG` branch |
| state | outer `case` |
| trigger | inner `case` |
| actions | statements in the case |
| **regular** transition | **assign** the state variable |
| **internal** transition | **leave the state variable alone** |

Delete the old code.

---

## Part D — Reflect, then test

### 14. Notice what just happened

The state machine isn't necessarily *smaller* than the old code — mostly because
of the repeated button handling. But:

> The coding experience was **very different from any other code you've written**.
> Once you worked out a few simple mapping rules, **you applied them
> mechanically. The coding required little thinking, because all the thinking was
> done when the state machine was designed.**

That also means **a modeling tool could generate this automatically** — and QM
can (Lesson 41), though with a different implementation strategy.

Two properties you now have:

| Property | Meaning |
|---|---|
| **Traceability** | every part of the code maps to a part of the diagram — **required in safety-critical software** |
| **Extensibility** | to add a state or event you know **exactly** what to change |

### 15. Build and run

The compiler reminds you to remove `isLedOn` from the constructor; otherwise the
state machine builds with **zero errors and zero warnings**.

Load it onto the board: **BlinkyButton works exactly as before.**

---

## Exercises

1. **Add a state.** Add a "fast blink" state entered after a double press. Which
   parts of the code did you touch?
2. **Break it deliberately.** Assign the state variable inside an internal
   transition and describe the resulting misbehaviour.
3. **Invalid state.** Force the state variable to a bogus value and confirm the
   `default:` assertion catches it.
4. **Count the repetition.** How many lines are duplicated between `off` and
   `on`? Extrapolate to a machine with 8 states.
5. **Fix the calculator.** Sketch (on paper) a state machine for the four-function
   calculator, and identify the state that the `3 + - = 4 =` sequence broke.
6. **Traceability check.** Pick three random lines of your `dispatch` and point to
   the exact diagram element each came from.
7. **Stack comparison.** Measure stack usage of the state-machine version versus
   the original blocking µC/OS-II threads.

---

## Self-check

- [ ] I reproduced the calculator crash and can explain its root cause
- [ ] I can define "state" as an equivalence class of histories
- [ ] My diagram has an initial transition, two states, and internal transitions
- [ ] The `isLedOn` flag is gone, replaced by one state variable
- [ ] The state machine lives in `dispatch()` and blocks nowhere
- [ ] Regular transitions assign the state variable; internal ones don't
- [ ] There is a `default:` assertion for invalid states
- [ ] I can point to the duplicated transitions that motivate hierarchy
