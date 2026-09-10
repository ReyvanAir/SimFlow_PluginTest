# SimFlow — modular task & flow framework for Unreal Engine 5.6

**v1.1.4** — simpler placement setup: one `Settle Mode` on a zone in place of
five settling and filtering fields, and four lead fields on *Place Object In
Zone*. See [`CHANGELOG.md`](CHANGELOG.md).

Looking for the previous release? It is tagged [`v1.1.3`](https://github.com/ReyvanAir/SimFlow/tree/v1.1.3),
with a `release/1.1.3` branch alongside it.

A data-driven system for building VR simulations, tutorials and any gameplay that
is really a *sequence of things the player has to do*. Author flows in a node
graph, run them from a component, and drive your VR UI from the events they fire.

Built for UE **5.6** (also compiles on 5.4/5.5 — see *Engine compatibility* below).

**Full documentation:** [reyvanair.github.io/SimFlow](https://reyvanair.github.io/SimFlow/)

The Markdown cheat sheet is in [`docs/Cheatsheet.md`](docs/Cheatsheet.md).

---

## What you get

| Requirement | How SimFlow covers it |
|---|---|
| Auto & manual start | `Start Mode` on the flow component: Manual, On Begin Play, On First Tick, After Delay |
| Pause / Resume | `PauseFlow()` / `ResumeFlow()` / `TogglePause()`, optionally following the game pause |
| Retry / Skip / Fail | `RetryCurrentTask()`, `SkipCurrentTask()`, `FailCurrentTask()`, plus per-task retry limits and auto-retry |
| Sequential tasks | Chain Task nodes — the default and simplest case |
| Parallel tasks | `Parallel` + `Join` nodes, or a `Parallel Group` task |
| Condition-based execution | `Branch` node with pluggable `SimFlowCondition` objects; quiz wrong answers, timeouts and score thresholds all covered |
| Branching | `Branch`, `Random Branch`, `Loop`, and per-result output pins on every Task node |
| Save & load | `FSimFlowSaveState` + `USimFlowSaveGame`, exact-state or from-last-checkpoint resume, `Checkpoint` node with auto-save |
| Modularity | `Sub Flow` node runs another flow asset; tasks and conditions are Blueprint-subclassable |
| Multiplayer | Optional server-authoritative replication with client mirroring — see *Multiplayer* below |
| Knowing *which* object | `SimFlow Identity` component + `Actor Query` — "that button" or "any foam extinguisher" |
| Detecting the **wrong** answer | `Mismatch Policy` on Wait For Event, `Wrong Item Policy` on Place Object, `Out Of Order Policy` on Ordered Sequence |
| Placement checking | `SimFlow Zone` + `Place Object In Zone` task, with hand-release and settle detection |
| Ordered procedures | `Ordered Sequence` task — press these five in this order, out-of-order input is a first-class outcome |
| Debrief / grading | `FSimFlowMistake` list on the instance: what, when, how wrong — persisted in the save |

---

## Installing

1. Copy the `SimFlow` folder into `YourProject/Plugins/SimFlow`.
2. Right-click your `.uproject` → **Generate Visual Studio project files** (or run
   `GenerateProjectFiles`).
3. Build the editor target. SimFlow is a C++ plugin, so the project needs a C++
   module. If yours is Blueprint-only, add any empty C++ class once from the editor
   and Unreal will convert it.
4. Enable **SimFlow** in *Edit → Plugins* if it is not on already.

---

## Five-minute quick start

1. **Content Browser → right-click → SimFlow → SimFlow Graph.** You get an asset
   with a `Start` node already in it.
2. Open it. Right-click the graph → **Tasks → Task**. Wire `Start.Out` into
   `Task.In`.
3. Select the Task node. In the Details panel set **Task** to one of the built-ins
   (`Delay`, `Log Message`, `Quiz`, `Wait For Event`, `Go To Location`, …) or to a
   Blueprint subclass of `SimFlowTask` you wrote yourself.
4. Add a **Finish** node and wire `Task.Completed` into it.
5. Put a **SimFlow Component** on an actor (your Game Mode, a level actor, or the VR
   pawn). Set **Flow Asset** to your graph and **Start Mode** to *Auto - On Begin Play*.
6. Press Play. Type `SimFlow.Debug 1` in the console to see live state on screen.

Or skip steps 1–4: **Tools → SimFlow → Create Sample VR Tutorial Flow** builds a
complete example that exercises every feature, and opens it.

---

## Concepts

### Flow asset
The authored graph. It is a template: nothing in it is mutated at runtime.

### Flow instance
A running copy. The component creates one and it duplicates every node, so two
actors can run the same flow independently, and a `Sub Flow` node can nest another.

Execution is **queue driven**, not recursive — a chain of a thousand instant nodes
will not blow the stack, and a runaway loop is caught and reported rather than
hanging the editor.

### Nodes

| Node | What it does |
|---|---|
| **Start** | Entry point. Several are allowed; each has a name you can pass to `StartFlowFromEntry`. |
| **Task** | Runs one `SimFlowTask`. Outputs: `Completed`, `Failed`, `Skipped`, `TimedOut`. Has `TimeLimit`, auto-retry and an optional abort condition. |
| **Delay** | Waits. Respects pause. |
| **Branch** | Evaluates conditions in order, leaves through the first match (or `Default`). Can fire *all* matches to fan out. |
| **Random Branch** | Weighted random output, with optional no-repeats. |
| **Parallel** | Fires every output at once. |
| **Join** | *Wait For All* to converge, or *Wait For Any* for the race/timeout pattern. |
| **Loop** | `LoopBody` / `Continue` / `Completed`, with an iteration count or a break condition. |
| **Sub Flow** | Runs another flow asset, optionally sharing the blackboard both ways. |
| **Checkpoint** | Marks a resume point and can auto-save. |
| **Set Blackboard Value** | Writes a key inline, no task object needed. |
| **Finish** | Ends the flow as Complete / Fail / Abort. |

Any output pin you leave unwired on a Task node falls through to `Completed`, so a
straightforward linear tutorial stays visually clean.

### Tasks
`USimFlowTask` is the unit of work. Subclass it in Blueprint, implement
**On Task Start**, and call **Finish Task** when the player has done the thing.
`On Task Tick`, `On Task Pause`, `On Task Resume`, `On Task End` and `On Task Retry`
are there when you need them.

Built-ins: `Delay`, `Log Message`, `Set Blackboard Value`, `Wait For Event`,
`Wait For Condition`, `Go To Location`, `Quiz`, `Parallel Group`,
`Place Object In Zone`, `Ordered Sequence`.

Per-task settings worth knowing: `Display Name` and `Instruction` (what your VR
panel shows), `bAllowRetry` / `MaxRetries`, `bAllowSkip`, `ScoreOnSuccess` /
`ScoreOnFailure`.

### Conditions
`USimFlowCondition` answers yes/no. Drop instances straight into a Branch case, a
Wait For Condition task, or a Task node's abort slot.

Built-ins: `Blackboard Compare`, `Score Threshold`, `Last Task Result Is`,
`Elapsed Time`, `Event Was Raised`, `All Of (AND)`, `Any Of (OR)`, `Constant`,
`Player Near Location`. Every one has a `bInvert` box so you rarely need a NOT.

### Recognising objects

Flows need to name things in the level — *that* button, *a* foam extinguisher —
and the plugin keeps that separate from what your Blueprints do.

**The rule: Blueprint reports neutral facts, the flow asset decides if they were
correct.** A button broadcasts that it was pressed and sends itself as the
payload; it knows nothing about the current exercise. The flow holds the answer.
Change which button is right by editing one field in the flow asset — no
Blueprint touched, and the same room works for a dozen different scenarios.

Add a **SimFlow Identity** component to the item Blueprint (not to each level
instance) and set its `Identity Tags`:

```
BP_Extinguisher_Foam  →  Item.Extinguisher.Foam
BP_Extinguisher_CO2   →  Item.Extinguisher.CO2
BP_Wrench             →  Item.Tool.Wrench
```

Tasks then refer to objects through an **Actor Query**, which resolves in order:

| Field | Use it for |
|---|---|
| `Specific Actor` | "that button" — one placed level actor |
| `Blackboard Key` | a target chosen at runtime (randomised assignments) |
| `Required Tags` | "any foam extinguisher" — the only form that covers spawned copies |
| `Required Class` / `Required Actor Tag` | narrowing, and actors you cannot add a component to |

Because tags nest, one field gives you three levels of strictness —
`Item.Extinguisher` accepts either extinguisher, `Item.Extinguisher.Foam` only
one — and it grades *how wrong* a mistake was:

| Player used | vs. `Item.Extinguisher.Foam` | Feedback you can give |
|---|---|---|
| `Item.Extinguisher.Foam` | `Exact` | correct |
| `Item.Extinguisher.CO2` | `Related` | "Right idea — wrong agent for this fire class." |
| `Item.Tool.Wrench` | `No Match` | "That isn't a fire extinguisher." |

`Min Related Tag Depth` (default 2) sets how many leading tag nodes two tags must
share to count as a near miss.

### Zones and placement

Drop a **SimFlow Zone** in the level, size the box, and give its identity
component a tag like `Zone.PartsBin`. The zone reports what is inside *without
judging it* — the task does the judging.

A trainee holding an item over the bin has not put it down, so the zone
distinguishes overlapping from placed. How patient it is about that is one field,
`Settle Mode`: *Instant* for sockets and snap points, *Standard* (a 0.35 s pause,
and physics objects below speed 20) for anything put down by hand, *Custom* to
dial in `Settle Time`, `Settle Speed Threshold` and `Require Detached` yourself.
Pick the item back up and the timer restarts.

Tell it when an item is held — `SimFlow Identity → Set Held (true)` where your grab
succeeds, `false` on release. The zone can otherwise only guess from attachment,
which is wrong for any framework that grips with a physics constraint rather than
reparenting the actor (VRExpansion's default grip included), and a trainee could
hold an item steady over the zone and pass without letting go.

The **Place Object In Zone** task ties it together, and needs four fields: the
`Zone`, the `Accepted Items`, a `Required Count`, and a `Wrong Item Policy` of
*Ignore*, *Count Mistake* (keep waiting, let them correct themselves) or
*Fail Task*. Bind `On Wrong Item Placed` for the buzzer or hint — it passes the
match quality, so a near miss and a random prop can say different things.

Anything `Accepted Items` does not match is already wrong, so the optional
`Rejected Items` is for one job only: subtracting from a family you otherwise
accept. Take `Item.Extinguisher` but reject `Item.Extinguisher.CO2`, and the rule
still holds when someone adds a new extinguisher variant later.

### Ordered procedures

**Ordered Sequence** handles "press these five in this order" — startup
checklists, lockout/tagout, pre-flight. Every candidate broadcasts the same event
tag with itself as the payload; the task holds the order. `Out Of Order Policy`
is *Ignore*, *Count Mistake* (stay on the step), *Restart Sequence* (back to step
one) or *Fail Task*. Doing step 4 when step 2 was expected is recorded as a near
miss; touching an unrelated prop is ignored unless you set
`Unlisted Input Is Mistake`.

### Mistakes and debrief

Trainees are graded on what they got wrong, so mistakes are a real record rather
than a counter. Each `FSimFlowMistake` carries the kind tag, a ready-to-show
description, the severity, the object involved and the time into the run. They
live on the instance, survive save/load, and fire `On Mistake Recorded` for an
instructor dashboard.

```
Get Mistakes            → the whole list, for a debrief screen
Get Mistakes Of Kind    → e.g. everything tagged SimFlow.Mistake.WrongItem
Get Mistake Count       → the quick number
```

The `Mistakes` blackboard key is still incremented alongside, so conditions you
have already written keep working.

### Blackboard
Per-run key/value store: score, quiz answers, whatever your scenario needs. It is
what conditions read and what gets serialised into a save. Well-known keys the
built-ins use: `Score`, `Mistakes`, `LastResult`, `LastAnswerIndex`,
`LastAnswerCorrect`, `WrongAttempts`, `CurrentStep`.

### Events
Gameplay tags are how the world talks to the flow. From a grab component, a
button, an anim notify — anywhere — call **Broadcast Flow Event** with a tag.
A `Wait For Event` task listening for that tag (or a parent of it) completes.

Native tags shipped with the plugin live in `SimFlowGameplayTags.h`
(`SimFlow.Event.Grab`, `SimFlow.Event.Interact`, `SimFlow.Event.Placed`,
`SimFlow.Mistake.WrongItem`, …). Add your own there or in the Gameplay Tags
project settings.

**Send the object with the tag.** `Broadcast Flow Event` takes a payload — pass
the actor (or the component; it is unwrapped to its owner). A Wait For Event task
with an `Expected Payload` query then accepts only the intended sender, which is
what lets every button in a room share one tag. Leave the query empty and any
sender satisfies the task, exactly as before 1.1.2.

---

## Common patterns

**Timeout on a task** — set `TimeLimit` on the Task node and wire the `TimedOut`
pin wherever the remediation lives.

**Timeout on a whole section** — `Parallel` into the section and a `Delay`, then a
`Join` set to *Wait For Any*. Whichever finishes first wins.

**Quiz with remediation** — Quiz task, `Failed` pin into a hint task, hint task's
`Completed` back into the Quiz node's `In`. Set `MaxRetries` on the quiz to bound it.

**Pass/fail on score** — `Branch` with a `Score Threshold` condition; `Default`
pin goes to a Finish node in Fail mode.

**Reusable modules** — build "put on the PPE" once as its own flow asset, then drop
a `Sub Flow` node into every scenario that needs it.

---

## Save and load

```
// Blackboard, node states, elapsed time, raised events, last checkpoint
FlowComponent->QuickSave();                 // default slot
FlowComponent->SaveFlowToSlot("Slot1", 0);

FlowComponent->QuickLoad();
FlowComponent->LoadFlowFromSlot("Slot1", 0, ESimFlowLoadMode::FromLastCheckpoint);
```

Two load modes:

* **Exact State** — every node that was active comes back active, with its elapsed
  time and retry count. Tasks restart their `On Task Start` so any world setup is
  reapplied.
* **From Last Checkpoint** — the blackboard is restored and execution resumes from
  the last `Checkpoint` node the player passed. More forgiving, and usually the
  better choice for a VR session that was interrupted.

Several flows can live in one slot (keyed by the component's `Flow Save Id`).
`USimFlowSubsystem::SaveAllFlowsToSlot` writes all of them at once.

Object references in the blackboard are *not* saved — restore them in
`On Load Task State` or by looking the actor up again by tag.

---

## Multiplayer

Off by default. Tick **Replicate Flow** on the component and the flow becomes
server-authoritative.

### How it works

Only the server runs nodes and tasks — clients never execute anything, so they
cannot diverge. What replicates is a compact `FSimFlowNetState`: the run state,
which node Guids are active, progress, and each active task's start time and retry
count.

Nodes, tasks and conditions never go over the wire. Every client already has the
flow asset loaded, so it resolves a Guid back to the authored node and reads the
display name, instruction, quiz question and options locally. Bandwidth stays flat
no matter how large the graph grows.

Presentation events — task started, task finished, checkpoint reached, quiz
presented — go out as multicasts carrying only a node Guid. The blackboard
replicates as an entry array so client UI can show score and your own keys.

### Setup

1. Put the component on a **replicated** actor. The **Game State** for one shared
   scenario everybody sees; the **Player State** for a separate flow per trainee.
2. Tick **Replicate Flow** on the component.
3. Add a **SimFlow Player Component** to your PlayerController Blueprint. That is
   what lets clients talk back to the server — without it, clients can watch but
   not act, and you get a warning in the log saying so.

### Client controls

The usual calls work unchanged on clients: `RetryCurrentTask`, `SkipCurrentTask`,
`PauseFlow`, `SubmitQuizAnswer`, `SendEvent`. Each notices it is on a client and
forwards to the server for you, so the same Blueprint works in single player and
multiplayer.

Nothing a client sends is trusted. Every request arrives at
`IsRequestAuthorised` on the player component before anything happens — override
it to build instructor-only controls, or to stop a trainee skipping someone else's
task. `bRestrictToOwnedFlows` gives you a reasonable default.

For an instructor panel, `USimFlowStatics::RequestFlowControl` takes the right
route automatically from either side.

### Caveats

* **Save and load are server-only.** Calling them on a client logs a warning and
  does nothing.
* **Object references in the blackboard do not replicate** — same limitation as
  saving. Resolve actors locally by tag instead.
* **The blackboard resends its whole array** whenever any key changes. Fine for
  tens of keys at tutorial pacing; move to a `FastArraySerializer` if you ever push
  hundreds.
* **Presentation multicasts are reliable.** Normal for a flow that changes task
  every few seconds; not something to fire dozens of times a second.
* `Random Branch` is resolved on the server only, so every client agrees on the
  outcome.

---

## UI

Reparent your VR widget Blueprint to **SimFlow Status Widget**. It binds itself to
the primary running flow (or one you name) and gives you `On Task Started`,
`On Quiz Presented`, `On Flow Finished`, plus `Get Task Name`, `Get Instruction`,
`Get Progress`, `Get Score`, `Get Time Remaining Text` and ready-made
`Request Retry` / `Request Skip` / `Request Pause Toggle` handlers for buttons.

For the quiz, bind **On Quiz Presented**, read `Question` and `Options` off the
task, and call `Submit Answer` with the index the player picked.

---

## Debugging

* `SimFlow.Debug 1` in the console — on-screen overlay with the active nodes,
  countdowns, retry counts and the whole blackboard.
* `bShowDebugHUD` on a single component for a per-flow overlay.
* `GetDebugText()` returns the same text as a string, so you can put it on a
  world-space panel — which is what you actually want inside a headset.
* The **Validation** tab in the graph editor lists missing tasks, dead ends,
  unreachable nodes and dangling links. It refreshes on every graph edit.

---

## Extending

**A new task type** — create a Blueprint from `SimFlowTask`. Implement
*On Task Start*, do your thing, call *Finish Task* with Succeeded / Failed.

**A new condition** — create a Blueprint from `SimFlowCondition`, implement
*Evaluate*.

**A new node type** — C++ only. Subclass `USimFlowNode`, override `BuildPins()`
and `ExecuteInput()`, and it appears in the graph's right-click menu automatically
under whatever `GetNodeCategory()` returns.

**Building flows in code** — `USimFlowAsset::AddNode` / `ConnectNodes` are public
and Blueprint-callable. `SimFlowSampleBuilder.cpp` is a complete worked example.

---

## Engine compatibility

Targets 5.6, and builds on 5.4/5.5 too.

Unreal has been migrating graph editor coordinates from `FVector2D` to the
`FVector2f`-backed `UE::Slate::FDeprecateVector2DParameter`, and the exact spelling
of `FEdGraphSchemaAction::PerformAction`'s location parameter differs between
versions. `SimFlowEditorCompat.h` deduces that type directly from the base class
declaration rather than hardcoding it, so the overrides track whatever your engine
actually declares.

## Layout

```
SimFlow/
  SimFlow.uplugin
  CHANGELOG.md
  Resources/Icon128.png
  Source/
    SimFlowRuntime/            game module — ships in your build
      Public/ Private/
        SimFlowTypes            enums, FSimFlowValue, pin structs
        SimFlowAsset            the flow template
        SimFlowNode(s)          node base + all built-in nodes
        SimFlowTask(s)          task base + built-in tasks
        SimFlowCondition(s)     condition base + built-in conditions
        SimFlowBlackboard       per-run key/value store
        SimFlowInstance         the executor
        SimFlowComponent        designer-facing API
        SimFlowSubsystem        registry, global controls, debug HUD
        SimFlowSaveGame         save structures
        SimFlowStatics          Blueprint helpers
        SimFlowStatusWidget     UMG base class
        SimFlowGameplayTags     native tags
        SimFlowIdentity         identity component, actor query, match grading
        SimFlowZone             placement volume with settle detection
    SimFlowEditor/             editor module — stripped from packaged builds
        SimFlowGraph/Schema/GraphNode   the node graph
        SimFlowAssetEditor              the editor window
        SimFlowAssetFactory             new-asset creation
        AssetDefinition_SimFlowAsset    content browser integration
        SimFlowSampleBuilder            Tools menu sample generator
```

---

## License

MIT — see [`LICENSE`](LICENSE).
