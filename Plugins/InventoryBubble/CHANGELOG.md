# Changelog

## 1.1.1

A fix for the one thing 1.1.0 uncovered: an item taken out of a bubble and put straight
back in would sometimes drop to the floor a fraction of a second after it was stored.

### Fixed

- **A deferred physics restore no longer lands underneath an item that has been stored
  again.** 1.0.1 taught the bubble to wait for a VRE grip to let go before handing
  simulation back, and it waits on a 0.1 s timer. Re-storing the item runs on its own
  0.1 s timer, and the two race. When the recheck wins, the item is captured and the
  restore arrives a moment later, switching simulation on inside the bubble. That tears
  the item off the anchor, Layer B reads the broken attachment as an external grab, and
  the bubble empties itself. The log shows all four steps inside 350 ms:

  ```
  01:47:05.428  capturing 'EditorCube38'
  01:47:05.495  restored physics on 'EditorCube38' now that its grip has released
  01:47:05.769  stored 'EditorCube38'
  01:47:05.777  detected 'EditorCube38' was taken by an external grab
  ```

  Capture now takes the pending restore over rather than letting it fire: it cancels the
  timer and folds the physics the item is owed into the fresh snapshot, so those values
  come back at the next real release instead. The deferred timer also refuses to
  re-simulate an item it finds sitting in a bubble, which covers an item handed to a
  different bubble than the one that let go of it.

  The race is as old as 1.0.1 - 1.1.0 only made it visible. Idle motion used to rewrite
  the stored item's transform every frame, which pinned it back onto the anchor and hid
  the whole thing. With idle motion off by default there is nothing left to put it back.

- **A stored item no longer loses its physics settings when it is captured mid-grip.**
  The same cause with a quieter symptom: the snapshot was taken while simulation was
  still suppressed by the pending restore, so it recorded "this item does not simulate"
  and a later release left the item hanging in the air.

## 1.1.0

A performance pass on the one thing that scales badly: a level holding many *occupied*
bubbles. Empty bubbles already cost nothing. No behaviour changes to capture, release,
or any of the VRE handling from 1.0.1.

### Changed

- **`bIdleSpin` and `bIdleBob` now default to `false`.** Idle motion is the only thing
  that makes an occupied bubble write its item's transform every frame, and that write —
  a `TeleportPhysics` move of a body deliberately left at `QueryOnly` collision so grab
  systems can trace it — is by far the most expensive thing a bubble does. It is now
  opt-in per bubble.

  **This changes existing levels.** Unreal only serialises properties that differ from
  the class default, so a placed bubble that was never edited did not store `bIdleSpin`
  at all and will now load with it off. Tick the boxes on the bubbles that should still
  animate.

- **An occupied bubble can now stop ticking entirely.** Previously `bAutoDetectGrab`
  alone kept the tick alive, so turning both idle motions off still left every occupied
  bubble ticking every frame. Layer C already knew when it had bound a real grab
  delegate — `BindToItem`'s return value was simply discarded. It is now used: while a
  GrabComponent binding is live, the Layer B poll switches off and the bubble idles at
  zero cost.

  VRE items deliberately keep polling. A VRE physics grip never detaches the item and
  never touches those delegates, so `IsGrippedByVRExpansion` inside `CheckForExternalGrab`
  is the only thing that ever notices it. Only a binding to an actual `GrabComponent`
  counts — the adapter's fallback of binding delegates on the item actor itself is not
  trusted for this, since any actor can expose a parameterless `OnGrabbed` without it
  firing on every grab path.

- **Documented `bAutoDetectGrab` as "one pointer comparison per frame" — it is not.**
  For VRE items it is a reflected `IsHeld` call through `ProcessEvent`. Troubleshooting
  now says so.

### Added

- **`IdleTickInterval`** (default `0.033` s) — the gap between ticks while a bubble is
  only animating a stored item or polling for a grab. Capture and release blends, and
  the reject flash, always run at full rate regardless of it. Grab detection latency
  becomes at worst one interval, which matters only if you turn idle motion back on.

