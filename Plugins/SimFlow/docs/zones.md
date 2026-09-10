# Zones

**Actor:** `SimFlow Zone` (`ASimFlowZone`)
**Place via:** drag a **SimFlow Zone** into the level
**Class group:** SimFlow

A zone is a named volume a flow can ask questions about. "What is in the parts bin?"

It solves two problems at once. The first is naming a place: a zone carries its own
[Identity](identity.md) component, so a task finds it by tag (`Zone.PartsBin`)
exactly the way it finds items. One identity mechanism for the whole plugin rather
than a second one bolted on for zones.

The second is knowing when something has actually been put down. A trainee holding
an object *over* the bin hasn't placed it, so a zone tracks not only what overlaps
but whether each object has settled.

What a zone won't do is have an opinion about right and wrong. It reports what's
there and [Place Object In Zone](tasks/place-object-in-zone.md) does the judging.
That split is why the same bay can be the correct answer in one exercise and a
distractor in the next without anyone touching the level.

## What a zone is made of

| Field | Type | Default | Meaning |
|---|---|---|---|
| Box | Box Component | auto-created | The volume. Read-only in Details — resize it on the placed actor, not in Blueprint defaults. |
| Identity | SimFlow Identity | auto-created | Names the zone. Set its **Identity Tags** to something like `Zone.PartsBin`. |
| Settle Mode | Enum | `Standard` | How careful the zone is about calling an object placed. |
| Draw Debug | Bool | `false` | Draws the box in the level, green while tracking something and silver when empty. |

That is the whole setup for a normal zone: size the box, tag the identity, leave
Settle Mode alone.

**Settle Mode** has three values:

| Value | Means |
|---|---|
| `Instant` | Placed the moment it is inside and out of the trainee's hands. No pause, no speed check. |
| `Standard` | A `0.35 s` pause, and physics objects have to drop below `20` speed. Suits anything put down by hand. |
| `Custom` | Reveals **Settle Time**, **Settle Speed Threshold** and **Require Detached** under **Settling** so you can dial it in. |

Under `Custom`:

| Field | Type | Default | Meaning |
|---|---|---|---|
| Settle Time | Float (s, min 0) | `0.35` | How long an object must sit still inside the zone before it counts as placed. |
| Settle Speed Threshold | Float (min 0) | `20.0` | Simulating objects must also drop below this speed. `0` skips the check. |
| Require Detached | Bool | `true` | An object still attached to something isn't considered placed. |

Switching *away* from Custom resets those three to the chosen mode's values, so the
numbers you see under Custom are always the numbers the zone is actually using.

The rest lives behind the **Advanced** arrow, and is there for the awkward cases:

| Field | Type | Default | Meaning |
|---|---|---|---|
| Track Filter | [Actor Query](actor-query.md) | *empty* | Only actors passing this are tracked at all. |
| Require Identity Component | Bool | `true` | Ignore untagged actors, the player pawn included. |
| Broadcast Flow Events | Bool | `false` | Also raises `SimFlow.Event.Placed` and `SimFlow.Event.Removed` on every flow with the actor as payload, so a plain [Wait For Event](tasks/wait-for-event.md) task can use the zone too. |

## Getting the setup wrong

A zone with no **Identity Tags** still works, but no tag query can find it — you'd
have to point at it with `Specific Actor`. That's usually an oversight rather than a
choice.

Leaving **Track Filter** empty is the normal and recommended case; it falls back to
`Require Identity Component`. Leaving it empty *and* turning
`Require Identity Component` off is the combination to avoid, because then the zone
tracks everything that overlaps, player pawn included.

**Settle Mode** set to `Instant` means an object settles on the first tick it's at
rest and skips the speed check entirely, so a sliding physics object can count as
placed — it's meant for buttons, sockets and snap points rather than loose props.
Under `Custom`, a **Settle Time** of `0` and a **Settle Speed Threshold** of `0` do
the same thing one field at a time. And a **Box** left at its default size is often
far too small to catch anything — resize it in the level.

## How an object is judged put down

Every tick, for each tracked actor, the zone asks whether it's at rest.

First, is it flagged held (`SimFlow Identity → Is Held`)? If so it is not at rest,
full stop. This check comes first and overrides everything below it.

