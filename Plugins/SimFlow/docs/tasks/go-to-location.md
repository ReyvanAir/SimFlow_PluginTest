# Go To Location

**Class:** `USimFlowTask_GoToLocation`
**Select via:** [Task node](../nodes/task.md) → **Task** dropdown → **Go To Location**

Blocks until the player pawn reaches a location — the bread and butter of VR
tutorials. "Walk to the control panel", "move to the muster point".

It watches the local player pawn, which in a VR project is the VR pawn, and succeeds
once that pawn is within `Acceptance Radius` of the target.

## Where it sends them

| Field | Type | Default | Meaning |
|---|---|---|---|
| Target Location | Vector | `0,0,0` | The destination. Comes with a movable widget in the viewport — drag it rather than typing coordinates. |
| Target From Blackboard Key | Name | `None` | Read the target from this [blackboard](../blackboard.md) vector key instead. |
| Relative To Flow Owner | Bool | `false` | Treat `Target Location` as relative to the flow owner actor. |
| Acceptance Radius | Float (cm, min 1) | `150.0` | How close counts as arrived. |
| Ignore Z | Bool | `true` | Ignore the vertical axis. |
| Draw Debug Sphere | Bool | `false` | Draws a sphere at the target while the task runs. |

Plus the [fields every task has](README.md#the-fields-every-task-has).

The target resolves in a fixed order: **Target From Blackboard Key** wins if it's
set, otherwise **Target Location**, and either result is offset by the flow owner's
transform when **Relative To Flow Owner** is on. `Get Resolved Target Location`
gives you the final world position, which is what a waypoint marker in your UI
should be reading.

## The origin trap

An unset `Target Location` is `0,0,0`, which is a perfectly valid location — the
world origin. Nothing warns you, and the task looks broken while the trainee is
asked to walk to the middle of the map. A blackboard key that was named but never
written does the same thing, since a missing key reads as a zero vector.

Two related cases: with no player pawn the distance check can't run and the task
waits, and an `Acceptance Radius` set too small may never be satisfied in VR,
because the pawn's origin doesn't necessarily get that close. 150 cm is a sensible
floor.

## Ignore Z and VR

`Ignore Z` defaults to on for a reason. A pawn's location in VR sits at the
play-space floor or at the HMD depending on your rig, and HMD height varies with the
person wearing it. Measure 3D distance to a floor-level target and the radius
suddenly depends on how tall the trainee is — the HMD being roughly 170 cm up means
the distance never drops below a small radius at all.

Leave it on unless you genuinely need the vertical component, like distinguishing
floors of a building.

## Walk to the control panel

Getting the trainee to the panel before the next step:

1. Add a [Task node](../nodes/task.md) with **Task** = **Go To Location**.
2. Select the Task node and drag the **Target Location** widget in the viewport to
   the spot in front of the panel. No coordinates to type.
3. Set **Acceptance Radius** to `200`, generous enough for a room-scale play space.
4. Leave **Ignore Z** on.
5. Tick **Draw Debug Sphere** while testing so you can see the target.
6. Set **Instruction** to `Walk to the control panel`.
7. On the Task node, set **Time Limit** to `90` and wire `Timed Out` to a hint that
   highlights the panel.

For a destination picked during the run — a randomly chosen fault location, say —
have your Blueprint write the vector to the flow's blackboard with `Set Vector`
under a key like `FaultLocation`, then set **Target From Blackboard Key** to match.
`Target Location` is ignored from then on.

## If arrival is never detected

**The trainee is standing on the spot and nothing happens.** `Acceptance Radius` is
too small, or `Ignore Z` was turned off and HMD height is being counted. Turn on
`Draw Debug Sphere` to see where the target actually sits.

**The task completes immediately.** The target is already inside the radius. With
`Relative To Flow Owner` on and a target of `0,0,0`, the target *is* the owner.

**It works in the editor but not in VR.** Check `Ignore Z`, and make sure you're
measuring to the pawn rather than the camera.

**The waypoint marker is in the wrong place.** Read
`Get Resolved Target Location` rather than `Target Location`, so the relative and
blackboard forms are accounted for.

*Next: [Wait For Condition](wait-for-condition.md) ·
[Player Near Location](../conditions.md#player-near-location)*
