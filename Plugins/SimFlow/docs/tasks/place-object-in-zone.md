# Place Object In Zone

**Class:** `USimFlowTask_PlaceObject`
**Select via:** [Task node](../nodes/task.md) → **Task** dropdown → **Place Object In Zone**

"Put the foam extinguisher in the bay."

This task watches a [Zone](../zones.md) and judges what turns up in it. The zone
reports what is there without any opinion about right and wrong; the task holds the
opinion.

That split is the whole point. The same bay can be the correct answer in one
exercise and a distractor in the next without touching a single Blueprint — the
level stays neutral and the flow asset holds the answer. The task also grades how
wrong a wrong answer was, so your feedback can say "close, that is the CO2 unit"
rather than just "no".

## Fields

| Field | Type | Default | Meaning |
|---|---|---|---|
| Zone | [Actor Query](../actor-query.md) | *empty* | Which zone to watch. Usually a tag like `Zone.PartsBin`, or the zone actor itself. |
| Accepted Items | [Actor Query](../actor-query.md) | *empty* | What belongs there. Tags are the useful form — they cover spawned copies. |
| Required Count | Int (min 1) | `1` | How many accepted items must be in the zone at once. |
| Wrong Item Policy | Enum | `Count Mistake (Keep Waiting)` | What happens when the wrong thing is placed. |

Two queries and two choices is the whole task. The rest is behind the **Advanced**
arrow, and most drills never touch it:

| Field | Type | Default | Meaning |
|---|---|---|---|
| Rejected Items | [Actor Query](../actor-query.md) | *empty* | Explicit wrong answers, always treated as a mistake. |
| Require Settled | Bool | `true` | Wait for the item to be put down and let go, rather than reacting while it is still held. Leave it on and let the zone's [Settle Mode](../zones.md) decide how patient to be. |
| Report Each Wrong Item Once | Bool | `true` | Report a given wrong item once, not every time it re-settles. |
| Placed Item Blackboard Key | Name | `None` | Stores the last accepted item. |