Then, is it attached? With `Require Detached` on — which every mode but `Custom`
implies — an actor with an attach parent isn't at rest. Finally, is it slow enough?
When the mode's speed threshold is above 0,
a physics-simulating root component is compared against its physics linear velocity,
and anything else against the actor's own velocity.

An actor at rest accumulates still-time, and when that reaches the mode's settle time the
zone marks it settled and fires **On Actor Settled**. Picking the object back up
resets it completely — still-time returns to zero and the settled flag clears, so it
has to settle again from scratch.

### Why the held flag exists

Attachment is only one of the ways a VR framework can hold an object. VRExpansion,
for one, holds most grip types with a physics constraint and never reparents the
actor, so an object in the trainee's hand can look perfectly detached from out here.

Call `Set Held(true)` from wherever your grab succeeds and `Set Held(false)` on
release, and placement checks become exact regardless of how grabbing is
implemented. See [held state](identity.md#held-state).

## Delegates and queries

| Delegate | Fires when | Use it for |
|---|---|---|
| On Actor Entered | An actor overlaps, before it has settled | Highlighting, hover feedback |
| On Actor Exited | The actor stops overlapping | Clearing feedback |
| On Actor Settled | The actor is genuinely put down | This is the one tasks listen to |

The Blueprint-pure queries are `Get Contained Actors` for everything overlapping
whether settled or not, `Get Settled Actors` for what's been put down and left
alone, `Contains Actor` and `Is Actor Settled` for single-actor checks, and
`Get Display Name Text` for the zone's display name in UI and mistake text.

## What it needs

The [identity system](identity.md) names the zone and supplies the held state that
settling depends on; [Actor Query](actor-query.md) backs the `Track Filter` field.
And the box has to actually generate overlap events, with items carrying collision
that responds to it.

On the other side, [Place Object In Zone](tasks/place-object-in-zone.md) depends on
zones, and fails at start if it can't resolve one.

## A parts bin

Letting a flow ask "is the foam extinguisher in the bay?"

1. In Project Settings → Gameplay Tags, add `Zone.ExtinguisherBay` and
   `Item.Extinguisher.Foam`.
2. Drag a **SimFlow Zone** into the level beside the bay.
3. Select the zone, select its **Box** component, and scale it in the viewport to
   cover the bay. Do this on the level instance.
4. On the zone's **Identity** component, set **Identity Tags** to
   `Zone.ExtinguisherBay` and **Display Name** to `Extinguisher Bay`.
5. Leave `Track Filter` empty and `Require Identity Component` on, so the zone
   tracks any item carrying an identity component and ignores the player.
6. Check the extinguisher has a collision primitive that generates overlap events
   with the box.
7. Turn on `Draw Debug` while testing. The box turns green when it's tracking
   something, which tells you immediately whether overlaps are arriving.

A [Place Object In Zone](tasks/place-object-in-zone.md) task with Zone =
`Zone.ExtinguisherBay` will now find it.

> **Screenshot needed:** a SimFlow Zone selected in the level with Draw Debug on,
> showing the green box around a parts bin.

## If nothing is tracked or settled

**Nothing is ever tracked.** Overlaps aren't reaching the box. Check the item has
collision that overlaps the box's object type, and that the box is big enough. Turn
on `Draw Debug` — a silver box is tracking nothing.

**Objects count as placed while still in the trainee's hand.** Held state isn't
wired up and your VR framework doesn't reparent on grab. This is the most common
zone problem by a wide margin.

**The player pawn is being tracked.** `Require Identity Component` was turned off
with no `Track Filter` set.

**Objects settle, then immediately un-settle, over and over.** The object is
jittering above the mode's speed threshold, which is common for physics objects
resting on uneven collision. Switch **Settle Mode** to `Custom` and raise
`Settle Time`, or raise the threshold.

**A task can't find the zone.** The zone's Identity has no tags, or two zones share
a tag and the wrong one resolved first — see
[resolving to a single actor](actor-query.md#resolving-to-a-single-actor).

**Resizing the box in the Blueprint did nothing.** Zones are sized per level
instance. Resize the placed actor's Box component in the viewport.

*Next: [Place Object In Zone](tasks/place-object-in-zone.md) ·
[Identity](identity.md) · [Actor Query](actor-query.md)*
