# Glossary

Every term SimFlow's documentation uses in a specific way, defined once. Other pages
link here rather than re-explaining.

| Term | Meaning |
|---|---|
| Flow asset | The authored graph you create in the Content Browser (`SimFlow Graph`). A template — it holds no runtime state. |
| Flow instance | The running copy of a flow asset, created by a SimFlow Component when the flow starts. Nodes and tasks are duplicated into it, so two actors can run the same asset independently. |
| Node | A box in the graph. Controls where execution goes — branch, loop, wait, finish. See the [node reference](nodes/README.md). |
| Task | The unit of work, held inside a Task node. "Wait for the trainee to press the button" is a task. See the [task reference](tasks/README.md). |
| Condition | A reusable true/false test, used by Branch cases, Loop break conditions and the Wait For Condition task. See [Conditions](conditions.md). |
| Pin | A connection point on a node. Execution arrives at an input pin and leaves through an output pin. |
| Blackboard | The per-flow key/value store. Holds score, quiz answers, and anything else you want to remember across tasks. See [Blackboard](blackboard.md). |
| Identity | The SimFlow Identity component on an actor, carrying the gameplay tags that say what that object is. See [Identity](identity.md). |
| Identity tag | A gameplay tag on an identity component, e.g. `Item.Extinguisher.Foam`. |
| Actor Query | The "which object do I mean?" struct used by tasks and zones. See [Actor Query](actor-query.md). |
| Match quality | How well an actor answered a query: `No Match`, `Related (Near Miss)`, or `Exact Match`. |
| Zone | A `SimFlow Zone` actor — a named volume that reports what is inside it. See [Zones](zones.md). |
| Settled | A zone's judgement that an object has actually been put down inside it, rather than merely overlapping while still held. |
| Held | Flagged by `Set Held` on an identity component. A held object never counts as settled. |
| Mistake | A recorded thing the trainee got wrong, with a timestamp, a severity and a description. Collected on the flow instance for a debrief. |
| Entry | A `Start` node. A flow may have several, each with its own `Entry Name`. |
| Checkpoint | A marked safe resume point in a flow, which can auto-save. |
| Authority | The machine actually executing a flow. In single player that's always you; with replication on, it's the server. |
| Trainee | The person using the finished simulation, as distinct from the author building the flow. |

## Two distinctions worth getting right early

**Node against task.** A Task *node* is a box in the graph with `Completed`,
`Failed`, `Skipped` and `Timed Out` output pins. The *task* is the object you drop
into that node's `Task` field, and it's what actually does the work. The node
handles retry, timeout and routing; the task handles the work. Several built-ins
exist in both forms — there's a Delay node and a Delay task — and
[the Delay node page](nodes/delay.md) covers when to use which.

**Asset against instance.** Editing a flow asset while the game runs changes
nothing, because the instance duplicated the nodes when it started. Stop and restart
the flow to pick up authoring changes.

*Next: [Documentation index](README.md) · [Getting started](getting-started.md)*
