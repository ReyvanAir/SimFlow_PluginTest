# SimFlow Component

**Component:** `SimFlow Component` (`USimFlowComponent`)
**Add via:** Add Component → SimFlow Component
**Class group:** SimFlow

The component runs a flow. It's the whole designer-facing runtime API — start,
pause, resume, retry, skip, fail, save, load.

Drop it on any actor, point it at a [flow asset](glossary.md), and choose when it
starts.

Which actor should host it depends on what you're building. A **Game Mode** suits a
single-player scenario, and has the advantage of existing before the level's actors
do. **A level actor** suits a flow tied to one room or machine. In multiplayer, the
**Game State** gives one shared scenario everyone sees and the **Player State**
gives a flow per trainee. A **VR pawn** works for flows that follow the player
between levels, with the caveat that the pawn may respawn.

For replication the host actor has to replicate itself, which is why Game State and
Player State are the recommended homes there.

## Fields

The two that matter most: **Flow Asset** (null by default) is the flow this
component runs, and **Entry Name** (`Default`) picks which [Start](nodes/start.md)
node to begin from.

Under **Start**:

| Field | Type | Default | Meaning |
|---|---|---|---|
| Start Mode | Enum | `Manual` | When the flow begins. |
| Auto Start Delay | Float (s, min 0) | `1.0` | Only shown when Start Mode is `Auto - After Delay`. |

Under **Network**:

| Field | Type | Default | Meaning |
|---|---|---|---|
| Replicate Flow | Bool | `false` | Run server-authoritatively and mirror to clients. Read-only at runtime. |
| Net Refresh Interval | Float (s, min 0.1) | `1.0` | How often the server refreshes the elapsed-time field. Structural changes replicate immediately. |
| Replicate Blackboard | Bool | `true` | Send the blackboard to clients so their UI can read score and answers. |

Under **Save**:

| Field | Type | Default | Meaning |
|---|---|---|---|
| Flow Save Id | Name | `None` | Identifies this flow inside a save file, and addresses it over the network. |
| Default Save Slot Name | String | `SimFlowSave` | Slot used by Quick Save and Quick Load. |
| Default Save User Index | Int | `0` | Platform user index. |
| Auto Resume From Save On Begin Play | Bool | `false` | When a save exists on BeginPlay, resume from it instead of starting fresh. |
| Default Load Mode | Enum | `Exact State` | `Exact State` or `From Last Checkpoint`. |

And under **Debug / Advanced**, **Show Debug HUD** (bool, `false`) draws this flow's
status on screen and is also toggled globally by `SimFlow.Debug`, while
**Follow Game Pause** (bool, `true`) pauses the flow when the game pauses, on the
authority only.

## Start Mode

`Manual` waits for you to call `Start Flow`. `Auto - On Begin Play` starts on
BeginPlay, `Auto - On First Tick` on the first tick after it, and
`Auto - After Delay` waits `Auto Start Delay` seconds.

Prefer **On First Tick** over **On Begin Play**. On BeginPlay, other actors in the
level may not have begun play yet, so a flow that immediately goes looking for a
[zone](zones.md) or an item can fail to find it. By the first tick every actor has
begun play, which makes it the safer default.

## Controls

All of these are safe to call from either side. On a client with `Replicate Flow`
on, they forward to the server through the local **SimFlow Player Component**, which
the PlayerController has to have for the forwarding to work.

| Function | Notes |
|---|---|
| Start Flow | Starts from `Entry Name` |
| Start Flow From Entry (Name) | Starts from a named [Start](nodes/start.md) node |
| Stop Flow | |
| Restart Flow | |
| Pause Flow / Resume Flow / Toggle Pause | |
| Retry Current Task | Honours the task's `Allow Retry` and `Max Retries` |
| Skip Current Task | Honours the task's `Allow Skip` |
| Fail Current Task | Drives the Task node's `Failed` pin |
| Send Event (Tag, Payload) | Raises an event tag on this flow |
| Submit Quiz Answer (Index) | Answers the [quiz](tasks/quiz.md) currently on screen |

## Save and load

Save and load are authority-only. On a client they log and return.

