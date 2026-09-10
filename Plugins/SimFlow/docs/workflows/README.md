# Example workflows

Three complete flows built from the nodes and tasks in the reference. Each shows the
whole picture — level setup, tags, graph and UI bindings — rather than one feature
in isolation.

If you're new, work through them in order. Each one introduces a little more.

| Workflow | Shows |
|---|---|
| [Fire extinguisher drill](fire-extinguisher-drill.md) | Identity, zones, placement judging, near-miss feedback, scoring |
| [Valve startup procedure](valve-startup-procedure.md) | Ordered sequences, out-of-order handling, progress UI, checkpoints |
| [Assessment with debrief](assessment-with-debrief.md) | Quizzes, branching on score, mistakes, sub-flows, a debrief screen |

## What they assume

All three assume you've been through [getting started](../getting-started.md): the
plugin is installed, you can create a flow asset, and there's a
[SimFlow Component](../simflow-component.md) somewhere running it.

They also assume the general shape of a SimFlow project. Items and controls carry an
[identity](../identity.md) with gameplay tags saying what they are. Props broadcast
events and know nothing about the exercise. And the flow asset holds the answers —
which object is right, in what order, in which zone.

That separation is what lets you build one level and many exercises from it.

## A note on tags

Every example uses tags like `Item.Extinguisher.Foam` and `Zone.PartsBin`. None of
these ship with the plugin. The `SimFlow.*` event and mistake tags do, but your own
object hierarchy is yours to create in Project Settings → Gameplay Tags.

Group things that are plausibly confusable under a shared parent, or the near-miss
feedback these examples rely on won't work. See
[why depth matters](../identity.md#why-depth-matters-more-than-it-looks).

*Next: [Node reference](../nodes/README.md) · [Task reference](../tasks/README.md)*
