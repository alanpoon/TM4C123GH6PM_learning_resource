# Lesson 34 — Knowledge: Event-Driven Programming Part 2 — Active Objects

**Video:** <https://youtu.be/l69ghMpsp6w> · **Transcript:** <https://www.state-machine.com/course/lesson-34.txt>
**Hands-on companion:** [PRACTICE.md](PRACTICE.md)

---

## The big idea

> **The Active Object design pattern combines the RTOS, object-oriented
> programming and event-driven programming** — the three trends this course calls
> **"Modern"**, in contrast to the still-dominant **"Traditional"** approach based
> on a plain RTOS and **shared-state concurrency**, which dates from the early
> 1980s.

This lesson is a **paradigm shift**, and you build your own minimal active-object
framework on top of µC/OS-II to get there.

---

## 1. The perils of blocking

Starting from a conventional RTOS design, follow the chain of consequences:

```
threads synchronize by BLOCKING in-line
        ↓
a blocked thread is unresponsive to any OTHER event
        ↓
workaround: create MORE THREADS (they can wait in parallel)
        ↓
threads become expensive and unmanageable — and they need the same data
        ↓
SHARED-STATE CONCURRENCY
        ↓
threads inherit all the problems of interrupts → RACE CONDITIONS → system failure
        ↓
fix with MUTUAL EXCLUSION mechanisms (mutexes, …)
        ↓
which ironically cause MORE BLOCKING  ─────┐  (vicious circle)
        ↓                                   │
complicated timing analysis → MISSED DEADLINES → system failure
```

> Context switching in an RTOS **hijacks the interrupt hardware**, so **threads
> inherit all the problems of interrupts**, including races around shared
> resources.

Experienced developers therefore became **very wary of blocking**.

## 2. Herb Sutter's three best practices

From *"Prefer Using Active Objects Instead of Naked Threads"*:

| # | Best practice | Which node of the vicious circle it eliminates |
|---|---|---|
| **1** | **Keep data isolated, private to a thread where possible** | **shared-state concurrency** — without sharing there is no need for mutual exclusion, removing a big part of the circle |
| **2** | **Communicate among threads via asynchronous messages** | **synchronization by blocking** |
| **3** | **Organize your thread's work around a message pump** | **blocking** itself |

Best practice 2 packs two concepts you met in Lesson 33:

- **Event objects** (messages) — packaged data designed for communication,
  carrying **what happened** ("timeout-occurred", "button-was-pressed") plus
  optional **event parameters** (e.g. an "ethernet-packet-arrived" event carrying
  the whole packet).
- **Asynchronous communication** — senders **only post** the event; they **do not
  wait in-line** until it is processed.

> **Blocking cannot be eliminated completely** — at least not with a traditional
> RTOS, since every RTOS thread must block somewhere in its superloop. But best
> practice 3 prescribes the **only allowed structure** for that loop — the
> **message pump** — which **restricts blocking to exactly one place**.

## 3. The Active Object pattern

> The pattern provides **a layer on top of naked threads**, which otherwise let
> you do anything — including troublesome things.

**An active object has:**

- **private data** (like any object),
- **a private thread**,
- **a private event queue**.

**The only way to interact with it is by posting events to its queue**, and
**posting is asynchronous** — the event is placed in the queue with no waiting
for processing. Processing happens in the **event loop running in its private
thread**, and may involve **sending secondary events to other active objects or
to itself**.

### True encapsulation for concurrency

> Active objects are **the most stringent form of object-oriented programming**,
> because asynchronous communication makes them **truly encapsulated**.
>
> **Traditional OOP encapsulation — in C++, C# or Java — does not really
> encapsulate anything in terms of concurrency.** Any operation on an object runs
> **in the caller's thread**, and the object's private data are subject to **the
> same race conditions as global data** — not encapsulated at all. To become
> thread-safe, operations must be explicitly protected by a mutex or monitor,
> which causes **additional blocking** and demands constant attention.
>
> In contrast, **all private data of an active object are truly encapsulated for
> concurrency without any mutual exclusion mechanism**, because they can only be
> accessed from the active object's own thread.

