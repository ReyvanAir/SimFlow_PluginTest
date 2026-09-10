# Events

**Statics:** `USimFlowStatics`
**Consumed by:** [Wait For Event](tasks/wait-for-event.md), [Ordered Sequence](tasks/ordered-sequence.md), [Event Was Raised](conditions.md#event-was-raised)

Events are how the world talks to a running flow.

A grabbable object, a button, an animation notify or a zone raises a gameplay tag,
optionally carrying a payload — the object the event is about. Tasks listening for
that tag wake up and decide whether the payload was the right object.

The intent is that the level stays neutral and the flow asset holds the answer. Ten
buttons can broadcast the same tag, knowing nothing about the procedure, while the
flow decides which one counted.

## Raising an event

| Function | Reaches | Use when |
|---|---|---|
| Broadcast Flow Event (Tag, Payload) | Every running flow | The usual case — a prop doesn't know which flow is running |
| Send Flow Event (Flow Save Id, Tag, Payload) | One specific flow | Several flows are running and only one should hear it |
| Send Event on a component (Tag, Payload) | That component's flow | You already have the component reference |

All three are Blueprint-callable. The two statics need a world context, which
Blueprint supplies for you.

### Always pass the owning actor as the payload

```
Broadcast Flow Event
    Event Tag = SimFlow.Event.ButtonPressed
    Payload   = self          <- from the ACTOR's graph, not a widget's
```

A payload is graded by an [Actor Query](actor-query.md), which accepts an Actor, or
an Actor Component that it unwraps to its owner. Anything else scores `No Match` and
logs a warning.

The most common first bug is broadcasting `self` from inside a UMG widget graph. A
`UUserWidget` is neither an Actor nor an Actor Component, so it can never satisfy a
payload check. Broadcast from the actor that owns the widget instead — see
[payloads that are not actors](actor-query.md#payloads-that-are-not-actors).

## Tags shipped with the plugin

These are native tags, available with no setup.

| Tag | Meaning |
|---|---|
| `SimFlow.Event` | Parent of all built-in event tags |
| `SimFlow.Event.Generic` | General-purpose event |
| `SimFlow.Event.Interact` | The trainee interacted with something |
| `SimFlow.Event.Grab` | Object picked up |
| `SimFlow.Event.Release` | Object let go |
| `SimFlow.Event.ButtonPressed` | Button press |
| `SimFlow.Event.Placed` | Raised by a [zone](zones.md) when an object settles (opt-in) |
| `SimFlow.Event.Removed` | Raised by a zone when an object leaves (opt-in) |

The two zone tags only fire when that zone's **Broadcast Flow Events** is on.

Add your own in **Project Settings → Project → Gameplay Tags**, or inline from any
tag picker. Keep them in your own namespace rather than under `SimFlow.` — that
hierarchy belongs to the plugin, and a future version may add to it.

## Child tag matching

Listening tasks default to `Match Child Tags = true`, so a task listening for
`Sim.Grab` also accepts `Sim.Grab.Extinguisher`.

That lets a hierarchy carry the specificity for you:

```
Sim.Grab                  <- a task listening here hears all three
Sim.Grab.Extinguisher
Sim.Grab.Wrench
```

Turn it off when you want an exact-tag-only match.

## Event history

The flow instance remembers which tags have been raised during the run, and two
things read that history.
[Event Was Raised](conditions.md#event-was-raised) is a condition asking "has this
ever happened?", and [Wait For Event](tasks/wait-for-event.md)'s **Accept Already
Raised** finishes the task immediately if the tag fired earlier in the run.

Setting an `Expected Payload` disables Accept Already Raised, and the task logs a
Verbose note saying so. A past event kept only its tag — there's no payload left to
check — so honouring it would let the wrong object satisfy the task. The safe
behaviour is to wait for a fresh event, so that's what it does.

## A button that advances the flow

Making a physical VR button complete the current task:

1. Open the button actor's Blueprint (`BP_StartButton`).
2. On whatever fires when it's pressed, add **Broadcast Flow Event** with Event Tag
   `SimFlow.Event.ButtonPressed` and Payload `self`.
3. Add a **SimFlow Identity** component to the button and tag it, say
   `Control.Button.Start`. See [Identity](identity.md).
4. In the flow, add a Task node running [Wait For Event](tasks/wait-for-event.md)
   with Event Tag `SimFlow.Event.ButtonPressed` and Expected Payload → Required Tags
   of `Control.Button.Start`.

Every other button can now broadcast the same tag harmlessly. Only the one tagged
`Control.Button.Start` satisfies this task.

## If an event never lands

**The task never fires.** The tag doesn't match. Check `Match Child Tags`, and
confirm the broadcast actually runs with a print node — a tag mismatch is silent.

**A warning about a payload that is not an Actor or ActorComponent.** You broadcast
from a widget. Broadcast from the owning actor with `Payload = self`.

**Every flow reacts to one prop.** `Broadcast Flow Event` reaches all running flows.
Use `Send Flow Event` with a `Flow Save Id` to target one.

**Accept Already Raised isn't working.** An `Expected Payload` is set, which disables
it.

**The event fires but the wrong object satisfies the task.** `Expected Payload` is
empty, so any sender counts.

**A client's event does nothing in multiplayer.** Events have to reach the
authority. Use the component's `Send Event`, which forwards from clients when the
PlayerController has a **SimFlow Player Component**.

*Next: [Wait For Event](tasks/wait-for-event.md) · [Actor Query](actor-query.md) ·
[SimFlow Component](simflow-component.md)*
