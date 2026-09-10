# Random Branch node

**Class:** `USimFlowNode_RandomBranch`
**Add via:** right-click → **Flow Control → Random Branch**
**Pins:** In → Out 0 … Out N

Picks one output at random, optionally weighted.

Use it to vary a scenario between runs — which fault occurs, which room the task
happens in, which distractor appears — so repeat trainees can't just memorise the
sequence.

## Fields

| Field | Type | Default | Meaning |
|---|---|---|---|
| Num Outputs | Int (2–16) | `2` | How many output pins. |
| Weights | Array of Float | *empty* | Per-output weights. A missing entry counts as 1. |
| Avoid Repeats | Bool | `false` | Never pick the same output twice running within one run. |

Pins are named `Out_0` … `Out_N`, and values outside 2–16 are clamped.

Weights are relative, not percentages. `45/45/10` and `9/9/2` behave identically,
and they don't need to sum to anything in particular.

## How the pick is made

Each output's weight comes from `Weights` by index, or 1 if there's no entry;
negative weights clamp to 0. If `Avoid Repeats` is on, the previously picked
output's weight is forced to 0. Then a weighted roll picks one.

If the total weight comes to 0, the node falls back to a uniform pick across all
outputs. That's a deliberate safety net — setting every weight to 0 doesn't break
the node, it just makes the choice even.

A few consequences worth knowing. A weights array shorter than `Num Outputs` applies
what's listed and defaults the rest to 1; a longer one ignores the extras. A single
weight of 0 means that output is never picked, unless all of them are 0. And
`Avoid Repeats` with only two outputs strictly alternates, since there's only ever
one remaining option.

## Wire every output

An unwired pin ends execution there. On a random branch that produces a bug which
only shows up some of the time, which is a miserable thing to debug. Wire all of
them, even the ones you think are unreachable.

## Avoid Repeats is per-run

The "last picked" memory lives on the node instance, created when the flow starts.
Restarting forgets it, so a fresh run can repeat the previous run's choice. If you
need it to survive a restart, persist the last choice in the
[blackboard](../blackboard.md) and gate on it with a [Branch](branch.md).

## Three fault scenarios, one rare

Each run presents one of three faults, with the rare one appearing about a tenth of
the time:

1. Right-click → **Flow Control → Random Branch**.
2. Set **Num Outputs** to `3`.
3. Under **Weights**, add three entries: `45`, `45`, `10`.
4. Tick **Avoid Repeats** so the same fault doesn't come up twice running.
5. Wire each output to its scenario, converging them with a [Join](join.md) if they
   share an ending.

```
                    ┌──────────────────┐
   ─────────────────┤In  Random Branch │Out 0 ──▶ pressure fault   (45%)
                    │    45 / 45 / 10  │Out 1 ──▶ valve fault      (45%)
                    │    avoid repeats │Out 2 ──▶ sensor fault     (10%)
                    └──────────────────┘
```

## If a branch never comes up

**One branch never happens.** Its weight is 0, or `Weights` has fewer entries than
you think and the indexing is off by one. Weights index from 0.

**The flow sometimes just stops.** An unwired output pin got picked.

**Avoid Repeats didn't prevent a repeat across runs.** It's per-instance and resets
on restart.

*Next: [Branch node](branch.md) · [Join node](join.md)*
