# Log Message

**Class:** `USimFlowTask_Log`
**Select via:** [Task node](../nodes/task.md) → **Task** dropdown → **Log Message**

Prints a message, then succeeds immediately.

Where it earns its keep is blocking out a flow before the real tasks exist. Fill a
graph with Log Message tasks, run it end to end, confirm the branching and routing
are right, then replace them one at a time with real work.

## What it prints

| Field | Type | Default | Meaning |
|---|---|---|---|
| Message | String | `SimFlow` | The text to print. |
| Print To Screen | Bool | `true` | Also print on screen, not only to the log. |
| Screen Duration | Float (s, min 0) | `3.0` | How long the on-screen message stays. |

Plus the [fields every task has](README.md#the-fields-every-task-has).

An empty message prints an empty line — harmless, but not much use. With
`Print To Screen` off it goes to the output log only, and with `Screen Duration` at
`0` the on-screen message appears and disappears in the same frame, which amounts to
the same thing.

The task always finishes with `Succeeded` on the frame it starts. It never fails and
never blocks.

## Blocking out a flow before building it

To prove the shape of a five-stage exercise before writing any real tasks:

1. Create the flow with [Start](../nodes/start.md), five
   [Task nodes](../nodes/task.md), whichever [Branch](../nodes/branch.md) you plan
   to use, and a [Finish](../nodes/finish.md).
2. Set every Task node's **Task** to **Log Message**, with messages `Stage 1`,
   `Stage 2` and so on.
3. Press Play and watch them print in order.
4. Confirm the branching works by seeding the blackboard with a
   [Set Blackboard Value](../nodes/set-blackboard.md) node.
5. Replace each Log Message with the real task, one at a time.

An empty [Task node](../nodes/task.md) also passes straight through, so you could
leave the `Task` slot empty instead — but then you don't know *which* node you just
passed through, which is the whole reason for doing this.

## If nothing prints

**Nothing appears on screen.** `Print To Screen` is off, or `Screen Duration` is
`0`. Check the Output Log.

**The messages all appear at once.** They're meant to. Log Message finishes on the
frame it starts, so a chain of them runs in a single tick. Insert
[Delay nodes](../nodes/delay.md) to watch the flow at a readable pace.

**You wanted the message during a wait, not instead of it.** Set the real task's
**Instruction** field — that's what tutorial UI reads. See
[task fields](README.md#the-fields-every-task-has).

*Next: [Task node](../nodes/task.md) · [Delay node](../nodes/delay.md) ·
[Troubleshooting](../troubleshooting.md)*
