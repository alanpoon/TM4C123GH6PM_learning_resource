# Lesson 33 — Practice: Dissecting a Win32 Event-Driven Program

**Concepts behind this exercise:** [KNOWLEDGE.md](KNOWLEDGE.md)
**Video:** <https://youtu.be/rfb2JI1GGIc>

---

## Projects in this lesson

| Directory | Toolchain | Target |
|---|---|---|
| `win32-vc/` | **Visual Studio** (`hellowin.sln`) | **Windows desktop** — not the board |

> **This lesson runs on your PC, not the LaunchPad.** The GUI is where
> event-driven programming was invented, and the original **Win32 API in C**
> shows its concepts in their **simplest and most direct form** — more directly
> than any modern, higher-level Windows API.
>
> Visual Studio Community Edition is free after registration. The program is
> adapted from the "Hello-Windows" example in **Charles Petzold's "Programming
> Windows"** (1988) — the Windows programming bible of its day. Written in the
> 1980s API, it still builds and runs on 64-bit Windows.

---

## Part A — Understand the problem GUIs posed

Before reading the code, reason about why sequential code cannot work.

### 1. The command-line case

One input (keyboard), one output location (the bottom of a scrolling screen). So
the code keeps its **sequential structure**: wait for a key → echo it → process
it → maybe print more. **There is never a question of where output goes.**

### 2. The GUI case

Write the pseudocode yourself and watch it fail:

| Problem | Failure |
|---|---|
| Two input sources | Block on the keyboard → **unresponsive to the mouse**; block on the mouse → unresponsive to the keyboard |
| Even if you could wait for both | Downstream you must **check which one arrived** |
| Keyboard output location | You must know **which part of the screen is active** — the **keyboard focus** |
| Mouse is 2-D | X/Y plus button state are **not enough**; you need **which object is at those coordinates** |

Since that lookup happens for *every* mouse input, let the GUI system always do
it — which means **the mouse effectively generates many more kinds of input**,
all depending on the constantly changing screen.

> GUI complexity is **not in the same ballpark** as the command line. It required
> a different way of thinking: **focus on the inputs** — the **events** — and let
> them **drive the software**.

---

## Part B — Walk the code

### 3. `WinMain()` — the entry point

Plays the role of `main()`, but takes more parameters because a GUI application
is more complex (some unused in HelloWin).

### 4. Recognize the OOP (Lessons 29–32)

The window-class initialization is **object-oriented programming in C**:

- **attributes** of the `wnd` window-class instance: the window style, the mouse
  cursor, the class name;
- **a "virtual function"** — the **window procedure** (`WndProc`) specific to this
  window class.

> Notice which polymorphism implementation Win32 chose: **embedding the pointer
> to the virtual function directly in the attribute structure** — the alternative
> from Lesson 32.

Then `RegisterClass()` registers it, and **`CreateWindow()` plays the role of a
constructor**, creating a window object from the registered class. Finally the
window is shown and updated.

### 5. Read the event loop

After all initialization, `WinMain` enters the **event loop** (a.k.a. **message
loop** / **message pump**) — **the most important part of every event-driven
program**:

```c
while (1) {
    if (GetMessage(&msg, NULL, 0, 0) == 0) {  /* BLOCKS for keyboard/mouse/screen */
        break;                                 /* 0 => the application closed      */
    }
    DispatchMessage(&msg);                     /* calls the registered wind proc   */
}
```

When any event occurs, Windows **records it as a message object and places it in
this application's message queue**; `GetMessage()` then unblocks and copies it
out.

**This is how the event loop solves "wait for multiple inputs simultaneously".**

### 6. Extract the three key properties

**1 — Message objects and the queue.** They **serve only for communication**, and
can be stored in the queue and retrieved later. Therefore events can be delivered
**both while the loop waits and while it is busy**.

Crucially, Windows **only records and queues the event — it does NOT wait for it
to be processed.** That is **asynchronous** delivery: producer (Windows) and
consumer (your app) run **independently**. Part D demonstrates this.

