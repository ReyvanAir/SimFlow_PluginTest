# Actor Query

**Struct:** `FSimFlowActorQuery`
**Appears in:** [Place Object In Zone](tasks/place-object-in-zone.md), [Wait For Event](tasks/wait-for-event.md), [Ordered Sequence](tasks/ordered-sequence.md), [Zones](zones.md)

An Actor Query answers the question "which object does this task mean?"

It's the single targeting mechanism used everywhere in SimFlow. Rather than each
task inventing its own way to point at an object, they all embed this struct, so
learning it once covers targeting across the whole plugin.

The design point worth grasping early: a query doesn't return yes or no. It grades
the answer as `No Match`, `Related (Near Miss)` or `Exact Match`. That's what lets a
flow tell "you grabbed the CO2 unit instead of the foam one" apart from "you grabbed
a wrench".

## The seven fields

| Field | Type | Default | Meaning |
|---|---|---|---|
| Specific Actor | Soft Object Ptr (Actor) | *null* | One particular actor placed in the level. |
| Blackboard Key | Name | `None` | Reads the target from a [blackboard](blackboard.md) object key at runtime. |
| Required Tags | Gameplay Tag Container | *empty* | Matches any actor whose [identity](identity.md) carries these tags. |
| Require All Tags | Bool | `true` | `true` means the actor needs every tag; `false` means any one will do. |
| Required Class | Class (Actor) | *null* | Extra narrowing by actor class. |
| Required Actor Tag | Name | `None` | Plain `AActor` tag, for actors you can't add a component to. |
| Min Related Tag Depth | Int (min 1) | `2` | How many leading tag nodes count as a near miss. |

In the common case you fill in exactly one of the first three. `Required Class` and
`Required Actor Tag` are narrowing modifiers rather than primary selectors.

## Which field to use

| You want to say | Use | Works for spawned objects? |
|---|---|---|
| "That button, the one on the left wall" | Specific Actor | No — it points at one placed actor |
| "Any foam extinguisher" | Required Tags | Yes |
| "Whatever they picked up a moment ago" | Blackboard Key | Yes |
| "Any actor of class BP_Valve" | Required Class | Yes, but coarse |
| "That third-party actor I can't modify" | Required Actor Tag | Yes |

Required Tags is the one to reach for by default. It's the only primary selector
that survives objects being spawned, duplicated or streamed in.

## An empty query matches nothing

`MatchActor` returns `No Match` immediately when no field is set. It does *not* fall
through to "match anything".

What that means in practice depends on where the query sits, and the difference
catches people out:

| Where | An empty query means |
|---|---|
| Wait For Event → Expected Payload | The payload isn't checked at all, so any sender satisfies the task |
| Place Object In Zone → Zone | The task fails immediately with a warning; it can't find a zone |
| Place Object In Zone → Accepted Items | Nothing can ever be accepted, and the task never completes |
| Place Object In Zone → Rejected Items | No explicit rejections, which is the normal case |
| Zone → Track Filter | Falls back to `Require Identity Component`, the normal case |
| Ordered Sequence → step Target | That step can never be satisfied |

The asymmetry between the first two rows is worth pausing on. An empty
`Expected Payload` is a deliberate "don't care"; an empty `Accepted Items` is a dead
end. The reason is that the payload check is skipped entirely when the query is
unset, whereas the accepted-items check always runs and always scores `No Match`.

## How matching works

The resolver checks fields in order, and the first one that's set decides the
outcome. **Specific Actor** compares pointers, giving an Exact Match or falling
through. **Blackboard Key** reads the object key and compares. **Required Tags** is
the graded path. Failing all of those, **Required Class** and **Required Actor Tag**
on their own give an Exact Match when both constraints pass.

Only the tag path can produce `Related`. With tags set, an **Exact Match** means the
tag test passed — `HasAll` or `HasAny`, depending on `Require All Tags` — and the
class and actor-tag constraints passed too. **Related (Near Miss)** means the tag
test failed, but the actor's tags share at least `Min Related Tag Depth` leading
nodes with a requested tag. **No Match** is neither.

