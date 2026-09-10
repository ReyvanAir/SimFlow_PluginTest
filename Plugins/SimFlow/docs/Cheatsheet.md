# SimFlow cheat sheet

## Component API (Blueprint)

```
// Lifecycle
StartFlow()                    StartFlowFromEntry(Name)
StopFlow()                     RestartFlow()
PauseFlow()   ResumeFlow()     TogglePause()

// Player controls
RetryCurrentTask()  SkipCurrentTask()  FailCurrentTask()

// Events
SendEvent(GameplayTag, Payload)

// Save / load
QuickSave()  QuickLoad()
SaveFlowToSlot(Slot, UserIndex)
LoadFlowFromSlot(Slot, UserIndex, LoadMode)
HasSaveInSlot(Slot, UserIndex)
SaveFlowState() -> FSimFlowSaveState
LoadFlowState(State, LoadMode)

// Queries
GetRunState()  IsFlowRunning()  IsFlowPaused()
GetCurrentTask()  GetCurrentTaskName()  GetCurrentInstruction()
GetProgress()  GetScore()  GetBlackboard()  GetDebugText()
```

## Component events

`OnFlowStarted` · `OnFlowPaused` · `OnFlowResumed` · `OnFlowFinished(State)`
`OnTaskStarted(Node, Task)` · `OnTaskFinished(Node, Task, Result)` · `OnTaskRetried(Node, Task)`
`OnCheckpointReached(Checkpoint)` · `OnQuizPresented(Quiz)`

## Writing a task (Blueprint)

1. New Blueprint → parent class `SimFlowTask`.
2. Set `Display Name` and `Instruction` in Class Defaults.
3. Implement **On Task Start** — spawn markers, bind delegates, highlight objects.
4. Call **Finish Task** (Succeeded / Failed) when the player is done.
5. Implement **On Task End** to clean up. It runs on skip, fail and abort too.

Useful inside a task: `Get Blackboard`, `Get Flow Owner`, `Get Player Pawn`,
`Get Saved Value` / `Set Saved Value` (survives save/load).

## Writing a condition (Blueprint)

1. New Blueprint → parent class `SimFlowCondition`.
2. Implement **Evaluate** → return a bool.
3. Drop an instance into a Branch case, a Wait For Condition task, or a Task
   node's Abort Condition slot.

## Talking to the flow from the world

```
// In your grab component / button / anim notify:
Broadcast Flow Event (WorldContext, Tag = SimFlow.Event.Grab, Payload = self)
```

A `Wait For Event` task listening for `SimFlow.Event.Grab` (or any parent tag,
with Match Child Tags on) completes.

**Always pass the object as the payload.** Send `self` (an actor, or a component —
it is unwrapped to its owner). The sender stays neutral; the flow decides whether
it was the right one.

## Recognising which object

```
On the item Blueprint:  add SimFlow Identity, set Identity Tags = Item.Extinguisher.Foam
In the task:            Expected Payload / Accepted Items = an Actor Query
```

Actor Query resolves in this order — fill in one field:

| Field | Means |
|---|---|
| `Specific Actor` | that one placed actor |
| `Blackboard Key` | a target picked at runtime |
| `Required Tags` | that kind of thing — covers spawned copies |
| `Required Class` / `Required Actor Tag` | narrowing / actors you cannot modify |

Match quality, because tags nest:

| Actor is | vs. expected `Item.Extinguisher.Foam` |
|---|---|
| `Item.Extinguisher.Foam` | `Exact` — passes |
| `Item.Extinguisher.CO2` | `Related` — near miss, right family |
| `Item.Tool.Wrench` | `No Match` |

`Min Related Tag Depth` (default 2) sets the near-miss threshold.

## Wrong-answer policies

| Task | Property | Options |
|---|---|---|
| Wait For Event | `Mismatch Policy` | Ignore / Count Mistake / Fail Task |
| Place Object In Zone | `Wrong Item Policy` | Ignore / Count Mistake / Fail Task |
| Ordered Sequence | `Out Of Order Policy` | Ignore / Count Mistake / Restart Sequence / Fail Task |

*Count Mistake* records the error and keeps waiting, so the trainee can correct
themselves. That is usually what you want in training; *Fail Task* drives the
node's `Failed` pin for a remediation branch.

## Zones

```
Drop an ASimFlowZone, size the Box, set its Identity Tags = Zone.PartsBin
```

| Delegate | Fires when |
|---|---|
| `On Actor Entered` | overlap begins — may still be in the trainee's hand |
| `On Actor Settled` | not held, and at rest for as long as `Settle Mode` asks |
| `On Actor Exited` | overlap ends |

