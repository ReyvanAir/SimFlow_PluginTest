# Set Blackboard Value node

**Class:** `USimFlowNode_SetBlackboard`
**Add via:** right-click → **Data → Set Blackboard Value**
**Pins:** In → Out

Writes a [blackboard](../blackboard.md) key inline in the graph, with no task object
involved. Execution passes straight through.

Handy for seeding a key before a [Branch](branch.md) reads it, resetting a counter
between attempts, or recording which path a run took.

## Fields

| Field | Type | Default | Meaning |
|---|---|---|---|
| Key | Name | `None` | Which blackboard key to write. |
| Value | SimFlow Value | Type `None` | Set the **Type** first and the matching value field appears. |
| Add | Bool | `false` | Add to the existing value instead of replacing it. |

Values can be `Bool`, `Int`, `Float`, `String`, `Name`, `Vector` or `Object` — see
[Blackboard · values and types](../blackboard.md#values-and-types).

## Set against Add

With `Add` off, the write replaces whatever was stored. With it on, numbers add,
vectors add component-wise, and strings *concatenate*. The key is created if it
doesn't already exist.

That string behaviour catches people out: adding `"1"` to `"1"` gives you `"11"`,
not `2`. Use `Int` for counters.

## Two silent failures

Leaving **Key** as `None` still performs the write, under the literal key name
`None`, with no warning. Leaving **Value Type** as `None` writes an unset value that
conditions can't usefully compare against. Neither announces itself.

A key-name typo has the same quiet quality — it produces a condition that's simply
always false. [Blackboard · when it misbehaves](../blackboard.md#when-it-misbehaves)
covers the diagnosis.

Adding to a missing key creates it with the delta as its value. Adding across
mismatched types coerces on a best-effort basis rather than failing.

## Node or task?

There's a [Set Blackboard Value task](../tasks/set-blackboard.md) with identical
fields. Use the node for a simple write in the graph, which is the usual case and
keeps the graph readable. Use the task when you want it inside a
[Parallel Group](../tasks/parallel-group.md), or want scoring or a `Task Id`
attached to the write.

## Seed a key, then branch on it

Suppose a [Branch](branch.md) checks `Attempts` at the top of the flow, before
anything has written it.

1. Right after [Start](start.md), right-click → **Data → Set Blackboard Value**.
2. Set **Key** to `Attempts`, **Value → Type** to `Int`, **Int Value** to `0`, and
   leave **Add** off.
3. After each failed attempt, add another Set Blackboard Value node with the same
   key, `Int Value` of `1`, and **Add** on.
4. The Branch can now compare `Attempts` reliably, because the key always exists.

```
   Start ──▶ Set "Attempts" = 0 ──▶ … ──▶ Set "Attempts" += 1 ──▶ Branch
```

Seeding like this saves you relying on **Blackboard Compare**'s
`Result When Key Missing` fallback, which is easy to forget is even there.

## When it misbehaves

**A later condition never sees the value.** The key names differ. They're
case-insensitive, but spelling isn't forgiven. Print the blackboard with
`SimFlow.Debug 1`.

**A counter reads "111".** The type is `String` with `Add` on, so it concatenated.
Switch to `Int`.

**The value vanished after loading a save.** The type is `Object`, and object
references are stripped on save.

*Next: [Blackboard](../blackboard.md) ·
[Set Blackboard Value task](../tasks/set-blackboard.md) · [Branch node](branch.md)*
