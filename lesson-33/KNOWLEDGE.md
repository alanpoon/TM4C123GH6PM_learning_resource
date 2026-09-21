# Lesson 33 — Knowledge: Event-Driven Programming Part 1 — The GUI Origins

**Video:** <https://youtu.be/rfb2JI1GGIc> · **Transcript:** <https://www.state-machine.com/course/lesson-33.txt>
**Hands-on companion:** [PRACTICE.md](PRACTICE.md)

---

## The big idea

The graphical user interface **required a new programming paradigm**, because
sequential code fundamentally cannot cope with multiple asynchronous input
sources. The answer — **event-driven programming** — was worked out in the 1980s
and is demonstrated most directly by the original **Win32 API in C**.

> The single property that sets an event-driven program most apart from a
> sequential one is **NO BLOCKING inside application-level code.**

---

## 1. Why the GUI broke sequential programming

**Command-line system:** one input device (keyboard), one output location (the
bottom of a scrolling screen). The software keeps its **traditional sequential
structure**: wait for a key, echo it, process it, maybe print more. **There is no
question of where output goes.**

**GUI:** everything changes.

| Problem | Why sequential code can't handle it |
|---|---|
| **Multiple input sources** (keyboard *and* mouse) | If you block waiting for the keyboard you are **unresponsive to the mouse**, and vice versa. You need to wait for **multiple inputs simultaneously** — and then determine which arrived. |
| **Where does output go?** | With a keyboard you must know which part of the screen is active — today called the **keyboard focus**. |
| **Mouse input is 2-D** | Raw X/Y coordinates plus button state are **not enough to act on**. You must know **which object is at those coordinates**. |

Since the object lookup would happen for *every* mouse input, you may as well let
the GUI system always do it — which means **the mouse effectively produces many
more kinds of input** (per-object inputs), all depending on the constantly
changing situation on the screen.

> GUI programming introduced complexity **not in the same ballpark** as the
> command line. It is an entirely different ball game, and it required a
> different way of thinking.

## 2. The key insight: focus on the inputs

The enabling insight was to focus on the inputs themselves — called **events** or
**messages**: key presses, mouse moves, and the secondary inputs from on-screen
objects (buttons, icons, scroll bars…).

> **The focus on events means the events drive the software** — not the other way
> round, as in sequentially coded command-line programs.

## 3. The event loop

After initialization (register a window class, create a window, show it), a
Win32 program enters the **event loop** — also called the **message loop** or
**message pump** — where the real work happens.

```c
while (1) {
    if (GetMessage(&msg, NULL, 0, 0) == 0) {   /* BLOCKS until an event arrives */
        break;                                  /* 0 => application closed       */
    }
    DispatchMessage(&msg);                      /* calls YOUR "window proc"      */
}
```

> **This is the most important part of every event-driven program.**

### Three key properties

**1. Message objects and the queue.**

> The event loop uses special **message objects** to record all events
> potentially interesting to the application. These objects **serve only for
> communication** and can be stored in the **event queue** and retrieved later.

Because of the queue, events can be delivered **both when the loop is waiting and
when it is busy** processing previous events.

The system **only records the event and queues it — it does NOT wait for it to be
processed**. That is **asynchronous** delivery: the event **producer** (Windows)
executes **independently** of the event **consumer** (your application).

This also solves the "wait for multiple inputs simultaneously" problem.

**2. Run-to-completion (RTC).**

> `DispatchMessage()` **must complete and return to the event loop** before the
> loop gets to the next event. So event processing proceeds in **Run-to-Completion
> steps that cannot be interrupted by the processing of any other event.**

**3. Inversion of control.**

> The event loop **calls your application code**.
>
> This is **backwards** from all your previous experience: with an RTOS **your
> application called the RTOS**. Now the event-driven system **calls your
> application**.

> **Inversion of control is the key characteristic of all event-driven systems and
> the essence of event-driven programming.** It is literally what "events drive
> the application" means.

## 4. The window procedure

```c
LRESULT CALLBACK WndProc(HWND me, UINT sig, WPARAM wParam, LPARAM lParam)
```

The four parameters are exactly the first four attributes of the `MSG` structure.
Renamed here to reveal the concepts:

| Win32 name | Renamed | Meaning |
|---|---|---|
| `hwnd` | **`me`** | the window instance — the "wind proc" is a **member function** of the window class (Lesson 29) |
| `message` | **`sig`** | **the signal** — *what kind* of event this is (modern event-driven terminology) |
| `wParam`, `lParam` | — | **event parameters** — extra information whose meaning depends on the signal |

