# ADR-0003 — The port table lives in C++, not QML

**Status:** Accepted · **Source:** commit `2599abb`, `src/app/NodePorts.h:55-58` · **Pinned by:** `portTableCoversEveryNodeType`, `portTableInvariantsHold`, `onlyDataDependenciesAreRequired`

## Context

Every node type needs a description of its ports: names, kinds, what each input accepts,
whether it is required, whether it joins the draw list. That description is presentation
data *and* structural data, so it could plausibly live with the UI.

## Decision

The table is a single static structure in C++ (`NodePorts.cpp`). `NodeTypes.qml` keeps
**presentation only** — icon, title, blurb.

## Consequences

The deciding argument is testability:

> "This table lives in C++ rather than in QML because flattening, cycle detection, the
> deletion cascade and link validation all read it, and all of them must run headless in
> the tests with no QML engine anywhere."

All four of those consumers are in `mvapp`, which links no Quick and no Multimedia and is
therefore fully testable. Moving the table to QML would make every one of them require a
QML engine.

It also means there is **one place** that decides structural questions:
- `Node::isVisual()` reads `spec().visual` rather than being overridden per class.
- The add menu's model is `nodeGraph.creatableTypes()` — everything not `essential`.
- The editor asks `portsFor(typeId)` rather than knowing anything itself.

Costs:
- Two files must be kept in step. **Nothing mechanically checks that `NodeTypes.qml`
  covers `creatableTypes()`** — add a node type and forget it, and the add menu shows an
  undecorated entry.
- `NodeTypeSpec::visual` must agree with the class hierarchy, because `flatten` branches
  on `qobject_cast<VisualNode*>` rather than on the flag. Nothing enforces that either.
- `nodeTypeSpec()` is a linear scan returning the first match, so a duplicate `typeId`
  would shadow silently — which is exactly why the test checks for duplicates.

## Revisiting it

Generating one from the other would remove the drift risk. The table is the natural
source, since it is the one that must be readable headless — a build step emitting the
QML presentation stub from the C++ table would close the gap without moving anything.

Moving the table *to* QML is not viable while the tests must run without a QML engine.
