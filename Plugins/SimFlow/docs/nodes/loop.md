# Loop node

**Class:** `USimFlowNode_Loop`
**Add via:** right-click → **Flow Control → Loop**
**Pins:** In, Continue → Loop Body, Completed

Repeats a section of the graph, either a fixed number of times or until a condition
passes.

It's the one node whose wiring isn't obvious, because it needs a return wire.
**Loop Body** goes out to the section you want to repeat, and the end of that
section comes back into **Continue**. Miss the return wire and the body runs exactly
once, then the flow stops.

## Fields

| Field | Type | Default | Meaning |
|---|---|---|---|
| Iterations | Int (min 0) | `3` | How many times to run the body. `0` means "until the break condition passes". |
| Break Condition | Instanced [condition](../conditions.md) | *null* | Evaluated before each iteration. Passing exits through `Completed`. |
| Max Iterations | Int (min 1) | `1000` | Safety valve so a runaway loop can't hang the game. |
| Iteration Blackboard Key | Name | `None` | Writes the current 0-based iteration into this [blackboard](../blackboard.md) key. |

The last two are advanced settings you can usually leave alone.

## The pins

`In` is the entry, and it resets the iteration counter to zero. `Continue` is where
the body returns to ask for another pass. `Loop Body` fires once per iteration and
`Completed` fires when the loop is done.

The node stays active while looping and is re-entrant, so arriving at `Continue`
while it's already active is the normal path rather than an error.

## How each iteration is decided

Every time `In` or `Continue` fires, the node works through this in order:

1. `In` sets the counter to 0; `Continue` increments it.
2. Hit **Max Iterations**? Log a warning and exit via `Completed`.
3. **Iterations** greater than 0 and the counter has reached it? Exit via
   `Completed`.
4. **Break Condition** set and passing? Exit via `Completed`.
5. Otherwise write the iteration key if one is set, and fire **Loop Body**.

The order matters more than it looks. The break condition is checked *before* the
body runs, so a condition that's already true on entry means the body never runs at
all.

## Three attempts, or until they get it right

Letting the trainee retry a placement up to three times, stopping early on success:

1. Right-click → **Flow Control → Loop**. Set **Iterations** to `3`.
2. Set **Break Condition** to **Blackboard Compare**: key `Placed`, operation `==`,
   value type `Bool`, `true`.
3. Set **Iteration Blackboard Key** to `Attempt` so a widget can show "Attempt 2 of
   3".
4. Wire **Loop Body** into a [Task node](task.md) running
   [Place Object In Zone](../tasks/place-object-in-zone.md).
5. After that task, add a [Set Blackboard Value](set-blackboard.md) node writing
   `Placed = true`.
6. Wire that node's `Out` back into the Loop's `Continue` pin.
7. Wire **Completed** onward.

```
        ┌──────────────┐
  ──────┤In    Loop    │Loop Body──▶ Task (Place Object) ──▶ Set "Placed"
   ┌───▶┤Continue      │Completed──▶ next section              │
   │    └──────────────┘                                       │
   └───────────────────────────────────────────────────────────┘
```

Since the break condition is checked before each iteration, a successful first
attempt exits after one pass.

## When it misbehaves

**The body runs once and the flow stops.** `Continue` isn't wired back. This is the
classic Loop mistake and worth checking before anything else.

**The log says "hit MaxIterations".** `Iterations` is 0 and the break condition never
passes, so it ran the full thousand. Fix the condition or set a real count.

**The body never runs.** The break condition was already true on entry.

**The loop stalls partway through.** Something in the body ended without returning to
`Continue`, often a task leaving through an unwired `Failed` pin. Wire the failure
paths back to `Continue` as well, or turn on the Task node's `Fallback To Completed`.

**The attempt counter reads one low.** The iteration key is 0-based; add 1 for
display.

*Next: [Task node](task.md) · [Conditions](../conditions.md) ·
[Join node](join.md)*
