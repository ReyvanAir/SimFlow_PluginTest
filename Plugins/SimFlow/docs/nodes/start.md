# Start node

**Class:** `USimFlowNode_Entry`
**Add via:** right-click → **Flow Control → Start**
**Pins:** *(no input)* → Out

Execution begins here. Every flow needs at least one Start node, and a new flow
asset comes with one already placed.

You can have more than one. Each carries its own name, which is how a single asset
holds several related scenarios — the full run, a short demo, a "resume at part
two" entry. The [SimFlow Component](../simflow-component.md) picks between them at
runtime.

## The one field

| Field | Type | Default | Meaning |
|---|---|---|---|
| Entry Name | Name | `Default` | Pass this to `Start Flow From Entry` to begin here. |

Names must be distinct. If two Start nodes are both called `Default`, which one
runs is undefined — the flow will start, but not necessarily where you meant.

A blank or `None` name is worse: nothing can address that node, so it never runs
at all. Same outcome if the component asks for an entry that no Start node
matches. Both sides default to `Default`, so this only bites after someone renames
one and forgets the other.

## A demo entry alongside the full run

Say you want one asset that runs the whole 20-minute exercise, or jumps to a
5-minute cut for a trade show.

1. Leave the existing Start node's **Entry Name** as `Default` and wire it to the
   full sequence.
2. Right-click → **Flow Control → Start** to add a second one.
3. Set its **Entry Name** to `Demo`.
4. Wire it to the shortened section.
5. At runtime, call **Start Flow From Entry** with `Demo` — or set the component's
   **Entry Name** to `Demo` before it auto-starts.

```
  ┌──────────────┐
  │ Start        │Out──▶ full exercise
  │ "Default"    │
  └──────────────┘

  ┌──────────────┐
  │ Start        │Out──▶ short demo
  │ "Demo"       │
  └──────────────┘
```

Both entries share the same [blackboard](../blackboard.md) keys and the same
[Finish](finish.md) nodes. Only the way in differs.

## If it isn't starting

Check the component's `Entry Name` against the node's — after a rename they drift
apart, and the mismatch is silent. If the flow starts and instantly ends instead,
the `Out` pin isn't wired to anything.

Adding a second Start node does not break the first, despite appearances. Two
nodes sharing a name does.

*Next: [Finish node](finish.md), or the [component](../simflow-component.md) that
chooses the entry.*
