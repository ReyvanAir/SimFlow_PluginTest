# Troubleshooting

Each entry is a symptom, its cause, and the fix. Turn on logging first:

```
Log LogInventoryBubble VeryVerbose
```

and tick `Show Debug` on the bubble so you can see the capture volume.

---

## 1. The item is never captured

**No log line at all** means the overlap never fired.

| Cause | Fix |
|---|---|
| The actor tag is missing or misspelled | `ItemTag` defaults to `Item`, case-sensitive. The tag goes on the **Actor** (Class Defaults → Tags), not on a component. |
| The item has no collision that overlaps | The item needs a `UPrimitiveComponent` whose object type is `WorldDynamic` or `PhysicsBody`. `PhysicsActor` preset is the easy answer. |
| The item's collision is `NoCollision` or query-disabled | Set **Collision Enabled** to `Collision Enabled (Query and Physics)`. |
| The item never physically reaches the sphere | Raise `CaptureRadius`, or enable `Show Debug` to see where the volume actually is. |
| The bubble is scaled in the level | Scale the actor to 1,1,1 and use `CaptureRadius` instead. Actor scale multiplies the sphere and desynchronises it from the mesh. |

**A `rejected ... (reason N)` line** means it was seen and refused. Look the number up in `API_Reference.md` — reason 2 (`TagMismatch`) and 7 (`Invalid`) are not broadcast to `OnItemRejected` but are still logged at Verbose.

---

## 2. The item is captured but does not shrink

| Cause | Fix |
|---|---|
| `ShrinkScale` is 1.0 | Set it to `0.2`. |
| The item implements `IInventoryBubbleItemInterface` and returns a large `GetStoredScaleMultiplier` | The multiplier stacks on top of `ShrinkScale`. Return `1.0` to leave it alone. |
| Something else drives the item's transform every frame | An animation, a movement component, or a grab system fighting the bubble. Disable it while stored, or set `bIdleSpin`/`bIdleBob` off and check whether the item then holds position. |
| `CaptureBlendTime` is very long | It is blending, just slowly. Default is `0.35`. |

---

## 3. The item falls through the world after release

| Cause | Fix |
|---|---|
| The item's original collision profile was already broken before capture | The bubble restores exactly what it captured — including a bad profile. Check the item's collision **before** storing it. |
| Released inside geometry | It was shrunk; at full size it may intersect the floor. Raise the bubble or lower `ShrinkScale`. |
| Physics never re-enabled | Only components that were simulating at capture time get simulation back. If the item was not simulating when captured, it will not be simulating after. That is intentional. |
| `ReleaseItem(false)` was called | `bRestorePhysics = false` deliberately leaves simulation off. Pass `true`. |

---

## 4. The item is re-captured instantly after release

| Cause | Fix |
|---|---|
| `ReCaptureCooldown` is 0 | Set it to `0.75`. |
| The player holds the item inside the volume and the cooldown expires | Expected. Either raise the cooldown or set `bRejectItemsHeldByPlayer = true` (the default) so a held item is refused. |
| A *different* bubble grabbed it | Cooldowns are per-bubble by design. Space bubbles further apart, or gate on `Get Bubble Holding Item` in a `Can Accept Item` override. |

---

## 5. A second item gets swallowed while one is already stored

This should be impossible — `DetermineRejectReason` returns `AlreadyOccupied` before anything is touched. If you see it:

| Cause | Fix |
|---|---|
| A `Can Accept Item` override does not call the parent | Always call **Parent: Can Accept Item** and AND your own logic onto it. Skipping the parent drops the occupied check. |
| Two bubbles overlap each other | Each is holding one item — they are just visually coincident. Check with `Get All Bubbles` → `Get Stored Item`. |

---

## 6. The bubble is invisible

| Cause | Fix |
|---|---|
| No material assigned and the engine default is nearly invisible | Author `M_InventoryBubble` — `First_Test_Walkthrough.md` §2. |
| `BubbleOpacity` is 0 | Raise it to `0.35`. |
| The assigned material is not `Translucent` | Set **Blend Mode** to `Translucent` in the material. |
| The material has no parameter matching `BaseColorParameterName` | The DMI write silently does nothing. Either rename the material's parameter to `BaseColor` or point `BaseColorParameterName` at the real name. |
| `/Engine/BasicShapes/Sphere` failed to load | Check the log for a `ConstructorHelpers` warning. Assign a static mesh to `BubbleMesh` manually. |

---

## 7. The colour never changes between empty and occupied

The parameter names must match the material. The bubble writes `BaseColorParameterName`, `OpacityParameterName` and `EmissiveParameterName` to a Dynamic Material Instance; an unmatched name is a silent no-op in Unreal. Open the material, check the exact parameter names, and set the three `...ParameterName` properties to match.

---

## 8. VRE symbols not found / link errors mentioning VRExpansionPlugin

