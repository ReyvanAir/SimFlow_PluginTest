# Finish node

**Class:** `USimFlowNode_Finish`
**Add via:** right-click → **Flow Control → Finish**
**Pins:** In → *(no output)*

Ends the run and reports a result. It is the counterpart to [Start](start.md), and
the only node that actually terminates a flow.

Use as many as the scenario needs: a success ending, a failure ending, an "aborted
because the trainee walked out" ending. Each reports its own result to your debrief
UI.

## The two fields

| Field | Type | Default | Meaning |
|---|---|---|---|
| Finish Mode | Enum | `Complete (Success)` | The result reported for the whole run. |
| Stop Other Branches | Bool | `true` | Stop any parallel branches still running. Leave it on unless you have a reason. |

Finish Mode maps straight onto the final run state — `Complete (Success)` gives
`Completed`, `Fail` gives `Failed`, `Abort` gives `Aborted`. That state reaches your
UI through the component's **On Flow Finished** delegate, and you can read it at any
time with `Get Run State`.

## Pass and fail endings

To report success or failure so a debrief screen shows the right result:

1. After the final task, add a [Branch](branch.md).
2. Give it one case: **Score Threshold**, `>=`, `70`, labelled `Passed`.
3. Right-click → **Flow Control → Finish**, set **Finish Mode** to
   **Complete (Success)**, and wire the `Passed` case into it.
4. Add a second Finish node with **Finish Mode** = **Fail**. Wire the **Default**
   pin to that one.
5. In your HUD Blueprint, bind **On Flow Finished** and switch on the final state.

```
                    ┌──────────┐   Passed   ┌────────────────┐
   last task ───────┤ Branch   ├───────────▶│ Finish         │
                    │ Score>=70│            │ Complete       │
                    │          │  Default   └────────────────┘
                    │          ├───────────▶┌────────────────┐
                    └──────────┘            │ Finish  (Fail) │
                                            └────────────────┘
```

The [mistake list](../glossary.md) collected on the instance is still intact at this
point, which is what the debrief reads.

## The flow that never ends

There is no implicit completion in SimFlow. If execution simply runs out of wiring,
the flow sits in `Running` forever and nothing reports finished. That accounts for
most "my flow never ends" reports: either no Finish node on the path that actually
executed, or the pin leading to it was never wired.

Two other cases worth recognising:

A branch getting cut off mid-task is `Stop Other Branches` working as intended. If
you wanted both to complete, converge them through a [Join](join.md) in **Wait For
All** mode before finishing.

`On Flow Finished` reporting `Completed` when the trainee clearly failed means the
Finish node they reached still had the default mode on it. Add a separate Fail node
rather than switching the mode at runtime.

When several Finish nodes are reached in the same tick, the first to execute ends
the run; with `Stop Other Branches` on, the others never arrive.

*Next: [Start node](start.md) · [Join node](join.md) ·
[SimFlow Component](../simflow-component.md)*
