# Valve startup procedure

**Shows:** [Ordered Sequence](../tasks/ordered-sequence.md) · out-of-order handling ·
progress UI · [Checkpoints](../nodes/checkpoint.md) · [Loop](../nodes/loop.md)

## The scenario

A plant startup procedure. The trainee must open three valves **in the correct
order**, then confirm at the panel:

1. Valve A (inlet)
2. Valve B (bypass)
3. Valve C (main)

Opening C first is not the same kind of error as turning a random wheel — the
trainee knows the procedure but not the order. This workflow makes that distinction
automatically.

## 1. Tags

```
Control.Valve.A
Control.Valve.B
Control.Valve.C
Control.Panel.Confirm
```

`SimFlow.Event.Interact` ships with the plugin.

## 2. The valves

Add a [SimFlow Identity](../identity.md) to the **valve Blueprint** once, then set
the tag **per level instance** (select each placed valve and set its Identity Tags):

| Instance | Identity Tags | Display Name |
|---|---|---|
| Inlet valve | `Control.Valve.A` | `Valve A` |
| Bypass valve | `Control.Valve.B` | `Valve B` |
| Main valve | `Control.Valve.C` | `Valve C` |

In the valve Blueprint, when it is turned:

```
Broadcast Flow Event   Event Tag = SimFlow.Event.Interact   Payload = self
```

**Every valve broadcasts the identical event.** None of them knows the order — that
is the design. The same three valves can be used by a dozen different procedures.

Do the same for the confirm button with `Control.Panel.Confirm`.

## 3. The flow

```
  ┌───────┐   ┌────────────────────┐   ┌──────────────┐   ┌────────────────┐   ┌────────┐
  │ Start │──▶│ Task               │──▶│ Checkpoint   │──▶│ Task           │──▶│ Finish │
  │       │   │ Ordered Sequence   │   │ ValvesOpen   │   │ Wait For Event │   │        │
  └───────┘   │ (A → B → C)        │   │ auto save    │   │ (confirm)      │   └────────┘
              └────────────────────┘   └──────────────┘   └────────────────┘
```

### Node 1 — Ordered Sequence

| Field | Value |
|---|---|
| Task | [Ordered Sequence](../tasks/ordered-sequence.md) |
| Event Tag | `SimFlow.Event.Interact` |
| Match Child Tags | on |
| Out Of Order Policy | `Count Mistake (Stay On Step)` |
| Unlisted Input Is Mistake | **off** |
| Step Blackboard Key | `CurrentStep` |
| Display Name | `Startup procedure` |
| Score On Success | `30` |

**Steps** — click **+** three times:

| # | Target → Required Tags | Instruction |
|---|---|---|
| 0 | `Control.Valve.A` | `Open the inlet valve (A)` |
| 1 | `Control.Valve.B` | `Open the bypass valve (B)` |
| 2 | `Control.Valve.C` | `Open the main valve (C)` |

`Unlisted Input Is Mistake` is **off** because other props in the plant share
`SimFlow.Event.Interact`. Leaving it on would flag every unrelated door handle as a
procedural error.

### Node 2 — Checkpoint

| Field | Value |
|---|---|
| Checkpoint Id | `ValvesOpen` |
| Auto Save | on |
| Save Slot Name | *(empty — uses the component default)* |

### Node 3 — Wait For Event

| Field | Value |
|---|---|
| Task | [Wait For Event](../tasks/wait-for-event.md) |
| Event Tag | `SimFlow.Event.Interact` |
| Expected Payload → Required Tags | `Control.Panel.Confirm` |
| Mismatch Policy | `Ignore (Keep Waiting)` |
| Instruction | `Confirm at the panel` |

`Ignore` here because by this point any further valve fiddling is not worth
recording as a mistake.

## 4. The progress UI

The task publishes the current step in two ways — use whichever suits your widget:

| Route | How |
|---|---|
| **Blackboard** | Read the `CurrentStep` int key. Easiest if your widget already reads blackboard values. |
| **Direct** | Call `Get Current Step Index` and `Get Current Step Instruction` on the task. |

For a checklist that ticks off as they go, bind **On Step Completed**
(Step Index, Target).

> **Both are 0-based.** Display `CurrentStep + 1` as "Step 1 of 3".

For the current prompt, `Get Current Step Instruction` returns the step's own
`Instruction` text — that is why each step has one, separate from the task's
overall `Display Name`.

## 5. Wrong-order feedback

Bind **On Wrong Input** (Payload, Expected Step Index). The severity is already
worked out for you — read it from the recorded [mistake](../glossary.md), or infer
it from whether the payload is one of your valves:

| Situation | Recorded severity | Suggested message |
|---|---|---|
| Turned a valve, wrong moment | **Related (Near Miss)** | *"Not yet — valve B comes first."* |
| Turned something unrelated | **No Match** | *(ignored entirely, with the settings above)* |

The task records `SimFlow.Mistake.WrongOrder` with a ready-made description such as
*"Step 2: used Valve C - expected Control.Valve.B"*, and increments the
`WrongAttempts` [blackboard](../blackboard.md) key.

## 6. Choosing the strictness

The single field that changes the character of this exercise is
**Out Of Order Policy**:

| Policy | Feels like |
|---|---|
| `Ignore` | Free exploration — nothing is recorded |
| `Count Mistake (Stay On Step)` | **Training.** Records the error, lets them find the right valve |
| `Count Mistake And Restart` | **Strict procedure.** One slip on step 3 sends them back to A |
| `Count Mistake And Fail Task` | **Assessment.** Wire `Failed` to a debrief |

For a graded run, switch to `Count Mistake And Fail Task` and wire the Task node's
`Failed` pin to a remediation section — then loop back into the sequence to try
again.

## 7. Adding a retry loop

To allow three attempts before failing the whole exercise:

1. Set **Out Of Order Policy** to `Count Mistake And Fail Task`.
2. Wrap the Ordered Sequence Task node in a [Loop node](../nodes/loop.md):
   - **Iterations** = `3`
   - **Iteration Blackboard Key** = `Attempt`
3. Wire **Loop Body** → the Task node's `In`.
4. Wire the Task node's `Failed` **back into the Loop's `Continue` pin**.
5. Wire the Task node's `Completed` onward to the checkpoint.
6. Wire the Loop's `Completed` (i.e. attempts exhausted) to a
   [Finish](../nodes/finish.md) node set to **Fail**.

```
        ┌──────────────┐ Loop Body ──▶ Ordered Sequence ──Completed──▶ Checkpoint ──▶ …
  ──────┤In    Loop    │                      │
   ┌───▶┤Continue  x3  │                   Failed
   │    │              │Completed──▶ Finish (Fail)
   │    └──────────────┘
   └──────────────────────────────────────────┘
```

## Troubleshooting this workflow

**The task fails the moment it starts.**
`Event Tag` is empty — an Ordered Sequence with no tag fails immediately (an empty
*Steps* list, by contrast, succeeds immediately).

**It hangs on step 1 even when valve A is turned.**
The step's `Target` does not match. Check the *level instance* carries the tag — it
is easy to set tags on the Blueprint and forget the per-instance ones.

**Every door handle in the plant counts as a mistake.**
`Unlisted Input Is Mistake` is on. Turn it off, or give the valves a dedicated event
tag.

**The progress widget shows "Step 0 of 3".**
The index is 0-based. Add 1.

**The game hitches at the checkpoint.**
Auto-save writes synchronously. That is fine once between stages — just do not put a
saving checkpoint inside the retry loop.

*Next: [Ordered Sequence](../tasks/ordered-sequence.md) ·
[Assessment with debrief](assessment-with-debrief.md)*
