# Lesson 37 — Knowledge: State Machines Part 3 — Input-Driven State Machines

**Video:** <https://youtu.be/E2Im7jLDDG4> · **Transcript:** <https://www.state-machine.com/course/lesson-37.txt>
**Hands-on companion:** [PRACTICE.md](PRACTICE.md)

---

## The big idea

Most state machines you'll find online are **not** event-driven. They are
**input-driven** (polled) machines descended directly from **1950s hardware
design** — and that ancestry explains almost everything about them, including
their weaknesses.

> **The reliability and robustness of a state machine depends not just on its
> type — input-driven versus event-driven — but also on the environment in which
> the state machine runs.**

---

## 1. Hardware origins

Interest in state machines began almost **seventy years ago**, when in the
**1950s George Moore and Edward Mealy** of Bell Telephone published seminal
papers on formally synthesizing digital circuits.

> **State machines originated in hardware design** — software was in its infancy.
> This has **deeply influenced** them, so that **even today, state machines used
> in software carry a lot of baggage from their hardware origins.**

### Combinational vs. sequential circuits

**Combinational** circuits (AND, OR, XOR gates and combinations) have **no
internal state**: their outputs depend **only and entirely on the inputs**, and
are fully described by a **truth table**.

But consider **detecting a rising edge** of input A — output Y goes high only
when A changes 0→1.

> **This cannot be solved with purely combinational logic**, because the circuit
> must **remember the previous state of A**.

The solution adds a **D flip-flop** — a memory element holding one bit: **the
state of the circuit** — plus a periodic **clock**, which controls when the
flip-flop stores new information.

> The clock **provides a global "event"** to the system and determines when
> inputs and outputs are allowed to change.

### Synchronous vs. asynchronous circuits

Clocked circuits are **synchronous**, and both Mealy and Moore focused on those.

> **Synchronous means all signals may change only while the clock is high and
> must remain stable when it is low.** That ties everything to the clock and is
> quite restrictive.

Circuits without a clock are **asynchronous** — the original Mealy paper
discusses them, and names their biggest problem: **race conditions**, apparently
**well known already in the 1950s**. In hardware they arise from **propagation
delays**, so outputs depend on relative timing.

> **The problem of race conditions in hardware turns out to be so intractable
> that to this day virtually all digital electronics, including all embedded
> CPUs, are synchronous.**

## 2. Mealy vs. Moore

An important insight of the original papers was to **abstract the states by
naming them** (the association between names and flip-flop values is **binary
encoding**) and then depict the truth-table information as a **state diagram**.

| | **Mealy** | **Moore** |
|---|---|---|
| Circuit | a **direct connection from the input** to the output logic | **no** such connection — **only the current state** determines the output |
| Diagram | transitions labelled with **inputs and outputs** | transitions labelled with **inputs only**; **outputs written inside the states** |
| States needed | fewer | **typically more** (the edge detector needs 2 flip-flops instead of 1) |
| Speed | output rises **a clock cycle sooner** — it responds directly to the input | one cycle slower |
| Risk | that faster reaction **can occasionally lead to race conditions**, because the state may change in the same clock cycle as the inputs, and the two can race | safer |

> **For these reasons Moore state machines are often preferred**, even though
> they are a bit slower and a bit more complex.

### The generic structure

A **Current State register** of D flip-flops driven by a common clock; a
combinational block computing the **Next State** from the current state **and**
the inputs; the next state fed back to become the current state on the **next**
clock cycle; and a second combinational block producing the **Outputs** from the
current state — **plus the inputs, but only in a Mealy machine.**

## 3. Input-driven state machines in software

Search the web for "software state machine" and you'll find diagrams that look
very different from the event-driven ones of Lessons 35–36 — and not merely
cosmetically (circles vs. rounded rectangles).

> **The real difference is in what drives the transitions, because these are
> apparently NOT events.** Instead, transitions are labelled with **inputs** and
> with **expressions built from those inputs** — logical OR, AND, comparisons,
> `TRUE`, "always".

