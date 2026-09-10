# Troubleshooting

Organised by symptom. Find the one that matches, then follow the link for detail.

## Start with these two tools

The debug overlay, from the console:

```
SimFlow.Debug 1
```

draws live flow state on screen — the active node, the current task, elapsed time
and the whole blackboard. `SimFlow.Debug 0` turns it off.

Reach for it before anything else. Most "it doesn't work" reports are answered by
seeing which node is actually active and what the blackboard holds. You can also
tick **Show Debug HUD** on the [SimFlow Component](simflow-component.md) for a
single flow, or call `Get Debug Text` to put the same information into a world-space
VR widget.

Then the log. SimFlow logs under the `LogSimFlow` category, so open the Output Log
and filter for it. A good number of misconfigurations log a warning naming the exact
problem: a task with no zone, an event task with no tag, a loop hitting its
iteration cap. Check the log before guessing — the silent failures below are the
ones that log nothing, which is exactly what makes them hard.

## Nothing happens at all

Work down this list. Is `Flow Asset` set on the component? Is `Start Mode` still
`Manual`, with nothing calling `Start Flow`? Does the component's `Entry Name` match
a [Start](nodes/start.md) node's `Entry Name` — both default to `Default`, so a
rename on one side breaks it silently. Is there a Start node at all? And is Start's
`Out` pin actually wired, since an unwired pin ends execution without a word.

## The flow starts, then can't find things

`Start Mode` is `Auto - On Begin Play`. Other actors may not have begun play yet, so
a task that immediately looks for a [zone](zones.md) or an item finds nothing.
Switch to **Auto - On First Tick**.

## The flow never ends

There's no [Finish](nodes/finish.md) node on the path that ran, or the pin leading
to it is unwired. Execution running out of wiring does not complete a flow — it just
stops, with the run state still `Running`.

## The flow ends too early

Either a [Finish](nodes/finish.md) node was reached with `Stop Other Branches` on,
which is the default, while another branch was still working — converge branches
with a [Join](nodes/join.md) first — or a [Sub Flow](nodes/sub-flow.md) or
[Task node](nodes/task.md)'s `Failed` pin is unwired and ended that line of
execution.

## A task never completes

The most common category by far.

| Symptom | Likely cause |
|---|---|
| Nothing in the log | An empty [Actor Query](actor-query.md) or an empty [condition](conditions.md). Both are silently false |
| [Wait For Condition](tasks/wait-for-condition.md) hangs | Its `Condition` slot is empty, and an unset condition is false forever |
| [Place Object In Zone](tasks/place-object-in-zone.md) hangs | `Accepted Items` is empty, so nothing can ever match |
| [Ordered Sequence](tasks/ordered-sequence.md) hangs on a step | That step's `Target` is empty or doesn't match |
| [Quiz](tasks/quiz.md) hangs | Nothing calls `Submit Answer` |
| [Wait For Event](tasks/wait-for-event.md) hangs | The tag never arrives — see below |

While developing, set a `Time Limit` on the [Task node](nodes/task.md) and wire
`Timed Out` to a [Log Message](tasks/log-message.md). A hang then becomes a message
naming the task.

## A task completes instantly when it should wait

Several tasks pass through by design when a required field is empty, so that a
half-built flow still runs end to end.

| Task | Empty field | Result |
|---|---|---|
| [Task node](nodes/task.md) | `Task` | Warns, triggers `Completed` |
| [Wait For Event](tasks/wait-for-event.md) | `Event Tag` | Warns, succeeds |
| [Ordered Sequence](tasks/ordered-sequence.md) | `Steps` | Warns, succeeds |
| [Sub Flow](nodes/sub-flow.md) | `Sub Flow` | Warns, triggers `Completed` |

All four log a warning, so check `LogSimFlow`.

## A task fails immediately

[Place Object In Zone](tasks/place-object-in-zone.md) does this when its `Zone`
query found no zone. The warning distinguishes an empty query, a named actor that is
not a zone, a level with no zones in it, and zones that exist but are tagged
something else — the last case lists every zone and its identity tags.
[Ordered Sequence](tasks/ordered-sequence.md) does it when `Event Tag` is empty.

## Events don't reach the flow

The most common first bug is broadcasting `self` from inside a UMG widget graph. A
`UUserWidget` is neither an Actor nor an Actor Component, so it can never satisfy a
payload check. It logs a warning naming the offending class. Broadcast from the
owning actor with `Payload = self` instead.

After that: does the tag actually match, given `Match Child Tags` — a mismatch is
silent. Is the broadcast running at all, which a print node next to it will tell
you. Are you on a client, where events have to reach the authority and the
component's `Send Event` is the call that forwards. And did you target the right
flow — `Broadcast Flow Event` hits every running flow, while `Send Flow Event`
targets one by `Flow Save Id`. See [Events](events.md).

