# Parallel Group

**Class:** `USimFlowTask_ParallelGroup`
**Select via:** [Task node](../nodes/task.md) → **Task** dropdown → **Parallel Group**

Runs several child tasks at once inside a single node.

Reach for it when you want a small parallel group without cluttering the graph with
[Parallel](../nodes/parallel.md) and [Join](../nodes/join.md) nodes. "Play the
narration while the trainee dons the helmet" becomes one node rather than five.

## Fields

| Field | Type | Default | Meaning |
|---|---|---|---|
| Tasks | Array of instanced tasks | *empty* | The child tasks. Each gets its own class dropdown and inline settings. |
| Wait For All | Bool | `true` | With this off, the group finishes as soon as the first child does. |
| Fail If Any Child Fails | Bool | `true` | Any child failing fails the whole group. |

Plus the [fields every task has](README.md#the-fields-every-task-has).

An empty `Tasks` array leaves nothing to wait for and no child to report completion,
so the group never advances on its own. Treat that as a configuration error rather
than a way to no-op.

With `Wait For All` on, a child that never finishes holds the group forever; put a
`Time Limit` on the [Task node](../nodes/task.md). With it off, the first child to
finish ends the group *and* the others with it, so don't rely on their side effects
having completed.

## What the children inherit

Child tasks are full tasks, so each carries its own
[presentation, rules and scoring fields](README.md#the-fields-every-task-has). Three
consequences are worth knowing.

Each child's scoring applies. Three children with `Score On Success` of 10 add 30,
on top of whatever the group itself scores.

Pause and resume propagate to every child.

The group's `Time Limit` lives on the [Task node](../nodes/task.md), not on the
children, and children have no individual timeout. That's the main limitation here:
you can't give one child of a group its own time limit. If you need per-branch
timeouts, use real [Parallel](../nodes/parallel.md) and [Join](../nodes/join.md)
nodes so each branch gets its own Task node.

## Group, or Parallel and Join?

| Use Parallel Group | Use [Parallel](../nodes/parallel.md) + [Join](../nodes/join.md) |
|---|---|
| Two or three simple concurrent tasks | Branches with several steps each |
| You want the graph compact | You want to see the structure in the graph |
| The branches share one outcome | Each branch needs its own timeout, retry or routing |
| No per-child failure routing needed | You need a `Failed` pin per branch |

## Narration alongside a physical step

A narration line plays while the trainee puts on the helmet. Moving on when the
helmet is on — the narration shouldn't hold things up.

1. Add a [Task node](../nodes/task.md) with **Task** = **Parallel Group**.
2. Under **Tasks**, click **+** twice.
3. Child 0 is [Wait For Event](wait-for-event.md), event tag `SimFlow.Event.Grab`,
   with Expected Payload → Required Tags of `Item.Helmet`.
4. Child 1 is [Delay](delay.md) with a Duration of `8` — the length of the narration
   — with your audio triggered from the Instruction UI.
5. Turn **Wait For All** off, so donning the helmet early moves things along.
6. Turn **Fail If Any Child Fails** off, since the delay can't fail.
7. Set **Display Name** to `Don the helmet`.

If instead the trainee must both don the helmet *and* hear the whole briefing, leave
**Wait For All** on.

## If the group ends at the wrong time

**The group never finishes.** `Wait For All` is on and a child is waiting for
something that never arrives.

**A child's work got cut short.** `Wait For All` is off, so the first child to
finish ended the group and took the others with it.

**The score is higher than expected.** Every child's `Score On Success` contributes,
on top of the group's own.

**One child needs its own timeout.** Not possible inside a group. Use
[Parallel](../nodes/parallel.md) and [Join](../nodes/join.md) nodes.

**A child failure failed everything.** `Fail If Any Child Fails` is on by default.
Turn it off for optional children.

*Next: [Parallel node](../nodes/parallel.md) · [Join node](../nodes/join.md) ·
[Task node](../nodes/task.md)*
