# Lesson 35 — Knowledge: State Machines Part 1 — What Is a State Machine?

**Video:** <https://youtu.be/EBSxZKjgBXI> · **Transcript:** <https://www.state-machine.com/course/lesson-35.txt>
**Hands-on companion:** [PRACTICE.md](PRACTICE.md)

---

## The big idea

Event-driven code cannot block, so it **loses the stack context between events**
and needs some other way to remember where it is. The ad-hoc answer — flags and
variables — degenerates into spaghetti. **The state machine is the optimal
answer.**

> **State machines are an ideal mechanism to specify the behavior of Active
> Objects**, because both assume **run-to-completion** semantics.

---

## 1. Two ways to manage context between events

### Automatic (sequential/blocking)

In an RTOS thread the waiting happens by **blocking** inside `OSTimeDly()`. After
unblocking, the code **knows exactly which event it received**, so it needs
nothing special to remember where in the sequence it was.

> This is **automatic context management** — convenient, **if you can live with
> hard-coded event sequences**. But it is **expensive**: the context of a blocked
> thread requires **a whole private stack in precious RAM**.

### Manual (event-driven)

Event-driven code **cannot block** and must **return to the event loop after
every event**, so it **loses the stack context**.

> Event-driven systems therefore **use less stack space**, but require **a
> different form of preserving context from one event to the next**.

The popular approach is **manual context management** — programmers invent flags
and variables, such as the `isLedOn` flag of Lesson 34.

## 2. Why manual context management fails

> In most real-life programs this approach is **almost guaranteed to degenerate
> into unwieldy "spaghetti" code.**

The worked example is a **Visual Basic 4-operation calculator**. The sequence
`3`, `+`, `-`, `=`, `4`, `=` **crashes** it with a run-time error.

> One might argue that is a meaningless sequence of events — but it is also
> obvious that **no user actions should crash the software like that.**

Looking at its code: **managing the context is the central concern, and most of
the code is devoted to it** — `DecimalFlag`, `NumOps`, and more. Event handlers
check these flags in nontrivial conditions, modify them, and re-check them, so
**it is really hard to know which path will be taken** in a given situation.

> A 4-operation calculator is **already complex enough to reach the limit** of
> manual context management: **attempts to fix one bug tend to create another.**
>
> **The real problem is that the variables and flags provide too much redundant
> and poorly organized information, which can easily become internally
> inconsistent.**

## 3. Relevant history and the concept of "state"

The improvised approach reflects a **correct intuition**: you don't need to
remember the *whole* history of past events — only certain **aspects** of it.

> To handle events correctly you only need the **relevant history** — what
> influences the **future** behavior of the system. **Events don't contribute
> equally to that relevant history.**

**Example — a keyboard controller.** It generates lower-case or upper-case codes
depending on whether **Shift** has been pressed or released. It **does not
matter** which other keys were pressed, or how many. So the whole relevant
history reduces to **two classes**: "normal" and "shifted".

### The definition

> **"State" of a system is an *equivalence class* of past histories of a system,
> all of which are equivalent in the sense that the future behavior of the system
> given any of these past histories will be identical.**

> The concept of state is **the most efficient representation of the relevant
> history** — the **minimum information** that captures only the aspects relevant
> to future behavior, abstracting away all irrelevant ones.

### The state machine

> A **state machine** is the set of all states — equivalence classes of relevant
> histories — **plus the rules for changing from one state to another**. Those
> rules are called **transitions**, and they capture the fact that **some events
> contribute to the relevant history while others do not.** Events that influence
> future behavior trigger the state transitions.

## 4. State diagrams (UML notation)

> A very nice aspect of the state-machine concept is its **compelling graphical
> representation**.

| Element | Notation |
|---|---|
| **State** | rounded-corner rectangle with the name in the name compartment |
| **Regular transition** | an **arrow** labelled with the triggering event |
| **Internal transition** | the triggering event written **inside** the state — **executes actions but causes no change of state** |
| **Actions** | listed **after a slash**, allowed on **both** kinds of transitions |
| **Initial transition** | required in every state machine; points to the **default state**, active when the machine is created and **before any events are dispatched** |

## 5. Run-to-completion — the universal feature

> One universal feature of **all** state machine formalisms is **Run-to-Completion
> (RTC) event processing**: a state machine can process **only one event at a
> time**, so the processing of one event **must end before the processing of the
> next can begin.**

> **Surprise: the RTC semantics universally assumed in all state machine
> formalisms matches exactly the RTC semantics assumed in all event-driven
> systems**, such as active objects. **In other words, state machines are an ideal
> mechanism to specify the behavior of active objects.**

## 6. Implementation: the nested-`switch` technique

