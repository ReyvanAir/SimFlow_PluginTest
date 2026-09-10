# Parallel node

**Class:** `USimFlowNode_Parallel`
**Add via:** right-click → **Flow Control → Parallel**
**Pins:** In → Out 0 … Out N

Fires every output at once, so several sections of the graph run at the same time.

Reach for it when things genuinely happen together: a countdown ticking while the
trainee works, ambient narration over a procedure, two independent sub-tasks. Pair
it with a [Join](join.md) when the branches need to come back together.

`Num Outputs` is the only field — an int from 2 to 16, default `2`, clamped if you
go outside that. Changing it rebuilds the pins, which are named `Out_0` … `Out_N`.
Reducing the count drops the removed pins and their links; links on the surviving
pins are kept.

## Unwired pins are skipped, not fired

The node snapshots which pins actually have links and fires only those. An unused
output costs nothing, and wiring only some of them is fine.

The flip side is that if you wire none of them, the node finishes without
triggering anything and that line of execution quietly ends.

Branches that never converge each run to their own end. If one of them reaches a
[Finish](finish.md) with `Stop Other Branches` on, the rest are killed wherever
they happen to be.

## Parallel is not threaded

Branches are interleaved on the game thread, tick by tick. "Parallel" here means
several parts of the graph are active at once, not that anything runs concurrently.
Don't rely on ordering within a single tick.

## A timer running alongside the work

The trainee performs a procedure while a 60-second countdown runs; whoever finishes
first ends the section.

1. Right-click → **Flow Control → Parallel**. Leave **Num Outputs** at `2`.
2. Wire **Out 0** into the procedure — a [Task node](task.md) running
   [Ordered Sequence](../tasks/ordered-sequence.md).
3. Wire **Out 1** into a [Delay node](delay.md) set to `60`.
4. Add a [Join](join.md) with **Mode** = **Wait For Any (Race)**.
5. Wire the end of the procedure into the Join's **In 0**, and the Delay's `Out`
   into **In 1**.
6. Wire the Join's `Out` onward.

```
              ┌────────────┐  Out 0 ──▶ Ordered Sequence ──┐   ┌──────────┐
   ───────────┤  Parallel  │                               ├──▶│   Join   ├──▶
              │            │  Out 1 ──▶ Delay 60s ─────────┘   │ Wait Any │
              └────────────┘                                   └──────────┘
```

Whichever branch arrives first wins, and the Join swallows the loser so the
downstream section cannot run twice.

## If only one branch runs

Only one branch running usually means the other pin isn't wired. The section *after*
the branches running twice means both reached it independently — put a
[Join](join.md) in **Wait For All** mode between them. A branch cut off mid-task
means something downstream hit a [Finish](finish.md) with `Stop Other Branches` on.

*Next: [Join node](join.md) · [Parallel Group task](../tasks/parallel-group.md)*