Plus the [fields every task has](README.md#the-fields-every-task-has).

## The two empty-field failures look nothing alike

Leave **Zone** empty and the task fails immediately: at start it logs *"could not
resolve a SimFlow Zone from its Zone query"* and finishes with `Failed`, driving the
[Task node](../nodes/task.md)'s `Failed` pin. No waiting, no retry.

Leave **Accepted Items** empty and the task starts perfectly happily, then never
completes. An empty query scores `No Match` against everything, so every item placed
is judged wrong, in silence, forever.

So: if the task fails at once, check the Zone. If it never completes, check Accepted
Items. Leaving **Rejected Items** empty is the normal case — with it empty, anything
`Accepted Items` doesn't match is already wrong. Fill it in only to carve one item
out of a family you otherwise accept.

A few related traps. A tag in the **Zone** query is matched against the level's
`SimFlow Zone` actors only, so a prop wearing the same tag is ignored rather than
winning the lookup and failing the task. Two *zones* sharing a tag is still
ambiguous — the first world match wins and which one that is is undefined, so give
them distinct tags; see
[resolving to a single actor](../actor-query.md#resolving-to-a-single-actor).
Pointing **Specific Actor** at something that is not a zone does still fail at
start, because naming the wrong actor outright is a different mistake from a tag
that happens to be shared. A
`Required Count` higher than the number of items that exist can never be satisfied.
And `Require Settled` with held state unwired may mean items never settle at all if
your VR framework holds them by constraint; with it off, the task reacts the instant
an item overlaps, including while it is still in the trainee's hand.

## How an item is judged

When the zone reports an item as settled — or merely contained, with
`Require Settled` off — the task grades it.

The item is graded against **Accepted Items**, giving `Exact`, `Related` or
`No Match`. That grading is the whole story when `Rejected Items` is empty, which is
the normal case — anything that isn't an `Exact` match is a wrong item.

**Rejected Items only vetoes an accept.** An item matching it can never be `Exact`,
even when `Accepted Items` would have let it through; it drops to `Related`. It
cannot promote anything, so an item that was already `No Match` stays `No Match` —
listing `Item.Wrench` there doesn't make a wrench a near miss. How wrong a wrong
answer is remains a question about the tags.

An `Exact` match is stored to `Placed Item Blackboard Key` if you set one, fires
`On Correct Item Placed`, and re-counts the zone. Anything else fires
`On Wrong Item Placed` with the match quality, builds a mistake description
("Placed CO2 Extinguisher in Extinguisher Bay - expected Item.Extinguisher.Foam"),
and applies the **Wrong Item Policy**.

### Counting, and what that implies

The task re-counts on every settle and finishes with `Succeeded` when the number of
Exact-matching items currently in the zone reaches `Required Count`.

Because it counts what is there now rather than what has ever been there, two things
follow. The right item already sitting in the zone when the task starts counts
immediately, since the task evaluates the zone contents at start. And taking a
correct item back out decrements the count again.

### Re-reporting a wrong item

With `Report Each Wrong Item Once` on, which is the default, a wrong item is
reported once. Taking it back out of the zone clears that memory, so putting it back
tells them again. That is deliberate: a repeated mistake is worth repeating the
feedback for.

## Wrong Item Policy

**Ignore (Keep Waiting)** stays silent, for when wrong items are simply irrelevant.
**Count Mistake (Keep Waiting)**, the default, records a `SimFlow.Mistake.WrongItem`
mistake and carries on waiting, letting the trainee correct themselves. **Count
Mistake And Fail Task** records it and drives the `Failed` pin, which is the
high-stakes assessment setting.

## Delegates

| Delegate | Signature | Use for |
|---|---|---|
| On Correct Item Placed | (Item, Placed Count) | A chime, a green highlight, a progress counter |
| On Wrong Item Placed | (Item, Match Quality) | Feedback — a near miss deserves a different hint from a random object |

`Get Resolved Zone` and `Get Accepted Count` are available while the task runs.

## What it needs to work

A [SimFlow Zone](../zones.md) in the level is a hard dependency; the task fails
without one. Tag-based queries need [identity](../identity.md) components on the
items. And if `Require Settled` is on and your framework does not reparent on grab,
you need the held state wired up.

## The right extinguisher in the bay

The trainee must put the foam extinguisher in the bay. The CO2 unit is a plausible
wrong answer that should be called out specifically.

Setting up the world:

1. In Project Settings → Gameplay Tags, add `Zone.ExtinguisherBay`,
   `Item.Extinguisher.Foam` and `Item.Extinguisher.CO2`.
2. Place a [SimFlow Zone](../zones.md) at the bay, size its Box, and set its
   identity tags to `Zone.ExtinguisherBay` with Display Name `Extinguisher Bay`.
3. On `BP_Extinguisher_Foam`, add a [SimFlow Identity](../identity.md) with
   `Item.Extinguisher.Foam` and Display Name `Foam Extinguisher`. Same for the CO2
   one.
4. In your grab logic, call `Set Held(true)` on grab and `Set Held(false)` on
   release.

Setting up the task:

5. Add a [Task node](../nodes/task.md) and set **Task** to **Place Object In Zone**.
6. **Zone → Required Tags** = `Zone.ExtinguisherBay`.
7. **Accepted Items → Required Tags** = `Item.Extinguisher.Foam`.
8. Leave **Required Count** at `1`. Nothing under **Advanced** needs touching.
9. Set **Wrong Item Policy** to **Count Mistake (Keep Waiting)**.
10. Set **Instruction** to `Place the foam extinguisher in the bay`.
11. Set **Score On Success** to `10`.

Then bind **On Wrong Item Placed** and switch on the match quality. `Related` gives
*"Close, that is the CO2 unit. You want foam."*; `No Match` gives *"That does not
belong in the extinguisher bay."*

The CO2 unit scores `Related` on its own, because it shares `Item.Extinguisher` with
the accepted tag at the default
[Min Related Tag Depth](../actor-query.md#min-related-tag-depth) of 2.

> **Screenshot needed:** the Details panel of a Task node with Place Object In Zone
> selected, showing the Zone and Accepted Items queries filled in.

Note that none of that needed **Rejected Items** — the CO2 unit grades as a near
miss on its tags alone. Where the field earns its place is subtracting from an
open-ended family: to accept any extinguisher *except* the CO2 one, set **Accepted
Items → Required Tags** to `Item.Extinguisher` and **Rejected Items → Required Tags**
to `Item.Extinguisher.CO2`. The veto makes the CO2 unit a mistake even though it
matches the accepted tag, and the rule keeps holding when someone adds
`Item.Extinguisher.Water` later.

## When it misbehaves

**The task fails the instant it starts.** The `Zone` query found no zone. The
warning in the log says which of the four ways it went wrong — an empty query, a
named actor that is not a zone, no zones in the level at all, or zones that exist
but do not carry the tag. The last of those lists every zone present and what it is
actually tagged, which is usually enough to spot the mismatch without leaving the
log window.

**The task never completes, whatever is placed.** `Accepted Items` is empty.

**Items never register as placed.** They are not settling — usually held state is not
wired and your VR framework holds by constraint rather than attachment. Turn on the
zone's `Draw Debug` and read
[how settling works](../zones.md#how-an-object-is-judged-put-down).

**The correct item is placed and nothing happens.** Its identity tags do not match
`Accepted Items`, or `Require All Tags` is on with several tags requested. Check with
`SimFlow.Debug 1`.

**Wrong items are reported over and over.** `Report Each Wrong Item Once` is off, or
the trainee keeps taking the item out and putting it back, which re-arms the report
on purpose.

**Everything wrong is No Match and nothing is ever a near miss.** The tag hierarchy
is too flat — see
[why depth matters](../identity.md#why-depth-matters-more-than-it-looks).

**The wrong zone is being watched.** Two zones share a tag.

**The task completed immediately on start.** The correct item was already in the
zone, and the task evaluates existing contents at start by design.

*Next: [Zones](../zones.md) · [Actor Query](../actor-query.md) ·
[Identity](../identity.md)*