> This encapsulation is **not a programming-language feature** — it is no harder
> in C than in C++ — but it **requires programming discipline** to avoid sharing
> resources: the **"shared-nothing principle"**.

**And event-based communication makes that discipline practical:** instead of
sharing a resource, **a dedicated active object becomes the manager or broker of
that resource**, and the rest of the system accesses it **only via events posted
to that broker**.

## 4. Building the framework: µC/AO

A minimal active-object layer over µC/OS-II — `uc_ao.h` and `uc_ao.c`.

### Events

```c
typedef uint16_t Signal;
enum {
    INIT_SIG,       /* reserved: dispatched to each AO right before its event loop */
    USER_SIG        /* the first signal available to applications                  */
};

typedef struct { Signal sig; } Event;             /* the base event */

typedef struct {                                   /* example subclass */
    Event super;                                   /* inherits Event  */
    uint8_t packet[...];                           /* event parameter */
} EthernetEvent;
```

Event parameters are added by **inheritance** (Lesson 30).

### The Active class

```c
typedef void (*DispatchHandler)(struct Active *me, Event const *e);

typedef struct Active {
    OS_PRIO prio;             /* the private THREAD -- in µC/OS-II, its priority  */
    OS_EVENT *queue;          /* the private EVENT QUEUE                          */
    DispatchHandler dispatch; /* the "virtual" operation, supplied by subclasses  */
} Active;

void Active_ctor (Active *me, DispatchHandler dispatch);
void Active_start(Active *me, ...);      /* starts the internal thread */
void Active_post (Active *me, Event const *e);   /* ASYNCHRONOUS posting */
```

Notes on the design choices:

- The µC/OS-II name `OS_EVENT` is **a bit of a misnomer** here — it is the
  operating-system object used to **deliver** the `Event` instances defined above.
- A **message queue** is **somewhat of an overkill**: it can be written and read
  by multiple threads, whereas an AO's queue needs only **a single reader**. But
  it is the closest approximation available.
- `dispatch` uses the **VTABLE-embedded-in-the-object** implementation of
  polymorphism (Lesson 32) — with just **one** virtual function, that makes the
  most sense.
- Subclasses add their **private data** by inheritance, exactly as event
  subclasses add parameters.

### Inversion of control in the framework

> **EVERY active object's thread runs the SAME event-loop function**, defined
> inside the AO module and made **`static`** to hide it completely.
>
> **This is a drastic departure from the traditional way of creating RTOS
> threads**, where each thread runs its own custom function defined in
> **application-level** code, outside the RTOS.
>
> **Control over the thread function now resides in the framework, not the
> application** — the **inversion of control** from Lesson 33.

Each instance of that same loop operates on a **different** `me` pointer, passed
as the thread's argument.

### The event loop

```c
static void Active_eventLoop(void *pdata) {
    Active *me = (Active *)pdata;          /* cast back to the AO instance */

    static Event const initEvt = { INIT_SIG };
    (*me->dispatch)(me, &initEvt);         /* the INIT event, before the loop  */

    while (1) {
        Event const *e = OSQPend(me->queue, 0U, &err);  /* the ONLY blocking   */
        (*me->dispatch)(me, e);                          /* virtual dispatch    */
    }
}
```

It mirrors the Windows structure of Lesson 33: the INIT event corresponds to
`WM_CREATE` sent by `CreateWindow()`; `OSQPend()` corresponds to `GetMessage()`
and is **the only place where blocking is allowed** — when the queue is empty;
the virtual dispatch corresponds to `DispatchMessage()`.

### Immutable events

This interface posts **pointers to events**, not whole objects by value as
Windows did.

> For events **modified outside** a given active object you must be **very
> careful to avoid race conditions**. But **events without parameters can be
> `const` and allocated in ROM**. Such **immutable** events can be **shared
> freely**, because they pose **no concurrency risk**.

## 5. Time events

Converting a blocking delay raises the question: **where is the event in a time
delay?**

> **Every blocking call always corresponds to an event**, even if you don't
> initially know what to call it. Time delays correspond to **Time Events**.

