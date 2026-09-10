# Getting started

Install SimFlow, build a flow that does something, and watch it run. About fifteen
minutes end to end.

New to the vocabulary? Keep the [glossary](glossary.md) open in a second tab.

## 1. Install the plugin

SimFlow is a C++ plugin, so your project needs a C++ module. That's the one
requirement that catches people out.

1. Copy the `SimFlow` folder into `YourProject/Plugins/SimFlow`.
2. Right-click your `.uproject` → **Generate Visual Studio project files** (or run
   `GenerateProjectFiles`).
3. Build the editor target.
4. Enable **SimFlow** in *Edit → Plugins* if it isn't already on.

If your project is Blueprint-only, add any empty C++ class once from the editor
(*Tools → New C++ Class → None*). Unreal converts the project to a C++ project and
step 2 becomes possible. You never have to write C++ after that — everything below
is Blueprint and the Details panel.

SimFlow 1.1.4 targets Unreal Engine 5.6.0.

To check the install took, right-click in empty space in the Content Browser and
look for a **SimFlow** section with **SimFlow Graph** in it. If it's there, the
plugin is loaded.

## 2. The fastest look: the sample flow

Before building anything by hand, generate the sample:

**Tools → SimFlow → Create Sample VR Tutorial Flow**

That builds a complete flow exercising most of the plugin, and opens it. Reading it
top to bottom is the quickest way to see how nodes, tasks and conditions fit
together. Then come back and build your own.

## 3. Build your first flow

**Content Browser → right-click → SimFlow → SimFlow Graph.** Name it `F_HelloFlow`
and open it. There's already a **Start** node in the graph — every flow needs one,
and that's where execution begins.

### Add a task

1. Right-click the graph → **Tasks → Task**. A Task node appears.
2. Drag from `Start`'s **Out** pin onto the Task node's **In** pin.
3. Select the Task node. In the Details panel find the **Task** field, click the
   dropdown, and choose **Log Message**.
4. The Task field expands into that task's own settings. Set **Message** to
   `Hello from SimFlow`.

The `Task` field is empty by default, and a Task node with no task assigned logs a
warning and passes straight through its `Completed` pin rather than failing. See
[the Task node](nodes/task.md) for why.

### Finish the flow

Right-click the graph → **Flow Control → Finish**, then wire the Task node's
**Completed** pin into the Finish node's **In** pin.

Your graph now reads Start → Task (Log Message) → Finish.

```
  ┌───────┐      ┌──────────────────┐          ┌────────┐
  │ Start │ Out──┤In   Task         │Completed─┤In      │
  │       │      │     (Log Message)│          │ Finish │
  └───────┘      │                  │Failed    └────────┘
                 │                  │Skipped
                 │                  │Timed Out
                 └──────────────────┘
```

Save the asset.

### Run it

1. In your level, pick an actor to host the flow. The Game Mode, a level actor or
   your VR pawn all work — [SimFlow Component](simflow-component.md) covers which to
   choose and why.
2. **Add Component → SimFlow Component**.
3. Set **Flow Asset** to `F_HelloFlow`.
4. Set **Start Mode** to **Auto - On First Tick**.
5. Press Play.

`Hello from SimFlow` should print on screen.

On First Tick rather than On Begin Play, because on BeginPlay other actors in the
level may not have begun play yet — so a flow that immediately goes looking for a
zone or an item can fail to find it. More on that under
[Start Mode](simflow-component.md#start-mode).

### See what it's doing

Open the console with `~` and type:

```
SimFlow.Debug 1
```

Live flow state draws on screen: which node is active, the current task, elapsed
time, and the blackboard. This is the most useful debugging tool in the plugin.
Reach for it before anything else.

## 4. Make it react to the world

A flow that logs a message isn't yet interesting. The next step is having it wait
for the trainee to actually *do* something.

Select the Task node, change **Task** to **Wait For Event**, and set **Event Tag**
to `SimFlow.Event.Interact`.

The flow now blocks until something raises that tag. From any Blueprint in your
level — a button, a grabbable object, an animation notify — call **Broadcast Flow
Event** with `Event Tag` = `SimFlow.Event.Interact` and `Payload` = `self`.

Press Play, trigger the Blueprint, and the flow advances.

Pass `self` as the payload from the owning actor, not from a widget. A UMG widget is
neither an Actor nor an Actor Component and can never satisfy an object check. It's
a common first bug — [Wait For Event](tasks/wait-for-event.md#when-it-misbehaves)
has the details.

## 5. Where to go next

| If you want to… | Read |
|---|---|
| Understand what an object *is*, so tasks can recognise it | [Identity](identity.md) |
| Say "put the extinguisher in the bay" | [Place Object In Zone](tasks/place-object-in-zone.md) |
| Say "press these three buttons in order" | [Ordered Sequence](tasks/ordered-sequence.md) |
| Branch on state, loop, run things in parallel | [Node reference](nodes/README.md) |
| Remember things between tasks | [Blackboard](blackboard.md) |
| See complete worked examples | [Workflows](workflows/README.md) |
| Fix something that isn't working | [Troubleshooting](troubleshooting.md) |

*Next: [Documentation index](README.md) · [Glossary](glossary.md)*