**Where do those "inputs" come from?** Straight from hardware state machines,
where inputs were **bits or groups of bits**. In software, groups of bits that
can change are **variables** — e.g. a boolean `pilot's lever` (UP/DOWN), or a
16-bit `msCounts`.

> And those boolean expressions of inputs are **simply guard conditions**
> (Lesson 36). More advanced tools such as **Stateflow** (MathWorks) label
> transitions with explicit guards — note the characteristic square brackets.

### When do they run?

| | Event-driven | Input-driven |
|---|---|---|
| Runs | **only when there is a new event** | **"always" or "periodically"** — and typically **you cannot tell from the diagram how often** |
| Idle behavior | **waits, doing nothing** | **takes all available CPU cycles**, whether or not anything interesting happened |

Sometimes context reveals it — a Simulink model driven by a "Clock" implies
periodic execution. But many run "always", **executed from the `while(1)`
superloop directly in `main()`** — exactly the non-blocking blinky of Lesson 21.

### The names

The terminology **is not firmly established**:

| Name | Emphasizes |
|---|---|
| **Input-driven** | transitions driven by inputs, not events |
| **Controller** | common in process control |
| **Periodic** | they often run periodically — **but misleading**: in a superloop, execution is very irregular. With all guards false a pass may take **microseconds**; with a guard true it may take **many milliseconds** — several orders of magnitude, so there is no well-defined "period" |
| **Polled** | they **constantly poll the inputs to discover the really interesting events** |

> Polled machines **combine event discovery (by polling inputs) with the state
> machine logic**. In that sense, **the inputs are "proto-events"**: variables
> that must be polled and combined in guard expressions to distill the real,
> interesting events.

## 4. The core hazard: asynchronous inputs

> The inputs are typically **external** to the state machine, originating
> directly in **peripheral registers** or in **interrupts** — e.g. Arduino's
> `digitalRead()` (a GPIO register) and `millis()` (updated in the clock-tick
> ISR).
>
> **The external inputs are variables SHARED between concurrent entities** —
> peripherals and interrupts. And **anything that changes asynchronously with
> respect to your code can lead to race conditions and data corruption.**

