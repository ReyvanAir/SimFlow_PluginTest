# Wait For Condition

**Class:** `USimFlowTask_WaitForCondition`
**Select via:** [Task node](../nodes/task.md) → **Task** dropdown → **Wait For Condition**

Blocks until a [condition](../conditions.md) becomes true.

Where [Wait For Event](wait-for-event.md) reacts to a discrete moment, this task
polls a continuous state: the player is holding the drill, valve rotation is past 90
degrees, the score is high enough. It can also require the condition to *hold* for a
while, which is how you say "stand still in the safe zone for three seconds".

## Fields

| Field | Type | Default | Meaning |
|---|---|---|---|
| Condition | Instanced [condition](../conditions.md) | *null* | The test to evaluate. |
| Check Interval | Float (s, min 0) | `0.1` | Seconds between evaluations. `0` evaluates every frame. |
| Required Hold Time | Float (s, min 0) | `0.0` | The condition must stay true this long before the task succeeds. |

Plus the [fields every task has](README.md#the-fields-every-task-has).

## An empty condition waits forever

An unset condition evaluates as false, so the task blocks indefinitely and nothing
is logged. [Wait For Event](wait-for-event.md) at least logs and passes through when
its tag is missing; this one doesn't. A task hanging with no log output is usually
an empty `Condition` slot, so check that first.

A condition that's already true when the task starts succeeds almost immediately —
after the first check, plus the hold time if one is set. Conditions are evaluated
from the first check rather than only on a change.

## How the interval and hold time interact

The condition is evaluated every `Check Interval` seconds. While it reads true, hold
time accumulates. The moment it reads false, the accumulated hold time resets to
zero.

With `Check Interval` at `0.1` and `Required Hold Time` at `3.0`, that means the
condition has to read true on roughly 30 consecutive checks, and a single false
reading starts the count over. A flickering condition paired with a hold time may
never complete at all.

Raise `Check Interval` for expensive conditions — a distance test every frame is
cheap, a condition that scans actors is not. But a long interval and a short hold
time between them can miss brief windows entirely.

## Stand in the safe zone for three seconds

The trainee has to reach the muster point and stay there briefly.

1. Add a [Task node](../nodes/task.md) with **Task** = **Wait For Condition**.
2. Set **Condition** to **Player Near Location**, with the muster point's world
   coordinates, a radius of `200`, and Ignore Z on since HMD height varies.
3. Set **Required Hold Time** to `3.0`.
4. Leave **Check Interval** at `0.1`.
5. Set **Instruction** to `Go to the muster point and wait`.
6. On the Task node, set **Time Limit** to `60` and wire `Timed Out` to a hint.

Stepping out of the radius resets the three seconds. That's intended — they have to
actually stay put.

To wait on a flag from another system instead, use **Blackboard Compare** with key
`DrillReady`, operation `==`, value `Bool` `true`, and have your Blueprint call
`Set Bool` on the flow's blackboard when it's ready.

## If the task hangs

**The task hangs with nothing logged.** The `Condition` slot is empty.

**The hold time never completes.** The condition is flickering. Widen the tolerance
with a larger radius or `Ignore Z`, or lower the hold time.

**Performance drops during this task.** `Check Interval` is `0` with an expensive
condition.

**A brief event gets missed.** `Check Interval` is too long to catch a short-lived
state. Polling is the wrong tool for an instant — use
[Wait For Event](wait-for-event.md).

*Next: [Conditions](../conditions.md) · [Wait For Event](wait-for-event.md) ·
[Go To Location](go-to-location.md)*