**2 — Run-to-completion.** `DispatchMessage()` **must return before the loop
takes the next event**, so processing happens in **RTC steps that cannot be
interrupted by processing any other event**.

**3 — Inversion of control.** The loop **calls your code**.

> Backwards from everything so far: with an RTOS **you called the RTOS**; now
> **the system calls you.** **This is the essence of event-driven programming** —
> literally what "events drive the application" means.

### 7. Read the window procedure

Compare the four parameters against the **`MSG` structure** (in `WinUser.h`,
included from `Windows.h`) — they are its first four attributes. The renamings in
this project expose the concepts:

| Win32 | Renamed to | Because |
|---|---|---|
| `hwnd` | **`me`** | the wind proc is a **member function** of the window class (Lesson 29) |
| `message` | **`sig`** | it is **the signal** — what kind of event this is |
| `wParam`, `lParam` | — | **event parameters**, whose meaning depends on the signal |

Inside, the job is to work out **what kind of message** this is — via a
**`switch` on the integer signal**, with one `case` per message kind. The
symbolic names (`WM_CREATE` = 1, `WM_DESTROY` = 2, …) are listed in `WinUser.h`.

### 8. Notice four details

1. **`PostQuitMessage()`** in `WM_DESTROY` inserts `WM_QUIT` into the program's
   **own** queue, which terminates the loop — **an application asynchronously
   posting events to itself**.
2. **Two-way communication:** the wind proc sets a **`status`** (e.g.
   `WIN_HANDLED`) and **returns it to Windows**. `InvalidateRect()` is the same
   idea in the other direction — telling Windows the window needs repainting.
3. **`WM_PAINT`** is generated when Windows decides part or all of the window
   must be repainted; here it draws the current counter values.
4. **The counters are `static`** — and this matters:

> They **must outlive the many invocations and *returns* from the wind proc.**
> Defined as local automatic variables they would **go out of scope on every
> return.**

The counters are incremented in `WM_KEYDOWN` and `WM_MOUSEMOVE`, each followed by
`InvalidateRect()` — without it, the new value wouldn't be redrawn.

---

## Part C — The default handler

### 9. Run the program and take stock

The application has a title bar, and you can **resize, move, minimize, restore
and maximize** it — it looks and feels like any Windows application.

**Yet you only explicitly coded counting of key presses and mouse moves.**

### 10. Find out why

The `default:` case calls the **default window proc** supplied by Windows.

To appreciate it, **scan the hundreds of `WM_` signals in `WinUser.h`**. Most pass
through your wind proc, and **most are handled by the default proc** — leaving
you **blissfully unaware** of the complexity, needing to know only the handful
you actually handle.

### 11. Name the pattern

> Think of it as **layered in a hierarchy**: **your code is at the lowest level
> and gets the first shot at every event.** When it doesn't handle one, the event
> is **not ignored** — it passes **up** to the Windows system.

| Name | Emphasizes |
|---|---|
| **Ultimate Hook** | the ease of hooking your code to every event |
| **Programming by Difference** | that you only code the **differences** from default behaviour |

**In OOP terms:** the **Windows System is a base class** with hundreds of virtual
functions (one per signal); **applications are subclasses** overriding selected
ones in their wind procs.

---

## Part D — Prove that blocking ruins it

### 12. Write the sequential version

Goal: briefly blink an "LED" after any keypress. The natural place is the
**`WM_KEYDOWN`** case. Code it sequentially, as you would in an RTOS thread:

```c
case WM_KEYDOWN:
    ++wm_keydown;
    led = "ON";
    InvalidateRect(...);     /* ask for a repaint */
    Sleep(200);              /* Windows' equivalent of an RTOS delay() -- BLOCKS */
    led = "OFF";
    InvalidateRect(...);
    break;
```

There are no LEDs on a PC, so display the state as text. The `led` pointer must
be **`static`**, like the counters. Add it to the painted string in `WM_PAINT`.