- **A global idle motion switch**, for a comfort or performance option: the
  `InventoryBubble.IdleMotion` console variable (`1` default, `0` off), and the
  Blueprint-callable `AInventoryBubbleActor::SetIdleMotionEnabled` /
  `IsIdleMotionEnabled` / `RefreshIdleMotionState`. Per-bubble flags still apply on top —
  the switch can only take motion away. Bubbles already in the level pick the change up
  immediately through a console variable sink and stop ticking when nothing else needs
  them, so it is a real performance switch and not just a visual one.

- `UInventoryBubbleGrabAdapter::IsBoundToGrabComponent`, so the bubble can tell an
  authoritative GrabComponent binding from the actor-level fallback.

## 1.0.1

Everything in this release came out of testing the plugin against VR Expansion Plugin
in a real project. Nothing in VRE was modified.

### Fixed

- **A stored item could never be grabbed back out.** Capture set the item's components
  to `NoCollision`, which made it invisible to every trace and overlap in the world, so
  grab systems that find items by tracing could not see it. Stored items now keep
  `QueryOnly` collision: still no physical blocking, but traceable.
- **The bubble swallowed items out of the player's hand.** Held detection tested a
  single level of attachment and asked whether that component's owner was a `APawn`.
  VRE parents a held item under its grasping-hand actor, a plain `AActor`, so the test
  never reached the pawn. Detection now asks VRE directly through
  `IVRGripInterface::IsHeld`, and the attachment fallback walks the whole chain and
  each link's ownership chain.
- **"Push the item in and let go" did nothing.** Capture ran only from `BeginOverlap`,
  which fires on entry. An item refused there - almost always because it was still
  held - got no second chance, because releasing it inside raises no new event. Items
  refused for a temporary reason are now re-tested while they remain inside the volume.
- **Items came out of the bubble still shrunk.** VRE caches a grip's `RelativeTransform`
  when the grip is made and re-imposes its scale every tick, so a grip made on a stored
  item kept re-applying the shrunken size. The live grip's scale is now rewritten via
  `SetGripRelativeTransform`.
- **Items floated after being taken out and released.** Restoring simulation while a
  grip was still live recreated the physics body underneath the grip's constraint,
  leaving the grip registered but inert, so the item was never handed back to physics.
  Scale and collision are restored immediately; simulation and gravity now wait for the
  grip to actually release.
- **The most common setup mistake was undiagnosable.** A tag mismatch was refused
  silently, so putting the `Item` tag on a component instead of the Actor produced no
  log output at all. All refusals now log their reason at Verbose.

### Changed

- `ShrinkScale` default raised from `0.2` to `0.35`.
- New properties: `bCaptureOnRelease` (default on) and `RecheckInterval` (default 0.1s).
- Architecture, API reference, VR integration and troubleshooting docs updated to match
  the implemented behaviour.

### Known issues

- **Large items often fail to be captured.** Small items (a dagger) store reliably;
  larger VRE melee weapons frequently are not captured at all. Not yet diagnosed. The
  refusal reason now appears in the log at Verbose, which is the place to start.
  Workaround worth trying: raise `CaptureRadius` to 40-50 and lower `RecheckInterval`
  to 0.05, in case the item leaves the volume between checks.
- **No content ships with the plugin.** There is no material and no `BP_InventoryBubble`,
  so a placed bubble uses the engine sphere's default material and renders opaque grey
  until `BubbleMaterial` is set. State colours and the reject flash need a material with
  the expected parameters.
- **The host project must be able to compile C++.** This is a source plugin with no
  prebuilt binaries, so it cannot be dropped into a Blueprint-only project as-is.
- **Grab systems that neither re-attach nor implement `VRGripInterface` are not
  detected.** Call `TryStoreItem` / `ReleaseItem` directly - see `VR_Integration.md`.
- **Scale correction is VRE-specific.** Any other framework that caches a full transform
  including scale at grab time will re-impose the shrunken size the same way.
- **No replication.** Single-player only for now.

## 1.0.0

Initial release.
