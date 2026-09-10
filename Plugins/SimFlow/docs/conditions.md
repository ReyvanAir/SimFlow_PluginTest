# Conditions

**Base class:** `USimFlowCondition`
**Used by:** [Branch](nodes/branch.md) cases, [Loop](nodes/loop.md) break conditions, [Task node](nodes/task.md) abort conditions, [Wait For Condition](tasks/wait-for-condition.md)

A condition is a reusable true/false test about the current run.

They're instanced sub-objects rather than assets, so you don't create one and
reference it. You pick a condition class from a dropdown in the Details panel and
configure it inline, wherever a condition is asked for.

Every condition has an **Invert** field (bool, default `false`) that flips the
result, which saves you needing a NOT wrapper. It's applied *after* the condition
evaluates, composites included — so an inverted **All Of** means "not all of them",
not "none of them".

## Blackboard Compare

Compares a [blackboard](blackboard.md) key against a literal value. The workhorse.

| Field | Type | Default | Meaning |
|---|---|---|---|
| Key | Name | `None` | Which key to read |
| Operation | Enum | `==` | `==`, `!=`, `<`, `<=`, `>`, `>=` |
| Value | SimFlow Value | Type `None` | The literal to compare against |
| Result When Key Missing | Bool | `false` | What to return when the key doesn't exist |

When the key is missing the comparison isn't attempted at all — the condition just
returns `Result When Key Missing`. That's the setting to change when a check should
pass on the first run, before anything has written the key.

Values coerce rather than fail, so a String `"5"` compares equal to an Int `5`. See
[type coercion](blackboard.md#type-coercion).

## Score Threshold

Shorthand for a compare against the well-known `Score` key. **Operation** defaults
to `>=` and **Threshold** to `100.0`.

## Last Task Result Is

True when the most recently finished task ended with the given result.
**Expected Result** defaults to `Succeeded`, with `Failed`, `Skipped`, `Timed Out`
and `Aborted` also available.

Useful on a Branch immediately after a Task node, when you want one branch to handle
several failure modes together.

## Elapsed Time

True when the flow has been running longer or shorter than a given time.
**Operation** defaults to `>=` and **Seconds** to `60.0`.

It measures whole-flow elapsed time, not time on the current task. For a per-task
limit, use the [Task node](nodes/task.md)'s `Time Limit`.

## Event Was Raised

True when an **Event Tag** has been raised on this flow at any point in the run.

This is a "has it ever happened" test against the run's event history, not a live
wait. To block until an event arrives, use
[Wait For Event](tasks/wait-for-event.md).

## All Of (AND) and Any Of (OR)

Logical composites over a **Conditions** array of child conditions. Add children
with the **+** button; each gets its own class dropdown, so they nest as deep as you
like.

An empty list is a trap. Standard logic makes an empty AND true and an empty OR
false, so an **All Of** you added but never filled in will pass every single time.
If a composite is behaving oddly, check it actually has children.

## Constant

Always returns the configured **Value** (bool, default `true`). Useful as a
placeholder while blocking out a flow, and as an explicit "always take this branch"
case.

## Player Near Location

True when the player pawn is within a radius of a world location. Very common in VR
flows.

| Field | Type | Default | Meaning |
|---|---|---|---|
| Location | Vector | `0,0,0` | The world location |
| Radius | Float (cm, min 1) | `150.0` | How close counts |
| Location From Blackboard Key | Name | `None` | When set, uses this key's vector instead of `Location` |
| Ignore Z | Bool | `true` | Ignore the vertical axis, since HMD height varies |

## Empty conditions and empty lists

A Branch case with no condition is treated as false, so that case never fires. This
is the one that bites: a case you added but never configured fails silently, and
execution falls through to `Default`.

The rest of the empty cases follow the same logic. A Blackboard Compare with `Key`
set to `None` looks up a key called `None`, almost certainly doesn't find it, and
returns `Result When Key Missing`. An Event Was Raised with no tag is false, because
an invalid tag was never raised. An **All Of** with no children is true and an
**Any Of** with no children is false. Player Near Location with no player pawn is
false.

## Pass or remediate

Sending trainees who scored under 70 to a remediation section at the end of an
assessment:

1. Add a [Branch](nodes/branch.md) node after the last task.
2. Add one case, labelled `Needs remediation`.
3. Set its **Condition** to **Score Threshold**, operation `<`, threshold `70`.
4. Wire that case's pin to the remediation section.
5. Wire **Default** to the pass section.

To require both a score and no more than two mistakes, use **All Of** with two
children — **Score Threshold** at `>=` `70`, and **Blackboard Compare** on
`Mistakes` at `<=` Int `2` — and put that on the *pass* case rather than the
remediation one.

## Writing your own

Conditions are `Blueprintable`. Create a Blueprint Class from **SimFlow Condition**,
override the **Evaluate** event, and return your answer.

It's worth also overriding **Get Condition Description** to return a short string.
It's shown on graph nodes and in the debug HUD, and it makes a graph far easier to
read at a glance.

Inside the Blueprint, `Cached Instance` gives you the running
[flow instance](glossary.md) for world context and blackboard access, and
`Get Blackboard From` is a convenience accessor. Your new class appears in every
condition dropdown automatically.

## If a test gives the wrong answer

**A branch case never fires.** Its condition slot is empty, and an unset condition
is false.

**An All Of composite always passes.** It has no children.

**A comparison never becomes true on the first run.** The key doesn't exist yet, so
the condition returns `Result When Key Missing`. Either seed the key with a
[Set Blackboard Value](nodes/set-blackboard.md) node at the start of the flow, or
flip that setting.

**Elapsed Time fires far earlier than expected.** It measures the whole flow's
runtime, not the current task's.

**An abort condition fires constantly.** Abort conditions are evaluated every frame
while the task runs, so make sure the test isn't already true the moment the task
starts.

*Next: [Branch node](nodes/branch.md) · [Blackboard](blackboard.md) ·
[Wait For Condition](tasks/wait-for-condition.md)*