### 13. Observe failure 1 — the LED never updates

Press a key once: the **keyboard counter increments, but the LED state does not
change.** Your code does **not** work as imagined.

### 14. Observe failure 2 — the application becomes a "pig"

Press several keys in quick succession: the program **freezes**, the counter
doesn't update, then after a considerable while **jumps all at once** by a big
increment. Add mouse wiggling and **both** counters freeze then jump together.

**Why:** the blocking `Sleep()` stops the wind proc returning quickly.

> **When the event loop spins too slowly, events accumulate in the queue.** Only
> when all the blocked messages are processed does the loop unclog and flush
> everything — hence the jumps.
>
> Windows programmers call such an application a **"pig"**. You don't want to be
> one.
>
> **Rule of thumb: anything taking more than about 100 ms should be broken into
> shorter pieces using events.**

### 15. Understand why the LED never updated

This reason is the deeper one:

> From the event-driven perspective, **every blocking call really means waiting
> for some event**, and unblocking means it occurred. But that event is
> **delivered in the middle of processing another event** (`WM_KEYDOWN`) — which
> **violates the run-to-completion semantics** the whole system assumes.

Concretely: **`InvalidateRect()` before the `Sleep()` has no effect**, because
the wind proc **never returns to Windows** at that point — so Windows has no
chance to send `WM_PAINT`. You never see the update.

> **Blocking is a bad idea in event-driven systems for two reasons: it clogs the
> event loop and destroys responsiveness to *all* events; and it violates
> run-to-completion.**
>
> **This is the most important takeaway: sequential programming and event-driven
> programming are two distinct paradigms that don't mix well — always keep them
> separate.**

---

## Part E — The event-driven solution

### 16. Replace blocking with a timer

Use the Windows facility designed for exactly this — a **timer** that generates a
**`WM_TIMER`** event a given number of milliseconds in the future:

```c
case WM_KEYDOWN:
    ++wm_keydown;
    led = "ON";
    InvalidateRect(...);
    SetTimer(me, TIMER_ID, 200, NULL);    /* and RETURN immediately */
    break;

case WM_TIMER:
    led = "OFF";
    InvalidateRect(...);
    KillTimer(me, wParam);   /* otherwise the timer keeps expiring periodically */
    break;
```

Note this case uses **`wParam`** — here it holds the **ID of the timer** that
generated the message.

### 17. Verify

- **Isolated keypresses:** the LED momentarily changes to RED after each one.
- **Bursts of keypresses while wiggling the mouse:** the LED status changes
  correctly **and both counters keep updating** — **the application stays
  responsive.**

---

## Exercises

1. **Measure the pig.** Time how long the blocking version takes to flush 20
   queued keypresses. Compare against 20 × 200 ms.
2. **Handle one more event.** Add a `WM_LBUTTONDOWN` case and count left clicks.
3. **Remove the default.** Delete the `default:` case (or return 0 instead) and
   list which standard behaviours you lose.
4. **Periodic timer.** Remove `KillTimer()` and describe what happens.
5. **State without `static`.** Make a counter automatic and explain what you
   observe.
6. **Post to yourself.** Use `PostMessage()` to send your application a custom
   message and handle it.
7. **Map the concepts.** For each of the three event-loop properties (queued
   asynchronous delivery, RTC, inversion of control), write down what the
   equivalent would be in an embedded system. (Lesson 34 gives the answers.)

---

## Self-check

- [ ] I can explain why two input sources break sequential code
- [ ] I found the "virtual function" pointer embedded in the window-class struct
- [ ] I can name the three key properties of the event loop
- [ ] I can define *signal* and *event parameters* and point to both in the code
- [ ] I know why the wind proc's counters must be `static`
- [ ] I can explain the Ultimate Hook in OOP terms
- [ ] I reproduced both blocking failures and can explain each one separately
- [ ] I replaced the blocking delay with a timer and confirmed responsiveness
