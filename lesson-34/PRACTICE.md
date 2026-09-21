# Lesson 34 — Practice: Building an Active Object Framework

**Concepts behind this exercise:** [KNOWLEDGE.md](KNOWLEDGE.md)
**Video:** <https://youtu.be/l69ghMpsp6w>

---

## Projects in this lesson

| Directory | Toolchain / kernel | Target |
|---|---|---|
| `tm4c123-ucos2-keil/` | KEIL + **µC/OS-II** — the **sequential** starting point | EK-TM4C123GXL |
| `tm4c123-uc_ao-keil/` | KEIL + **µC/AO** — the **active-object** result | EK-TM4C123GXL |
| `stm32c031-ucos2-keil/`, `stm32c031-uc_ao-keil/` | the same pair | NUCLEO-C031C6 |
| `ucos2/`, `CMSIS/`, `ek-tm4c123gxl/`, `nucleo-c031c6/` | — | kernel and support code |

> **Compare the two projects directly** — they are the before and after of this
> lesson.
>
> µC/OS-II is used here so you get exposed to another popular RTOS; Silicon Labs
> released it on GitHub under the permissive **Apache-2** licence. The (slightly
> modified) sources live in `qpc/3rd_party`, so you need **both** the lesson ZIP
> and **`qpc.zip`**, unzipped into the same directory.

## What the program does

Blinks the **green** LED; pressing **SW1** speeds up the blinking. The **blue**
LED is on while the button is held, to visualize the button state.

---

## Part A — Study the sequential design and its costs

### 1. Read the two threads

**`main_blinky`** — like Lesson 25, but uses µC/OS-II's **`OSTimeDly()`** to
**block and wait in-line**. The delay comes from **`shared_blink_time`**, shared
with the button thread and therefore protected by a **priority-ceiling mutex**.

**`main_button`** — the usual `while(1)` superloop. It **waits on a semaphore**
signalled when the button is pressed; then turns the blue LED on and updates
`shared_blink_time` (mutex again); then **waits on another semaphore** for the
release, and turns the blue LED off.

### 2. Trace the chain of consequences

Small as it is, this program demonstrates the general principles of RTOS design —
and their costs. Follow the chain yourself:

```
threads synchronize by BLOCKING in-line
   ↓  a blocked thread is unresponsive to any OTHER event
   ↓  workaround: MORE THREADS (they can wait in parallel)
   ↓  expensive, unmanageable — and they need the same data
   ↓  SHARED-STATE CONCURRENCY  (e.g. shared_blink_time)
   ↓  threads inherit interrupt problems -> RACE CONDITIONS -> failure
   ↓  fix with MUTUAL EXCLUSION (mutexes)
   ↓  which ironically causes MORE BLOCKING   (vicious circle)
   ↓  complicated timing analysis -> MISSED DEADLINES -> failure
```

> Context switching **hijacks the interrupt hardware**, so **threads inherit all
> the problems of interrupts**.

### 3. Learn the three best practices

From Herb Sutter's *"Prefer Using Active Objects Instead of Naked Threads"*:

| # | Practice | Removes |
|---|---|---|
| 1 | **Keep data isolated, private to a thread where possible** | shared-state concurrency — no sharing → no mutual exclusion |
| 2 | **Communicate among threads via asynchronous messages** | synchronization by blocking |
| 3 | **Organize your thread's work around a message pump** | blocking itself |

> **Blocking can't be eliminated entirely** with a traditional RTOS — every
> thread must block somewhere. But practice 3 prescribes the **only allowed
> structure**, restricting blocking to **one place**.

Together they form the **Active Object pattern**, which **provides a layer on top
of naked threads** — which otherwise let you do anything, including troublesome
things.

---

## Part B — Build the µC/AO framework

Two files: **`uc_ao.h`** and **`uc_ao.c`** (provided; review them).

### 4. Define events

```c
typedef uint16_t Signal;
enum {
    INIT_SIG,     /* reserved: dispatched to each AO right before its event loop */
    USER_SIG      /* the first signal available to applications                  */
};

typedef struct { Signal sig; } Event;

typedef struct {              /* example: parameters added by INHERITANCE (L30) */
    Event super;
    uint8_t packet[64];
} EthernetEvent;
```

### 5. Define the Active class

