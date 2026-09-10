# Branch node

**Class:** `USimFlowNode_Branch`
**Add via:** right-click → **Flow Control → Branch**
**Pins:** In → Case 0 … Case N, Default

Condition-based routing, and the main decision point in most flows. Branch
evaluates each case in order and leaves through the first one that passes, or
through `Default` when none do. Pass/fail routing, difficulty selection,
remediation, "have they already done this?" — all of it lands here.

## Fields

| Field | Type | Default | Meaning |
|---|---|---|---|
| Cases | Array | *empty* | The ordered list of conditions. Each one adds an output pin. |
| Fire All Matching Cases | Bool | `false` | Fire every case that passes instead of only the first, fanning out in parallel. |
| Has Default Pin | Bool | `true` | Show a `Default` pin, taken when nothing matched. |

Each case holds a **Label** (a string; empty gives you `Case N` on the pin) and a
**Condition** — an instanced [condition](../conditions.md) object. An empty
condition slot counts as false, so that case simply never fires.

Internally the pins are named `Case_0`, `Case_1` and so on, rebuilt whenever you add
or remove a case. Links survive as long as the pin name still exists, which means
inserting a case in the middle renumbers everything after it and can move your
wires. Add new cases at the end.

## Order matters

Cases are tested top to bottom and the first pass wins, unless `Fire All Matching
Cases` is on. Put the most specific case first:

```
Case 0:  Score >= 90     "Distinction"
Case 1:  Score >= 70     "Pass"
Default:                 "Fail"
```

Reverse those two and everything scoring 90 or above leaves through `Pass`, because
it gets tested first and passes.

## The silent stop

If nothing matches and `Has Default Pin` is off, the node finishes without
triggering anything and that line of execution stops dead. This is a legitimate way
to end a branch, which is exactly what makes it hard to spot — it looks identical to
a mistake. Worth remembering when a flow stalls somewhere near a Branch.

A case pin left unwired behaves the same way: it fires, and goes nowhere.

## Three-way routing on score

Routing to distinction, pass or remediation at the end of an assessment:

1. Right-click → **Flow Control → Branch**.
2. Under **Cases**, click **+** twice.
3. Case 0: label `Distinction`, condition **Score Threshold**, `>=`, `90`.
4. Case 1: label `Pass`, condition **Score Threshold**, `>=`, `70`.
5. Leave **Has Default Pin** on — `Default` is the remediation route.
6. Wire each pin to its section.

```
                     ┌──────────────────┐
   last task ────────┤In   Branch       │Distinction──▶ certificate
                     │                  │Pass────────▶ debrief
                     │                  │Default─────▶ remediation
                     └──────────────────┘
```

To require both a score and a mistake limit for the pass, set that case's condition
to **All Of** with two children — see
[Conditions](../conditions.md#all-of-and-and-any-of-or).

## When it misbehaves

**A case never fires.** Its condition slot is empty, or an earlier and broader case
is catching everything before it gets there.

**Everything falls through to Default.** The conditions are probably reading a
blackboard key that doesn't exist. A missing key makes **Blackboard Compare** return
`Result When Key Missing`, which defaults to false. Confirm with `SimFlow.Debug 1`.

**Wires moved after adding a case.** Pins are named by index, so inserting in the
middle renumbers the ones after it.

**Two branches ran at once.** `Fire All Matching Cases` is on. Converge them with a
[Join](join.md).

*Next: [Conditions](../conditions.md) · [Random Branch](random-branch.md) ·
[Join node](join.md)*
