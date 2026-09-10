# API Reference

All types live in the `InventoryBubble` runtime module.

## AInventoryBubbleActor

### Components

| Name | Type | Notes |
|---|---|---|
| `BubbleRoot` | `USceneComponent` | Root. |
| `BubbleMesh` | `UStaticMeshComponent` | Visible sphere. `NoCollision`, no shadow, no overlaps. Defaults to `/Engine/BasicShapes/Sphere`. |
| `CaptureVolume` | `USphereComponent` | `QueryOnly`, object type `WorldDynamic`, overlaps `WorldDynamic` + `PhysicsBody`, all else ignored. |
| `ItemAnchor` | `USceneComponent` | Where a stored item settles. Offset by `AnchorOffset`. |

### Properties — Eligibility

| Property | Type | Default | Description |
|---|---|---|---|
| `ItemTag` | `FName` | `Item` | Actor tag a candidate must carry. `None` disables the tag check. |
| `CaptureRadius` | `float` | `20.0` cm | Drives both the sphere collision radius and the mesh scale. Min 1. |
| `ReCaptureCooldown` | `float` | `0.75` s | Per-actor lockout after release. `0` disables. |

### Properties — Storage

| Property | Type | Default | Description |
|---|---|---|---|
| `ShrinkScale` | `float` | `0.35` | Stored size as a fraction of original world scale. Clamped 0.01–1. |
| `CaptureBlendTime` | `float` | `0.35` s | Blend in. `0` snaps. |
| `ReleaseBlendTime` | `float` | `0.2` s | Blend out on a **non-grab** release. Ignored when the item is held. |
| `EaseCurve` | `UCurveFloat*` | `nullptr` | Sampled over 0–1. Falls back to smoothstep. |
| `StoredRotation` | `FRotator` | `0,0,0` | Rotation offset applied at the anchor. |
| `AnchorOffset` | `FVector` | `0,0,0` | Anchor offset from bubble centre. |
| `bDisablePhysicsWhileStored` | `bool` | `true` | Disable simulation and gravity while stored, and drop collision to `QueryOnly`. Query collision is deliberately kept so grab systems that find items by tracing can still see the stored item. |
| `bCaptureOnRelease` | `bool` | `true` | Keep watching an item that was refused on entry (held, on cooldown, bubble busy) while it stays inside the volume, and capture it once it becomes eligible. Without this, capture can only happen on the entry overlap event. |
| `RecheckInterval` | `float` | `0.1` s | How often a refused-but-still-inside candidate is re-tested. The timer only runs while at least one candidate is waiting. |

### Properties — Idle Motion

| Property | Type | Default |
|---|---|---|
| `bIdleSpin` | `bool` | `false` |
| `IdleSpinRate` | `float` | `45.0` deg/s |
| `bIdleBob` | `bool` | `false` |
| `IdleBobAmplitude` | `float` | `1.5` cm |
| `IdleBobSpeed` | `float` | `2.0` |
| `IdleTickInterval` | `float` | `0.033` s |

Both motions are **off by default as of 1.1.0**. Idle motion is the only thing that
makes an occupied bubble write its item's transform every frame, and that write — a
teleport of a live query body — is the most expensive thing a bubble does. Turn it on
for the few bubbles meant to draw the eye.

`IdleTickInterval` is the gap between ticks while a bubble is only animating or polling
for a grab; `0` means every frame. Capture and release blends, and the reject flash,
always run at full rate regardless of it.

#### Global idle motion switch

| Member | Signature | Notes |
|---|---|---|
| `SetIdleMotionEnabled` | `static void (bool)` | Turns idle motion off for every bubble at once — a comfort or performance option. Per-bubble `bIdleSpin` / `bIdleBob` still apply on top: this only ever takes motion away. |
| `IsIdleMotionEnabled` | `static bool ()` | False when the global switch is off. |
| `RefreshIdleMotionState` | `void ()` | Re-evaluates whether this bubble needs to tick. Call after changing an idle property at runtime; the global switch does it for you. |

The same switch is the `InventoryBubble.IdleMotion` console variable (`1` default, `0`
off). Bubbles already in the level pick a change up immediately, and stop ticking
entirely when nothing else needs them, so it is a real performance switch.

### Properties — Appearance

| Property | Type | Default |
|---|---|---|
| `BubbleColorEmpty` | `FLinearColor` | `(0.15, 0.45, 1.0, 1.0)` |
| `BubbleColorOccupied` | `FLinearColor` | `(0.25, 0.70, 1.0, 1.0)` |
| `BubbleColorRejecting` | `FLinearColor` | `(1.0, 0.15, 0.10, 1.0)` |
| `BubbleOpacity` | `float` | `0.35` |
| `RejectFlashTime` | `float` | `0.25` s |
| `BubbleMaterial` | `UMaterialInterface*` | `nullptr` — falls back to the mesh's material |
| `BaseColorParameterName` | `FName` | `BaseColor` |
| `OpacityParameterName` | `FName` | `Opacity` |
| `EmissiveParameterName` | `FName` | `EmissiveIntensity` |

### Properties — Integration / Debug