> A **Time Event** is an event that an active object can request to be **posted
> to its own event queue at some time in the future.**

```c
typedef struct {
    Event super;        /* a TimeEvent IS an Event                 */
    Active *act;        /* which AO requested the timeout          */
    uint32_t timeout;   /* down-counter, decremented each tick     */
    uint32_t interval;  /* reload value: 0 = one-shot              */
} TimeEvent;

void TimeEvent_ctor  (TimeEvent *me, Signal sig, Active *act);
void TimeEvent_arm   (TimeEvent *me, uint32_t timeout, uint32_t interval);
void TimeEvent_disarm(TimeEvent *me);
void TimeEvent_tick  (void);     /* CLASS-WIDE ("static"): no 'me' pointer */
```

Semantics worth noting:

- `timeout == 0` means **disarmed** — not ticking.
- When the counter reaches zero the event is **posted** and the counter is
  **reloaded from `interval`**, so the time event **re-arms itself**
  automatically. **`interval == 0` naturally gives a one-shot** time event,
  disarmed after expiring once.
- **`TimeEvent_tick()` is a class-wide operation** servicing **all** instances
  every system clock tick.

### Race conditions inside your own framework

The implementation keeps a **static array of registered TimeEvent pointers**
(prefixed `l_` to mark them module-local). Those variables are **implicitly
shared** among concurrent threads **and** the clock-tick ISR, through the
TimeEvent operations — **so they need mutual-exclusion protection**.

The **µC/OS-II critical section** is the right tool, because the accesses are
very short — **the same mechanism µC/OS-II uses internally** for its own
variables.

> **Interesting observation: your active-object layer is an event-driven
> *extension* of a traditional RTOS, and as such it faces the same challenges as
> other parts of the RTOS code — including avoiding internal race conditions.**

`TimeEvent_tick()` itself **needs no critical section**, because it is called
from the clock-tick interrupt, which **can never be preempted by an RTOS
thread**.

## 6. Converting sequential code to event-driven

The recipe, applied to a Button thread:

1. **Create a subclass** of `Active` (embed it as `super`, Lesson 30).
2. **Write the `dispatch` operation** — typically a **`switch` on the event
   signal**, one case per event.
3. **`INIT_SIG`** establishes the initial state.
4. **Each formerly blocking wait becomes a case**: where the code waited on a
   button-press semaphore, it now handles a `BUTTON_PRESSED` signal.
5. **Write the constructor**, calling the base constructor to attach the specific
   `dispatch` implementation.
6. **Delete the sequential code**, including the blocking semaphores — but
   **keep the stack**, still needed for the AO's internal thread. Add the
   **event-queue buffer**, the **AO instance**, and a **global pointer** so other
   code (e.g. the BSP) can post to it.
7. **In the BSP**, replace semaphore signalling with **posting events** — using
   **immutable `const` events in ROM** where there are no parameters.

> **The most important thing to remember: in event-driven code you NEVER block to
> wait for events. All waiting is external to the dispatch function and happens
> at the top of the event loop.**

### Declare the global pointer as the superclass

```c
extern Active *AO_button;      /* Active, NOT Button */
```

> Intentional: it **completely encapsulates any private data the subclass might
> have.** The rest of the application **does not know about that data** and can
> only interact with the instance as a generic Active object.

### Handling state that a blocking call used to carry

Sequential code had **two** blocking calls — one for LED-on time, one for
LED-off. Event-driven code has **one** TIMEOUT event, so it must **remember**
whether the LED is on.

> That information must **survive the calls and *returns* from `dispatch`**, so
> it **definitely cannot live on the stack as an automatic variable.**

The ideal place is **the private data of the active object** — e.g. an `isLedOn`
flag, initialized in the constructor. (Compare the `static` counters in Lesson
33.)

### Priming the pump

The time event is armed only in the TIMEOUT case — which never runs until a
TIMEOUT occurs. **The INIT event solves this**, being dispatched to every AO
before its event loop. Either arm it explicitly in `INIT_SIG`, or
**intentionally fall through** from INIT into TIMEOUT.

