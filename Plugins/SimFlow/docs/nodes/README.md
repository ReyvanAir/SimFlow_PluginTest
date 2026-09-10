# Node reference

A node is a box in the flow graph. Nodes control where execution goes; a
[task](../tasks/README.md) is the work that happens inside a [Task node](task.md).

Right-click in the graph to add one. The menu is grouped by the categories below.

## Flow Control

| Node | Purpose |
|---|---|
| [Start](start.md) | Where execution begins. Every flow needs one. |
| [Delay](delay.md) | Waits, then continues. |
| [Branch](branch.md) | Takes the first output whose condition passes. |
| [Random Branch](random-branch.md) | Picks one output at random, optionally weighted. |
| [Parallel](parallel.md) | Fires every output at once. |
| [Join](join.md) | Converges parallel branches — wait for all, or race. |
| [Loop](loop.md) | Repeats a section of the graph. |
| [Finish](finish.md) | Ends the whole flow with a result. |

## Tasks

| Node | Purpose |
|---|---|
| [Task](task.md) | Runs a single [task](../tasks/README.md). The one you'll place most. |

## Composition

| Node | Purpose |
|---|---|
| [Sub Flow](sub-flow.md) | Runs another flow asset as a child. This is what makes flows modular. |

## Persistence

| Node | Purpose |
|---|---|
| [Checkpoint](checkpoint.md) | Marks a safe resume point, and can auto-save. |

## Data

| Node | Purpose |
|---|---|
| [Set Blackboard Value](set-blackboard.md) | Writes a [blackboard](../blackboard.md) key inline in the graph. |

## What every node has in common

Execution arrives at an input pin and leaves through an output pin. Most nodes have
a single `In` and a single `Out`; the interesting ones have more.

An output pin with nothing wired to it ends that line of execution. That isn't an
error — a flow can legitimately have branches that stop — which is why a misrouted
pin fails quietly instead of loudly. When a section of your flow never runs, an
unwired pin is the first thing to check.

Every node also carries a **Node Comment**, a free-form multi-line string drawn on
the node in the graph. It's purely for whoever reads the asset next.

The lifecycle is short. Execution arrives at an input pin and calls `Execute Input`.
The node may then stay active, ticking every frame — Task, Delay and Sub Flow all
do. Eventually it pushes execution out through an output pin, which normally
deactivates it.

Nodes are duplicated per running flow, so they can safely hold runtime state and two
actors can run the same asset independently. It's also why editing an asset mid-play
changes nothing until you restart; see
[asset against instance](../glossary.md) in the glossary.

Most nodes ignore execution arriving while they're already active. **Join** and
**Loop** are the exceptions, built to be re-entered — which is exactly how a loop
body returns to its loop node.

## Node or task?

Several capabilities exist in both forms. There's a Delay node and a Delay task, a
Set Blackboard node and a Set Blackboard task.

| Use the node when | Use the task when |
|---|---|
| It's a simple step in the graph | You want the [Task node](task.md)'s timeout, retry or abort handling around it |
| You want the graph to read clearly | You want it inside a [Parallel Group](../tasks/parallel-group.md) |
| You don't need a `Failed` pin | You want scoring, an instruction, or a `Task Id` |

The node forms are lighter, the task forms richer. For a bare pause, the
[Delay node](delay.md) is the better choice.

*Next: [Task reference](../tasks/README.md) · [Conditions](../conditions.md) ·
[Glossary](../glossary.md)*
