# Join node

**Class:** `USimFlowNode_Join`
**Add via:** right-click → **Flow Control → Join**
**Pins:** In 0 … In N → Out

Converges parallel branches. Join is the partner to [Parallel](parallel.md), and it
answers the question "several things are running, and I need one continuation —
when does it fire?"

There are two answers, and they solve quite different problems. **Wait For All**
fires once every connected input has been triggered: finish both sub-tasks, then
continue. **Wait For Any (Race)** fires on the first input and ignores the rest,
which is how you build timeouts and races.

`Num Inputs` (2–16, default `2`) sets the pin count and `Mode` picks between the
two. Pins are named `In_0` … `In_N`, values outside the range are clamped, and
reducing the count drops those pins along with their links.

Something upstream has to produce the branches in the first place: either a
[Parallel](parallel.md) node, or a [Branch](branch.md) with
`Fire All Matching Cases` turned on.

## What each mode actually does

Wait For All records each arriving input. Once the number of *distinct* inputs
received reaches `Num Inputs`, it fires `Out` and deactivates, resetting its
counters — which is what lets a Join sit inside a [Loop](loop.md) and work on every
pass. Inputs count uniquely, so the same pin firing twice counts once; two arrivals
on `In_0` will not satisfy a two-input join.

Wait For Any fires `Out` on the first arrival, then deliberately stays active so it
can absorb the losing branch when it turns up. Without that, the downstream section
would run a second time.

## Matching Num Inputs to reality

This is the one that bites. In Wait For All, if `Num Inputs` is higher than the
number of pins you actually wired, the join can never be satisfied and the flow
stalls with nothing in the log. Wire two branches, leave the count at 3, and nothing
downstream ever runs.

So: set the count to the number of branches you actually wired. A branch that never
arrives has the same effect — the join waits forever.

## Waiting for both

The trainee must don a helmet and sign the permit before proceeding, in either
order.

1. Add a [Parallel](parallel.md) node with `Num Outputs` = 2.
2. Wire each output into its own [Task node](task.md).
3. Add a Join, `Num Inputs` = 2, **Mode** = **Wait For All**.
4. Wire each task's `Completed` into `In 0` and `In 1`.
5. Wire `Out` onward.

## Racing the clock

The trainee has 60 seconds; either they finish or the time runs out.

1. Add a [Parallel](parallel.md) node.
2. `Out 0` into the real task, `Out 1` into a [Delay node](delay.md) of `60`.
3. Add a Join with **Mode** = **Wait For Any (Race)**.
4. Wire both into it, and `Out` onward.

To find out which branch won, have each one write a
[Set Blackboard Value](set-blackboard.md) before the join — `Outcome = "finished"`
against `"timeout"` — and read it with a [Branch](branch.md) afterwards.

```
   Task ──────────┐   ┌──────────────┐
                  ├──▶│ Join         ├──▶ Branch on "Outcome"
   Delay 60s ─────┘   │ Wait For Any │
                      └──────────────┘
```

## If the flow stalls here

**Stalled at the join, nothing logged.** `Num Inputs` exceeds the number of branches
that actually arrive. Silent by design; check the count first.

**The section after the join ran twice.** Either two branches were wired straight
into the continuation with no join at all, or you used Wait For All in a situation
that wanted Wait For Any.

**A join inside a loop only worked on the first pass.** It should reset when it
fires, so check the join is genuinely being re-entered rather than bypassed on later
iterations.

*Next: [Parallel node](parallel.md) · [Loop node](loop.md) ·
[Branch node](branch.md)*
