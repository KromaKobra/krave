# ADR-0020 — Properties are `MEMBER`, so serialization walks the meta-object

**Status:** Accepted · **Source:** `src/app/Node.h:18-27`, `src/app/Project.h:14-18` · **Pinned by:** `roundTripPreservesEveryProperty`, `blurNodesRoundTripEverySetting`, `transformNodesRoundTripTheirPlacement`

## Context

A node has settings. Each one normally needs: a member, a getter, a setter, a change
signal, a line in the save function, a line in the load function, and a control in the
inspector. Seven places to keep in step, and the save/load pair is the one people forget.

The blur node alone has twelve properties.

## Decision

Declare every property as:

```cpp
Q_PROPERTY(qreal gamma MEMBER m_gamma NOTIFY changed)
```

`MEMBER` for direct access, and **one shared `changed()` signal** for the whole object.
`Project` then serializes by walking the `QMetaObject`:

```cpp
for (int i = Node::staticMetaObject.propertyOffset(); i < meta->propertyCount(); ++i) {
    const QMetaProperty prop = meta->property(i);
    if (!prop.isWritable() || !prop.isStored()) continue;
    // …
}
```

## Consequences

> "Properties are declared with Q_PROPERTY(... MEMBER ...) on purpose: it gives QML
> two-way bindings and lets Project.cpp serialize by walking the meta-object, so adding a
> property here costs no serialization code and no panel plumbing."

A new property costs **one line**. It persists automatically, and it binds in QML
automatically.

Starting at `Node`'s property offset skips `QObject::objectName`; running to the concrete
class's count picks up subclass properties for free.

**Writability is the filter.** Read-only properties — `typeId`, `nodeId`, `bands`,
`barCount`, `inactiveEdgeBars`, `level`, `bass`, `mid`, `treble` — are skipped because they
are derived or live data. That is also why `id` and `type` are written as **siblings** of
`props` rather than inside it: keeping them non-writable is exactly what keeps them out of
the walk.

### The shared signal

> "They all share one `changed` NOTIFY signal. QML re-evaluates the affected bindings on
> it, and the slight over-invalidation is far cheaper than hand-writing a signal per
> property."

This is what makes a property edit reach the canvas the same frame with **no apply step**.
`NodeGraph` connects it to both `dataChanged` and `graphChanged`.

### The failure mode to know about

A property declared **without** `MEMBER`, or marked not-stored, will silently reset to its
default on every load — no error, and it only shows up when someone reopens a file.
`blurNodesRoundTripEverySetting` exists specifically for this:

> "Nothing in Project.cpp knows what a blur is: the save walks the QMetaObject, so twelve
> new properties persist for free. This pins that — a property declared without
> MEMBER/NOTIFY, or not stored, would silently reset to its default on every load."

Better still, the round-trip tests compare **via a meta-walk too**, so a property added
later is covered automatically without anyone remembering to extend the test.

### The cost

No validation and no setter hook. Values arrive from QML or from `setProperty` unchecked —
enum strings fall back silently at use time, numeric ranges exist only as inspector
bounds. The bundled example stores `minHz: -72`, well outside the slider's 10..500.
→ [node-reference](../4-code/node-reference.md#enums-and-their-fallbacks)

## Revisiting it

If a property ever needs validation or a side effect on write, it can take a real
`READ`/`WRITE` pair — the meta-walk handles those identically, since it only asks
`isWritable()`.

What must not change is the *default*: a plain setting should stay a one-line `MEMBER`
declaration. The moment properties need hand-written serialization, twelve-property nodes
stop being cheap.
