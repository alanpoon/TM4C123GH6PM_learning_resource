# Lesson 37 — Practice: Buffering Inputs in a Polled State Machine

**Concepts behind this exercise:** [KNOWLEDGE.md](KNOWLEDGE.md)
**Video:** <https://youtu.be/E2Im7jLDDG4>

---

## Projects in this lesson

| Directory | Contents |
|---|---|
| `tm4c123-keil/` | KEIL — the input-driven BlinkyButton in a `while(1)` superloop |
| `CMSIS/`, `ek-tm4c123gxl/` | support code |

> **Start from a copy of Lesson 21**, not Lesson 36 — this lesson goes back to
> the **foreground/background** superloop, because that is where input-driven
> state machines live.

---

## Part A — Recognize what you already built

### 1. Find the state machine you wrote in Lesson 21

At the end of Lesson 21 you wrote a **non-blocking** blinky directly inside the
`while(1)` superloop. **That was an input-driven state machine** — you just
didn't have the name for it.

Look at its shape:

- a **`switch` on the state variable**, one case per state;
- inside each case, characteristic **`if` statements** — these are the **guard
  conditions** typical of an input-driven machine (no second-level switch on an
  event signal, because there are no events).

### 2. Reverse-engineer the diagram

Draw it. Note that **all actions are associated with transitions** — so this is a
**Mealy-type** machine (Moore machines put actions in states).

### 3. Identify the input — and the hazard

There is **one** input: `BSP_tickCtr()`.

That function reads **`l_tickCtr`**, which is **constantly updated in
`SysTick_Handler()`**. So the input **changes asynchronously** to the background
loop.

Worse: **the tick-counter input is accessed twice in each state**, so **it could
be different at each access point.**

> Tolerable in this toy example — **but buffering would make it more robust for
> any future modification.** And note: **the asynchronous nature does not change**
> whether you access the variable directly or through `BSP_tickCtr()`.

---

## Part B — Buffer the input

### 4. Read it once per pass

```c
while (1) {
    uint32_t now = BSP_tickCtr();   /* buffer the input ONCE per RTC step */
    switch (state) {
        ...  /* use 'now' everywhere, never BSP_tickCtr() again */
    }
}
```

Replace **all** downstream references to `BSP_tickCtr()` with `now`.

> **Buffering means copying external inputs into local variables, which are then
> guaranteed not to change over the run-to-completion step.**

### 5. Verify

Build and load: **the green LED still blinks happily.**

### 6. Understand why this matters — the landing-gear bug

From the article *"Embedded State Machine Implementation"*: a state machine
controlling an aircraft's **landing gear**, called from a `while(1)` superloop.
Its guard detects a rising edge by comparing `gear_lever` with
`prev_gear_lever`.

> If `gear_lever` changes asynchronously — in an interrupt, or read directly from
> a GPIO — it can be **DOWN when the guard is evaluated but UP when stored** into
> `prev_gear_lever`. **The rising edge is missed, the guard never becomes true,
> and this aircraft will never take off.**

**Buffering is the fix.**

---

## Part C — Add a second input

Goal: **SW1 pressed → blue LED on; released → blue LED off.**

### 7. Add the BSP access function

```c
uint32_t BSP_SW1(void) {      /* read the GPIO register directly */
    return (GPIOF->DATA_Bits[BTN_SW1] != 0U) ? 1U : 0U;
}
```

Don't forget to **define the SW1 button bit** and to configure the pin in
`BSP_init()`: **GPIO input**, **digital function enabled**, **pull-up resistor
enabled**.

### 8. Buffer it, and remember the previous value

```c
uint32_t button = BSP_SW1();              /* buffer the input */
static uint32_t prev_button = 1U;         /* 1 = SW1 inactive (pull-up) */
```

