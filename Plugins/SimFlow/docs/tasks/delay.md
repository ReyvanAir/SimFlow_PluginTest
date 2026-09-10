# Delay task

**Class:** `USimFlowTask_Delay`
**Select via:** [Task node](../nodes/task.md) → **Task** dropdown → **Delay**

Waits a fixed number of seconds, then succeeds. Respects pause.

There's also a [Delay node](../nodes/delay.md), and for a plain pause in the graph
that's the better choice. Use the task form when you need something the node can't
do.

## How long it waits

| Field | Type | Default | Meaning |
|---|---|---|---|
| Duration | Float (s, min 0) | `1.0` | Base wait time. |
| Random Extra | Float (s, min 0) | `0.0` | Randomly adds up to this many seconds on top of `Duration`. |

Plus the [fields every task has](README.md#the-fields-every-task-has).

The actual wait is `Duration + random(0, Random Extra)`, rolled once when the task
starts. With both at 0 the task finishes almost immediately, effectively a
pass-through; with `Duration` at 0 and `Random Extra` set, you get a purely random
wait between zero and that value.

Pausing the flow stops the timer and resumes it where it left off — unless the base
`Tick While Paused` field is on, in which case the delay keeps counting through the
pause. That's almost never what you want.

## Node or task?

| Use the [Delay node](../nodes/delay.md) | Use the Delay task |
|---|---|
| A plain pause, which is the usual case | You want `Random Extra` |
| You want the graph to read clearly | You want it inside a [Parallel Group](parallel-group.md) |
| | You want a `Task Id`, an instruction, or scoring attached |
| | You want the [Task node](../nodes/task.md)'s abort condition around it |

## Varied ambient pacing

An idle scenario where a radio call arrives every 20 to 30 seconds, so it doesn't
feel scripted:

1. Add a [Task node](../nodes/task.md) with **Task** = **Delay**.
2. Set **Duration** to `20`.
3. Set **Random Extra** to `10`, making the wait 20–30 seconds.
4. Wire `Completed` into the radio-call task.
5. Wrap the pair in a [Loop node](../nodes/loop.md) with `Iterations` = `0` and a
   break condition, so the calls repeat until the scenario ends.

An abort condition on the Task node lets a real event cut the wait short: set
**Abort Condition** to **Event Was Raised** with your interrupt tag and
**Abort Result** to `Succeeded`.

## If the wait is wrong

A delay running longer than configured means `Random Extra` is set — it adds on top
of `Duration` rather than blending into it — or the flow was paused. A delay that
doesn't pause with the game has `Tick While Paused` on, or the component's
`Follow Game Pause` off.

If what you wanted was a timeout rather than a pause, use the
[Task node](../nodes/task.md)'s `Time Limit`, or race a delay against the work with
a [Join](../nodes/join.md) in **Wait For Any** mode. And if the graph is filling up
with Task nodes that only wait, that's what the [Delay node](../nodes/delay.md) is
for.

*Next: [Delay node](../nodes/delay.md) · [Task node](../nodes/task.md)*
