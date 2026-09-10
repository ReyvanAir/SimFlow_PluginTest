# Checkpoint node

**Class:** `USimFlowNode_Checkpoint`
**Add via:** right-click → **Persistence → Checkpoint**
**Pins:** In → Out

Marks a safe resume point, and optionally writes a save when execution reaches it.
Nothing blocks — execution passes straight through.

Two things happen when it fires. The component's **On Checkpoint Reached** event
goes out, so your UI can flash "Progress saved" or advance a stage indicator. And
the node becomes the anchor for the **From Last Checkpoint** load mode: a flow
loaded that way re-runs from the last checkpoint the trainee passed instead of
restoring a half-finished task.

## What it saves

| Field | Type | Default | Meaning |
|---|---|---|---|
| Checkpoint Id | Name | `None` | Identifies this checkpoint. Set it. |
| Auto Save | Bool | `false` | Write to a SaveGame slot the moment this node is reached. |
| Save Slot Name | String | *empty* | Only shown with Auto Save on. Empty means the component's default slot. |
| Save User Index | Int | `0` | Only shown with Auto Save on. |

Leaving `Checkpoint Id` at `None` doesn't break anything — the event still fires and
the save still happens — but your UI can't tell one checkpoint from another, and a
debrief can't report which stage the trainee got to. It costs nothing to name them.

Auto Save on a client does nothing but log; saving is authority-only.

## Auto Save writes synchronously

The SaveGame file is written at the moment the node executes, on the game thread.
Between stages that's fine. Inside a [Loop](loop.md) body it's a disk write every
iteration, and it will hitch visibly. Put the checkpoint outside the loop.

## Stage boundaries in a long exercise

For a 40-minute exercise the trainee can leave and pick up at the stage they
reached:

1. On the component, set **Default Save Slot Name** (say `TraineeProgress`) and tick
   **Auto Resume From Save On Begin Play**.
2. Set **Default Load Mode** to **From Last Checkpoint**.
3. Between each stage, right-click → **Persistence → Checkpoint**.
4. Give each a distinct **Checkpoint Id**: `Stage1Complete`, `Stage2Complete`, and
   so on.
5. Tick **Auto Save** on each, leaving **Save Slot Name** empty so they share the
   component's slot.
6. Bind **On Checkpoint Reached** in your HUD.

```
   stage 1 ──▶ ┌────────────────┐ ──▶ stage 2 ──▶ ┌────────────────┐ ──▶ stage 3
               │ Checkpoint     │                 │ Checkpoint     │
               │ Stage1Complete │                 │ Stage2Complete │
               │ auto save      │                 │ auto save      │
               └────────────────┘                 └────────────────┘
```

Re-launching resumes at the start of the last completed stage. Remember what that
mode actually restores: the blackboard, and the position in the graph. Not a
half-finished task.

## If resuming lands in the wrong place

Loading that resumes at the very beginning means no checkpoint had been passed yet,
or the flow has none at all — `From Last Checkpoint` has nothing to anchor to, so
use `Exact State` instead. Loading that restores a half-finished task instead of the
stage start is the same setting the other way round.

Repeated hitching is an auto-saving checkpoint inside a loop. Nothing saving at all
in multiplayer is the authority-only rule.

*Next: [Save and load](../simflow-component.md#save-and-load) · [Loop node](loop.md)*
