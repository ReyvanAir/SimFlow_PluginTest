# Sub Flow node

**Class:** `USimFlowNode_SubFlow`
**Add via:** right-click → **Composition → Sub Flow**
**Pins:** In → Completed, Failed

Runs another flow asset as a child. This is what makes flows modular.

Extract "don the PPE" or "perform the safety check" into its own asset once, then
call it from every scenario that needs it. Fix it in one place and every caller gets
the fix.

## Fields

| Field | Type | Default | Meaning |
|---|---|---|---|
| Sub Flow | SimFlow Asset | *null* | The child flow to run. |
| Entry Name | Name | `Default` | Which [Start](start.md) node of the child to begin from. |
| Inherit Blackboard | Bool | `true` | Copy the parent [blackboard](../blackboard.md) into the child at start. |
| Write Back Blackboard | Bool | `true` | Copy the child's blackboard back into the parent when it ends. |

`Completed` is taken when the child finishes successfully, `Failed` when it fails or
aborts.

## How state moves between parent and child

The two blackboard settings give you four arrangements:

| Inherit | Write Back | Behaviour |
|---|---|---|
| on | on | The default. The child sees everything and its changes come back — closest to inlining the section. |
| on | off | The child reads parent state but its changes are discarded. A sandbox. |
| off | on | The child starts clean and hands results back. Good for a self-contained scored section. |
| off | off | Fully isolated. |

Both directions are a merge that overwrites by default, not a replace, so keys the
child never touched keep their parent values on write-back. Object references move
across fine — this is an in-memory merge, not a save.

## An empty Sub Flow node is a silent pass-through

Leave `Sub Flow` null and the node logs a warning and triggers `Completed`. It
passes through rather than failing, exactly like an empty [Task node](task.md), and
it won't draw attention to itself. The log line to search for is *"has no asset
assigned"*.

A few other ways it goes quiet. An `Entry Name` that matches no Start node in the
child means the child can't start. A child with no [Finish](finish.md) node never
reports completion, so the parent waits on this node forever. And a flow that calls
itself recurses infinitely — there is no depth guard, so don't.

## Pause, skip and fail

The node forwards all three to the child. Pausing the parent pauses the child. Skip
is always allowed here and ends the child. Fail ends the child and leaves through
`Failed`.

## A reusable PPE check

Every scenario starts by making the trainee put on a helmet and gloves.

1. Create a flow asset `F_PPECheck` with its own Start, two [Task nodes](task.md),
   and a [Finish](finish.md).
2. Have it write a result key — a [Set Blackboard Value](set-blackboard.md) node
   setting `PPEComplete = true` before Finish.
3. In your main flow, right-click → **Composition → Sub Flow**.
4. Set **Sub Flow** to `F_PPECheck` and leave **Entry Name** as `Default`.
5. Leave both blackboard options on.
6. Wire **Completed** to the rest of the scenario and **Failed** to an abort path.

```
   Start ──▶ ┌────────────────┐ Completed ──▶ main scenario
             │ Sub Flow       │
             │ F_PPECheck     │ Failed ─────▶ abort
             └────────────────┘
```

With write-back on, `PPEComplete` is readable in the parent afterwards, so a later
[Branch](branch.md) can check it.

## If the child never returns

**The node completed instantly and nothing happened.** `Sub Flow` is empty.

**The parent hangs on the sub flow.** The child has no Finish node.

**Child results are missing in the parent.** `Write Back Blackboard` is off.

**The child overwrote a parent key you wanted kept.** Write-back overwrites by
default — turn it off, or use distinct key names in the child.

**The editor hangs on Play.** A flow is calling itself, directly or through a cycle.

*Next: [Task node](task.md) · [Blackboard](../blackboard.md) ·
[Start node](start.md)*