## 7. Active objects are composable

Why were there two active objects at all?

> Only because you **translated a sequential design**. The **real and only
> reason** the original design had two threads is that **sequential superloops
> are not composable, due to unresponsiveness**. Making them composable again was
> the entire reason for an RTOS — at the expense of multiple threads.
>
> **None of that applies to event-driven code, which remains responsive to all
> events. Event-driven code IS directly composable.**

So merge Blinky and Button into **one** `BlinkyButton` active object: move the
private data over, merge the dispatch cases (combining duplicates like INIT).

**And then the payoff:**

> `shared_blink_time` **is no longer shared between threads.** It becomes
> **internal, fully encapsulated private data** of the active object.
>
> With no possible race conditions, **you can get rid of the mutex** — which not
> only reduces overhead but **eliminates potential blocking**, giving **more
> predictable timing** and code **better suited to hard real-time applications**.

Deleting the Button AO also **frees significant RAM** — its stack and queue
buffer.

**Two properties demonstrated:**

1. **An active object can hold much more functionality than a traditional
   blocking thread**, because you no longer need expensive threads just to stay
   responsive to events.
2. **Combining functionality inside event-driven active objects makes it much
   easier to avoid sharing resources** than with traditional blocking threads.

## 8. RTOS vs. event-driven framework

> In your framework you used a few RTOS services — **threads and message
> queues**. But a traditional RTOS **does not provide much else** needed in
> event-driven programming, which you had to create yourself.
>
> At the same time, **most mechanisms a traditional RTOS provides are of no use**
> in programming event-driven active objects. **Not only unhelpful — they can be
> outright harmful, because most are based on blocking, which is not allowed
> inside active objects.**
>
> **Even though a traditional RTOS can be used to implement an event-driven
> framework, it is not the best fit.**

---

## Key takeaways

1. Blocking sets off a vicious circle ending in missed deadlines.
2. The three best practices: **isolate data**, **communicate by asynchronous
   messages**, **organize work around a message pump**.
3. An **active object** = private data + private thread + private event queue.
4. **Posting is asynchronous**; it is the **only** way to interact with an AO.
5. Active objects give **true encapsulation for concurrency**, which ordinary OOP
   does not.
6. This needs **discipline** (shared-nothing), helped by **broker AOs** for
   shared resources.
7. **All AOs run the same framework event loop** — inversion of control.
8. **Blocking is allowed in exactly one place**: getting the next event.
9. **Every blocking call corresponds to an event**; delays become **time events**.
10. State that must survive `dispatch` returns belongs in the **AO's private
    data**.
11. **Event-driven code is directly composable** — merging AOs removes sharing
    and mutexes.
12. A traditional RTOS is usable but **not the best fit** for this style.

---

## Glossary

| Term | Meaning |
|---|---|
| Active object | Private data + private thread + private event queue |
| Asynchronous posting | Placing an event in a queue without waiting |
| Message pump | The event loop structuring an AO's thread |
| Shared-nothing principle | Discipline of not sharing resources between AOs |
| Broker AO | An AO that owns a resource others access via events |
| Immutable event | A `const` parameterless event in ROM, safe to share |
| Time event | An event posted to an AO at a future time |
| One-shot / periodic | `interval == 0` / `interval != 0` |
| `INIT_SIG` | Reserved signal dispatched before the event loop starts |
| Composability | Ability to combine functionality without interference |

---

## Pitfalls to remember

- **Any blocking call inside `dispatch`** — the cardinal sin.
- **Automatic variables holding state** across dispatch returns.
- **Forgetting to prime the pump** in `INIT_SIG` — nothing ever starts.
- **Posting mutable shared events** — reintroduces races.
- **Exposing the subclass type** in a global pointer — leaks private data.
- **Forgetting critical sections inside your own framework** — it has the same
  internal-race problem as an RTOS.
- **Creating one AO per formerly-blocking thread** — event-driven code composes;
  merge them.

---

**Next:** Lessons 35–42 cover **state machines**, which give event-driven code
the structure it needs (instead of the "big ball of mud" warned about in Lesson
21).
