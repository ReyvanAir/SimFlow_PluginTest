# Identity

**Component:** `SimFlow Identity` (`USimFlowIdentityComponent`)
**Add via:** Add Component → SimFlow Identity
**Class group:** SimFlow

The identity system answers one question: what is this object?

Without it a flow can only refer to objects by pointing at a specific actor placed
in a level, which breaks the moment an object is spawned at runtime, duplicated, or
swapped for a different variant. Identity replaces "that actor over there" with "any
foam extinguisher", and that survives all three.

An identity component carries gameplay tags describing what its actor is. A task
asks for `Item.Extinguisher` and any foam or CO2 extinguisher answers; ask for
`Item.Extinguisher.Foam` and only the foam one does.

Gameplay tags rather than the actor's own `Tags` array, because gameplay tags are
validated at author time, they autocomplete in the Details panel, and they nest.
That nesting is what lets a task tell a near miss — `Item.Extinguisher.CO2` when it
wanted `Item.Extinguisher.Foam` — apart from something completely wrong like
`Item.Wrench`. See [match quality](actor-query.md#how-matching-works).

## What the component holds

| Field | Type | Default | Meaning |
|---|---|---|---|
| Identity Tags | Gameplay Tag Container | *empty* | What this object is, e.g. `Item.Extinguisher.Foam`. Several tags are fine. |
| Display Name | Text | *empty* | Shown in mistake text and tutorial UI. Falls back to the actor's name. |
| Identity Id | Name | `None` | A stable id for your own analytics and script. Nothing in SimFlow reads it. |

**Identity Tags** is the load-bearing field; everything else is optional. Leave it
empty and the actor is still *tracked* by zones — it has an identity component,
which is what `Require Identity Component` checks — but it can never satisfy a
tag-based [Actor Query](actor-query.md), because there's nothing to match. An
identity component with no tags is almost always an oversight.

Multiple tags are allowed and often useful; an object can be both
`Item.Extinguisher.Foam` and `Item.Heavy`. How multiple tags are matched is
controlled by the *query's* `Require All Tags` setting, not by anything here.

**Display Name** is purely presentational. It shows up in generated mistake
descriptions — "Placed CO2 Extinguisher in the bay, expected Foam Extinguisher" —
and anywhere you call `Get Identity Display Name`. Left empty, SimFlow falls back to
`AActor::GetName()`, which gives you something like `BP_Extinguisher_C_2`. Fine for
debugging, poor in front of a trainee, so set it on anything a trainee might get
wrong.

### Identity Id, and what it isn't

`Identity Id` is a free-form Name field that no part of SimFlow reads.

It isn't a query field — [Actor Query](actor-query.md) has no `Identity Id` option —
it isn't written into save data, and no built-in task or condition consults it.
Setting it has no effect on flow behaviour at all.

What it's *for* is a stable, human-chosen handle you read from your own Blueprint or
C++: analytics events, telemetry, external LMS reporting, addressing a specific
actor from your own script. Read it straight off the component:

```
Get Component By Class (SimFlow Identity) → Identity Id
```

To make an object findable by a flow, give it **Identity Tags**. If you want tasks
to target one specific instance, the supported routes are the query's
`Specific Actor` field for a placed level actor, or `Blackboard Key` for one chosen
at runtime — see [which field to use](actor-query.md#which-field-to-use).

## Held state

Two functions on the component, and the part most VR projects need. **Set Held**
(callable, takes `bIn Is Held`) flags this object as currently held by the trainee.
**Is Held** (pure) reports whether something is holding it.

They exist because a [Zone](zones.md) can't reliably work this out for itself.
Attachment is only one of the ways a VR framework can hold an object — VRExpansion,
for instance, holds most grip types with a physics constraint and never reparents
the actor. An object sitting in the trainee's hand can look perfectly detached from
the outside, and the zone would wrongly judge it as placed.

So call `Set Held (true)` from wherever your grab succeeds and `Set Held (false)` on
release. Placement checks then become exact regardless of how grabbing is
implemented in your project.

The flag is transient. It isn't saved and resets to `false` when the game starts,
which is correct — nothing is being held at load time.

Held state is checked *before* the attachment fallback, so an object flagged held is
never at rest whatever its attachment or velocity says. See
[how an object is judged put down](zones.md#how-an-object-is-judged-put-down).

## How identities are created

Add the component to the **item Blueprint**, not to each copy you place in the
level. Every instance you place or spawn then carries the same tags automatically.

1. Open your item Blueprint, say `BP_Extinguisher_Foam`.
2. **Add Component → SimFlow Identity**.
3. Select it and set **Identity Tags** in the Details panel.
4. Set **Display Name** to something a trainee would recognise.
5. Compile and save.

That's the whole setup. There's no registration step, no manifest and no subsystem
to notify — tasks and zones discover identity components by looking at the actor
directly.

If your actors already implement `IGameplayTagAssetInterface`, SimFlow reads those
tags too: `Get Identity Tags` returns the identity component's tags and the
interface's owned tags appended together. A GAS project doesn't need a second
component on every actor, though adding one is still how you get `Display Name` and
held state.

## Adding your own tags

Identity tags are ordinary Unreal gameplay tags. SimFlow keeps no separate registry,
so you create them the way you create any gameplay tag in your project.

Through the editor: **Edit → Project Settings → Project → Gameplay Tags**, expand
**Gameplay Tags**, click **Add New Gameplay Tag**, and enter the name, e.g.
`Item.Extinguisher.Foam`. Add a comment if it helps. The tag lands in
`Config/DefaultGameplayTags.ini` in your project and is immediately available in
every tag picker. You can also add one inline from any **Identity Tags** picker via
**Add New Gameplay Tag** at the bottom of the dropdown.

### Tags shipped with the plugin

These are available with no setup:

| Tag | Used for |
|---|---|
| `SimFlow.Event` | Parent of all built-in event tags |
| `SimFlow.Event.Generic` | General-purpose event |
| `SimFlow.Event.Interact` | The trainee interacted with something |
| `SimFlow.Event.Grab` | Object picked up |
| `SimFlow.Event.Release` | Object let go |
| `SimFlow.Event.ButtonPressed` | Button press |
| `SimFlow.Event.Placed` | Raised by a zone when an object settles (opt-in) |
| `SimFlow.Event.Removed` | Raised by a zone when an object leaves (opt-in) |
| `SimFlow.Mistake` | Parent of all mistake kinds |
| `SimFlow.Mistake.WrongItem` | Wrong object placed |
| `SimFlow.Mistake.WrongTarget` | Wrong object in an event payload |
| `SimFlow.Mistake.WrongOrder` | Acted out of turn |
| `SimFlow.Mistake.WrongAnswer` | Wrong quiz answer |
| `SimFlow.Sample.GrabExtinguisher` | Used by the sample flow |
| `SimFlow.Sample.PullPin` | Used by the sample flow |

Note what isn't there: no `Item.*` or `Zone.*` tags ship with the plugin. Those
appear throughout this documentation as examples, but the hierarchy for your own
objects is yours to create — `Item.Extinguisher.Foam` won't exist in your project
until you add it.

Keep your item tags in your own namespace (`Item.`, `Zone.`, `Tool.`, or your
project's prefix) rather than under `SimFlow.`. That hierarchy belongs to the
plugin, and a future version may add to it.

## Naming rules

Identity tags follow Unreal's gameplay tag rules; SimFlow adds none of its own. The
hierarchy separator is `.`, so `Item.Extinguisher.Foam` is three levels deep. Spaces
are rejected, as are leading and trailing dots. A tag has to be registered in the
project's tag list before it can be selected — you can't type an arbitrary string
into a tag field. Tags match case-insensitively but store the case you enter, so
pick a convention and hold to it. There's no hard depth limit.

The editor validates as you type and won't let you create a malformed tag, so these
are mostly enforced for you rather than something to memorise.

### Why depth matters more than it looks

Tag depth isn't cosmetic. The `Min Related Tag Depth` setting on every
[Actor Query](actor-query.md#min-related-tag-depth) decides how many leading tag
nodes two tags must share before a wrong answer counts as a near miss rather than as
completely wrong — and that difference drives the feedback a trainee gets.

At the default of `2`:

| Query asks for | Trainee provides | Shared depth | Result |
|---|---|---|---|
| `Item.Extinguisher.Foam` | `Item.Extinguisher.Foam` | — | Exact Match |
| `Item.Extinguisher.Foam` | `Item.Extinguisher.CO2` | 2 (`Item.Extinguisher`) | Related (Near Miss) |
| `Item.Extinguisher.Foam` | `Item.Wrench` | 1 (`Item`) | No Match |

A flat hierarchy — `Extinguisher_Foam`, `Extinguisher_CO2`, `Wrench` — makes every
wrong answer identical and throws away the plugin's ability to say "right idea,
wrong extinguisher". Design the tree so things that are plausibly confusable share a
parent.

A workable convention:

```
Item.Extinguisher.Foam
Item.Extinguisher.CO2
Item.Extinguisher.Water
Item.Tool.Wrench
Item.Tool.Screwdriver
Zone.PartsBin
Zone.ExtinguisherBay
```

## Making an extinguisher recognisable

So a flow can ask for "the foam extinguisher" and get the right answer even for
copies spawned at runtime.

1. In Project Settings → Gameplay Tags, add `Item.Extinguisher.Foam` and
   `Item.Extinguisher.CO2`.
2. Open `BP_Extinguisher_Foam` and add a **SimFlow Identity** component.
3. Set **Identity Tags** to `Item.Extinguisher.Foam`.
4. Set **Display Name** to `Foam Extinguisher`.
5. Repeat for `BP_Extinguisher_CO2` with `Item.Extinguisher.CO2` and
   `CO2 Extinguisher`.
6. In whatever handles grabbing, call `Set Held (true)` on the grabbed actor's
   identity component on a successful grab, and `Set Held (false)` on release.
7. Place both extinguishers in the level.

A [Place Object In Zone](tasks/place-object-in-zone.md) task asking for
`Item.Extinguisher.Foam` now accepts the foam one and reports the CO2 one as a near
miss rather than a random wrong object, so your feedback can say "close, that's the
CO2 unit, you want foam".

> **Screenshot needed:** the Details panel of `BP_Extinguisher_Foam` showing the
> SimFlow Identity component with Identity Tags and Display Name filled in.

## What depends on this

[Actor Query](actor-query.md) reads identity tags to grade a match, [Zones](zones.md)
consult it for both `Require Identity Component` and settling, and
[Place Object In Zone](tasks/place-object-in-zone.md),
[Wait For Event](tasks/wait-for-event.md) and
[Ordered Sequence](tasks/ordered-sequence.md) all judge objects through it.

The component itself depends on nothing at runtime — it's self-contained, with no
subsystem or registration. The tags themselves live in your project's tag list.

## When it misbehaves

**The task never completes and nothing is logged.** The actor has no identity
component, or has one with no tags. A tag query against an actor with no identity
tags scores `No Match` silently. Confirm with `Actor Has Identity Tag` in a debug
print.

**Everything is a No Match and nothing is ever a near miss.** The tag hierarchy is
too flat, or `Min Related Tag Depth` is higher than your tags are deep. See
[why depth matters](#why-depth-matters-more-than-it-looks).

**Objects register as placed while still in the trainee's hand.** Held state isn't
wired up. Attachment alone isn't reliable in VR — see [held state](#held-state).

**An Identity Id is set and the task still can't find the object.** Nothing in
SimFlow reads `Identity Id`. Use Identity Tags.

**Tags added to the item in the level don't apply to spawned copies.** The component
went on a placed instance rather than on the Blueprint. Move it to the Blueprint so
every copy inherits it.

**A GAS actor matches tags it shouldn't.** `Get Identity Tags` merges
identity-component tags with `IGameplayTagAssetInterface` owned tags, so a GAS actor
owning a tag that collides with your item hierarchy will match. Keep the two
namespaces apart.

*Next: [Actor Query](actor-query.md) · [Zones](zones.md) ·
[Place Object In Zone](tasks/place-object-in-zone.md) · [Glossary](glossary.md)*
