# Fire extinguisher drill

**Shows:** [Identity](../identity.md) · [Zones](../zones.md) ·
[Place Object In Zone](../tasks/place-object-in-zone.md) ·
[Wait For Event](../tasks/wait-for-event.md) · near-miss feedback · scoring

## The scenario

The trainee must:

1. Walk to the fire point.
2. Pick up the **foam** extinguisher — not the CO2 one sitting beside it.
3. Put it in the extinguisher bay.

The CO2 unit is a deliberate distractor. Grabbing or placing it should produce
*"close — that's the CO2 unit, you want foam"*, not a generic buzzer. That
distinction is the whole reason this drill is built with identity tags rather than
direct actor references.

## 1. Tags

Project Settings → Project → Gameplay Tags. Add:

```
Item.Extinguisher.Foam
Item.Extinguisher.CO2
Zone.ExtinguisherBay
```

Note the shape: both extinguishers share `Item.Extinguisher`, two levels deep. At
the default [Min Related Tag Depth](../actor-query.md#min-related-tag-depth) of 2,
that is exactly what makes the CO2 unit a **near miss** rather than a random wrong
object.

`SimFlow.Event.Grab` already exists — it ships with the plugin.

## 2. The items

On **`BP_Extinguisher_Foam`** (the Blueprint, not a level instance):

1. **Add Component → SimFlow Identity**.
2. **Identity Tags** = `Item.Extinguisher.Foam`
3. **Display Name** = `Foam Extinguisher`

Repeat on **`BP_Extinguisher_CO2`** with `Item.Extinguisher.CO2` and
`CO2 Extinguisher`.

### Held state and the grab event

In whatever handles grabbing, on a **successful grab**:

```
Get Component By Class (SimFlow Identity) → Set Held (true)
Broadcast Flow Event  Event Tag = SimFlow.Event.Grab   Payload = self
```

On **release**: `Set Held (false)`.

> **Both halves matter.** `Set Held` is what stops the [zone](../zones.md) treating
> an object as "placed" while it is still in the trainee's hand — attachment alone
> is not reliable, because many VR frameworks hold by physics constraint without
> reparenting. And `Payload = self` must come from the **actor's** graph; a UMG
> widget can never satisfy a payload check. See
> [Events](../events.md#always-pass-the-owning-actor-as-the-payload).

## 3. The zone

1. Drag a **SimFlow Zone** into the level at the bay.
2. Select its **Box** component and scale it in the viewport to cover the bay.
3. On its **Identity** component: **Identity Tags** = `Zone.ExtinguisherBay`,
   **Display Name** = `Extinguisher Bay`.
4. Leave **Track Filter** empty and **Require Identity Component** on.
5. Tick **Draw Debug** while building — green means it is tracking something.

## 4. The flow

Create `F_ExtinguisherDrill`.

```
  ┌───────┐   ┌────────────────┐   ┌────────────────┐   ┌──────────────────┐   ┌────────┐
  │ Start │──▶│ Task           │──▶│ Task           │──▶│ Task             │──▶│ Finish │
  │       │   │ Go To Location │   │ Wait For Event │   │ Place Object     │   │Complete│
  └───────┘   │ (fire point)   │   │ (grab foam)    │   │ In Zone          │   └────────┘
              └────────────────┘   └────────────────┘   └──────────────────┘
```

### Node 1 — Go To Location

| Field | Value |
|---|---|
| Task | [Go To Location](../tasks/go-to-location.md) |
| Target Location | *drag the viewport widget to the fire point* |
| Acceptance Radius | `200` |
| Ignore Z | on |
| Instruction | `Move to the fire point` |
| *(Task node)* Time Limit | `90` |

### Node 2 — Wait For Event (grab the right one)

| Field | Value |
|---|---|
| Task | [Wait For Event](../tasks/wait-for-event.md) |
| Event Tag | `SimFlow.Event.Grab` |
| Expected Payload → Required Tags | `Item.Extinguisher.Foam` |
| Mismatch Policy | `Count Mistake (Keep Waiting)` |
| Payload To Blackboard Key | `HeldExtinguisher` |
| Instruction | `Pick up the foam extinguisher` |
| Score On Success | `10` |

Grabbing the CO2 unit now records a mistake and **keeps waiting**, so the trainee
can put it down and correct themselves.

### Node 3 — Place Object In Zone

| Field | Value |
|---|---|
| Task | [Place Object In Zone](../tasks/place-object-in-zone.md) |
| Zone → Required Tags | `Zone.ExtinguisherBay` |
| Accepted Items → Required Tags | `Item.Extinguisher.Foam` |
| Required Count | `1` |
| Require Settled | on |
| Wrong Item Policy | `Count Mistake (Keep Waiting)` |
| Instruction | `Place the foam extinguisher in the bay` |
| Score On Success | `20` |

Wire each Task node's **Completed** to the next, and the last to a
[Finish](../nodes/finish.md) node set to **Complete (Success)**.

Leave `Failed`, `Skipped` and `Timed Out` unwired — with `Fallback To Completed` on
(the default) they fall through, which keeps this first pass tidy.

## 5. The feedback

This is what makes the drill feel intelligent rather than binary.

On **node 2**, bind `On Payload Rejected` (Payload, Match Quality). On **node 3**,
bind `On Wrong Item Placed` (Item, Match Quality). In both, switch on the quality:

| Match Quality | Message |
|---|---|
| **Related (Near Miss)** | *"Close — that's the CO2 unit. You want foam for this fire."* |
| **No Match** | *"That's not an extinguisher."* |

Use `Get Identity Display Name` on the offending actor to name it in the message —
that is what `Display Name` on the identity component is for.

## 6. Run it

1. Add a [SimFlow Component](../simflow-component.md) to your Game Mode.
2. **Flow Asset** = `F_ExtinguisherDrill`, **Start Mode** = **Auto - On First Tick**,
   **Flow Save Id** = `ExtinguisherDrill`.
3. Console: `SimFlow.Debug 1` to see live state.
4. Press Play.

## Making it a different exercise

The point of building it this way: **the level never changes.**

| To ask for… | Change |
|---|---|
| The CO2 unit instead | Node 3's `Accepted Items` → `Item.Extinguisher.CO2` |
| Any extinguisher | `Accepted Items` → `Item.Extinguisher` |
| Any *except* CO2 | `Accepted Items` = `Item.Extinguisher`, `Rejected Items` = `Item.Extinguisher.CO2` |
| Two extinguishers in the bay | `Required Count` = `2` |
| A harsher drill | `Wrong Item Policy` → `Count Mistake And Fail Task`, and wire `Failed` to a retry section |

Not one of those touches a Blueprint or the level.

## Troubleshooting this workflow

**Node 3 fails the instant it starts.**
The `Zone` query found nothing. Check the zone is in the level and its Identity has
`Zone.ExtinguisherBay`. See [Place Object In Zone](../tasks/place-object-in-zone.md#when-it-misbehaves).

**The extinguisher never registers as placed.**
It is not settling — usually missing `Set Held(false)` on release, or no collision
overlap. Turn on the zone's `Draw Debug`.

**Grabbing does nothing.**
The grab event is not reaching the flow, or you broadcast from a widget. Add a print
node next to the broadcast to confirm it runs.

**The CO2 unit reports "No Match" instead of a near miss.**
The tags are not sharing two levels. `Extinguisher_CO2` and `Extinguisher_Foam` as
flat tags share nothing — they must be `Item.Extinguisher.CO2` and
`Item.Extinguisher.Foam`.

*Next: [Valve startup procedure](valve-startup-procedure.md) ·
[Zones](../zones.md) · [Troubleshooting](../troubleshooting.md)*
