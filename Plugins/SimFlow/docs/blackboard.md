# Blackboard

**Class:** `USimFlowBlackboard`
**Access via:** `Get Blackboard` on the [SimFlow Component](simflow-component.md), a task, a node or a condition

The blackboard is the per-flow key/value store — how a flow remembers things
between tasks. Score, quiz answers, which object the trainee picked up, how many
times they got something wrong.

There is one per running [flow instance](glossary.md). Two actors running the same
flow asset have separate blackboards and can't see each other's values.

Everything [conditions](conditions.md) and [Branch](nodes/branch.md) nodes read
comes from here, which makes the blackboard the join between what happened and what
the flow does about it.

## Values and types

A blackboard value is an `FSimFlowValue`, a small tagged union with a `Type` and one
payload field. Set the **Type** first in the Details panel and the matching value
field appears.

| Type | Stores | Survives save/load? |
|---|---|---|
| None | nothing (an unset value) | — |
| Bool | true/false | Yes |
| Int | int32 | Yes |
| Float | float | Yes |
| String | text | Yes |
| Name | FName | Yes |
| Vector | FVector | Yes |
| Object | a UObject reference | No — cleared on save |

Object values are runtime only. They're stripped when a flow is serialised into a
SaveGame, because a hard object pointer can't be meaningfully restored. If something
about an object needs to survive a save, store its display name as a String
alongside it.

### Type coercion

Values compare and convert on a best-effort basis rather than failing. **As Number**
turns a bool into 0 or 1 and parses a String with `Atof`; **As Bool** treats any
non-zero number as true; and **Add** adds where it makes sense — numbers add,
strings concatenate, vectors add component-wise.

That's also why `Add To Value` works on a key that doesn't exist yet. The missing key
reads as unset, and the delta becomes the new value.

## API

The generic accessors:

| Function | Notes |
|---|---|
| Set Value (Key, Value) | Writes, replacing whatever was there |
| Get Value (Key) | Returns an unset value when the key is missing |
| Has Value (Key) | Does the key exist? |
| Remove Value (Key) | Deletes one key |
| Clear All | Empties the blackboard |
| Add To Value (Key, Delta) | Adds to the existing value, creating the key if it's missing |

There are typed conveniences too — `Set Bool / Int / Float / String / Name / Vector
/ Object` and the matching getters. The getters take a **Default Value** used when
the key is absent, so `Get Int("Attempts", 0)` is safe on a fresh flow. `Get Object`
is the exception: no default, returns null.

For score, `Add Score` adds to the well-known `Score` key and `Get Score` reads it.

For serialisation, `To Entries` (with `bStripObjectReferences` defaulting to true)
flattens the blackboard to an array for saving, and `From Entries` (Entries,
`bClearFirst` defaulting to true) restores it. `Merge From` (Other,
`bOverwriteExisting` defaulting to true) copies every entry from another
blackboard, which is what [Sub Flow](nodes/sub-flow.md) uses to pass state between
parent and child.

**On Value Changed** (Key, New Value) fires on every write. Bind it to drive a live
score widget without polling.

## The well-known keys

The built-in tasks read and write these, so treat the names as reserved.

| Key | Type | Written by |
|---|---|---|
| `Score` | Float | Task success/failure scoring, `Add Score` |
| `Mistakes` | Int | [Quiz](tasks/quiz.md) when `Count Mistakes` is on |
| `LastResult` | — | The runtime, after each task finishes |
| `LastAnswerIndex` | Int | [Quiz](tasks/quiz.md) |
| `LastAnswerCorrect` | Bool | [Quiz](tasks/quiz.md) |
| `WrongAttempts` | Int | Incremented every time a task rejects the wrong object |
| `CurrentStep` | Int | [Ordered Sequence](tasks/ordered-sequence.md) |

Keys are `FName` and match case-insensitively, like all Unreal names, so `Score` and
`score` are the same key. Pick a convention anyway.

## A three-strikes rule

Routing the trainee to a remediation branch after three wrong attempts.

The built-in tasks already increment `WrongAttempts` whenever they reject a wrong
object, so there's nothing to write yourself. From there:

1. After the task, add a [Branch](nodes/branch.md) node.
2. Add one case with its **Condition** set to **Blackboard Compare**: key
   `WrongAttempts`, operation `>=`, value type `Int`, value `3`.
3. Label the case `Too many attempts` and wire it to your remediation section.
4. Wire **Default** to the normal continuation.
5. To reset the count for the next exercise, drop a
   [Set Blackboard Value](nodes/set-blackboard.md) node with key `WrongAttempts`,
   type `Int`, value `0`, and `Add` off.

Watch it work with `SimFlow.Debug 1` — the debug HUD prints the whole blackboard.

## When it misbehaves

The thing to understand first is that nothing here fails loudly. Reading a missing
key returns the supplied default, or an unset value for `Get Value`, with no
warning. A `None` key on a Set node writes under the literal name `None`. Comparing
a missing key returns
[Blackboard Compare](conditions.md#blackboard-compare)'s `Result When Key Missing`,
which defaults to false. A type mismatch coerces instead of failing, so a String
`"5"` compares equal to an Int `5`.

Which means a typo in a key name produces no error anywhere. It produces a condition
that's quietly always false.

**A condition is always false and nothing is logged.** The key name doesn't match
what wrote it. Print the blackboard with `SimFlow.Debug 1` and compare the spelling.

**A value survived into the next run.** The blackboard belongs to the instance, and
restarting the flow makes a fresh one — but `Merge From` in a
[Sub Flow](nodes/sub-flow.md) with write-back on pushes child values back into the
parent. That's intended; turn write-back off if you don't want it.

**An object reference is null after loading a save.** Expected. Store an identifying
String alongside it.

**`Add To Value` on a String did something surprising.** It concatenates — `"1"` plus
`"1"` is `"11"`. Use `Int` for counters.

**Score isn't changing.** `Score On Success` and `Score On Failure` are per-task
fields defaulting to `0`. See
[the fields every task has](tasks/README.md#the-fields-every-task-has).

## What depends on this

Conditions, [Branch](nodes/branch.md), [Loop](nodes/loop.md), the
[Set Blackboard node](nodes/set-blackboard.md) and
[task](tasks/set-blackboard.md), [Actor Query](actor-query.md) through its
`Blackboard Key` field, [Sub Flow](nodes/sub-flow.md), and save/load. The blackboard
itself only needs a running [flow instance](glossary.md) — it belongs to the
instance, not the asset.

*Next: [Conditions](conditions.md) ·
[Set Blackboard node](nodes/set-blackboard.md) ·
[SimFlow Component](simflow-component.md)*