```c
typedef void (*DispatchHandler)(struct Active *me, Event const *e);

typedef struct Active {
    OS_PRIO prio;             /* the private THREAD (µC/OS-II: its priority) */
    OS_EVENT *queue;          /* the private EVENT QUEUE                     */
    DispatchHandler dispatch; /* the "virtual" operation                     */
} Active;

void Active_ctor (Active *me, DispatchHandler dispatch);
void Active_start(Active *me, ...);
void Active_post (Active *me, Event const *e);      /* ASYNCHRONOUS */
```

Understand each choice:

- µC/OS-II's `OS_EVENT` name is **a misnomer here** — it is the OS object used to
  **deliver** your `Event` instances.
- A **message queue is overkill**: it supports multiple writers *and* readers,
  while an AO's queue needs only **one reader**. It is simply the closest
  approximation available.
- `dispatch` uses the **VTABLE-embedded-in-the-object** form of polymorphism
  (Lesson 32) — with one virtual function, that makes the most sense.
- Subclasses add **private data** by inheritance — **strictly encapsulated,
  accessible only from the AO's own thread**.

### 6. Find the inversion of control in `Active_start()`

It creates the queue (asserting success), then creates the µC/OS-II task. Look
carefully at **which function the task runs**:

> **EVERY active object's thread is based on the SAME event-loop function**,
> defined earlier in the file and made **`static`** to hide it completely in the
> module.
>
> **This is a drastic departure** from the traditional way, where each thread
> runs its **own custom function defined in application-level code**.
>
> **Control over the thread function now resides in the framework, not the
> application** — the **inversion of control** of Lesson 33.

Each instance of the same loop works on a **different `me`**, passed as the
`pdata` argument to `OSTaskCreateExt()` and cast back at the top of the loop.

### 7. Read the event loop, comparing it with Windows

```c
static void Active_eventLoop(void *pdata) {
    Active *me = (Active *)pdata;

    static Event const initEvt = { INIT_SIG };
    (*me->dispatch)(me, &initEvt);       /* cf. WM_CREATE from CreateWindow()  */

    while (1) {
        Event const *e = OSQPend(me->queue, 0U, &err);  /* cf. GetMessage()    */
        (*me->dispatch)(me, e);                          /* cf. DispatchMessage */
    }
}
```

> **`OSQPend()` is the ONLY place in the loop where blocking is allowed** — when
> the queue is empty.

Note the interface posts **pointers to events**, not whole objects by value as
Windows did:

> For events **modified outside** a given AO you must be **very careful to avoid
> race conditions**. But **parameterless events can be `const` and allocated in
> ROM** — such **immutable** events can be **shared freely**, posing **no
> concurrency risk**.

The remaining pieces are trivial: the constructor sets `dispatch`, and
`Active_post()` posts to the private queue.

---

## Part C — Convert the Button thread

### 8. Create the subclass

```c
typedef struct {
    Active super;      /* inherits Active -- must be FIRST (lesson 30) */
    /* Button has no private data */
} Button;
```

Keep the original sequential code as a reference for now — most of it will go.

### 9. Write the dispatch operation

```c
static void Button_dispatch(Button * const me, Event const * const e) {
    switch (e->sig) {
        case INIT_SIG:            /* establish the initial state: blue LED off */
            ...
            break;
        case BUTTON_PRESSED_SIG:  /* what the press-semaphore used to trigger   */
            ...
            break;
        case BUTTON_RELEASED_SIG: /* what the release-semaphore used to trigger */
            ...
            break;
    }
}
```

Note the conversion recipe: **each formerly blocking wait becomes a case.**

> **The most important thing to remember: in event-driven code you NEVER block to
> wait for events. All waiting is external to the dispatch function and happens
> at the top of the event loop.**

### 10. Write the constructor and delete the old code

The constructor calls the base constructor, attaching `Button_dispatch` to the
virtual operation.

Then **delete the sequential code, including the blocking semaphores** — but:

