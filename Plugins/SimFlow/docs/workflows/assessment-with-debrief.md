# Assessment with debrief

**Shows:** [Quiz](../tasks/quiz.md) · [Branch](../nodes/branch.md) on score ·
[Sub Flow](../nodes/sub-flow.md) · [mistakes](../glossary.md) · a debrief screen

## The scenario

A graded assessment that reuses the practical drills you have already built, adds a
knowledge check, then routes to pass / remediation and shows a debrief listing
everything the trainee got wrong.

This is where the pieces come together: **mistakes are collected across the whole
run**, not per task, which is exactly what a debrief needs.

## 1. Structure

```
  ┌───────┐   ┌──────────────┐   ┌──────────────┐   ┌────────────┐
  │ Start │──▶│ Sub Flow     │──▶│ Sub Flow     │──▶│ Task       │
  │       │   │ Extinguisher │   │ ValveStartup │   │ Quiz       │
  └───────┘   └──────────────┘   └──────────────┘   └─────┬──────┘
                                                          │
                    ┌─────────────────────────────────────┘
                    ▼
              ┌──────────┐ Distinction ──▶ Finish (Complete)
              │  Branch  │ Pass ─────────▶ Finish (Complete)
              │ on score │ Default ──────▶ remediation ──▶ Finish (Fail)
              └──────────┘
```

Reusing the drills as [Sub Flows](../nodes/sub-flow.md) is the point — the practical
exercises are authored once and called from the practice scenario *and* the
assessment.

## 2. Seed the blackboard

Right after [Start](../nodes/start.md), add a
[Set Blackboard Value](../nodes/set-blackboard.md) node:

| Field | Value |
|---|---|
| Key | `Score` |
| Value → Type | `Float` |
| Float Value | `0` |
| Add | off |