| Property | Type | Default | Description |
|---|---|---|---|
| `bAutoDetectGrab` | `bool` | `true` | Layer B attachment-change detection. Skipped automatically while Layer C holds a GrabComponent binding for the stored item, since grabs then arrive as events. |
| `bUseGrabComponentAdapter` | `bool` | `true` | Layer C adapter binding. |
| `bRejectItemsHeldByPlayer` | `bool` | `true` | Refuse items attached to a pawn. |
| `bShowDebug` | `bool` | `false` | Draw the capture volume, coloured by state. |

### Functions

| Signature | Kind | Returns |
|---|---|---|
| `bool TryStoreItem(AActor* Item)` | Callable | `true` if capture started. Fires `OnItemRejected` on failure. |
| `AActor* ReleaseItem(bool bRestorePhysics = true)` | Callable | The released actor, or `nullptr`. |
| `AActor* ForceEject()` | Callable | Instant release, no blend. |
| `void ClearCooldowns()` | Callable | — |
| `void NotifyItemGrabbed(AActor* Item)` | Callable | Layer C hook. No-op unless `Item` is the stored item. |
| `bool IsOccupied() const` | Pure | True while `Occupied` **or** `Capturing`. |
| `AActor* GetStoredItem() const` | Pure | — |
| `EInventoryBubbleState GetBubbleState() const` | Pure | — |
| `bool IsOnCooldown(const AActor*) const` | Pure | — |

### Overridable (BlueprintNativeEvent)

| Signature | Native default |
|---|---|
| `bool CanAcceptItem(AActor* Candidate)` | `DetermineRejectReason(...) == None` |
| `FTransform GetStoredItemTransform(AActor* Item) const` | Anchor transform + `StoredRotation`, scaled by `OriginalWorldScale × ShrinkScale × interface multiplier` |
| `void OnCaptureStarted(AActor* Item)` | empty |
| `void OnCaptureFinished(AActor* Item)` | empty |
| `void OnReleaseStarted(AActor* Item)` | empty |
| `void OnReleaseFinished(AActor* Item)` | empty |

### Delegates (all `BlueprintAssignable`)

| Delegate | Parameters |
|---|---|
| `OnItemStored` | `AActor* Item` |
| `OnItemRemoved` | `AActor* Item` |
| `OnItemRejected` | `AActor* Item`, `EInventoryBubbleRejectReason Reason` |
| `OnBubbleStateChanged` | `EInventoryBubbleState NewState` |

## IInventoryBubbleItemInterface

Optional. An actor with the right tag works without it.

| Function | Default | Description |
|---|---|---|
| `bool CanBeStored(const AInventoryBubbleActor* Bubble) const` | **`true`** | Return false to refuse this bubble. |
| `float GetStoredScaleMultiplier() const` | `0` → treated as `1.0` | Multiplier on top of `ShrinkScale`. |
| `void OnStoredInBubble(AInventoryBubbleActor* Bubble)` | empty | After the capture blend finishes. |
| `void OnRemovedFromBubble(AInventoryBubbleActor* Bubble)` | empty | After state is restored. |

> **Note on defaults.** UHT generates zero-initialised bodies for interface
> `BlueprintNativeEvent`s. `CanBeStored` would therefore default to `false` — refusing
> every item that implements the interface. The plugin declares
> `CanBeStored_Implementation` explicitly to suppress that stub and return `true`.
> `GetStoredScaleMultiplier` keeps UHT's `0`, and the bubble normalises any value `<= 0`
> to `1.0`. Both defaults are therefore safe, but if you add your own interface members,
> be aware of the underlying behaviour.

## UInventoryBubbleFunctionLibrary

| Function | Description |
|---|---|
| `FindNearestFreeBubble(WorldContext, Origin, MaxDistance = 500, Item = nullptr)` | Nearest bubble that can accept `Item`. Null `Item` means "nearest unoccupied". |
| `FindNearestBubble(WorldContext, Origin, MaxDistance = 500)` | Nearest, occupied or not. |
| `GetBubbleHoldingItem(WorldContext, Item)` | The bubble holding `Item`, or null. |
| `IsActorStored(WorldContext, Item)` | Convenience boolean. |
| `StoreInNearestBubble(WorldContext, Item, MaxDistance = 500)` | The bubble that accepted, or null. |
| `ReleaseFromNearestBubble(WorldContext, Origin, MaxDistance = 500)` | The released actor, or null. |
| `GetAllBubbles(WorldContext, OutBubbles)` | Every bubble in the level. |

`MaxDistance <= 0` means unlimited.

## Enums

**`EInventoryBubbleState`** — `Empty`, `Capturing`, `Occupied`, `Releasing`.

**`EInventoryBubbleRejectReason`** — `None`, `AlreadyOccupied`, `TagMismatch`,
`OnCooldown`, `HeldByPlayer`, `InterfaceRefused`, `BlueprintRefused`, `Invalid`, `Busy`.

> `OnItemRejected` is deliberately **not** fired for `TagMismatch` or `Invalid` — the
> floor, walls and the player overlap constantly, and firing for those would make the
> event useless. It fires for `AlreadyOccupied`, `OnCooldown`, `HeldByPlayer`,
> `InterfaceRefused`, `BlueprintRefused` and `Busy`.

## Logging

`LogInventoryBubble`. `Log` for capture/store/release/destroy and external-grab
detection; `Verbose` for rejections, adapter binding and skipped delegate binds.

```
Log LogInventoryBubble VeryVerbose
```