`Settle Mode`: `Instant` / `Standard` (0.35 s, speed 20) / `Custom`.
`Place Object In Zone` listens to **Settled** by default (`Require Settled`).

## Mistakes

```
Get Mistakes           → TArray<FSimFlowMistake> for a debrief screen
Get Mistakes Of Kind   → filter, e.g. SimFlow.Mistake.WrongItem
Get Mistake Count      → the number
On Mistake Recorded    → live delegate for an instructor HUD
```

Each entry: `Kind`, `Description`, `Severity`, `Involved`, `InvolvedName`,
`TimeSeconds`, `TaskId`. Cleared on `StartInstance`; saved with the flow, minus
the live object pointer.

In your own Blueprint task, call **Record Mistake** or **Apply Mismatch Policy**.

## Blackboard keys the built-ins use

| Key | Written by |
|---|---|
| `Score` | `ScoreOnSuccess` / `ScoreOnFailure` on any task, `AddScore` |
| `Mistakes` | any recorded mistake (Quiz, wrong item, wrong target, wrong order) |
| `LastResult` | every task when it finishes |
| `LastAnswerIndex`, `LastAnswerCorrect` | Quiz task |
| `WrongAttempts` | a task rejecting the wrong object |
| `CurrentStep` | Ordered Sequence task |

## Multiplayer

```
Component:  bReplicateFlow = true, on a REPLICATED actor (GameState / PlayerState)
PlayerController BP:  add a SimFlow Player Component
```

Client-safe calls (they forward to the server themselves):

```
StartFlow / StopFlow / RestartFlow
PauseFlow / ResumeFlow / TogglePause
RetryCurrentTask / SkipCurrentTask / FailCurrentTask
SendEvent(Tag, Payload)
SubmitQuizAnswer(Index)
Broadcast Flow Event (statics)
Request Flow Control (statics)   <- instructor panels
```

Server-only: `SaveFlowToSlot`, `LoadFlowFromSlot`, `QuickSave`, `QuickLoad`.

Client-side reads (all resolve from the replicated state + the local flow asset):

```
GetRunState / IsFlowRunning / IsFlowPaused
GetCurrentTask / GetCurrentTaskName / GetCurrentInstruction
GetCurrentQuiz / GetCurrentTaskRemainingTime
GetProgress / GetScore / GetBlackboard
GetNetState / IsClientMirror / HasFlowAuthority
OnNetStateChanged   <- fires when the server's state arrives
```

Gate instructor-only controls by overriding **Is Request Authorised** on the
SimFlow Player Component. Client requests are never trusted; they all pass
through it on the server.

## Console

```
SimFlow.Debug 1     on-screen status overlay for every running flow
SimFlow.Debug 0     off
```

## Gotchas

* A Task node's unwired `Failed` / `Skipped` / `TimedOut` pins fall through to
  `Completed`. Turn off `bFallbackToCompleted` if you want a hard stop instead.
* `Join` in *Wait For Any* mode stays alive after firing so it can absorb the
  losing branch. The losing branch's task keeps running until the flow ends — for
  a timeout on a single task, prefer the Task node's own `TimeLimit`.
* Object references in the blackboard are not saved. Re-resolve them in
  **On Load Task State**.
* The editor graph is stripped from packaged builds; only the node data ships.
  Never store gameplay state on the graph node — put it on the runtime node.
* In multiplayer, `GetCurrentTask()` on a client returns the **authored template**
  task, not a live one. Its text is correct; its runtime counters are not — read
  those from `GetNetState()` instead.
* `Accept Already Raised` is **ignored** when a Wait For Event task has an
  `Expected Payload` — a past event kept only its tag, so there is no object left
  to check and honouring the flag would let the wrong one through. A verbose log
  line says so.
* `Specific Actor` is a soft pointer resolved without a sync load. An actor in an
  unloaded World Partition cell will not resolve — use tags or a blackboard key
  for streamed content.
* **Tell the Zone when an item is held.** On grab: `SimFlow Identity → Set Held (true)`.
  On release: `false`. Without it the Zone falls back to `GetAttachParentActor()`, which
  is wrong for any grab system that uses a physics constraint instead of reparenting —
  VRExpansion's default grip among them — and a trainee can hold an item over the zone
  and pass the step without letting go.
* A zone only sees items whose collision responds to `OverlapAllDynamic`.
* **A widget cannot be a payload.** `self` in a UMG graph is a `UUserWidget` — neither an
  Actor nor an Actor Component — so any Actor Query scores `No Match` against it and the
  task never completes. Broadcast from the owning actor instead. The log names the
  offending class when this happens.
