# Set Blackboard Value (task)

**Class:** `USimFlowTask_SetBlackboard`
**Select via:** [Task node](../nodes/task.md) → **Task** dropdown → **Set Blackboard Value**

Writes a [blackboard](../blackboard.md) key, or adds to it, then finishes
immediately.

The [node form](../nodes/set-blackboard.md) has identical fields and is the better
choice for a simple write in the graph. Use the task when you need it inside a
[Parallel Group](parallel-group.md), or want a `Task Id` or scoring attached to the
write.

## Fields

| Field | Type | Default | Meaning |
|---|---|---|---|
| Key | Name | `None` | Which blackboard key to write. |
| Value | SimFlow Value | Type `None` | Set the **Type** first and the matching value field appears. |
| Add | Bool | `false` | Add to the existing value instead of replacing it. |

Plus the [fields every task has](README.md#the-fields-every-task-has).

Leaving **Key** as `None` writes under the literal key name `None`, with no warning.
Leaving **Value Type** as `None` stores an unset value that conditions can't
usefully compare against. Both are silent, and a key-name typo has the same
character — it yields a condition that's quietly always false. See
[Blackboard · when it misbehaves](../blackboard.md#when-it-misbehaves).

Adding to a missing key creates it with the delta as the value. Adding across
mismatched types coerces rather than failing. Adding on a `String` concatenates, so
`"1"` plus `"1"` is `"11"` — use `Int` for counters.

## Scoring a step inside a parallel group

While the trainee performs a check, record that the briefing was delivered as part
of the same group, so it can't be skipped separately.

1. Add a [Task node](../nodes/task.md) with **Task** =
   [Parallel Group](parallel-group.md).
2. Child 0 is the real check task.
3. Child 1 is **Set Blackboard Value**: key `BriefingDelivered`, value type `Bool`,
   value `true`, Add off.
4. Leave the group's **Wait For All** on.

A later [Branch](../nodes/branch.md) can then test `BriefingDelivered` with
**Blackboard Compare**.

For a plain write between two steps in the graph, the
[node form](../nodes/set-blackboard.md) reads better and costs less.

## When it misbehaves

**A later condition never sees the value.** The key names differ. They're
case-insensitive, but spelling isn't forgiven. Print the blackboard with
`SimFlow.Debug 1`.

**A counter became "111".** Type is `String` with `Add` on. Switch to `Int`.

**The value vanished after loading a save.** The type is `Object`, and object
references are stripped on save — see
[values and types](../blackboard.md#values-and-types).

*Next: [Set Blackboard Value node](../nodes/set-blackboard.md) ·
[Blackboard](../blackboard.md) · [Parallel Group](parallel-group.md)*
