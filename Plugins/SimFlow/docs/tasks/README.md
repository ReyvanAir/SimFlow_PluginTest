# Task reference

A task is the unit of work in a flow. It lives inside a
[Task node](../nodes/task.md), which handles retry, timeout and routing while the
task gets on with the job.

You don't create task assets. Select a [Task node](../nodes/task.md), open the
**Task** dropdown in the Details panel and pick a class; its settings appear inline
underneath.

## The built-in tasks

| Task | Purpose |
|---|---|
| [Delay](delay.md) | Waits a number of seconds. |
| [Log Message](log-message.md) | Prints a message. Good for blocking out a flow. |
| [Set Blackboard Value](set-blackboard.md) | Writes a [blackboard](../blackboard.md) key. |
| [Wait For Event](wait-for-event.md) | Blocks until an event tag is raised, optionally by the right object. |
| [Wait For Condition](wait-for-condition.md) | Blocks until a [condition](../conditions.md) becomes true. |
| [Go To Location](go-to-location.md) | Blocks until the player reaches a place. |
| [Quiz](quiz.md) | A multiple-choice question. |
| [Parallel Group](parallel-group.md) | Runs several child tasks at once inside one node. |
| [Place Object In Zone](place-object-in-zone.md) | "Put the foam extinguisher in the bay." |
| [Ordered Sequence](ordered-sequence.md) | "Press these three buttons, in this order." |

## The fields every task has

These come from the task base class and sit above each task's own settings.

Under **Presentation**: **Display Name** (text, empty) shows in the graph, the debug
HUD and your tutorial UI, falling back to the class name; **Instruction**
(multi-line text, empty) holds the longer wording like "Pick up the fire
extinguisher", which you read back with `Get Current Instruction`; and **Task Id**
(name, `None`) is an optional tag so UI and analytics can identify this task. Any
[mistake](../glossary.md) records the Task Id it happened under.

Under **Rules**:

| Field | Type | Default | Meaning |
|---|---|---|---|
| Allow Retry | Bool | `true` | Whether the player may retry this task through the component. |
| Max Retries | Int (min 0) | `0` | `0` is unlimited. Once exceeded, a retry request fails the task instead. |
| Allow Skip | Bool | `true` | Whether the player or instructor may skip it. |
| Tick While Paused | Bool | `false` | Keeps ticking through a pause. Leave it off unless you know you want it. |

Under **Scoring**: **Score On Success** and **Score On Failure**, both floats
defaulting to `0.0`, added to the `Score` blackboard key when the task succeeds, or
when it fails or times out. The failure one is usually negative.

Both scoring fields default to zero, so a flow scores nothing until you set them.
That's the answer to "why does `Get Score` always return 0".

## The task lifecycle

**On Task Start** fires as the task becomes active — do your setup there.
**On Task Tick** runs every frame while the task is running and not paused, and
**On Task Pause** / **On Task Resume** mirror the flow's pause state. You call
**Finish Task** with a result when the work is done, and **On Task End** fires
afterwards however it ended, which is where bindings get cleaned up.

`Finish Task` is guarded against being called twice in one activation, so a
double-fire from an event binding does no harm.

While a task runs you can read `Elapsed Time` (which excludes paused time),
`Retry Count`, `Is Running`, `Is Paused`, plus `Flow Instance` and `Owning Node`.

## Judging the wrong object

Three of the built-in tasks decide whether the trainee acted on the right object,
and they share a vocabulary for what to do when the answer is no.

**Mismatch Policy**, used by [Wait For Event](wait-for-event.md) and
[Place Object In Zone](place-object-in-zone.md):

| Policy | Behaviour |
|---|---|
| Ignore (Keep Waiting) | Say nothing and keep waiting. The wrong object simply isn't the one we want. |
| Count Mistake (Keep Waiting) | Record a [mistake](../glossary.md) and keep waiting, so the trainee can correct themselves. The default. |
| Count Mistake And Fail Task | Record a mistake and fail, driving the node's `Failed` pin. |

**Out Of Order Policy**, used by [Ordered Sequence](ordered-sequence.md):

| Policy | Behaviour |
|---|---|
| Ignore | Ignore anything that isn't the expected step. |
| Count Mistake (Stay On Step) | Record a mistake but hold on the current step. The default. |
| Count Mistake And Restart | Record a mistake and send the trainee back to step one. |
| Count Mistake And Fail Task | Record a mistake and fail the task. |

## Writing your own

Tasks are `Blueprintable`. Create a Blueprint Class from **SimFlow Task**, override
**On Task Start** for your setup, call **Finish Task** with a result when the work
is done, and override **On Task End** to unbind whatever you bound. Your class then
appears in the Task dropdown on its own.

Retry, skip, timeout, pause and scoring all come free from the
[Task node](../nodes/task.md). The base class also gives you `Get Flow Owner` for
the actor owning the flow component, `Get Player Pawn` (the VR pawn in a VR
project), `Get Blackboard`, `Record Mistake(Kind, Involved, Description, Severity)`,
`Apply Mismatch Policy(...)` for the shared right-event-wrong-object handling, and
`Set Saved Value` / `Get Saved Value` for state that has to survive a save and load.

*Next: [Task node](../nodes/task.md) · [Node reference](../nodes/README.md) ·
[Glossary](../glossary.md)*
