# Architecture

## The state machine

```
                  TryStoreItem / overlap accepted
        ┌──────────────────────────────────────────────┐
        │                                              ▼
   ┌─────────┐                                   ┌───────────┐
   │  Empty  │                                   │ Capturing │
   └─────────┘                                   └───────────┘
        ▲                                              │
        │                                    blend finished (FinishCapture)
        │                                              ▼
        │                                        ┌──────────┐
        │   grab detected (Layer B / C)           │ Occupied │
        ├────────────────────────────────────────┤          │
        │   item destroyed                       └──────────┘
        │                                              │
        │                                    ReleaseItem, not held,
        │                                    ReleaseBlendTime > 0
        │                                              ▼
        │            blend finished              ┌───────────┐
        └────────────────────────────────────────│ Releasing │
                     (FinishRelease)             └───────────┘
```

`Empty` and `Occupied` are resting states. `Capturing` and `Releasing` are timed blends
driven from `Tick`.

Two edges skip `Releasing` entirely and go straight back to `Empty`:

- **A grab.** Blending would fight the hand that now holds the item, so `ReleaseItem`
  restores instantly whenever `IsActorHeldByPlayer` is true. `ForceEject` does the same
  by temporarily zeroing `ReleaseBlendTime`.
- **The stored item being destroyed.** `HandleStoredItemDestroyed` clears the reference
  and resets, and `Tick` re-checks validity every frame as a backstop.

### Ticking

The actor sets `PrimaryActorTick.bStartWithTickEnabled = false` and
`RefreshTickEnabled()` re-evaluates after every state change. It ticks only while
capturing, releasing, occupied-with-idle-motion-or-grab-detection, flashing a rejection,
or drawing debug. An idle empty bubble costs nothing.

## Why grab detection has three layers

Grab systems in UE are not standardised. The VR Template's `GrabComponent` is a
Blueprint class with no native type; VRE has its own `IVRGripInterface`; most projects
roll their own. Rather than pick one, the plugin layers them so the lowest layer always
works.

### Layer A — the core API

`TryStoreItem(AActor*)` and `ReleaseItem(bool)` are the only functions that mutate
state. Everything else in the plugin — overlaps, adapters, the function library — calls
into these. Any grab system can drive the bubble by calling them directly, and that path
has no dependencies at all.

This is why the class is testable without a headset: the same two functions the VR path
uses are `BlueprintCallable`.

### Layer B — attachment-change detection

While `Occupied`, `CheckForExternalGrab()` compares the stored item's root attach parent
against `ItemAnchor` each tick. Any grab system that re-attaches the item to a hand
breaks that link, and the bubble treats the break as a grab: restore state, apply the
cooldown, fire `OnItemRemoved`, go `Empty`.

This is the mechanism that makes the "zero integration work" claim true. It costs one
pointer comparison per frame and covers every framework that attaches, which is nearly
all of them.

The limitation: a grab system that does **not** re-attach — one that drives the item
purely by setting its transform each frame, or through a physics constraint — will not
be detected by attachment alone. VRE is exactly such a system for its physics grips, so
Layer C supplements this with VRE's own `IsHeld` (below). Any other framework in that
category should call `NotifyItemGrabbed` or `ReleaseItem` directly.

### Capture on release

`BeginOverlap` fires only on the entry transition. An item refused at that moment — most
commonly because it is still in the player's hand — would otherwise never get a second
chance, because letting go inside the volume generates no new event. The bubble
therefore keeps refused-but-temporary candidates in a `PendingCandidates` set, drained
by `EndOverlap`, and re-tests them on a timer while they remain inside. This is what
makes "push the item in and let go" work. The timer exists only while the set is
non-empty, so an idle bubble stays idle. Governed by `bCaptureOnRelease` and
`RecheckInterval`.

### Layer C — optional adapters

`UInventoryBubbleGrabAdapter` binds to grab notifications when it can find them.

For the **VR Template**, there is no C++ type to link against, so the adapter finds a
component whose class name contains `GrabComponent` and looks for multicast delegate
properties named `OnGrabbed` / `OnDropped` by reflection. It binds **only if the
delegate signature takes no parameters** — checked via `SignatureFunction->NumParms`.
Binding a handler with the wrong arity would corrupt the stack when the delegate fires,
so a template revision that changes the signature degrades to Layer B rather than
crashing.