Moreover, as `WinMain` shows, the "wind proc" is not just a member function — it
is a **virtual** member function specific to the registered window class. The
Win32 designers used the simple technique of **embedding the function pointer
directly in the attribute structure** (Lesson 32's alternative implementation).

### The dispatch `switch`

Windows programmers discriminate on the **integer signal**:

```c
switch (sig) {
    case WM_CREATE:     ... status = WIN_HANDLED; break;
    case WM_DESTROY:    PostQuitMessage(0); ... break;
    case WM_PAINT:      ... break;
    case WM_KEYDOWN:    ++wm_keydown;   InvalidateRect(...); break;
    case WM_MOUSEMOVE:  ++wm_mousemove; InvalidateRect(...); break;
    default:            status = DefWindowProc(...); break;   /* <== see below */
}
return status;
```

Three points worth extracting:

- **`PostQuitMessage()`** inserts `WM_QUIT` into the program's own queue, which
  terminates the event loop — an example of an application **asynchronously
  posting events to itself**.
- **Two-way communication:** Windows tells the wind proc which message to
  process; the wind proc **returns a status** back. `InvalidateRect()` is the
  same idea — telling Windows the window needs repainting.
- **State must be `static`.** The counters are `static` because they **must
  outlive the many invocations and *returns* from the wind proc**. Local
  automatic variables would go out of scope on every return.

## 5. The default handler — "Ultimate Hook"

The `default:` case calls the **default window proc** supplied by Windows. It
looks unimpressive, but it is how the **characteristic look and feel** of the GUI
comes about: the HelloWin application can be resized, moved, minimized, restored
and maximized — though **you only coded counting of key presses and mouse
moves**.

Scan the **hundreds** of `WM_` signals in `WinUser.h`: most of them pass through
your wind proc, and **most are handled by the default proc**, leaving you
**blissfully unaware** of the complexity.

> Think of the design as **layered in a hierarchy**: **your code is at the lowest
> level and gets the first shot at every event**. When it doesn't explicitly
> handle an event, the event is **not ignored** — it is passed **up** to the
> Windows system.

Two names for this design, emphasizing different aspects:

| Name | Emphasizes |
|---|---|
| **Ultimate Hook** | the ease of "hooking up" your code to every event |
| **Programming by Difference** | that you only program the **differences** from the default behaviour |

**In OOP terms** (Lessons 29–32): the **Windows System is the base class** with
hundreds of virtual functions, one per message signal; **applications are
subclasses** that **override** selected ones in their wind procs.

## 6. Why blocking destroys an event-driven program

Try the sequential approach inside a wind proc — blink an LED after a keypress
using `Sleep(200)` (Windows' equivalent of an RTOS `delay()`):

**Symptom 1 — the application becomes a "pig".**

Press several keys quickly and the program **freezes**; the counters don't
update, then **jump all at once** by a big increment.

> The blocking `Sleep()` prevents the wind proc returning quickly to the event
> loop. **When the event loop spins too slowly, events accumulate in the queue.**
> Only when all the blocked messages are finally processed does the loop unclog
> and flush everything — hence the sudden jumps.
>
> Windows programmers call such an application a **"pig"** — and you don't want
> to be one.
>
> **Old rule of thumb: anything taking more than about 100 ms should be broken
> into shorter pieces using events.**

**Symptom 2 — the LED update doesn't happen at all.** This reason is more
interesting:

> From the event-driven perspective, **every blocking call really means waiting
> for some event to happen**, and unblocking means the event occurred. That event
> is **delivered in the middle of processing another event** — which **violates
> Run-to-Completion semantics**, which every event-driven system assumes.

Concretely: `InvalidateRect()` before the `Sleep()` **has no effect**, because
the wind proc **never returns to Windows** at that point — so Windows has no
chance to send `WM_PAINT`. You never see the update.

> **Blocking is a BAD IDEA in event-driven systems for two reasons: it clogs the
> event loop and destroys responsiveness to *all* events; and it violates
> run-to-completion semantics.**
>
> **Sequential programming and event-driven programming are two distinct
> paradigms that don't mix well — always keep them separate.**

## 7. The event-driven solution

Instead of blocking, use a facility designed for the purpose — a **timer**, set
to generate a **`WM_TIMER` event** a given number of milliseconds in the future:

- `WM_KEYDOWN` → turn the LED on, `SetTimer(...)`, return immediately.
- `WM_TIMER` → turn the LED off, and **`KillTimer()`** (otherwise the timer keeps
  expiring periodically). Note this case uses **`wParam`**, which here holds the
  **ID of the timer** that generated the message.

Now isolated keypresses momentarily light the LED, and **bursts of keypresses
while wiggling the mouse** keep both counters updating — **the application stays
responsive**.

---

## Key takeaways

1. Multiple asynchronous input sources are what broke sequential programming.
2. Focus on **events**: they drive the program, not the other way round.
3. The **event loop** is the heart of every event-driven program.
4. Events are **queued** and delivered **asynchronously** — the producer never
   waits for the consumer.
5. Processing proceeds in **run-to-completion** steps.
6. **Inversion of control** is the essence of event-driven programming.
7. An event carries a **signal** (what happened) plus **parameters**.
8. State that must survive returns has to be **`static`** (or an object
   attribute).
9. Unhandled events pass to a **default handler** — the Ultimate Hook /
   Programming by Difference, i.e. polymorphism at system scale.
10. **Never block in application-level event-driven code** — use timers and
    events instead.

---

## Glossary

| Term | Meaning |
|---|---|
| Event / message | An object recording something that happened |
| Signal | The kind of event |
| Event parameters | Extra data qualifying the event |
| Event loop / message pump | Get an event, dispatch it, repeat |
| Event queue | Buffer holding posted events |
| Asynchronous posting | The producer does not wait for processing |
| Run-to-completion (RTC) | An event is processed fully before the next begins |
| Inversion of control | The framework calls your code |
| Keyboard focus | Which screen object receives keyboard input |
| Default handler | Framework-supplied behaviour for unhandled events |
| Ultimate Hook / Programming by Difference | Two names for the hierarchical default design |
| "Pig" | An application that clogs the event loop |

---

## Pitfalls to remember

- **Blocking in an event handler** — the cardinal sin.
- **Handlers longer than ~100 ms** — break them up with events.
- **Automatic variables holding state** across invocations — they vanish on
  return.
- **Expecting an effect before you return** to the framework (e.g. repainting).
- **Forgetting to kill a one-shot timer** — it becomes periodic.
- **Failing to pass unhandled events to the default handler** — you lose the
  standard behaviour.

---

**Next:** Lesson 34 applies all of this to **real-time embedded systems** and
arrives at the **Active Object** pattern.
