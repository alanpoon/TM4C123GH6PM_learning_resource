# Lesson 36 — Practice: Building the Time Bomb

**Concepts behind this exercise:** [KNOWLEDGE.md](KNOWLEDGE.md)
**Video:** <https://youtu.be/RGQRllvrMyY>

---

## Projects in this lesson

| Directory | Contents |
|---|---|
| `tm4c123-uc_ao-keil/` | KEIL + µC/AO — the TimeBomb active object |
| `ucos2/`, `CMSIS/`, `ek-tm4c123gxl/` | kernel and support code |

Start from a copy of the Lesson 35 project, and open **`BlinkyButton.qm`** with
the freeware **QM** modeling tool (download from state-machine.com as part of the
QP-bundle).

## The specification

> After reset, the controller lights the **green** LED and waits for **SW1**.
> When you press the button, green goes off and timing starts: the **red** LED
> blinks on and off **3 times**, after which the bomb explodes — emulated by
> turning **all three LED colors on**.
>
> In short: green light; then **blink-pause, blink-pause, blink-pause, boom!**

---

## Part A — Understand the limitation you're about to hit

### 1. Recall why states already help

> A state machine is **the most optimal solution to the very common problem where
> a system must react to events based not only on the event type but also on the
> history of past events.**

In BlinkyButton the **same** `TIMEOUT` event does different things:

| State | TIMEOUT |
|---|---|
| `off` | LED **on**, go to `on` — **the transition is how the change is remembered** |
| `on` | LED **off**, go to `off` |

The reaction depends on **both** the event signal **and** the current state.

### 2. See where it isn't enough

> The whole structure of states and transitions is **fixed at design time**.
>
> **But what if the behavior depends on user input, not known until run time?**
> Then for some transitions **you cannot know at design time which state to go
> to.**

That is what **choice pseudostates** and **guard conditions** are for.

---

## Part B — Design the TimeBomb

### 3. Start from the previous model

Use the BlinkyButton model as the starting point. Save it as **`TimeBomb.qm`**
and rename the active object to **`TimeBomb`**.

### 4. The initial transition and `wait4button`

Initial transition: **green LED on** → a state where the bomb waits for the
button. Name it **`wait4button`**.

Inside `wait4button` the only event of interest is **`BUTTON_PRESSED`** — delete
the others. Its actions:

1. green LED **off**
2. red LED **on**
3. **arm the time event for ½ second**
4. transition to **`blink`** (press is followed by blink)

### 5. The `blink` state

Keep only **`TIMEOUT`** — the event you just armed. Its actions: red LED
**off**, **arm ½ second**, transition to **`pause`** (blink is followed by
pause).

### 6. The `pause` state — and the crux

Handle the just-armed **`TIMEOUT`** to get ready for another blink. **But where
should this transition go?**

> You **cannot naively go back to `blink`** — that would create an **endless
> loop** of blink and pause.

**The brute-force answer** — three separate groups of blink/pause states — is
**brain-dead**:

> not only because so many states **mindlessly repeat essentially the same
> behavior**, but because **the number of blinks is likely to change.** The next
> feature could easily be making it **user-adjustable** — so **you cannot know it
> at design time.**

### 7. The smart answer: a counter plus a choice pseudostate

Add a **`blink_ctr`** to the state machine. The `TIMEOUT` action in `pause` only
**decrements** it. Then add a **choice pseudostate** — a **diamond** — with two
guarded segments:

| Guard | Actions | Target |
|---|---|---|
| **`[blink_ctr > 0]`** | red LED on, arm ½ s | **`blink`** |
| **`[else]`** | **all LEDs on** (the explosion) | **`boom`** |

> **Guards are boolean expressions evaluated at run time.** If a guard is true
> that segment is taken; otherwise the transition is **disabled** — not taken.
> **`[else]`** is true only when **all** other guards from the same diamond are
> false.

**Don't forget the actions on each segment** — each must prepare for the state it
enters.

Add the **`boom`** state and point the else-segment at it.

### 8. Initialize the counter

Put it on a transition **upstream** of `blink` — **`wait4button`** is the natural
place, alongside the other `BUTTON_PRESSED` actions.

---

## Part C — Code it

Still hand-coded with the **nested-`switch`** technique from Lesson 35 (QM
*could* generate it, but the better strategies come in later lessons).

### 9. Rename and re-enumerate

Globally rename `BlinkyButton` → `TimeBomb` throughout the project (then fix the
few manual corrections). **Since it's only a name change, the project should
still build cleanly** — check that before going further.

Enumerate the new states:

```c
enum { WAIT4BUTTON, BLINK, PAUSE, BOOM } state;
```

Review the private data: **keep the time event**; **replace `blink_time` with
`blink_ctr`**; delete the now-unused initial blink-time constant.

