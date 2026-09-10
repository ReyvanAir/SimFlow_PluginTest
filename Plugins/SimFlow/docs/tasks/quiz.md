# Quiz

**Class:** `USimFlowTask_Quiz`
**Select via:** [Task node](../nodes/task.md) → **Task** dropdown → **Quiz**

A multiple-choice question.

The task holds the question, the options and the answer key. You build the widget:
bind `On Quiz Presented` from your VR widget, show the options, and call
`Submit Answer` when the trainee picks one.

Wrong answers can retry, fail, or simply continue — wire the
[Task node](../nodes/task.md)'s `Failed` pin to whatever remediation you want.

## The question and the answer key

| Field | Type | Default | Meaning |
|---|---|---|---|
| Question | Text (multi-line) | *empty* | The question text. |
| Options | Array of Text | *empty* | The choices, in display order. |
| Correct Option Index | Int (min 0) | `0` | Which option is right. 0-based. |
| Additional Correct Indices | Array of Int | *empty* | Also treat these as correct, for multi-answer questions. |
| Answer Blackboard Key | Name | `None` | Receives the submitted index. |
| Fail On Wrong Answer | Bool | `true` | A wrong answer immediately fails the task, driving the `Failed` pin. |
| Count Mistakes | Bool | `true` | Increments the `Mistakes` [blackboard](../blackboard.md) key on a wrong answer. |

Plus the [fields every task has](README.md#the-fields-every-task-has).

The index is 0-based, and that's the mistake people make here. The first option is
`0`, so setting `Correct Option Index` to `1` when you meant the first choice
silently marks the wrong answer correct.

An out-of-range `Correct Option Index` means no submitted answer can ever be right.
Empty `Options` presents a question with nothing to pick, so nothing can be
submitted and the task waits forever — as it does if nothing ever calls
`Submit Answer`. Put a `Time Limit` on the [Task node](../nodes/task.md) if the
question should expire.

## What a submission writes

Regardless of whether you set `Answer Blackboard Key`, every submission writes
`LastAnswerIndex` (int, the index submitted), `LastAnswerCorrect` (bool), and — when
`Count Mistakes` is on and the answer was wrong — increments `Mistakes`. These are
the [well-known keys](../blackboard.md#the-well-known-keys).

## How an answer is judged

The submitted index is checked against `Correct Option Index` and
`Additional Correct Indices`. `On Quiz Answered` then fires with the index and
whether it was correct, the blackboard keys above are written, and the task
finishes.

A correct answer finishes with `Succeeded`. A wrong one records a
`SimFlow.Mistake.WrongAnswer` mistake and then depends on `Fail On Wrong Answer`:
with it on the task finishes `Failed`, with it off the task finishes `Succeeded` and
the flow carries on, leaving you to branch on `LastAnswerCorrect` afterwards if you
care.

## Delegates and functions

| Member | Signature | Use for |
|---|---|---|
| On Quiz Presented | (Quiz) | Show your widget and populate it from the quiz |
| On Quiz Answered | (Answer Index, Correct) | Feedback before the flow moves on |
| Submit Answer | (Option Index) | Call this from your answer buttons |
| Is Correct Index | (Option Index) → Bool | Check an index without submitting |

The [SimFlow Component](../simflow-component.md) surfaces the same things — its own
`On Quiz Presented` delegate, `Get Current Quiz`, and `Submit Quiz Answer`. The
component's `Submit Quiz Answer` works from clients in multiplayer by forwarding to
the server, so prefer that route if you replicate.

## A knowledge check with remediation

Asking which extinguisher suits an electrical fire, routing a wrong answer to a
short teaching section and then re-asking.

Setting up the task:

1. Add a [Task node](../nodes/task.md) with **Task** = **Quiz**.
2. **Question** = `Which extinguisher is safe on an electrical fire?`
3. **Options**: `Water`, `Foam`, `CO2`.
4. **Correct Option Index** = `2`, since CO2 is third and the index is 0-based.
5. Leave **Fail On Wrong Answer** and **Count Mistakes** on.
6. Set **Score On Success** to `20` and **Score On Failure** to `-5`.

Wiring the widget:

7. Bind the component's **On Quiz Presented**, and in the handler read `Question`
   and `Options` from the quiz to build your buttons.
8. Each button calls **Submit Quiz Answer** on the component with its index.

Routing the result:

9. Wire **Completed** onward to the next section.
10. Wire **Failed** into a teaching section, and loop that back into this Task node's
    `In` pin to re-ask.

```
            ┌──────────────┐ Completed ──▶ next section
   ────────▶┤ Task (Quiz)  │
        ┌──▶│              │ Failed ─────▶ teaching section ──┐
        │   └──────────────┘                                  │
        └─────────────────────────────────────────────────────┘
```

To ask again without a hard fail, turn **Fail On Wrong Answer** off and put a
[Branch](../nodes/branch.md) on `LastAnswerCorrect` after the task instead.

## If answers are judged oddly

**The correct answer is marked wrong.** `Correct Option Index` is 0-based; the third
option is `2`.

**The quiz never appears.** Nothing is bound to `On Quiz Presented`, or your widget
isn't being created. The task draws nothing itself.

**The flow hangs at the quiz.** Nothing calls `Submit Answer`. Check your buttons
are wired, and add a `Time Limit` if the question should expire.

**A wrong answer continued as if correct.** `Fail On Wrong Answer` is off, which
finishes with `Succeeded` by design.

**Client answers do nothing in multiplayer.** Call the component's
`Submit Quiz Answer` rather than the task's `Submit Answer`.

*Next: [Blackboard](../blackboard.md) · [Branch node](../nodes/branch.md) ·
[SimFlow Component](../simflow-component.md)*
