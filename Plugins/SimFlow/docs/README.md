# SimFlow documentation

A task and flow framework for Unreal Engine 5.6 — build training simulations,
tutorials and assessments as authored graphs rather than Blueprint spaghetti.

New here? Start with [Getting started](getting-started.md). It's about fifteen
minutes from install to a flow you can watch run.

## Core concepts

The ideas the rest of the documentation assumes.

| Page | What it covers |
|---|---|
| [Getting started](getting-started.md) | Install, build your first flow, make it react to the world |
| [Glossary](glossary.md) | Every term defined once, including node against task and asset against instance |
| [SimFlow Component](simflow-component.md) | The component that runs a flow: start modes, save, networking, controls |
| [Blackboard](blackboard.md) | The per-flow key/value store, and the well-known keys |
| [Events](events.md) | How the world talks to a running flow |
| [Conditions](conditions.md) | Reusable true/false tests used by branches, loops and waits |

### Recognising objects

The part of SimFlow that lets a flow say "the foam extinguisher" instead of "that
actor over there".

| Page | What it covers |
|---|---|
| [Identity](identity.md) | What an object is: tags, display names, held state, `Identity Id` |
| [Actor Query](actor-query.md) | Which object a task means, and how near misses are graded |
| [Zones](zones.md) | Named volumes that report what's inside, and when it has settled |

## Nodes

A node controls where execution goes. The full index is in the
[node reference](nodes/README.md).

| Category | Nodes |
|---|---|
| Flow Control | [Start](nodes/start.md) · [Delay](nodes/delay.md) · [Branch](nodes/branch.md) · [Random Branch](nodes/random-branch.md) · [Parallel](nodes/parallel.md) · [Join](nodes/join.md) · [Loop](nodes/loop.md) · [Finish](nodes/finish.md) |
| Tasks | [Task](nodes/task.md), the one you'll place most |
| Composition | [Sub Flow](nodes/sub-flow.md) |
| Persistence | [Checkpoint](nodes/checkpoint.md) |
| Data | [Set Blackboard Value](nodes/set-blackboard.md) |

## Tasks

A task is the work that happens inside a [Task node](nodes/task.md). The full index
is in the [task reference](tasks/README.md).

| Task | Purpose |
|---|---|
| [Wait For Event](tasks/wait-for-event.md) | Block until an event tag is raised, optionally by the right object |
| [Place Object In Zone](tasks/place-object-in-zone.md) | "Put the foam extinguisher in the bay." |
| [Ordered Sequence](tasks/ordered-sequence.md) | "Press these three buttons, in this order." |
| [Wait For Condition](tasks/wait-for-condition.md) | Block until a condition becomes true |
| [Go To Location](tasks/go-to-location.md) | Block until the player reaches a place |
| [Quiz](tasks/quiz.md) | A multiple-choice question |
| [Parallel Group](tasks/parallel-group.md) | Run several child tasks at once in one node |
| [Delay](tasks/delay.md) | Wait a number of seconds |
| [Log Message](tasks/log-message.md) | Print a message. Ideal for blocking out a flow |
| [Set Blackboard Value](tasks/set-blackboard.md) | Write a blackboard key |

## Worked examples

Complete scenarios showing several nodes and tasks together, indexed in
[workflows](workflows/README.md).

| Workflow | Shows |
|---|---|
| [Fire extinguisher drill](workflows/fire-extinguisher-drill.md) | Identity, zones, placement judging, near-miss feedback |
| [Valve startup procedure](workflows/valve-startup-procedure.md) | Ordered sequences, out-of-order handling, checkpoints |
| [Assessment with debrief](workflows/assessment-with-debrief.md) | Quizzes, score branching, sub-flows, a mistake debrief |

## When something's wrong

[Troubleshooting](troubleshooting.md) is organised by symptom — by what you're
actually seeing.

Two tools to reach for before anything else. In the console:

```
SimFlow.Debug 1
```

draws live flow state on screen: active node, current task, elapsed time and the
whole blackboard. And filter the Output Log by `LogSimFlow`, since most
misconfigurations log a warning naming the exact problem.

Most silent hangs are an empty [Actor Query](actor-query.md) or an empty
[condition](conditions.md). Neither logs anything, because both are legitimately
false rather than broken.

## Elsewhere

| Page | What it is |
|---|---|
| [Cheatsheet](Cheatsheet.md) | Dense quick reference — API calls, tags, gotchas |
| [Single-page guide](index.html) | The full long-form guide, including C++ extension points and internals |
| [README](../README.md) | Plugin overview, install, feature summary |
| [CHANGELOG](../CHANGELOG.md) | Version history |

Terms used in a specific way are defined once in the [glossary](glossary.md) and
linked rather than re-explained.