**Why bother:** [Blackboard Compare](../conditions.md#blackboard-compare) returns
its `Result When Key Missing` setting (default `false`) when a key does not exist.
Seeding the key means the branch conditions behave predictably on the very first
evaluation instead of depending on that fallback. See
[Blackboard](../blackboard.md#when-it-misbehaves).

## 3. The sub flows

Add two [Sub Flow](../nodes/sub-flow.md) nodes:

| Field | Node 1 | Node 2 |
|---|---|---|
| Sub Flow | `F_ExtinguisherDrill` | `F_ValveStartup` |
| Entry Name | `Default` | `Default` |
| Inherit Blackboard | on | on |
| Write Back Blackboard | **on** | **on** |

**Write-back must be on** — it is how the score and mistake counts the children
accumulated come back to the parent. With it off, the assessment would always score
zero.

Wire each node's **Completed** onward. Wire **Failed** onward too (to the same
place) if a failed drill should still reach the debrief rather than ending the run —
otherwise an unwired `Failed` pin silently ends the assessment.

## 4. The quiz

| Field | Value |
|---|---|
| Task | [Quiz](../tasks/quiz.md) |
| Question | `Which extinguisher is safe on an electrical fire?` |
| Options | `Water`, `Foam`, `CO2` |
| Correct Option Index | `2` |
| Fail On Wrong Answer | **off** |
| Count Mistakes | on |
| Score On Success | `20` |
| Score On Failure | `-5` |

**`Fail On Wrong Answer` is off** deliberately: in an assessment a wrong answer
should cost marks and be recorded, not halt the run. The trainee finishes the
assessment and finds out at the debrief.

Bind the component's **On Quiz Presented** to build your widget, and call
**Submit Quiz Answer** on the component from the answer buttons — the component's
version forwards correctly from clients.

## 5. Branch on the result

Add a [Branch](../nodes/branch.md) node with two cases and the `Default` pin on.

**Case 0 — `Distinction`**

Condition = **All Of (AND)** with two children:

| Child | Settings |
|---|---|
| **Score Threshold** | Operation `>=`, Threshold `90` |
| **Blackboard Compare** | Key `Mistakes`, Operation `<=`, Value → Int `0` |

**Case 1 — `Pass`**

Condition = **Score Threshold**, Operation `>=`, Threshold `70`.

**Default** → the remediation section.

> **Order matters.** Distinction is tested first. Reverse the two and everyone
> scoring 90+ would leave through `Pass`, because it is evaluated first and passes.
> See [Branch · Order matters](../nodes/branch.md#order-matters).

## 6. The debrief

Bind **On Flow Finished** (Final State) on the
[SimFlow Component](../simflow-component.md).

### What to show

| Data | Where it comes from |
|---|---|
| Pass / fail | The `Final State` argument, or `Get Run State` |
| Score | `Get Score`, or the `Score` blackboard key |
| Mistake count | The `Mistakes` blackboard key |
| Wrong attempts | The `WrongAttempts` blackboard key |
| **The mistake list** | Collected on the flow **instance** |

The mistake list is the valuable part. Each [mistake](../glossary.md) carries:

| Field | Use |
|---|---|
| **Task Id** | Which task it happened in — set `Task Id` on your tasks to make this useful |
| **Kind** | `SimFlow.Mistake.WrongItem` / `.WrongTarget` / `.WrongOrder` / `.WrongAnswer` |
| **Description** | A ready-to-show sentence, e.g. *"Placed CO2 Extinguisher in Extinguisher Bay - expected Item.Extinguisher.Foam"* |
| **Severity** | `Related` = right idea, wrong choice; `No Match` = unrelated |
| **Involved Name** | The object's display name, captured at record time |
| **Time Seconds** | When in the run it happened |

`Description` is already written for you — a debrief list can be as simple as one
row per mistake showing `Time Seconds` and `Description`.

Group by **Severity** to make the debrief genuinely useful: a list of near misses
tells an instructor the trainee understood the task and slipped, while a list of
`No Match` errors says they did not understand it at all.

> **`Involved` is runtime-only** — the hard object pointer is cleared when a flow is
> saved. `Involved Name` is captured at record time precisely so a debrief survives
> a save/load. Use the name, not the pointer.

## 7. Run it

1. [SimFlow Component](../simflow-component.md) on the Game Mode.
2. **Flow Asset** = `F_Assessment`, **Start Mode** = **Auto - On First Tick**,
   **Flow Save Id** = `Assessment`.
3. `SimFlow.Debug 1` to watch score and blackboard live while you tune the
   thresholds.

## Variations

| To… | Do |
|---|---|
| Make the quiz blocking | `Fail On Wrong Answer` on, wire `Failed` to a teaching section that loops back |
| Weight the practical higher | Raise `Score On Success` on the drill tasks inside the sub flows |
| Add a time limit to the whole run | [Parallel](../nodes/parallel.md) a [Delay node](../nodes/delay.md) against the assessment, converge with a [Join](../nodes/join.md) in **Wait For Any** mode |
| Let them resume a long assessment | Add [Checkpoints](../nodes/checkpoint.md) between sections and set the component's `Default Load Mode` to `From Last Checkpoint` |
| Randomise which drill runs | Replace the two Sub Flow nodes with a [Random Branch](../nodes/random-branch.md) feeding one each |

## Troubleshooting this workflow

**The score is always 0.**
Either `Write Back Blackboard` is off on the sub flows, or the tasks inside them
have `Score On Success` left at the default `0`. Scoring is opt-in per task.

**Everything routes to Default.**
The score conditions are reading a key that does not exist, or is lower than you
think. Seed `Score` at the start and watch it with `SimFlow.Debug 1`.

**The distinction branch never fires.**
The `Pass` case is listed first and catching everything. Reorder so the most
specific case is first.

**The assessment ends early after a drill.**
A Sub Flow node's `Failed` pin is unwired, so a failed drill ended that line of
execution. Wire it.

**The debrief shows blank object names.**
The items have no `Display Name` on their [Identity](../identity.md) component, so
the name falls back to something like `BP_Extinguisher_C_2`.

**Mistakes are missing after a save/load.**
You are reading `Involved` rather than `Involved Name`. Object pointers are stripped
on save.

*Next: [Quiz](../tasks/quiz.md) · [Sub Flow](../nodes/sub-flow.md) ·
[Branch](../nodes/branch.md) · [Blackboard](../blackboard.md)*