| Function | Notes |
|---|---|
| Save Flow State | Returns a save-state struct |
| Load Flow State (State, Load Mode) | Restores from a struct |
| Save Flow To Slot (Slot, User Index) | |
| Load Flow From Slot (Slot, User Index, Load Mode) | |
| Quick Save / Quick Load | Uses `Default Save Slot Name` and `Default Save User Index` |
| Has Save In Slot (Slot, User Index) | |

**Exact State** restores the exact set of active nodes, elapsed times included.
**From Last Checkpoint** restores the blackboard and re-runs the flow from the last
[checkpoint](nodes/checkpoint.md) the trainee passed.

Object references in the blackboard are stripped on save — see
[values and types](blackboard.md#values-and-types).

## Queries

These work on clients too, reading replicated state and resolving names,
instructions and quiz content from the flow asset every machine already has.

| Function | Returns |
|---|---|
| Get Flow Instance / Get Blackboard | The live objects |
| Get Run State | `Not Started`, `Running`, `Paused`, `Completed`, `Failed`, `Aborted` |
| Is Flow Running / Is Flow Paused | |
| Get Current Task / Get Current Task Name / Get Current Instruction | For tutorial UI |
| Get Progress | 0–1 |
| Get Score | The `Score` blackboard key |
| Get Current Task Remaining Time | Seconds left on the task's time limit, or `-1` when it has none |
| Get Current Quiz | The quiz being asked, or null. On clients this is the authored template. |
| Is Client Mirror / Has Flow Authority | Which side am I on? |
| Get Debug Text | Multi-line status, ready for a world-space VR debug widget |

## Events

**On Flow Started**, **On Flow Paused** and **On Flow Resumed** do what they say, and
**On Flow Finished** carries the final state. **On Task Started**,
**On Task Finished** and **On Task Retried** are what your tutorial UI should be
driven from. **On Checkpoint Reached** fires at a [checkpoint](nodes/checkpoint.md),
**On Quiz Presented** is where you show your quiz widget, and
**On Net State Changed** fires on clients whenever replicated state changes — the
server fires it too.

## A flow on the Game Mode

Running a tutorial flow as soon as the level is ready, with a debug HUD while you're
building it:

1. Open your Game Mode Blueprint.
2. **Add Component → SimFlow Component**.
3. Set **Flow Asset** to your flow.
4. Set **Start Mode** to **Auto - On First Tick**.
5. Set **Flow Save Id** to something stable, like `MainTutorial`.
6. Tick **Show Debug HUD** while building.
7. Press Play.

To let a button in the level advance the flow, call **Broadcast Flow Event** from
that Blueprint with the tag and `Payload = self`. [Events](events.md) covers the
rest.

## When it misbehaves

**Nothing happens on Play.** `Start Mode` is `Manual` and nothing calls
`Start Flow`, or `Flow Asset` is empty. This is the most common one.

**The flow starts but immediately can't find a zone or item.** `Start Mode` is
`Auto - On Begin Play`. Switch to On First Tick.

**The flow doesn't start and `Entry Name` looks fine.** The
[Start](nodes/start.md) node's own `Entry Name` was renamed. They have to match, and
both default to `Default`, so this only surfaces after a rename.

**Client controls do nothing in multiplayer.** The PlayerController has no
**SimFlow Player Component**, so there's no route to the server.

**Clients never receive any state.** `Replicate Flow` is on but the owning actor
doesn't replicate.

**Save does nothing in a multiplayer session.** Save and load are authority-only.

**Editing the flow asset mid-play changes nothing.** Nodes are duplicated into the
instance when the flow starts. Stop and restart to pick up authoring changes.

**Two flows both respond to a `Broadcast Flow Event`.** It raises the tag on every
running flow. To target one, use `Send Flow Event` with that flow's `Flow Save Id` —
which is also why `Flow Save Id` is worth setting whenever you have more than one
flow or you're replicating. Leave it at `None` and save/load still works through the
slot name, but `Find Flow By Id` and client control routing have no way to address
this flow.

*Next: [Getting started](getting-started.md) · [Events](events.md) ·
[Blackboard](blackboard.md) · [Troubleshooting](troubleshooting.md)*