- **keep the stack** (still needed for the AO's internal thread);
- add the **memory buffer for the private event queue**;
- add the **Button instance**;
- add a **global pointer** so other code (the BSP) can post to it.

In `main.c`, call the constructor and then **`Active_start()`**.

### 11. Update the BSP

- Replace the semaphores with **event signal definitions**.
- Declare the global pointer as **`Active *`, not `Button *`**:

> Intentional: it **completely encapsulates any private data the subclass might
> have**. The rest of the application **doesn't know about it** and can interact
> only with a generic Active object.

- In `bsp.c`, replace semaphore signalling with **posting events**. Since
  `BUTTON_PRESSED`/`BUTTON_RELEASED` have no changing parameters, use
  **immutable `const` events in ROM**.

### 12. Test

Build (fixing the few details the compiler points out) and run.

- The **green LED blinks as before** — unsurprising, it's still the old
  sequential code.
- The **button works exactly as before** — but it is now **event-driven**.

**You just implemented your first active object.**

---

## Part D — Add time events

### 13. Find the event hiding in the delay

Converting Blinky raises the question: button-press and button-release were
obvious events, but **where is the event in a time delay?**

> **Every blocking call always corresponds to an event**, even if you don't
> initially know what to call it. **Time delays correspond to Time Events.**
>
> A **Time Event** is an event an active object can request to be **posted to its
> own queue at some time in the future.**

### 14. Add the TimeEvent class

```c
typedef struct {
    Event super;        /* a TimeEvent IS an Event                       */
    Active *act;        /* which AO requested the timeout                */
    uint32_t timeout;   /* down-counter, decremented every clock tick    */
    uint32_t interval;  /* reload value                                  */
} TimeEvent;

void TimeEvent_ctor  (TimeEvent *me, Signal sig, Active *act);
void TimeEvent_arm   (TimeEvent *me, uint32_t timeout, uint32_t interval);
void TimeEvent_disarm(TimeEvent *me);
void TimeEvent_tick  (void);      /* CLASS-WIDE: no 'me' pointer */
```

Semantics to get right:

- **`timeout == 0` means disarmed** — not ticking.
- On reaching zero the event is **posted** and the counter is **reloaded from
  `interval`**, so it **re-arms itself**. **`interval == 0` naturally gives a
  one-shot**, automatically disarmed after expiring once.
- **`TimeEvent_tick()` is a class-wide (`static`) operation** servicing **all**
  instances each system clock tick.

### 15. Implement it — and notice your own race conditions

Keep a **static array of registered TimeEvent pointers** plus a count, prefixed
**`l_`** to mark them module-local. The constructor initializes the members
**and registers the instance** in that array.

> **Important:** those local variables are **implicitly shared** among concurrent
> RTOS threads **and** the clock-tick ISR, through the TimeEvent operations — so
> there is a **potential for race conditions** and you must apply **mutual
> exclusion**.

Use the **fast µC/OS-II critical section**, since the accesses are very short —
**exactly the mechanism µC/OS-II uses internally** for its own variables. `arm`
and `disarm` likewise run inside critical sections.

> **Interesting observation:** your active-object layer is an event-driven
> **extension of a traditional RTOS**, and as such faces **the same challenges as
> other parts of the RTOS code** — including internal race conditions.

**`TimeEvent_tick()` needs no critical section**, because it is called from the
clock-tick interrupt, which **can never be preempted by an RTOS thread**. It
walks the registered instances: if armed, decrement; if just reached zero,
**post** the event to its AO and **reload from `interval`**.

Finally, **call `TimeEvent_tick()` from the µC/OS-II system clock-tick callback**
— otherwise nothing ticks.

---

## Part E — Convert the Blinky thread

### 16. Give Blinky a time event

Add a **`TimeEvent`** instance as private data, initialized in the constructor —
associated with `me->super` and given the **`TIMEOUT`** signal defined in
`bsp.h`.

### 17. Solve the "which half of the blink?" problem

Sequential code had **two** blocking calls (LED-on time and LED-off time).
Event-driven code has **one** TIMEOUT event, so it must **remember** whether the
LED is currently on.

> That information must **survive the calls and *returns* from `dispatch`** — so
> it **definitely cannot live on the stack as an automatic variable.**

Put it in the AO's **private data**: an `isLedOn` flag, initialized in the
constructor (assume the LED starts off).

```c
case TIMEOUT_SIG:
    if (!me->isLedOn) {
        BSP_ledGreenOn();
        me->isLedOn = true;
        TimeEvent_arm(&me->te, me->blink_time, 0U);
    }
    else {
        BSP_ledGreenOff();
        me->isLedOn = false;
        TimeEvent_arm(&me->te, me->blink_time * 3U, 0U);
    }
    break;
```

### 18. Prime the pump

**This code has a basic flaw:** the time event is armed **only** in the TIMEOUT
case — which never runs until a TIMEOUT occurs.

**The INIT event** — dispatched to every AO right before its event loop — is
exactly the answer. Either arm the time event explicitly in `INIT_SIG`, or —
since that merely repeats the TIMEOUT case with the flag cleared —
**intentionally fall through** from INIT into TIMEOUT.

### 19. Finish the conversion

Move the `shared_blink_time` handling ahead of the new AO, delete the sequential
code, and start the **Blinky AO** instead of the old thread.

Run it: **both Blinky and Button active objects work as expected.**

---

## Part F — The payoff: composability

### 20. Ask why there are two AOs at all

> They exist only because you **translated a sequential design**. The real and
> only reason the original had two threads is that **sequential superloops are
> not composable, due to unresponsiveness** — and making them composable again
> was the entire reason for an RTOS, at the cost of multiple threads.
>
> **None of that applies to event-driven code, which stays responsive to all
> events. Event-driven code IS directly composable.**

### 21. Merge them

1. Rename `Blinky` → **`BlinkyButton`**.
2. Move any private data from `Button` into it.
3. Move all of Button's events into `BlinkyButton_dispatch`, **merging duplicate
   cases** such as INIT.

### 22. Collect the reward

> **`shared_blink_time` is no longer shared between threads!** It becomes
> **internal, fully encapsulated private data** of the active object.

- Initialize `blink_time` in the constructor, like any member.
- **Delete the mutex.** With no possible races it is pointless — and removing it
  **not only reduces overhead but eliminates potential blocking**, giving **more
  predictable timing** and code **better suited to hard real-time applications**.
- Replace `shared_blink_time` with `me->blink_time`, clean up the temporaries.
- **Delete the Button AO entirely** — freeing **significant RAM**: its stack and
  queue buffer.
- Remove its creation and start; rename the global `AO_button` →
  `AO_blinkyButton`.

Verify the final code works as before. **It does.**

### 23. Extract the two properties

1. **An active object can hold much more functionality than a traditional
   blocking thread**, because you no longer need expensive threads just to stay
   responsive to events.
2. **Combining functionality inside event-driven active objects makes it much
   easier to avoid sharing resources** than with traditional blocking threads.

---

## Perspective

> You built your own active-object framework, which is **very different from an
> RTOS because it is based on inversion of control.**
>
> You used a few RTOS services — **threads and message queues** — but a
> traditional RTOS **provides little else** that event-driven programming needs.
> Worse, **most mechanisms it does provide are of no use here — and can be
> outright harmful, because most are based on blocking, which is not allowed
> inside active objects.**
>
> **A traditional RTOS can implement an event-driven framework, but it is not the
> best fit.**

---

## Exercises

1. **Add a third behaviour.** Add a new event and handle it in `BlinkyButton`
   without creating another AO.
2. **Periodic time event.** Use a non-zero `interval` and remove the re-arming.
   What changes?
3. **Broker AO.** Make one AO own the LEDs and have others request changes by
   posting events. What did you gain and lose?
4. **Mutable event.** Add an event with a parameter and work out exactly where a
   race could occur.
5. **Forget the pump.** Remove the INIT handling and observe the system doing
   nothing.
6. **Block anyway.** Call `OSTimeDly()` inside `dispatch` and measure the damage
   to responsiveness.
7. **Count the RAM.** Compare RAM usage of the two-AO and merged versions.
8. **Compare frameworks.** Diff your µC/AO against QP/C's `QActive` and list what
   a production framework adds.

---

## Self-check

- [ ] I can draw the "perils of blocking" chain from memory
- [ ] I can state the three best practices and what each eliminates
- [ ] My Active class has a private thread, a private queue and a virtual dispatch
- [ ] All AOs run the **same** static event loop — I can point to the inversion
      of control
- [ ] Blocking occurs in exactly one place: fetching the next event
- [ ] I used immutable `const` events in ROM for parameterless signals
- [ ] TimeEvent arm/disarm/register are protected; `tick()` correctly is not
- [ ] State surviving dispatch returns lives in the AO's private data
- [ ] I merged two AOs into one, removed the shared variable **and** the mutex