> **Analogy** (not a precise equivalence — don't take it too far): input-driven
> state machines are like **asynchronous** circuits; event-driven ones like
> **synchronous** circuits. **External inputs can change at any time, including
> during state machine processing. In contrast, events in an event-driven machine
> are queued and guaranteed not to change during the whole run-to-completion
> step.**

### A worked failure — landing gear

From the article *"Embedded State Machine Implementation"*: a state machine
controlling an **aircraft's landing gear**, called directly from the `while(1)`
superloop. The guard in state `GearDown` detects a rising edge of `gear_lever` by
comparing it against `prev_gear_lever`.

> If `gear_lever` changes asynchronously — in an interrupt, or read straight from
> a GPIO register — it can be **DOWN when the guard is evaluated but UP when the
> value is stored** into `prev_gear_lever`. **The rising edge is missed, the
> guard never becomes true, and this aircraft will never take off.**
>
> It **makes no difference** whether the input is accessed directly or through a
> function like `digitalRead()`.

### The fix: buffer the inputs

> **Copy the values of external inputs into local variables**, which are then
> **guaranteed not to change over the run-to-completion step** of the state
> machine.

## 5. From input-driven to event-driven

> **Every input-driven state machine can quite easily be converted into an
> event-driven one.**

Look at the buffered inputs: **wrap them in a structure**, name an instance
`evt`, and **you have created an event, where the inputs become the event
parameters.**

> Grouping the inputs has an additional benefit: it **packages together inputs
> that represent a consistent snapshot.**

**What signal does such an event carry?** It depends on when and how the machine
runs. Running "as often as possible" in a superloop, you might call it a generic
**`SAMPLE`** event.

> This is **a low-quality event**, with quite an erratic repetition rate.

Add a pointer `e` to that instance and access all inputs as `e->...` — and **the
code starts to resemble the `dispatch()` function of an event-driven state
machine.**

But note what this does *not* fix:

> **Single buffering of inputs, now inside an event, does not change the
> execution rate of the state machine.** Button bounces are still missed.

> **State machine applications form a whole spectrum** — from input-driven
> machines in a `while(1)` superloop with no buffering, through increasingly
> event-driven machines with generic events and guards, to fully event-driven
> machines running inside **active objects with full event queuing**. (That is
> why Lesson 35 introduced state machines in the active-object context.)

### The sampling-rate problem

> Because **sampling of the inputs is coupled to the execution speed of the state
> machine** — which varies widely with what the machine is doing — **you have no
> control** over whether fast input changes (like switch bounces) are caught.

The remedy is to **decouple polling from state machine execution**, which also
creates an opportunity to **pre-process raw inputs** (e.g. **debouncing**), and
then to add **buffering or queuing** so inputs are not lost.

> **But this leads more and more towards event-driven state machines.**

## 6. Where input-driven machines are the right choice

> **This does not mean input-driven state machines are all bad. On the contrary,
> they bring a number of advantages where external discovery and queuing of
> events is difficult or impractical.**

| Domain | Why |
|---|---|
| **Computer games** | code runs periodically on a generic **FRAME** event, 30–60 times per second. **Discovering interesting events is very much part of the game itself** (e.g. proximity of an enemy) and impractical to do externally |
| **Robotics** | software runs at a frame rate, and **which sensor inputs are interesting depends on what the robot is doing** and what is going on around it |
| **Event discovery for event-driven machines** | e.g. **switch debouncing** — which is itself a special input-driven, **Moore-type** state machine running periodically from the SysTick hook (and in the course's implementation, **32 such machines at once**, debouncing 32 switches) |

---

## Key takeaways

1. State machines come from **1950s hardware design**, and software inherited the
   baggage.
2. **Mealy**: outputs depend on state **and inputs**; **Moore**: on **state
   only** — slower but safer, and generally preferred.
3. Race conditions in asynchronous circuits are why **virtually all digital
   hardware is synchronous**.
4. **Input-driven** machines are driven by **variables**, tested with **guards**.
5. They run **always or periodically**, consuming CPU whether or not anything
   happened.
6. Their inputs are **asynchronously shared variables** — a genuine race hazard.
7. **Buffer inputs** into locals at the top of each run-to-completion step.
8. Wrapping buffered inputs in a struct **is** an event; you are on a spectrum,
   not a binary.
9. Buffering alone doesn't fix the **sampling rate** — decoupling and queuing do.
10. Input-driven machines are **right** for games, robotics, and event discovery
    such as debouncing.

---

## Glossary

| Term | Meaning |
|---|---|
| Combinational / sequential circuit | No internal state / has state |
| D flip-flop | One-bit memory element |
| Synchronous / asynchronous circuit | Clocked / unclocked |
| Binary encoding | Mapping state names to flip-flop values |
| Mealy / Moore machine | Outputs from state+inputs / from state only |
| Input-driven / polled state machine | Driven by polling variables, not queued events |
| Proto-event | A raw input that guards distill into a real event |
| Input buffering | Copying inputs to locals for the RTC step |
| Debouncing | Filtering mechanical switch noise |

---

## Pitfalls to remember

- **Reading an asynchronous input twice** in one pass — it may differ each time.
- **Edge detection on an unbuffered input** — the landing-gear bug.
- **Assuming a superloop machine has a "period"** — it varies by orders of
  magnitude.
- **Coupling input sampling to state machine speed** — you lose fast changes.
- **Expecting buffering alone to fix missed bounces.**
- **Thinking `digitalRead()` protects you** — a function call changes nothing.
- **Using an input-driven machine where queued events are available** — and vice
  versa.

---

**Next:** Lesson 38 covers **state tables** and **entry/exit actions**.
