# VR Integration

The bubble does not depend on any grab framework. Read `Architecture.md` for why there
are three layers; this document is the practical integration path for each setup.

## Decision table

| Your setup | What to do |
|---|---|
| UE5 VR Template | Nothing. If your GrabComponent's delegates take parameters, add one node (§1.2). |
| VR Expansion Plugin | Nothing. VRE code compiles in automatically. |
| Custom grab that **re-attaches** the item | Nothing — Layer B handles it. |
| Custom grab that **only sets the transform** | Call `Notify Item Grabbed` or `Release Item` (§3). |
| No VR at all | Call `Try Store Item` / `Release Item` (see `First_Test_Walkthrough.md` §7). |

---

## 1. UE5 VR Template

### 1.1 The zero-work path

1. Enable the plugin.
2. Drag `BP_InventoryBubble` into the level at a reachable height.
3. Add the actor tag `Item` to any Blueprint that already has a `GrabComponent`.
4. VR Preview. Push the item in; it shrinks. Grab it; it comes back full size.

Grabbing works because the template's grab re-attaches the item to the hand, which
breaks its attachment to `ItemAnchor` — Layer B sees that within one frame.

### 1.2 If you want an explicit hook

Layer C tries to bind to `OnGrabbed` / `OnDropped` on the GrabComponent by reflection,
but **only when the delegate takes no parameters**. If yours takes parameters (some
template revisions pass the hand or the component), the bind is skipped and you will
see this at `Verbose`:

```
LogInventoryBubble: Verbose: Delegate 'OnGrabbed' on 'GrabComponent' takes parameters -
                            skipping bind, Layer B will cover it.
```

That is not an error — Layer B still works. For an explicit hook, in your item Blueprint:

```
Event On Grabbed (from your GrabComponent)
    └─▶ Get Bubble Holding Item (Item = Self)
            └─▶ Is Valid ─▶ Notify Item Grabbed (Item = Self)
```

`Notify Item Grabbed` is a no-op unless that bubble is actually holding the item, so it
is safe to call unconditionally.

### 1.3 Which pawn

Use the template's `VRPawn` (Content → VRTemplate → Blueprints). No changes needed.

---

## 2. VR Expansion Plugin

### 2.1 Build-time detection

`InventoryBubble.Build.cs` probes for `VRExpansionPlugin.uplugin` under the project's
`Plugins/` and the engine's `Plugins/`. On a match it adds `VRExpansionPlugin` to
`PrivateDependencyModuleNames` and defines `WITH_VR_EXPANSION_PLUGIN=1`; otherwise `0`.
The build prints which path it took:

```
[InventoryBubble] VRExpansionPlugin found - VRE adapter enabled.
[InventoryBubble] VRExpansionPlugin not found - VRE adapter compiled out.
```

If you install VRE *after* building InventoryBubble, **rebuild** — the probe runs at
build time, not at startup.

### 2.2 Setup

1. Install VRE and rebuild.
2. Use a VRE pawn (`AVRCharacter` or your subclass).
3. Your grippable item already implements `VRGripInterface`. Add the actor tag `Item`.
   The tag goes on the **Actor**, not on a component, and it is case-sensitive — a tag on
   the component is the single most common reason nothing happens, and it is refused
   silently by design.
4. Place a bubble. Done.

VRE's grip re-attaches (or re-parents) the gripped actor, so Layer B covers the grab. The
compiled-in adapter additionally recognises `IVRGripInterface` implementers for
eligibility reporting.

### 2.3 VRE-specific notes

- **Held state is read from VRE directly.** VRE's physics-constraint grips drive the
  object through a physics handle and never reparent it, so attachment tells you nothing.
  The bubble calls `IVRGripInterface::Execute_IsHeld` instead, which is correct for every
  VRE grip type. This is what stops a bubble swallowing an item out of your hand, and
  what lets it notice you taking the stored item back. No setup on your side.
- **Push it in and let go.** Carrying a gripped item into the bubble does nothing on
  purpose — it is still held. Release it inside and it is captured within
  `RecheckInterval` (0.1 s). See `bCaptureOnRelease` in `API_Reference.md`.
- **Scale is corrected on the live grip.** VRE caches a grip's `RelativeTransform` when
  the grip is made and re-applies its scale every tick, so a grip made on a stored
  (shrunken) item would keep re-shrinking it after restore. The bubble rewrites that
  grip's scale via `SetGripRelativeTransform`, so the item comes back to full size in
  your hand rather than after you drop it.
- **`bRejectItemsHeldByPlayer`.** VRE attaches gripped actors under the pawn, so the
  bubble will refuse an item currently in a hand. That is intended: push the item in and
  let go, or set the flag false for wrist bubbles.
- The bubble never calls VRE grip functions itself. It only restores the physics state
  it captured, so VRE's own grip logic stays authoritative.

---

## 3. My grab system isn't listed

Use Layer A directly. Two functions, no dependencies:

```
When your system takes the item:
    Notify Item Grabbed (Item)       ← preferred, no-ops if we aren't holding it
       or
    Release Item (Restore Physics = true)

When you want to put something in:
    Try Store Item (Item)  →  returns false and fires On Item Rejected if refused
```

If your system re-attaches the item to anything other than `ItemAnchor`, you can skip
even that — Layer B detects it. Confirm with:

```
Log LogInventoryBubble Log
```

and look for:

```
LogInventoryBubble: '<Bubble>' detected '<Item>' was taken by an external grab.
```

If you never see that line, your system is not re-attaching, and you need the explicit
call above.

### Turning layers off

- `bAutoDetectGrab = false` disables Layer B — do this only if you drive releases
  yourself, or the bubble will never let go.
- `bUseGrabComponentAdapter = false` disables Layer C binding.

Layer A cannot be disabled; it is the implementation.
