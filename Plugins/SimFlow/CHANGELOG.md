# Changelog

All notable changes to SimFlow. Versions follow the plugin's `VersionName`.

## Unreleased

### Changed

- **A tag in a Place Object In Zone `Zone` query now matches zones only.** It used
  to resolve through the generic actor query, which returns the first actor in the
  level carrying the tag — so a prop with a `SimFlow Identity` wearing the zone's
  tag could win on iteration order, fail the cast to `ASimFlowZone`, and take the
  task down at start while the real zone sat there unmatched. The tag form now scans
  `ASimFlowZone` actors, so nothing else can shadow one. `Specific Actor` and
  `Blackboard Key` are unchanged: they name one actor, and being told that actor is
  not a zone is the useful answer there.

### Fixed

- **The Zone-query failure said "could not resolve" and nothing else.** One message
  covered four different mistakes with four different fixes, and named neither the
  actor it found nor its class. It now distinguishes an empty query, a named actor
  that turned out not to be a zone, a level containing no zones at all, and zones
  that exist but do not carry the tag — and in that last case lists every zone in
  the level with its actual identity tags, plus a note when a non-zone actor is
  wearing the tag you asked for.

## 1.1.4

A setup pass rather than a feature. Placing an object was the plugin's most
configured step and most of what it asked for was tuning nobody needs on a first
pass, so the fields that matter now lead and the rest is one enum away.
`.uplugin` Version 7.

### Changed

- **Placing an object takes fewer decisions to set up.** A Zone asked for seven
  settings before it did anything; a `SimFlow Zone` now shows **Settle Mode**
  (`Instant` / `Standard` / `Custom`) and **Draw Debug**, with `Settle Time`,
  `Settle Speed Threshold` and `Require Detached` appearing only under `Custom`,
  and `Track Filter`, `Require Identity Component` and `Broadcast Flow Events`
  moved behind **Advanced**. `Standard` is the old defaults exactly — `0.35 s`
  and a speed limit of `20` — so behaviour is unchanged.
- **Place Object In Zone** leads with the four fields a drill actually needs —
  `Zone`, `Accepted Items`, `Required Count`, `Wrong Item Policy`. `Require Settled`
  joins `Rejected Items`, `Report Each Wrong Item Once` and
  `Placed Item Blackboard Key` under **Advanced**.

### Fixed

- **A rejected item was always reported as a near miss.** `Place Object In Zone`
  graded anything matching `Rejected Items` as `Related` regardless of what it was,
  so listing an unrelated object there drove the "close, try again" branch of
  `On Wrong Item Placed` for something not close at all. Rejection now vetoes an
  accept without inventing similarity: the quality still comes from the tags, so the
  CO2 unit is still a near miss for foam and a wrench is still `No Match`.

No property was renamed or removed, so existing assets and Blueprints keep working.
A zone saved with tuned settling numbers loads as `Custom` and keeps them.

## 1.1.3

Two fixes found while writing a VRExpansion build guide against 1.1.2. Both are
about the plugin failing quietly: in each case the flow looked broken rather than
misconfigured, which is the worst way for a framework to be wrong.

### Added

- `Set Held` / `Is Held` on `USimFlowIdentityComponent`. Call `Set Held (true)`
  wherever your grab succeeds and `false` on release, and a Zone knows an item is
  in the player's hand instead of having to infer it.

### Fixed

- **A Zone could not tell that an item was still held.** It inferred release from
  `GetAttachParentActor()`, which only works for grab systems that reparent the
  actor. Most do not — of VRExpansion's twelve `EGripCollisionType` values only
  `AttachmentGrip` uses native attachment; the default
  `InteractiveCollisionWithPhysics` holds the object with a physics constraint and
  never reparents it. A gripped item therefore looked detached, so a trainee could
  hold it steady above the workbench and complete **Place Object In Zone** without
  letting go — the exact hole `Require Detached` was added to close.
  `ASimFlowZone::IsActorAtRest` now asks the object first and only falls back to
  attachment. The flag lives on the identity component rather than being sniffed
  from a VR plugin, so the runtime still depends on nothing about how grabbing is
  implemented.
