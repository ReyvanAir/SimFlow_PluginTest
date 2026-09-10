# InventoryBubble

A VR "inventory bubble" for Unreal Engine 5.6: a translucent sphere that captures a
grabbable item, shrinks it, holds exactly one, and hands it back at its original size
when the player pulls it out.

Drop the folder into `Plugins/`, enable it, and it works. No edits to your project's
C++ or build files, and no hard dependency on any VR framework.

### 📖 [Read the documentation &rarr;](https://reyvanair.github.io/Inventorybubble/)

The full **user guide** and **developer reference** are published as a single browsable
page: install and quickstart, every designer-facing property, Blueprint recipes, VR
integration, a verification checklist, and the architecture behind the three-layer grab
detection. The same content lives as Markdown in [`docs/`](docs/).

## Features

One item per bubble, with everything else that overlaps left completely untouched;
smooth eased capture rather than a snap; exact restore of scale, physics, gravity,
collision profile and attachment on release; a re-capture cooldown so an item you just
pulled out is not swallowed again; an optional item interface for per-item rules; idle
spin and bob; a parameter-driven reject flash; and a debug draw of the capture volume.

## Install

1. Copy `InventoryBubble/` into your project's `Plugins/` folder.
2. Regenerate project files and build (see [`docs/First_Test_Walkthrough.md`](docs/First_Test_Walkthrough.md)).
3. **Edit → Plugins**, search "InventoryBubble", enable, restart.

## 60-second quickstart

1. Drag an **Inventory Bubble Actor** into your level at chest height.
2. Make any physics actor storable by adding the actor tag `Item`.
3. Play. Push the item into the sphere — it shrinks and centres.
4. Grab it back out — it returns to full size with its physics restored.

Nothing else is required. Grab detection works with any framework because the bubble
watches for the item being re-attached away from it (Layer B, below).

## Compatibility

| Setup | Works? | What you do |
|---|---|---|
| No VR framework (plain PIE / mouse / custom code) | Yes | Call `Try Store Item` / `Release Item` |
| Any grab system that re-attaches the item | Yes | Nothing — Layer B detects it |
| UE5 VR Template (`GrabComponent`) | Yes | Nothing, or one `Notify Item Grabbed` node |
| VR Expansion Plugin | Yes | Nothing — VRE code compiles in automatically |
| Both installed | Yes | Both adapters coexist |

`InventoryBubble.Build.cs` probes for `VRExpansionPlugin.uplugin` at build time and
defines `WITH_VR_EXPANSION_PLUGIN` to 1 or 0 accordingly, so the module compiles in a
project that has neither, either, or both.

## No C++ required

Every behaviour is reachable from Blueprint.

**Properties** — `ItemTag`, `CaptureRadius`, `ReCaptureCooldown`, `ShrinkScale`,
`bCaptureOnRelease`, `RecheckInterval`,
`CaptureBlendTime`, `ReleaseBlendTime`, `EaseCurve`, `StoredRotation`, `AnchorOffset`,
`bDisablePhysicsWhileStored`, `bIdleSpin`, `IdleSpinRate`, `bIdleBob`,
`IdleBobAmplitude`, `IdleBobSpeed`, `IdleTickInterval`, `BubbleColorEmpty`, `BubbleColorOccupied`,
`BubbleColorRejecting`, `BubbleOpacity`, `RejectFlashTime`, `BubbleMaterial`,
`bAutoDetectGrab`, `bUseGrabComponentAdapter`, `bRejectItemsHeldByPlayer`, `bShowDebug`.

**Events** — `OnItemStored`, `OnItemRemoved`, `OnItemRejected`, `OnBubbleStateChanged`.

**Overridable** — `CanAcceptItem`, `GetStoredItemTransform`, `OnCaptureStarted`,
`OnCaptureFinished`, `OnReleaseStarted`, `OnReleaseFinished`.

**Callable** — `TryStoreItem`, `ReleaseItem`, `ForceEject`, `ClearCooldowns`,
`NotifyItemGrabbed`. **Pure** — `IsOccupied`, `GetStoredItem`, `GetBubbleState`,
`IsOnCooldown`.

See [`docs/API_Reference.md`](docs/API_Reference.md) for types and defaults, and [`docs/Blueprint_Usage.md`](docs/Blueprint_Usage.md) for
recipes.

## Content assets

The plugin ships **no `.uasset` files**. The actor loads `/Engine/BasicShapes/Sphere`
in its constructor and builds a Dynamic Material Instance from whatever material the
mesh carries, so it is fully functional out of the box.

To get the intended translucent look you author `M_InventoryBubble` yourself — it is a
five-minute job and [`docs/First_Test_Walkthrough.md`](docs/First_Test_Walkthrough.md) §2 walks through it. Assign it to
`BubbleMaterial` and the existing parameter-driven colour, opacity and reject flash
start working. The parameter names the DMI writes are themselves properties
(`BaseColorParameterName`, `OpacityParameterName`, `EmissiveParameterName`), so a
material using different names needs no code change.

## Replication

Not implemented in v1, and deliberately structured so it can be added without a
rewrite: all mutation goes through `TryStoreItem` / `ReleaseItem`, and the authoritative
state is a single `TWeakObjectPtr<AActor> StoredItem` plus an `EInventoryBubbleState`.
Adding `bReplicates`, a replicated `StoredItem`, and an `OnRep` that replays the visual
blend on clients is the whole job. See [`docs/Architecture.md`](docs/Architecture.md).

## Requirements

Unreal Engine 5.6. Runtime module, `Default` loading phase, no editor-only headers.

## Licence

Same terms as the host project.
