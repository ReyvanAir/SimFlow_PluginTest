# Wait For Event

**Class:** `USimFlowTask_WaitForEvent`
**Select via:** [Task node](../nodes/task.md) → **Task** dropdown → **Wait For Event**

Blocks until an event tag is raised on the flow, optionally by the right object.

This is the main way the world advances a flow. A button, a grabbable object or an
animation notify raises a tag, and this task is waiting for it.

`Expected Payload` is what makes it more than a doorbell. Ten buttons can broadcast
the same tag while knowing nothing about the procedure, and only the intended one
satisfies the task. [Events](../events.md) covers the broadcasting side.

## Fields

Under **Event**:

| Field | Type | Default | Meaning |
|---|---|---|---|
| Event Tag | Gameplay Tag | *empty* | The tag this task listens for. |
| Match Child Tags | Bool | `true` | Listening for `Sim.Grab` also accepts `Sim.Grab.Extinguisher`. |
| Accept Already Raised | Bool | `false` | If the tag was raised earlier in this run, finish straight away. |

Under **Payload**:

| Field | Type | Default | Meaning |
|---|---|---|---|
| Expected Payload | [Actor Query](../actor-query.md) | *empty* | Which object the event has to be about. |
| Mismatch Policy | Enum | `Count Mistake (Keep Waiting)` | What happens when the tag is right but the object isn't. |
| Payload To Blackboard Key | Name | `None` | Stores the accepted payload, so later tasks can refer to "the thing they picked". |

Plus the [fields every task has](README.md#the-fields-every-task-has).

An empty **Event Tag** logs *"WaitForEvent task has no tag set - finishing
immediately"* and finishes with `Succeeded` — a no-op pass-through rather than a
failure. An empty **Expected Payload** means the payload isn't checked at all, so
any sender satisfies the task. That's a legitimate "don't care" and often exactly
what you want.

If the tag is never raised, the task waits forever. Set a `Time Limit` on the
[Task node](../nodes/task.md) while developing.

## Why a payload check disables Accept Already Raised

Set both and the already-raised shortcut is ignored, with a Verbose note in the log.

The reason is that a past event kept only its tag — the payload wasn't retained.
Honouring "already raised" when there's a payload check would let the wrong object
satisfy the task silently, so the task waits for a fresh event instead.

## What counts as a payload

An Actor Component is unwrapped to its owning actor and graded normally, so a button
Blueprint sending the pressed component works fine.

A UMG widget does not. `UUserWidget` is neither an Actor nor an Actor Component, so
it scores `No Match` and logs a warning. Broadcast from the owning actor with
`Payload = self` instead.

## How a matching event is handled

The tag is compared first — `Matches Tag` when `Match Child Tags` is on, exact
equality otherwise — and a non-match is ignored silently.

If `Expected Payload` is set, the payload is then graded by the
[Actor Query](../actor-query.md). An `Exact` match carries on; anything else fires
`On Payload Rejected`, builds a mistake description like "Interacted with Wrench -
expected Item.Extinguisher.Foam", applies the **Mismatch Policy**, and does *not*
complete the task.

Once a payload is accepted it's stored to `Payload To Blackboard Key` if you set
one, and the task finishes with `Succeeded`.

The three mismatch policies are **Ignore (Keep Waiting)**, which stays silent;
**Count Mistake (Keep Waiting)**, the default, which records a
`SimFlow.Mistake.WrongTarget` mistake and carries on waiting; and **Count Mistake
And Fail Task**, which records it and drives the `Failed` pin.

`On Payload Rejected` gives you the payload and the match quality, which is enough
to sound a buzzer, outline the object red, or give a *different* hint for a near
miss than for something completely unrelated.

## The right valve, among five

The trainee must turn valve 3. All five valves broadcast the same tag.

1. Add a [SimFlow Identity](../identity.md) to the valve Blueprint, and on the level
   instances set Identity Tags to `Control.Valve.One` through `Control.Valve.Five`.
2. In the valve Blueprint, when it's turned, call **Broadcast Flow Event** with
   Event Tag `SimFlow.Event.Interact` and Payload `self`.
3. Add a [Task node](../nodes/task.md) and set **Task** to **Wait For Event**.
4. **Event Tag** = `SimFlow.Event.Interact`.
5. **Expected Payload → Required Tags** = `Control.Valve.Three`.
6. **Mismatch Policy** = **Count Mistake (Keep Waiting)**, so the trainee can
   self-correct.
7. Set **Payload To Blackboard Key** to `LastValve` if a later task needs to refer
   back to it.
8. Set **Instruction** to `Turn valve 3`.
9. Bind **On Payload Rejected** to flash the correct valve after a wrong attempt.

None of the five valves knows anything about the exercise. Moving the answer to
valve 4 is a one-field change in the flow asset.

## When it misbehaves

**The task completed instantly and nothing happened.** `Event Tag` is empty, which
finishes immediately by design.

**The task never fires though the event is broadcast.** The tags don't match. Check
`Match Child Tags`, and confirm the broadcast actually runs with a print node.

**A warning says the payload is "neither an Actor nor an ActorComponent".** You
broadcast `self` from a UMG widget graph. Broadcast from the owning actor instead —
this is the most common first bug.

**Any object satisfies the task.** `Expected Payload` is empty.

**Accept Already Raised does nothing.** You also set an `Expected Payload`, which
disables it.

**The flow hangs here forever.** Nothing raises the tag. Add a `Time Limit` on the
[Task node](../nodes/task.md) and wire `Timed Out` to a hint.

**Every flow in the level reacts.** `Broadcast Flow Event` reaches all running
flows; use `Send Flow Event` with a `Flow Save Id` to target one.

*Next: [Events](../events.md) · [Actor Query](../actor-query.md) ·
[Ordered Sequence](ordered-sequence.md)*