### Min Related Tag Depth

At the default of `2`:

| Query asks for | Actor has | Shared leading nodes | Result |
|---|---|---|---|
| `Item.Extinguisher.Foam` | `Item.Extinguisher.Foam` | — | Exact |
| `Item.Extinguisher.Foam` | `Item.Extinguisher.CO2` | 2 (`Item.Extinguisher`) | Related |
| `Item.Extinguisher.Foam` | `Item.Wrench` | 1 (`Item`) | No Match |

Raise it to `3` for stricter near misses, or lower it to `1` to make everything
sharing a root tag a near miss. Values below 1 are clamped.

None of this pays off unless your tag tree groups confusable things under a shared
parent — see [why depth matters](identity.md#why-depth-matters-more-than-it-looks).

## Resolving to a single actor

`Resolve()` returns one actor, and it behaves differently depending on the field.
**Specific Actor** returns it without forcing a synchronous load, so an actor in an
unloaded level comes back null. **Blackboard Key** returns the stored object cast to
`AActor`. **Tags or Class** scans the world and returns the first Exact Match.

That last case matters. A tag query describes a *set*, so "first match" is arbitrary
when several actors qualify. Resolve is meant for finding one thing — a
[zone](zones.md) — not for picking among items. If two zones share a tag, which one
you get is undefined.

## Payloads that are not actors

When a query grades an event payload it accepts an Actor, graded directly, or an
Actor Component, unwrapped to its owning actor first.

Anything else scores `No Match` and logs a warning. The usual culprit is a UMG
widget broadcasting `self` from a button's `OnClicked`, since a `UUserWidget` is
neither an Actor nor an Actor Component. Broadcast from the owning actor with
`Payload = self` instead.

SimFlow deliberately doesn't walk `GetTypedOuter<AActor>()` to find an actor behind
a widget. A widget created with `CreateWidget(PlayerController, ...)` outers to the
controller, so that would confidently match the wrong actor. Failing loudly beats
lying quietly.

## One query, three exercises

The same extinguisher bay is the right answer in one exercise and a distractor in
the next, without editing a single Blueprint.

1. Tag the bay's [zone](zones.md) identity `Zone.ExtinguisherBay`.
2. Tag the items `Item.Extinguisher.Foam` and `Item.Extinguisher.CO2`.
3. Exercise A is a Place Object In Zone with Zone `Zone.ExtinguisherBay` and
   Accepted Items `Item.Extinguisher.Foam`.
4. Exercise B uses the same zone with Accepted Items `Item.Extinguisher.CO2`. The
   foam unit now scores Related, so the flow can say "right family, wrong agent".
5. Exercise C sets Accepted Items to `Item.Extinguisher` and Rejected Items to
   `Item.Extinguisher.CO2`. Both are extinguishers, but the CO2 one is explicitly
   called out as the plausible-looking wrong answer.

The level never changed. The flow asset holds the answer.

## When it misbehaves

**The task never completes and nothing is logged.** The query is empty, or the actor
has no matching identity tags. An unset query scores `No Match` silently on the
accepted-items path.

**A warning about a payload that is "neither an Actor nor an ActorComponent".** You
broadcast from a widget.

**Everything is No Match and nothing is ever Related.** The tag tree is flat, or
`Min Related Tag Depth` exceeds your tag depth.

**Specific Actor is set but resolves to null.** The actor lives in a level that
isn't loaded, and Resolve won't force a synchronous load. Use tags for streamed
content.

**Two zones share a tag and the wrong one is used.** Tag queries resolve to the
first world match. Give each zone a distinct tag, or point at it with
`Specific Actor`.

**Require All Tags is on and nothing matches.** With several `Required Tags`, the
actor has to carry all of them. Set it to `false` for "any of these".

*Next: [Identity](identity.md) · [Zones](zones.md) · [Glossary](glossary.md)*