### 10. The initial transition

```c
if (e->sig == INIT_SIG) {
    BSP_ledGreenOn();
    me->state = WAIT4BUTTON;
    return;
}
```

### 11. `wait4button`

Handle only `BUTTON_PRESSED` — delete the rest:

```c
case WAIT4BUTTON: {
    switch (e->sig) {
        case BUTTON_PRESSED_SIG: {
            BSP_ledGreenOff();
            BSP_ledRedOn();
            TimeEvent_arm(&me->te, HALF_SEC, 0U);
            me->blink_ctr = 3U;
            me->state = BLINK;
            break;
        }
    }
    break;
}
```

### 12. `blink`

Handle only `TIMEOUT` — delete everything else:

```c
case BLINK: {
    switch (e->sig) {
        case TIMEOUT_SIG: {
            BSP_ledRedOff();
            TimeEvent_arm(&me->te, HALF_SEC, 0U);
            me->state = PAUSE;
            break;
        }
    }
    break;
}
```

### 13. `pause` — the choice point in code

`pause` is similar to `blink`, so copy it as a starting point, then add the
decision:

```c
case PAUSE: {
    switch (e->sig) {
        case TIMEOUT_SIG: {
            --me->blink_ctr;                  /* the first transition segment */

            if (me->blink_ctr > 0U) {         /* [blink_ctr > 0] */
                BSP_ledRedOn();
                TimeEvent_arm(&me->te, HALF_SEC, 0U);
                me->state = BLINK;
            }
            else {                            /* [else] */
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
```

> **The first branch is simply the IF that checks the guard condition; the
> complementary `[else]` guard is simply the ELSE branch.**

### 14. `boom`

It handles no events, so it is an **empty case**:

```c
case BOOM: {
    break;
}
```

### 15. Note the mapping

| Diagram | Code |
|---|---|
| segment before the diamond | statements before the `if` |
| guard condition | the `if` test |
| `[else]` | the `else` branch |
| actions on a segment | statements in that branch |
| target state | assignment to the state variable |
| state handling no events | empty `case` |

---

## Part D — Test, then change the requirement

### 16. Run it

Upload to the board and press **reset**: **the green LED lights up**, as
expected.

Press **SW1**: … three, two, one, **boom!** — all LEDs on.

### 17. Prove the design was worth it

Change the blink counter to **10**, rebuild, load, and press reset: green light.
Press the button: **ten, nine, eight, … one, boom!**

**One constant changed — no new states.** That is exactly what the brute-force
design could not have given you.

---

## Part E — The warning

### 18. Recognize what you just introduced

> **Guards are exactly the same IFs and ELSEs that the state machine was designed
> to eliminate in the first place** — recall the Visual Basic calculator
> spaghetti from Lesson 35.
>
> **Use guard conditions very judiciously.** Too many and **you'll find yourself
> back at square one**, with the guards effectively taking over all the relevant
> conditions in the system.
>
> On the other hand, **guards are sometimes unavoidable** — they add necessary
> flexibility.

> **One of the main challenges in becoming an effective state machine designer is
> developing a sense for which parts of the behavior belong in states and
> transitions at design time, and which need the run-time flexibility of guards.**
>
> **Challenge yourself to use as few guard conditions as possible.**

---

## Exercises

1. **User-adjustable count.** Make the number of blinks increase by one each time
   the button is pressed in `wait4button`. Which part of the design changed?
2. **Add a defuse.** Handle `BUTTON_PRESSED` during `blink`/`pause` to abort and
   return to `wait4button`. How many places did you touch, and what does that
   suggest? (Lesson 40.)
3. **Forget the else.** Delete the `else` branch and describe what the bomb does.
4. **Forget an action.** Remove the time-event arming from the `[blink_ctr > 0]`
   segment and explain the resulting behaviour.
5. **Count the states.** How many states would the brute-force design need for 10
   blinks? For a user-selectable 1–20?
6. **Guard audit.** Re-examine your machine and try to replace one guard with an
   extra state. Which version reads better?
7. **Draw it from the code.** Hand the `dispatch()` function to someone else and
   have them reconstruct the diagram — a traceability test.

---

## Self-check

- [ ] I can explain why `pause` cannot simply transition back to `blink`
- [ ] My diagram uses a diamond with a guarded segment and an `[else]` segment
- [ ] Each transition segment carries the actions that prepare its target state
- [ ] `blink_ctr` is initialized upstream, in `wait4button`
- [ ] The guards compiled into a plain `if`/`else` in the `PAUSE` case
- [ ] `boom` is an empty case that handles no events
- [ ] Changing the count from 3 to 10 required editing exactly one constant
- [ ] I can state why guards must be used sparingly
