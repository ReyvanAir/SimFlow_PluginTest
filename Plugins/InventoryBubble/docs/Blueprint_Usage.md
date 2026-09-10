# Blueprint Usage

Every recipe below is pure Blueprint. None require touching C++.

## Where to put the logic

Create a Blueprint child of `Inventory Bubble Actor` (right-click → Blueprint Class →
search "Inventory Bubble Actor"). Name it `BP_InventoryBubble`. Set your defaults in
the Class Defaults panel and bind events in the Event Graph.

---

## Flash red and play a sound on reject

On the bubble's Event Graph, bind **On Item Rejected**:

```
Event On Item Rejected (Item, Reason)
    └─▶ Switch on EInventoryBubbleRejectReason (Reason)
            Already Occupied ─▶ Play Sound at Location (Denied cue, GetActorLocation)
            On Cooldown      ─▶ (do nothing - this fires constantly while overlapping)
            Held By Player   ─▶ Play Sound at Location (Soft cue)
```

The bubble already flashes `BubbleColorRejecting` on its own via `RejectFlashTime`,
provided a material with an exposed colour parameter is assigned.

> Watch the `On Cooldown` case. While the player holds a just-released item inside the
> volume, overlaps keep firing. Filter that reason out or you will spam the sound.

---

## Play a sound and spawn an effect on store

```
Event On Item Stored (Item)
    ├─▶ Play Sound at Location   (Store cue, GetActorLocation)
    └─▶ Spawn Emitter at Location (Sparkle, GetActorLocation)
```

Use **On Capture Finished** instead if you want it at the end of the blend rather than
at the same moment — `On Item Stored` and `On Capture Finished` both fire in
`FinishCapture`, so in practice they are simultaneous.

---

## Only accept keys

Two ways.

**By tag (no graph work):** set `ItemTag` to `Key` in Class Defaults and tag your key
actors `Key`.

**By logic:** override `Can Accept Item` (right-click → Override Function).

```
Function Can Accept Item (Candidate) -> bool
    ├─▶ Parent: Can Accept Item (Candidate)      ← keep the built-in rules
    └─▶ AND  ( Candidate → Actor Has Tag "Key" )
        └─▶ Return
```

Always call the parent. Skipping it drops the occupied, cooldown and held-by-player
checks, and you will get double captures.

---

## Auto-eject after N seconds

```
Event On Item Stored (Item)
    └─▶ Set Timer by Event (Time = 5.0, Looping = false)  →  [Eject]

Custom Event [Eject]
    └─▶ Release Item (Restore Physics = true)
```

Clear the timer on **On Item Removed** so a player who grabs it early does not trigger
a stale eject.

---

## Wrist-mounted bubbles

Attach the bubble to the pawn rather than placing it in the level.

1. In your VR pawn, **Add Component → Child Actor Component**.
2. Set **Child Actor Class** to `BP_InventoryBubble`.
3. Attach it to the motion controller / hand mesh and offset it to the wrist.
4. Set `bRejectItemsHeldByPlayer` to **false** on that bubble — otherwise the item in
   the player's other hand can never be stored, because it is attached to the pawn.

That last step is the one people miss. The held-by-player check exists to stop a bubble
snatching an item out of a hand; on a wrist bubble that is exactly what you want.

---

## Show what is stored on a UI widget

```
Event On Bubble State Changed (New State)
    └─▶ Switch on EInventoryBubbleState
            Empty    ─▶ Set Text ("Empty")
            Occupied ─▶ Get Stored Item → Get Display Name → Set Text
```

---

## Non-VR testing from the level Blueprint

See `First_Test_Walkthrough.md` §7. The short version:

```
Event Key 1 (Pressed)
    └─▶ Store In Nearest Bubble (Item = <your cube ref>, Max Distance = 1000)

Event Key 2 (Pressed)
    └─▶ Release From Nearest Bubble (Origin = <cube location>, Max Distance = 1000)
```

Both are static library nodes — no bubble reference needed.

---

## Per-item rules with the interface

For rules that belong to the *item* rather than the bubble, add the
**Inventory Bubble Item** interface to the item Blueprint
(Class Settings → Interfaces → Add).

```
Function Can Be Stored (Bubble) -> bool
    └─▶ Return  NOT (Is Quest Active)      ← a quest item refuses storage mid-quest

Function Get Stored Scale Multiplier -> float
    └─▶ Return 0.5                          ← this item stores half as large again

Event On Stored In Bubble (Bubble)
    └─▶ Set Glow Enabled (true)
```

You only implement what you need; unimplemented members fall back to safe defaults.