- **A payload with no actor behind it failed silently.** `FSimFlowActorQuery::MatchObject`
  scored `No Match` and said nothing, so the task simply never completed. The usual
  cause is a UMG button broadcasting `self`: a `UUserWidget` is neither an Actor nor
  an Actor Component. It now logs a warning naming the payload's class and what the
  query wanted. The match still fails on purpose — resolving a widget through
  `GetTypedOuter<AActor>()` finds the owning PlayerController for anything made with
  `CreateWidget(OwningPlayer, ...)`, and a confident match against the wrong actor is
  worse than no match at all.

### Note for 1.1.2 users

Nothing breaks. `Set Held` is optional — omit it and Zones behave exactly as they
did on 1.1.2. Add the two nodes if your grab system doesn't reparent the actor,
which you can check quickly: if a held item never trips a Zone's `Require Detached`,
it doesn't.

## 1.1.2

The object-recognition release. Before this, SimFlow could tell you a trainee
had finished a step, but not *what they did wrong* — every task was a success
detector, and nothing in the plugin could refer to a specific object in the
level. 1.1.2 adds an identity layer, zone-based placement, ordered procedure
validation, and a first-class mistake record.

The design rule it follows: **Blueprint reports neutral facts, the flow asset
decides whether they were correct.** A button broadcasts that it was pressed and
knows nothing about the exercise; the flow holds the answer. That keeps one
level reusable across scenarios without editing any Blueprint.

### Added

**Object identity**

- `USimFlowIdentityComponent` — put it on the item Blueprint, set `Identity Tags`
  (e.g. `Item.Extinguisher.Foam`), optional `Display Name` and `Identity Id`.
  Every placed and spawned copy carries it.
- `FSimFlowActorQuery` — the "which object do I mean" struct used by all new
  tasks. Resolves in order: `Specific Actor` → `Blackboard Key` → `Required Tags`
  → `Required Class` / `Required Actor Tag`.
- `ESimFlowMatchQuality` — `No Match` / `Related` / `Exact`. Because gameplay tags
  nest, a CO2 extinguisher scores `Related` against an expected foam extinguisher
  while a wrench scores `No Match`, so feedback can distinguish a near miss from a
  random object. `Min Related Tag Depth` (default 2) sets the threshold.
- `USimFlowIdentityStatics` — Blueprint access to identity tags, display names and
  query matching. Falls back to `IGameplayTagAssetInterface`, so actors already
  tagged for GAS work without a second component.

**Zones**

- `ASimFlowZone` — a box volume that reports what is inside it *without judging
  it*. Carries a `USimFlowIdentityComponent` rather than its own tagging scheme,
  so one mechanism names both items and zones.
- Settling: `On Actor Entered` fires on overlap, but `On Actor Settled` waits until
  the object is detached from the hand, below `Settle Speed Threshold`, and has
  held still for `Settle Time`. Holding an item over a bin is not placing it.
  Picking it back up restarts the settle timer.
- Optional `Broadcast Flow Events` raises `SimFlow.Event.Placed` /
  `SimFlow.Event.Removed` with the actor as payload, so a plain Wait For Event
  task can use a zone too.

**Tasks**

- **Place Object In Zone** — `Zone`, `Accepted Items`, optional `Rejected Items`
  (explicit decoys always lose, even if they would otherwise pass),
  `Required Count`, `Require Settled`, `Wrong Item Policy`. Fires
  `On Wrong Item Placed` with the match quality. A wrong item is reported once
  until it leaves the zone and is placed again.