| Cause | Fix |
|---|---|
| VRE was installed after InventoryBubble was built | The probe runs at **build** time. Rebuild the editor target. |
| VRE lives somewhere the probe does not look | It searches the project's `Plugins/` and the engine's `Plugins/`, recursively. A VRE outside both is not found. Move it under one, or add `VRExpansionPlugin` to `PrivateDependencyModuleNames` manually and hard-define `WITH_VR_EXPANSION_PLUGIN=1`. |
| Partial VRE install | Confirm `VRExpansionPlugin.uplugin` exists and the plugin itself compiles on its own first. |

Check which path the build took — it prints `[InventoryBubble] VRExpansionPlugin found` or `not found` every build.

---

## 9. Grabbing does not release the item

| Cause | Fix |
|---|---|
| `bAutoDetectGrab` is off | Turn it back on unless you drive releases yourself. |
| Your grab system does not re-attach the item | Layer B detects attachment changes only. **VRE is already handled** — its `IsHeld` is consulted directly. For any other non-reattaching system, call `Notify Item Grabbed` or `Release Item` from your grab event — `VR_Integration.md` §3. |
| The stored item cannot be grabbed at all | Grab systems find items by tracing. Check `bDisablePhysicsWhileStored` left collision at `QueryOnly` and that nothing else set the item to `NoCollision` — an item with no collision is invisible to every trace. |
| The GrabComponent's delegate takes parameters | The Layer C bind is skipped on purpose (a mismatched signature would corrupt the stack). You will see a Verbose line saying so. Layer B still covers it, or add the explicit node. |

Diagnostic: watch for `detected '<Item>' was taken by an external grab.` If that line never appears, your system is not re-attaching.

---

## 10. The bubble ticks constantly / performance

An empty bubble with `bShowDebug` off should not tick at all. It ticks while capturing, releasing, occupied with idle motion or grab detection on, or flashing a rejection.

| Cause | Fix |
|---|---|
| `bShowDebug` left on | Turn it off for shipping — it forces a tick every frame. |
| Idle motion on many bubbles | `bIdleSpin` / `bIdleBob` write the item transform each frame, which is the single most expensive thing a bubble does. Both are off by default from 1.1.0. To kill it everywhere at once — including bubbles already placed with it on — use `InventoryBubble.IdleMotion 0` or `SetIdleMotionEnabled(false)`. |
| Ticking faster than it needs to | `IdleTickInterval` (default `0.033`) sets the gap between ticks while a bubble is only animating or polling. Raise it for background bubbles; blends and the reject flash ignore it and stay at full rate. |
| `bAutoDetectGrab` on many occupied bubbles | For VR Template items with a GrabComponent this now costs nothing — Layer C binds the event and the poll switches itself off. VRE items still poll, because a physics grip never detaches the item and `IsGrippedByVRExpansion` is the only thing that notices it; that poll is a reflected `IsHeld` call, so keep `IdleTickInterval` off zero if you have many. |

---

## 11. Items implementing the interface are always refused

Fixed in the shipped code, but worth knowing: UHT generates zero-initialised bodies for interface `BlueprintNativeEvent`s, so `CanBeStored` would default to `false`. The plugin declares `CanBeStored_Implementation` explicitly to return `true`.

If you **add your own** members to `IInventoryBubbleItemInterface`, remember the default is zero/false/empty. Declare the `_Implementation` in the header and define it in the `.cpp` when you need a different default. See `API_Reference.md`.

---

## 12. Editor crash or dangling item on level change

`EndPlay` calls `ForceEject`, so an item is never left attached with physics off. If you see a leak, check for a `Can Accept Item` override or an `On Item Stored` handler that re-parents the item away from `ItemAnchor` — that hides it from the teardown path.

---

## 13. The item shrinks while it is still in my hand

| Cause | Fix |
|---|---|
| `bRejectItemsHeldByPlayer` is off | Turn it back on. With it off the bubble captures on entry regardless of who is holding the item, and will pull it out of your hand. |
| A grab system the bubble cannot recognise | Held detection asks VRE via `IsHeld`, then walks the item's whole attachment chain looking for a `APawn` owner. A rig that neither reports through `VRGripInterface` nor attaches the item anywhere under the pawn is invisible to both. Call `Try Store Item` yourself, or override `CanAcceptItem` in a Blueprint subclass with your own held test. |

Diagnostic: `On Item Rejected` with reason `HeldByPlayer` is the bubble working correctly — release the item inside the volume and it will be captured.

---

## 14. The item comes out of the bubble still small

| Cause | Fix |
|---|---|
| The grab system re-imposes a scale it cached at grip time | This is what VRE does, and it is handled: the bubble rewrites the live grip's scale on release. Confirm with the Verbose line `rewrote the VRE grip scale for '<Item>'`. If it never appears, the grab was not seen as a VRE grip. |
| Another framework with the same behaviour | Any system that caches a full transform at grab time will re-apply the shrunken scale. Restore the size from your own drop event, or call `Release Item` before the grab is established. |
| `GetStoredScaleMultiplier` on the item's interface | It stacks on `ShrinkScale` and is applied at capture. It does not affect restore, which always uses the recorded `OriginalWorldScale`. |

