# First Test Walkthrough

Click-by-click, from a fresh copy to a verified pass. Assumes UE 5.6 and a C++ project.
If your project is Blueprint-only, see §1.4.

---

## 1. Install and build

### 1.1 Copy

Copy the `InventoryBubble` folder into your project's `Plugins` directory:

```
<YourProject>/
  Plugins/
    InventoryBubble/
      InventoryBubble.uplugin
      Source/
```

Create `Plugins/` if it does not exist.

### 1.2 Regenerate project files

Close the editor first.

- **Windows Explorer:** right-click `YourProject.uproject` → *Generate Visual Studio project files*.
- **Command line:**

```
"D:\EpicGames\UE_5.6\Engine\Build\BatchFiles\Build.bat" -projectfiles -project="D:\Path\To\YourProject.uproject" -game -rocket -progress
```

Adjust the engine path to your install.

### 1.3 Build

**Command line** — this is the exact form used to verify this plugin:

```
"D:\EpicGames\UE_5.6\Engine\Build\BatchFiles\Build.bat" YourProjectEditor Win64 Development -Project="D:\Path\To\YourProject.uproject" -WaitMutex
```

Expect `Result: Succeeded`. You should also see one of:

```
[InventoryBubble] VRExpansionPlugin found - VRE adapter enabled.
[InventoryBubble] VRExpansionPlugin not found - VRE adapter compiled out.
```

**Visual Studio 2022:** open the `.sln`, set **Development Editor** + **Win64**, build the `YourProject` target.

**Rider:** select the `YourProjectEditor | Development | Win64` configuration, Build.

### 1.4 Blueprint-only projects

The plugin has a C++ module, so the project must be able to compile one. Add any C++ class (**Tools → New C++ Class → None**) to convert the project, then follow §1.2.

### 1.5 Enable the plugin

Launch the editor. **Edit → Plugins**, search `InventoryBubble`, tick **Enabled**, restart when prompted.

---

## 2. Author the bubble material (optional but recommended)

The plugin ships no `.uasset` files. It works without this step — it falls back to the engine sphere's default material — but the bubble will be opaque grey rather than translucent blue.

1. **Content Browser → InventoryBubble Content**. If you do not see it, enable *Show Plugin Content* in the Content Browser's Settings filter.
2. New folder `Materials`. Right-click → **Material**. Name it `M_InventoryBubble`.
3. Open it. In **Details**: **Blend Mode** = `Translucent`, **Shading Model** = `Unlit`.
4. Add these nodes, each converted to a parameter (right-click → *Convert to Parameter*):
   - `Vector Parameter` named **BaseColor**
   - `Scalar Parameter` named **Opacity**, default `0.35` → into **Opacity**
   - `Scalar Parameter` named **EmissiveIntensity**, default `1.0`
   - Multiply BaseColor × EmissiveIntensity → into **Emissive Color**
   - Optional rim: `Fresnel` node, its Exponent driven by `Scalar Parameter` **FresnelPower** (default `3.0`), result × `Scalar Parameter` **FresnelIntensity** (default `1.0`), added into the emissive chain.
5. Save. Right-click `M_InventoryBubble` → **Create Material Instance** → name it `MI_InventoryBubble_Empty`. Repeat for `MI_InventoryBubble_Occupied`.
6. Select your bubble actor, set **Bubble Material** to `M_InventoryBubble`.

The parameter names are themselves properties (`BaseColorParameterName`, `OpacityParameterName`, `EmissiveParameterName`), so a material using different names needs no code change — point those properties at your names instead.

---

## 3. Create the test level

1. **File → New Level → Basic**. Save as `L_BubbleTest`.
2. It already has a floor and a directional light. Confirm the floor is at Z = 0.
3. Add a player start appropriate to your framework:
   - **VR Template:** delete the default Player Start, drag in `VRPawn` (Content → VRTemplate → Blueprints), set **Auto Possess Player** to `Player 0`.
   - **VRE:** drag in your VRE character, same Auto Possess setting.
   - **No VR:** leave the default Player Start; use the §7 keyboard path.

---

## 4. Create a test item

1. **Content Browser → Add → Blueprint Class → Actor**. Name it `BP_TestCube`.
2. Open it. **Add Component → Cube**.
3. Select the Cube component:
   - **Physics → Simulate Physics: checked**
   - **Collision → Collision Presets: `PhysicsActor`**
4. **Class Defaults → Actor → Tags**: add one element with the value `Item`.

   This is the step everything else depends on. The tag goes on the **Actor**, not on the component, and it is case-sensitive.
5. Framework extras:
   - **VR Template:** Add Component → `GrabComponent`, **Grab Type** = `Free`.
   - **VRE:** either add the `VRGripInterface` in Class Settings → Interfaces, or reparent `BP_TestCube` to VRE's `GrippableStaticMeshActor` instead of `Actor` — the reparent is simpler and gets you a working grip for free.
   - **No VR:** nothing extra.
6. Compile, Save. Drag two copies into the level, about 1 m above the floor.

---

## 5. Place the bubble

1. Drag **Inventory Bubble Actor** (or your `BP_InventoryBubble`) into the level.
2. Position at roughly `Z = 120` — chest height, reachable.
3. Check these defaults in the Details panel:

| Property | Value | Why |
|---|---|---|
| `Item Tag` | `Item` | Must match §4.4 exactly, including case |
| `Capture Radius` | `20` | Raise to `35` if you keep missing it in VR |
| `Shrink Scale` | `0.2` | Visible but clearly shrunk |
| `Re Capture Cooldown` | `0.75` | Leave it — zero causes instant re-swallow |
| `Show Debug` | **checked** for this test | Draws the capture sphere |

---

## 6. Run in VR Preview

Dropdown next to **Play** → **VR Preview**. (Plain **Play** for the §7 path.)

| Step | Expected |
|---|---|
| Look at the bubble | Translucent sphere, green debug wireframe (Empty) |
| Push a cube into it | Shrinks smoothly over ~0.35 s and centres on the anchor |
| Watch it | Slow spin and gentle bob; wireframe now cyan |
| Push the second cube in | Nothing happens to it — it keeps its physics and keeps falling |
| Grab the stored cube | Returns to full size instantly, physics restored |
| Release it mid-air | Falls normally |
| Push it straight back in | Refused for 0.75 s, then accepted |

---

## 7. Non-VR test path

No headset needed, and it drives the same Layer A functions the VR path uses.

1. Open **Blueprints → Open Level Blueprint**.
2. Select one `BP_TestCube` in the viewport, then in the Level Blueprint right-click → **Create a Reference to BP_TestCube**.
3. Build this graph:

```
Event Key 1 (Pressed)
    └─▶ Store In Nearest Bubble
            Item         = <BP_TestCube reference>
            Max Distance = 1000.0

Event Key 2 (Pressed)
    └─▶ Release From Nearest Bubble
            Origin       = (BP_TestCube reference → Get Actor Location)
            Max Distance = 1000.0

Event Key 3 (Pressed)
    └─▶ Get All Bubbles ─▶ ForEach Loop ─▶ Get Bubble State ─▶ Print String

Event Key 4 (Pressed)
    └─▶ <BP_TestCube reference> ─▶ Destroy Actor        (for checklist test 6)
```

4. If the keys do not respond, confirm the Event nodes' **Consume Input** is off and the default pawn has input enabled.
5. Compile, Save, **Play**.

Press **1** — the cube snaps into the nearest bubble and shrinks. Press **2** — it comes back and falls. Press **3** — prints `Empty` / `Occupied` for every bubble. Press **4** — destroys the stored cube.

This validates capture, restore, cooldown, rejection and the destroyed-item path with no VR hardware.

---

## 8. Verification checklist

| # | Test | Pass criteria |
|---|---|---|
| 1 | Item enters an empty bubble | Shrinks smoothly to `ShrinkScale`, ends centred on the anchor, no snap |
| 2 | Second item overlaps an occupied bubble | Completely unaffected — keeps physics, keeps falling, not shrunk, not attached. `On Item Rejected` fires **once** with `Already Occupied` |
| 3 | Grab the stored item | Exact original scale, `Simulate Physics` back on, gravity back on, original collision profile restored |
| 4 | Push the released item straight back in | Refused for `ReCaptureCooldown` seconds, then accepted |
| 5 | Drop the released item | Falls and collides exactly like one that was never stored |
| 6 | Destroy the stored item while inside | Bubble returns to Empty, no crash, no dangling reference |
| 7 | Two bubbles side by side | Each holds one item independently; neither steals the other's |
| 8 | Empty bubble left alone | With `Show Debug` off the actor is not ticking — confirm with `stat game` or a breakpoint in `Tick` |

For #3, verify numerically rather than by eye: note the cube's **Scale** in the Details panel before storing, and compare after release. They must be identical, not merely close.

---

## 9. What to look for in the Output Log

Filter to `LogInventoryBubble`. Raise verbosity first:

```
Log LogInventoryBubble VeryVerbose
```

| Test | Expected line |
|---|---|
| Startup | `InventoryBubble module started (VRE support: disabled).` |
| 1 | `'BubbleActor_1' capturing 'BP_TestCube_1'.` then `'BubbleActor_1' stored 'BP_TestCube_1'.` |
| 2 | `Verbose: 'BubbleActor_1' rejected 'BP_TestCube_2' (reason 1).` |
| 3 (VR / Layer B) | `'BubbleActor_1' detected 'BP_TestCube_1' was taken by an external grab.` |
| 3 (§7 path) | `'BubbleActor_1' released 'BP_TestCube_1'.` |
| 4 | `Verbose: 'BubbleActor_1' rejected 'BP_TestCube_1' (reason 3).` |
| 6 | `'BubbleActor_1' had its stored item destroyed - resetting to empty.` |

Reason numbers follow `EInventoryBubbleRejectReason` declaration order: 0 `None`, 1 `AlreadyOccupied`, 2 `TagMismatch`, 3 `OnCooldown`, 4 `HeldByPlayer`, 5 `InterfaceRefused`, 6 `BlueprintRefused`, 7 `Invalid`, 8 `Busy`.

If test 1 produces no log line at all, the overlap never happened — go to `Troubleshooting.md` §1.