For **VRE**, everything is behind `#if WITH_VR_EXPANSION_PLUGIN`, which
`InventoryBubble.Build.cs` defines by probing for `VRExpansionPlugin.uplugin` under the
project's and the engine's `Plugins` folders. The macro is always defined, so the
`#if` is never an undefined-symbol warning.

For VRE the adapter is not limited to delegates. Two VRE-specific behaviours are
necessary because VRE grips do not follow the attachment model:

- **Held state comes from VRE, not from attachment.** `IVRGripInterface::Execute_IsHeld`
  is the grip system's own answer and is correct for every grip type it supports,
  including physics-constraint grips that never reparent the actor. Both
  `IsActorHeldByPlayer` (so a held item is never swallowed) and `CheckForExternalGrab`
  (so a grab is noticed even without a detach) consult it.
- **The grip's cached scale is rewritten on release.** VRE stores a grip's
  `RelativeTransform` when the grip is made and re-imposes its scale every tick. A grip
  made on a stored item caches the *shrunken* scale, so restoring the item's size is
  undone a frame later. `RestoreVREGripScale` reads the live grip with `GetGripByID`,
  replaces the scale with the item's original, and writes it back with
  `SetGripRelativeTransform`.

Every VRE reference is inside `#if WITH_VR_EXPANSION_PLUGIN`, and no VRE type appears in
any header signature, so the class parses identically with and without the plugin. Both
configurations are expected to compile warning-free.

Layer C never mutates state itself. It calls `NotifyItemGrabbed`, which routes into Layer A.

## The stored-state contract

The promise is that a released item is indistinguishable from one that was never stored.
`CaptureItemState` snapshots, before changing anything:

- the actor's world transform and world scale;
- the root's previous attach parent and socket;
- per `UPrimitiveComponent`: `IsSimulatingPhysics`, `IsGravityEnabled`,
  `GetCollisionProfileName`, `GetCollisionEnabled`, `GetLinearDamping`,
  `GetAngularDamping`.

It then zeroes linear and angular velocity **before** disabling simulation (order
matters — disabling first can leave residual velocity that reapplies on re-enable),
disables gravity, and sets collision to `NoCollision` so a stored item cannot punch the
player.

`RestoreItemState` replays that snapshot in reverse, scale first. Physics simulation is
re-enabled last and only for components that were simulating originally, so a static
mesh that was never simulating does not start.

Scale is stored as **world** scale, not relative, so an item is restored to the size it
had in the level regardless of what it gets attached to in between.

## Known limitations

- **One item, by design.** While occupied, other overlaps are ignored entirely and only
  `OnItemRejected` fires. Multi-slot inventories are explicitly out of scope.
- **No replication.** See below.
- **Large items are unreliable to capture.** Small items store consistently; bigger
  ones (VRE's `MeleeBase` children - mace, shield, sickle) often are not captured at
  all. Unresolved as of 1.0.1. See `CHANGELOG.md`.
- **Transform-driven grab systems** are not auto-detected (Layer B limitation above).
- **Idle motion writes the item's transform every frame** while occupied. If something
  else also drives that transform, disable `bIdleSpin` and `bIdleBob`.
- **Cooldowns are per-bubble.** Two adjacent bubbles do not share a cooldown, so an item
  pulled from one can be immediately captured by its neighbour. That is usually what you
  want; if not, gate it in `CanAcceptItem`.
- The reject flash is driven on the DMI. With no material assigned there is no visual
  feedback, though `OnItemRejected` still fires.

## Adding replication later

The design keeps this cheap:

1. `bReplicates = true` in the constructor.
2. `UPROPERTY(ReplicatedUsing = OnRep_StoredItem) TWeakObjectPtr<AActor> StoredItem;`
   plus `BubbleState`, and a `GetLifetimeReplicatedProps` override.
3. Gate `TryStoreItem` / `ReleaseItem` on `HasAuthority()`.
4. In `OnRep_StoredItem`, run the same blend the server ran — the blend is pure
   presentation and reads only `BlendStartWorld` / `BlendTargetWorld`.

No restructuring is required because no state lives outside `StoredItem`,
`BubbleState` and `StoredState`, and `StoredState` is server-only bookkeeping.