> The main job of a state machine is to **remember its current state**. That needs
> a variable, commonly called the **state variable**.

The most straightforward representation is an **enumeration of all possible
states**:

```c
enum { OFF_STATE, ON_STATE } state;    /* private data of the active object */
```

> **The state variable replaces all the flags and variables of the improvised,
> manual context management.** In the toy BlinkyButton that just means dropping
> `isLedOn` — which may not look like much. But remember that in a more advanced
> state machine such as the calculator **you still have only one state
> variable**, replacing many.

### Where the code goes

> The ideal place is the **active object's `dispatch` function** — because the
> framework calls `dispatch` **only when there is an event to process**;
> otherwise the AO is completely dormant.
>
> **That is exactly how a state machine operates:** with no events it just waits
> in its current state doing **absolutely nothing**; only when an event arrives
> is it activated to process it in one of the transitions.

### The structure

```c
if (e->sig == INIT_SIG) {
    /* actions of the INITIAL transition */
    me->state = OFF_STATE;                /* the target of the initial transition */
    return;
}

switch (me->state) {                       /* level 1: discriminate on STATE      */
    case OFF_STATE: {
        switch (e->sig) {                  /* level 2: discriminate on SIGNAL     */
            case TIMEOUT_SIG: {
                /* actions */
                me->state = ON_STATE;      /* REGULAR transition: change state    */
                break;
            }
            case BUTTON_PRESSED_SIG: {
                /* actions -- INTERNAL transition: state variable UNCHANGED       */
                break;
            }
            ...
        }
        break;
    }
    case ON_STATE: { ... }
    default: {
        Q_ERROR();   /* an invalid state variable */
    }
}
```

Mapping rules, applied mechanically:

| Diagram element | Code |
|---|---|
| initial transition | the `INIT_SIG` branch |
| state | a `case` of the outer `switch` |
| transition trigger | a `case` of the inner `switch` |
| actions | statements inside that case |
| **regular** transition | **assign** the state variable |
| **internal** transition | **do not touch** the state variable |
| (safety) | `default:` with an assertion |

## 7. What you gain

> The whole coding experience is **very different from any other code you've
> written**. Once you work out a few simple mapping rules, **you apply them
> mechanically. The actual coding does not require much thinking, because all the
> thinking was already done when the state machine was designed.**

That also means **a modeling tool could generate such code automatically** by
applying the same rules — which is exactly what **QM** does (Lesson 41).

| Property | Meaning |
|---|---|
| **Traceability** | every part of the code corresponds to a part of the diagram — **a required property in safety-critical software** |
| **Extensibility** | to add states or events you know **exactly which parts of the code to change** |

## 8. The limitation to come

Handling the button events required **repeating the same internal transitions in
both states**.

> Such repetition is perhaps no big deal in a toy example, but **in real-life
> projects repetitions like that can, and will, pile up. This is exactly the
> problem that modern hierarchical state machines are designed to address.**
> (Lesson 40.)

---

## Key takeaways

1. Blocking code manages context **automatically** — at the cost of a stack per
   thread and hard-coded event sequences.
2. Event-driven code must manage context **explicitly**.
3. Ad-hoc flags produce redundant, inconsistent state → spaghetti.
4. **A state is an equivalence class of histories** with identical future
   behavior.
5. **A state machine = states + transitions.**
6. UML: rounded rectangles, arrows, internal transitions, actions after `/`, and
   a mandatory initial transition.
7. **All state machine formalisms assume run-to-completion** — matching
   event-driven systems exactly.
8. One **state variable** replaces many ad-hoc flags.
9. Put the state machine in the AO's **`dispatch`** function.
10. The nested-`switch` mapping is **mechanical**, giving **traceability** and
    **extensibility**.

---

## Glossary

| Term | Meaning |
|---|---|
| Relevant history | Only what influences future behavior |
| State | An equivalence class of past histories |
| Transition | A rule for changing state, triggered by an event |
| Internal transition | Executes actions without changing state |
| Action | Code executed on a transition, written after `/` |
| Initial transition | Points to the default state |
| Default state | Active before any event is dispatched |
| State variable | Variable holding the current state |
| Run-to-completion | One event fully processed before the next begins |
| Traceability | Correspondence between design and code |

---

## Pitfalls to remember

- **Inventing flags** instead of identifying states.
- **Assigning the state variable on an internal transition.**
- **Omitting the `default:` assertion** — an invalid state goes unnoticed.
- **Forgetting the initial transition** — the machine starts in no state.
- **Blocking inside a transition** — it violates RTC (Lesson 33).
- **Accepting duplicated transitions** across states — a signal that you need
  hierarchy (Lesson 40).

---

**Next:** Lesson 36 adds **guard conditions**, which give state machines run-time
flexibility.
