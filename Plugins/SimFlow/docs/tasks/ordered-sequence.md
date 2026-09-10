# Ordered Sequence

**Class:** `USimFlowTask_OrderedSequence`
**Select via:** [Task node](../nodes/task.md) → **Task** dropdown → **Ordered Sequence**

"Press these three buttons, in this order."

Every button broadcasts the same tag with itself as the payload and knows nothing
about the procedure. This task holds the order.

The reason for a dedicated task, rather than a chain of
[Wait For Event](wait-for-event.md) tasks, is that acting out of turn becomes a
first-class outcome instead of something you notice by accident. A chain of separate
tasks would simply ignore a premature press. This one can tell the trainee they did
the right thing at the wrong moment.

## Fields

Under **Sequence**:

| Field | Type | Default | Meaning |
|---|---|---|---|
| Event Tag | Gameplay Tag | *empty* | The event every candidate raises, e.g. `SimFlow.Event.ButtonPressed`. |
| Match Child Tags | Bool | `true` | Also accept child tags. |
| Steps | Array of steps | *empty* | The expected order. |
| Out Of Order Policy | Enum | `Count Mistake (Stay On Step)` | What happens when the trainee acts out of turn. |
| Unlisted Input Is Mistake | Bool | `false` | Treat input that isn't part of the sequence at all as an out-of-order mistake. |
| Step Blackboard Key | Name | `None` | Publishes the current step index, for a progress widget. |

Each step holds a **Target** ([Actor Query](../actor-query.md), empty) naming the
object that step expects — the second valve, the green button — and an
**Instruction** (multi-line text, empty) shown while it's the current step, readable
with `Get Current Step Instruction`.

Plus the [fields every task has](README.md#the-fields-every-task-has).

## No steps succeeds, no tag fails

An empty `Steps` array logs *"has no steps - finishing immediately"* and finishes
with `Succeeded`. An empty `Event Tag` logs *"has no event tag set - failing"* and
finishes with `Failed`.

The asymmetry is deliberate, because they're different kinds of mistake. An empty
step list is a flow nobody has filled in yet. A missing tag is a task that could
never have worked.

A step whose `Target` is unset is a third case, and the worst of them: an unset
query matches nothing, so the task hangs on that step with no complaint.

Two steps may share a target — the first press satisfies step 1, the second is
judged against step 2 and advances — so repeating an object in a sequence works
fine.

## How input is judged

For every event with a matching tag, the task asks:

1. **Is it the expected step?** An `Exact` match for the current step's `Target`
   fires `On Step Completed` and advances. If that was the last step, the task
   finishes with `Succeeded`.
2. **Is it a different step in the sequence?** Every other step's target is checked.
   This is what decides severity.
3. **Is it unlisted, with `Unlisted Input Is Mistake` off?** Ignored silently. That's
   the default, and it's right when unrelated props share the tag.
4. Otherwise the **Out Of Order Policy** applies.

A payload that's a widget scores `No Match` and logs a warning — see
[payloads that are not actors](../actor-query.md#payloads-that-are-not-actors).

### Severity is graded

When a mistake is recorded, its severity reflects what kind of error it was. Using a
step of the procedure at the wrong moment is **Related (Near Miss)**: they know the
procedure, they got the order wrong. Grabbing a prop that was never part of it is
**No Match**, a different kind of error entirely.

Either way it's recorded as `SimFlow.Mistake.WrongOrder` with a description like
*"Step 2: used Valve One - expected Control.Valve.Three"*, and the `WrongAttempts`
[blackboard](../blackboard.md) key is incremented.

## Out Of Order Policy

| Policy | Behaviour | Use for |
|---|---|---|
| Ignore | Nothing recorded, stay on the step | Free exploration |
| Count Mistake (Stay On Step) | Record and stay put. The default | Training — let them find it |
| Count Mistake And Restart | Record and go back to step one | Procedures where order is the whole point |
| Count Mistake And Fail Task | Record and fail, driving the `Failed` pin | Assessment |

Restart is the strict one. A single slip on step 5 sends the trainee back to step 1,
which is correct for a safety procedure where a wrong order invalidates everything,
and merely infuriating everywhere else.

## Delegates and accessors

| Member | Signature | Use for |
|---|---|---|
| On Step Completed | (Step Index, Target) | Tick off a checklist item |
| On Wrong Input | (Payload, Expected Step Index) | Feedback on a wrong press |
| Get Current Step Index | → Int | Progress UI. 0-based. |
| Get Current Step Instruction | → Text | The current step's instruction, for tutorial UI |

`Step Blackboard Key` publishes the same index to the blackboard, which is the
easier route for a widget already reading blackboard values.

## A three-valve startup procedure

The trainee must open valve A, then B, then C. Out-of-order attempts are recorded
but shouldn't end the exercise.

Setting up the world:

1. Add the tags `Control.Valve.A`, `Control.Valve.B` and `Control.Valve.C`.
2. Add a [SimFlow Identity](../identity.md) to the valve Blueprint, then set each
   level instance's tag and a Display Name (`Valve A` and so on).
3. In the valve Blueprint, when turned, call **Broadcast Flow Event** with Event Tag
   `SimFlow.Event.Interact` and Payload `self`.

Setting up the task:

4. Add a [Task node](../nodes/task.md) with **Task** = **Ordered Sequence**.
5. **Event Tag** = `SimFlow.Event.Interact`.
6. Under **Steps**, click **+** three times — step 0 targets `Control.Valve.A` with
   the instruction `Open valve A`, step 1 `Control.Valve.B` / `Open valve B`, step 2
   `Control.Valve.C` / `Open valve C`.
7. **Out Of Order Policy** = **Count Mistake (Stay On Step)**.
8. Leave **Unlisted Input Is Mistake** off, since other props share the interact tag.
9. Set **Step Blackboard Key** to `CurrentStep`.
10. Set **Score On Success** to `25`.

And the UI: bind **On Step Completed** to tick off a checklist entry, and read
`Get Current Step Instruction` to drive the on-screen prompt.

Turning valve C first now records a **Related** mistake — the trainee knows the
procedure but not the order — while turning an unrelated wheel is ignored.

> **Screenshot needed:** the Details panel showing the Steps array expanded with
> three entries, each with its Target query and Instruction.

## When it misbehaves

**The task completed instantly.** `Steps` is empty, which succeeds by design.

**The task failed instantly.** `Event Tag` is empty.

**The task hangs on one step.** That step's `Target` query is empty, or doesn't match
the object.

**Nothing is ever recorded as out of order.** The wrong objects aren't in the
sequence and `Unlisted Input Is Mistake` is off, or the policy is `Ignore`.

**Every unrelated prop counts as a mistake.** `Unlisted Input Is Mistake` is on and
other actors share the event tag. Turn it off, or use a dedicated tag.

**The progress widget is off by one.** `Get Current Step Index` is 0-based.

**The sequence keeps restarting.** The policy is **Count Mistake And Restart**.

*Next: [Wait For Event](wait-for-event.md) · [Events](../events.md) ·
[Actor Query](../actor-query.md)*