`prev_button` must be **`static`** — it has to survive each pass through the loop
(compare Lesson 33's static counters).

### 9. Detect both edges with guards

```c
if ((prev_button != 0U) && (button == 0U)) {      /* FALLING edge = pressed  */
    BSP_ledBlueOn();
    prev_button = button;
    /* internal transition -- do NOT change the state variable */
}
else if ((prev_button == 0U) && (button != 0U)) { /* RISING edge = released  */
    BSP_ledBlueOff();
    prev_button = button;
    /* again, no state change */
}
```

**Copy the same reactions into the other (ON) state case** — the familiar
duplication (Lesson 35).

Clean up any unused code and build.

---

## Part D — Measure the limits

### 10. Basic check

Load it: the green LED blinks as before, and **pressing SW1 lights the blue
LED**.

### 11. Look with a logic analyser

Traces, top to bottom: **SW1** (the button input), then **red**, **green**,
**blue** LEDs. Trigger on the **falling edge of SW1**.

Before pressing, only the green LED toggles, as expected. Press and release —
the trace triggers: **blue on at press, off at release.** Exactly as designed.

### 12. Find the bounce

Look more closely: **the SW1 trace bounced once** before settling into the
pressed state — the mechanical contact bouncing you met in Lesson 27.

In this capture the state machine **keeps up** with the noisy signal and toggles
the blue LED accordingly. But capture more traces and **sometimes it misses a
bounce or two**, e.g. on release.

> That might or might not be acceptable in your application — **but the problem
> is that you have no control.**
>
> **Sampling of the inputs is coupled to the execution speed of the state
> machine**, which varies widely depending on what the machine happens to be
> doing.

### 13. Know the remedy

Where you must keep up with inputs changing faster than your **worst-case
sampling rate**, you need to **decouple polling from state machine execution**.

That decoupling also gives you the chance to **pre-process raw inputs** — digital
filtering, **debouncing** the SW1 signal — and then to add **buffering or
queuing** so inputs are not lost.

> **But this leads more and more towards event-driven state machines.**

---

## Part E — Convert it to event-driven

### 14. Make an event out of the buffered inputs

> **Every input-driven state machine can quite easily be converted into an
> event-driven one.**

Wrap the buffered inputs in a structure and name an instance `evt`:

```c
struct { uint32_t now; uint32_t button; } evt;
evt.now    = BSP_tickCtr();
evt.button = BSP_SW1();
```

**You have just created an event, where the inputs became the event parameters.**

> Grouping them has an extra benefit: it **packages together inputs that
> represent a consistent snapshot.**

### 15. Consider the signal

What signal does this event carry? It depends on **when and how the machine
runs**. Running "as often as possible" in the superloop, call it a generic
**`SAMPLE`** event.

> This is **a low-quality event**, with quite an erratic repetition rate.

### 16. Access via an event pointer

Add a pointer to the instance and route all inputs through it:

```c
Event const *e = &evt;
...  e->now ...  e->button ...
```

> The code now **starts to resemble the `dispatch()` function of an event-driven
> state machine** from the previous lessons.

### 17. Test — and note what did NOT improve

Build and run: green blinks, SW1 toggles blue. But in the logic analyser you
**still catch bouncing and unreliable responses.**

> Of course — **single buffering of inputs, now inside an event, does not change
> the execution rate of the state machine.**

### 18. The most important observation of the lesson

> **The reliability and robustness of a state machine depends not just on its
> type — input-driven versus event-driven — but also on the environment in which
> it runs.**
>
> That is exactly why state machines were introduced in the context of
> **event-driven active objects with queuing of events** in Lesson 35.
>
> **State machine applications form a whole spectrum:** from input-driven
> machines in a `while(1)` superloop with no buffering, through increasingly
> event-driven machines with generic events and guards, to fully event-driven
> machines inside **active objects with full event queuing**.

### 19. Know when input-driven is the right answer

> **Input-driven state machines are not all bad** — they bring real advantages
> where external event discovery and queuing is difficult or impractical:

- **Computer games** — code runs on a generic **FRAME** event 30–60 times per
  second, and **discovering interesting events (e.g. enemy proximity) is very
  much part of the game itself**, impractical to do externally.
- **Robotics** — software runs at a frame rate, and which sensor inputs matter
  **depends on what the robot is doing**.
- **Discovering events for event-driven machines** — e.g. **switch debouncing**,
  which is itself a special input-driven **Moore-type** machine running
  periodically from the SysTick hook (the course's version runs **32 of them at
  once**, debouncing 32 switches — see Lesson 38).

---

## Exercises

1. **Provoke the race.** Read `BSP_tickCtr()` twice in one pass and construct a
   case where the two values differ enough to misbehave.
2. **Count the misses.** Over 50 button presses, how often does the machine miss a
   bounce? Correlate with what the machine was doing.
3. **Debounce it.** Add a simple periodic debouncing state machine in the SysTick
   hook and re-measure.
4. **Mealy → Moore.** Redraw your diagram with actions attached to states. Which
   reads better?
5. **Edge case.** What happens if the button changes state twice between two
   passes of the loop? Can the machine ever detect it?
6. **Measure the "period".** Instrument the loop and record minimum and maximum
   pass times. How many orders of magnitude apart are they?
7. **Cross-check.** Take the Lesson 36 TimeBomb and sketch it as an input-driven
   machine. What did you have to add?

---

## Self-check

- [ ] I recognized the Lesson 21 superloop blinky as an input-driven state machine
- [ ] I identified its single asynchronous input and where it comes from
- [ ] I buffered inputs into locals read once per run-to-completion step
- [ ] I can explain the landing-gear bug in one sentence
- [ ] My SW1 handling uses buffered input plus a `static` previous value
- [ ] I observed switch bounce and missed transitions on a logic analyser
- [ ] I converted the buffered inputs into an event with parameters
- [ ] I can state why that conversion alone did not improve reliability
- [ ] I can name three situations where input-driven machines are the right choice