- **Ordered Sequence** — an ordered list of steps, each with a target query and
  instruction text. `Out Of Order Policy` is `Ignore` / `Count Mistake` /
  `Restart Sequence` / `Fail Task`. Input belonging to another step of the
  procedure is recorded at `Related` severity; unrelated props at `No Match`, and
  `Unlisted Input Is Mistake` (default off) decides whether they are ignored.

**Mistake record**

- `FSimFlowMistake` — kind tag, description, severity, the object involved and a
  timestamp — recorded on `USimFlowInstance` and persisted in `FSimFlowSaveState`.
- `Record Mistake`, `Get Mistakes`, `Get Mistakes Of Kind`, `Get Mistake Count`,
  `Clear Mistakes`, and the `On Mistake Recorded` delegate.
- Task-level `Record Mistake` and `Apply Mismatch Policy` helpers on
  `USimFlowTask`, available to Blueprint task subclasses.
- New tags: `SimFlow.Mistake` with `.WrongItem`, `.WrongTarget`, `.WrongOrder`,
  `.WrongAnswer`; plus `SimFlow.Event.Placed` and `SimFlow.Event.Removed`.
- New blackboard keys: `WrongAttempts` and `CurrentStep`.

### Changed

- **Wait For Event no longer discards the event payload.** It gained
  `Expected Payload` (a query), `Mismatch Policy`
  (`Ignore` / `Count Mistake` / `Fail Task`), `Payload To Blackboard Key` and an
  `On Payload Rejected` delegate. Ten buttons can now broadcast the same tag while
  only the intended one advances the flow.
- Quiz now writes into the mistake record (`SimFlow.Mistake.WrongAnswer`) instead
  of only incrementing the `Mistakes` blackboard key, so quiz answers appear in
  the same debrief list as everything else. The key is still incremented, so
  existing conditions written against it are unaffected.

### Fixed

- `StartInstance` did not clear the mistake list, so restarting a flow carried the
  previous run's mistakes forward while the blackboard `Mistakes` counter was
  reset to zero by `ClearAll()` — the two records disagreed after any retry.
  Introduced and fixed within this release.
- `Wait For Event` with `Accept Already Raised` enabled would have accepted a
  previously-raised tag without checking the expected payload, since a past event
  retains only its tag. The flag is now ignored (with a verbose log line) when
  `Expected Payload` is set, rather than silently letting the wrong object pass.

### Known limitations

None of these are regressions; they are the edges of the new features.

- `Specific Actor` is a soft pointer resolved without a synchronous load. An actor
  in an unloaded World Partition cell or streaming sublevel will not resolve — use
  identity tags or a blackboard key for streamed content.
- Tag and class queries resolve via a linear `TActorIterator` scan. This runs at
  task start, not per frame, and is not cached; on very large levels prefer
  `Specific Actor` or a blackboard key for zones.
- Zone "still held" detection uses `GetAttachParentActor()`. This suits frameworks
  that reparent a grabbed actor. Frameworks that grab via physics constraints
  without reparenting cannot be detected, so an item held steady over a zone will
  settle — fixed in 1.1.3.
- A mistake's `Involved` object pointer is deliberately stripped when a flow is
  saved. Reloaded runs keep `Involved Name` for display but not a live reference.
- **Place Object In Zone** requires an `ASimFlowZone`; it will not accept an
  arbitrary trigger volume. It fails the task and logs a warning if the `Zone`
  query resolves to nothing.
- Zone overlap needs the item's collision to respond to the zone's
  `OverlapAllDynamic` profile. Items with `NoCollision` are never seen.
- Backward compatible: existing flow assets, saves and Blueprint task subclasses
  keep working. New properties all default to the pre-1.1.2 behaviour — leave
  `Expected Payload` empty and Wait For Event behaves exactly as before.

## 1.1.0

Initial public release. Node graph authoring, sequential and parallel tasks,
conditions, branching, pause/resume, retry/skip/fail, checkpoints, sub-flows,
save/load and multiplayer replication.
