# Delay node

**Class:** `USimFlowNode_Delay`
**Add via:** right-click → **Flow Control → Delay**
**Pins:** In → Out

Waits, then continues. It respects pause, so a paused flow doesn't advance the
timer.

Use it for pacing: a beat before narration, a gap between stages, or as the timer
arm of a race (see [Join](join.md)). The only field is **Duration**, a float in
seconds with a minimum of 0 and a default of `1.0`.

Set it to `0` and the node completes on the next tick, which makes it a one-frame
pass-through. Pause the flow mid-wait and the timer stops, resuming where it left
off. Leave `Out` unwired and the delay runs, then execution ends there.

## Node or task?

There is also a [Delay task](../tasks/delay.md). They wait the same way, so the
choice is about what else you need.

| Use the node | Use the task |
|---|---|
| A plain pause in the graph — the usual choice | You want a random extra amount on top (`Random Extra`) |
| You want the graph to stay readable | You want it inside a [Parallel Group](../tasks/parallel-group.md) |
| | You want a timeout or abort condition around it via the [Task node](task.md) |

The node is lighter and reads better in a graph. Reach for the task only when you
need something from the right-hand column.

## A beat before the next instruction

To let a narration line finish before the next task appears, drop a Delay set to
`2.5` between the narration task and the next [Task node](task.md).

```
   Task (narration) ──▶ Delay 2.5s ──▶ Task (next step)
```

Because the delay respects pause, a trainee who pauses mid-narration doesn't lose
the beat.

## If the timing looks wrong

A delay that seems to run long usually means the flow was paused, or the game was
paused and the component's `Follow Game Pause` is on, which it is by default. A flow
that stops after the delay has an unwired `Out`.

If what you actually wanted was a timeout rather than a pause, use the
[Task node](task.md)'s `Time Limit`, or race a Delay against the work with a
[Join](join.md) in **Wait For Any** mode.

*Next: [Delay task](../tasks/delay.md) · [Join node](join.md)*
