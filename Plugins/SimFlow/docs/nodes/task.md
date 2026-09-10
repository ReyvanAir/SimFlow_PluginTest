# Task node

**Class:** `USimFlowNode_Task`
**Add via:** right-click → **Tasks → Task**
**Pins:** In → Completed, Failed, Skipped, Timed Out

Runs a single [task](../tasks/README.md). It's the node designers reach for most,
and the bridge between the graph, which decides where execution goes, and the task,
which does the actual work.

That division is worth stating plainly: the node handles retry, timeout, early-out
and routing; the task handles the work. It's why every task gets timeout and retry
behaviour for free without implementing any of it.

## Fields

| Field | Type | Default | Meaning |
|---|---|---|---|
| Task | Instanced task | *null* | The task to run. Pick a class from the dropdown and its own settings appear inline. |
| Time Limit | Float (s, min 0) | `0.0` | Seconds before the task times out and leaves via `Timed Out`. `0` means no limit. |
| Auto Retry On Failure | Bool | `false` | Restart the task automatically when it fails. |
| Auto Retry Limit | Int (min 1) | `1` | How many automatic retries. Only shown with auto retry on. |
| Auto Retry On Timeout | Bool | `false` | Also restart on a timeout. Only shown with auto retry on. |
| Fallback To Completed | Bool | `true` | Unwired `Failed` / `Skipped` / `Timed Out` pins fall through to `Completed`. |

Under **Early Out** there are two more: an **Abort Condition**
([condition](../conditions.md), null by default) that ends the task early and is
evaluated every frame, and an **Abort Result** (default `Failed`) choosing which pin
that abort leaves through.

## The four output pins

`Completed` is taken when the task succeeded, `Failed` when it failed or was failed
by `Fail Current Task`, `Skipped` when it was skipped, and `Timed Out` when
`Time Limit` elapsed.

While `Fallback To Completed` is on, anything left unwired falls back to
`Completed`. That's what keeps simple linear flows tidy — wire `Completed`, ignore
the rest.

Turn the fallback off when a failure must not be allowed to look like a success.
With it off, an unwired `Failed` pin just ends that line of execution.

## An empty Task node passes through

Leave the `Task` slot empty and the node logs *"Task node has no task assigned -
passing through"* and triggers `Completed`. It does not fail.

This is deliberate. A blocked-out flow full of empty Task nodes still runs end to
end, so you can build and test the shape of a scenario before writing a single task.
The cost is that a genuinely forgotten task slot looks like success, so it's worth
knowing which of the two you're looking at.

Related quiet cases: a `Time Limit` of 0 means a task that never finishes blocks the
flow forever. An abort condition that's already true when the task starts aborts it
on the first frame, since conditions are evaluated from frame one. And with every
output pin unwired, the fallback takes `Completed` into nothing and the flow ends
there.

## Retry, skip and fail need both layers

The node and the task each get a say, and both have to allow it. The node holds
`Auto Retry On Failure`, `Auto Retry Limit` and `Auto Retry On Timeout`; the task
holds `Allow Retry`, `Max Retries` and `Allow Skip` (see
[task fields](../tasks/README.md#the-fields-every-task-has)). Turn on auto-retry at
the node while the task's own `Allow Retry` is off and nothing retries — the task's
rule wins.

Automatic retry happens inside the node: when the task fails, the node restarts it
without leaving through any pin. Only once the retries are used up does execution
leave via `Failed`.

Manual retry, skip and fail come from the outside instead — `Retry Current Task`,
`Skip Current Task` and `Fail Current Task` on the
[SimFlow Component](../simflow-component.md), usually wired to an instructor panel.

## A timed step that retries once

The trainee has 30 seconds to press the start button, with one automatic retry
before routing to a hint.

1. Right-click → **Tasks → Task**.
2. Set **Task** to [Wait For Event](../tasks/wait-for-event.md) with Event Tag
   `SimFlow.Event.ButtonPressed`.
3. Set **Time Limit** to `30`.
4. Tick **Auto Retry On Failure**, set **Auto Retry Limit** to `1`, and tick
   **Auto Retry On Timeout** so a timeout retries too.
5. Wire **Completed** onward.
6. Wire **Timed Out** to a hint section. It's only reached once the retry is used up.
7. Leave **Failed** and **Skipped** unwired to fall through to `Completed`.

```
              ┌────────────────────────┐
   Start ─────┤In   Task               │Completed──▶ next step
              │     (Wait For Event)   │Failed
              │     Time Limit 30s     │Skipped
              │     Auto Retry 1       │Timed Out──▶ hint
              └────────────────────────┘
```

Add an **Abort Condition** of
[Player Near Location](../conditions.md#player-near-location) with `Invert` on and
the task will also abort if the trainee walks away.

## When it misbehaves

**The node completed instantly and nothing happened.** The `Task` slot is empty.

**A failure looked like a success.** `Fallback To Completed` is on with `Failed`
unwired. Wire it, or turn the fallback off.

**The flow hangs on one task forever.** `Time Limit` is `0` and the task is waiting
on something that never arrives. Set a limit while developing, even a generous one.

**Auto retry does nothing.** Either the task's own `Allow Retry` is off, or the
result was a timeout and `Auto Retry On Timeout` isn't ticked.

**The task aborts immediately.** The abort condition was already true on frame one.

**`Timed Out` never fires though the task is stuck.** Auto retry with
`Auto Retry On Timeout` keeps restarting it; check `Auto Retry Limit`.

*Next: [Task reference](../tasks/README.md) · [Conditions](../conditions.md) ·
[SimFlow Component](../simflow-component.md)*