## Placement and zones

| Symptom | Cause |
|---|---|
| Objects count as placed while still held | Held state isn't wired. Call `Set Held(true/false)` from your grab logic — attachment alone is unreliable in VR |
| Nothing is ever tracked | No collision overlap, or the box is too small. Turn on the zone's `Draw Debug`; silver means nothing tracked |
| The player pawn is tracked | `Require Identity Component` was turned off |
| Objects settle then un-settle repeatedly | Jitter above `Settle Speed Threshold`. Raise `Settle Time` or the threshold |
| The wrong zone is used | Two zones share a tag, and tag queries take the first world match |

See [Zones](zones.md).

## Matching and identity

| Symptom | Cause |
|---|---|
| The right object isn't recognised | It has no [Identity](identity.md) component, or no tags on it |
| Everything is No Match, never a near miss | The tag tree is flat. Confusable things must share a parent — see [why depth matters](identity.md#why-depth-matters-more-than-it-looks) |
| `Require All Tags` blocks everything | With several tags requested the actor needs all of them. Set it to `false` for "any" |
| Tags on a placed instance don't apply to spawned copies | The component went on the level instance, not the Blueprint |
| Setting `Identity Id` didn't help | Nothing in SimFlow reads `Identity Id`. Use Identity Tags — see [Identity](identity.md#identity-id-and-what-it-isnt) |

## Conditions and branching

| Symptom | Cause |
|---|---|
| A branch case never fires | Its condition slot is empty, and an unset condition is false |
| Everything goes to `Default` | The conditions read a key that doesn't exist. Missing keys return `Result When Key Missing`, default `false` |
| An `All Of` always passes | It has no children. An empty AND is true |
| The wrong case fires | Cases are evaluated top to bottom and the first match wins. Put the most specific first |
| Execution stops at a branch | Nothing matched and `Has Default Pin` is off |

See [Conditions](conditions.md) and [Branch](nodes/branch.md).

## Blackboard

| Symptom | Cause |
|---|---|
| A value is never seen | Key name mismatch. Typos are silent — print the blackboard with `SimFlow.Debug 1` |
| A counter became `"111"` | The value type is `String` with `Add` on, so it concatenated. Use `Int` |
| An object reference is null after loading | Object values are stripped on save, by design |
| `Get Score` always returns 0 | `Score On Success` and `Score On Failure` default to `0` on every task. Scoring is opt-in |

See [Blackboard](blackboard.md).

## Loops and joins

| Symptom | Cause |
|---|---|
| The body runs once, then stops | The `Continue` pin isn't wired back. The classic Loop mistake |
| Log says "hit MaxIterations" | `Iterations` is 0 and the break condition never passes |
| The body never runs | The break condition is already true on entry — it's checked before each iteration |
| The loop stalls partway | Something in the body ended without returning to `Continue`, often an unwired `Failed` pin |

A flow stalling at a join is almost always `Num Inputs` set higher than the number
of branches that actually arrive. In `Wait For All` mode the join then waits
forever, in silence. Match `Num Inputs` to the branches you wired. See
[Loop](nodes/loop.md) and [Join](nodes/join.md).

## Editing and assets

| Symptom | Cause |
|---|---|
| Editing the flow mid-play changes nothing | Nodes are duplicated into the instance at start. Stop and restart the flow |
| Wires moved after adding a Branch case | Pins are named by index, so inserting in the middle renumbers later ones. Add at the end |
| The editor hangs on Play | A [Sub Flow](nodes/sub-flow.md) calls itself, directly or in a cycle. There's no recursion guard |
| No SimFlow entry in the Content Browser | The plugin isn't loaded, or the editor module failed to build. See [Getting started](getting-started.md) |

## Multiplayer

| Symptom | Cause |
|---|---|
| Clients see nothing | `Replicate Flow` is on but the owning actor doesn't replicate. Host on Game State or Player State |
| Client controls do nothing | The PlayerController has no **SimFlow Player Component**, so there's no route to the server |
| Save does nothing | Save and load are authority-only; on a client they log and return |
| Client UI has no blackboard values | `Replicate Blackboard` is off |

See the [component's network fields](simflow-component.md#fields).

## Still stuck

Run `SimFlow.Debug 1` and ask which node is actually active. Filter the Output Log
by `LogSimFlow`. Replace the suspect task with a
[Log Message](tasks/log-message.md) and see whether the flow reaches that node at
all. Then check the field the task needs isn't empty — most silent hangs are an
empty [Actor Query](actor-query.md) or an empty [condition](conditions.md), and
neither logs anything.

*Next: [Getting started](getting-started.md) · [Glossary](glossary.md)*
