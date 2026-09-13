# Guide · Add a property to an existing node

The short path. Serialization is free; the work is the control and the test.

---

## 1 · Declare it — `src/app/Node.h`

```cpp
Q_PROPERTY(qreal myThing MEMBER m_myThing NOTIFY changed)
```

and the member with its default:

```cpp
qreal m_myThing = 0.5;
```

Put it in the right section of the class (the separator's header groups them as
Resolution / Conditioning / Motion) and give it a short comment saying **what it fixes**,
not what it is. The existing comments are the standard — they explain the perceptual
problem the control solves.

**That is the whole persistence story.** `Project` serializes by walking the
`QMetaObject`, so a writable, stored property persists with no further code.
→ [ADR-0020](../decisions/0020-member-properties-and-meta-walk.md)

### The rules

- **Use `MEMBER`.** A property declared without it, or marked not-stored, silently resets
  to its default on every load. No error — it only surfaces when someone reopens a file.
- **Use the shared `changed()` signal.** Not a new per-property signal. The slight
  over-invalidation is intentional and is what makes the canvas update with no apply step.
- **Normalize any geometry.** Fractions of the canvas, never pixels.
  → [ADR-0019](../decisions/0019-normalized-geometry.md)
- **Time values are time constants**, authored in ms, converted to seconds.
  → [ADR-0010](../decisions/0010-smoothing-is-a-time-constant.md)

---

## 2 · Use it

Wherever the property is consumed:

- A **layer view** in `src/app/qml/layers/` reads `frame.layerData.myThing`. Always give a
  fallback — `frame.layerData ? frame.layerData.myThing : 0.5` — because the binding
  evaluates before the data is assigned.
- A **separator** property also needs a line in `SeparatorNode::analysisParams()`
  (`Node.cpp:174-218`) mapping it into `SpectrumParams`, and then real use in
  `SpectrumAnalyzer`.

> If you add a field to `SpectrumParams` that affects the **band layout** or the buffer
> sizes, it must also be added to the corresponding trigger set in `setParams`
> (`SpectrumAnalyzer.cpp:52-70`) — otherwise changing it appears to do nothing.
> `lowFreqMagnitude` is currently missing from that set and is the live example of this
> bug. → [dsp-pipeline](../4-code/dsp-pipeline.md#the-biggest-trap-setparams-runs-every-frame)

---

## 3 · Add a control

In the node's inspector under `src/app/qml/panels/inspectors/`:

```qml
ParamSlider {
    label: qsTr("My thing")
    info: qsTr("One line: what it fixes.")
    target: root.node; propertyName: "myThing"
    from: 0; to: 1; stepSize: 0.01
}
```

Pick the control from [4 · qml-kit](../4-code/qml-kit.md):

| Type | Control |
|---|---|
| Number | `ParamSlider` (slider + exact number field) |
| Boolean | `ToggleRow` |
| Enum, 2–3 options | `SegmentedControl` |
| Enum, more | `EnumCombo` |
| Colour | `ColorField` |
| Free text | `AppTextField` |

Conventions: sub-controls that depend on a toggle use `visible:` plus `sub: true` for
indentation. Every control gets a one-line `info`; only genuinely subtle ones get the long
`details` popup.

> **Do not skip this step.** `linearFloor`, `growUpward` and `fontFamily` are all real,
> serialized, renderer-read properties with **no interface at all** — settable only by
> hand-editing a `.mvproj`.

---

## 4 · Extend the round-trip test

`tests/tst_project.cpp`. Find the case for your node — `roundTripPreservesEveryProperty`,
`blurNodesRoundTripEverySetting`, `transformNodesRoundTripTheirPlacement` — and push the
new property **off its default** in the fixture.

The comparison itself is a meta-walk, so you do not have to add an assertion; you have to
add a *value*. A property left at its default would pass even if serialization dropped it.

---

## 5 · Update the docs

Add a row to the node's table in
[4 · node-reference](../4-code/node-reference.md): name, type, default, range, the control,
and what it does. If it is a separator property, also add it to the parameter mapping table
in [4 · dsp-pipeline](../4-code/dsp-pipeline.md).

---

## Verify

```bash
./scripts/build.sh --test
./scripts/build.sh --run
```

In the app: change the value, confirm the canvas responds **immediately** (there is no
apply step), save, reopen, confirm it survived. The save/reopen step is what catches a
missing `MEMBER`.

---

## See also

- [add-a-node-type](add-a-node-type.md) — the full version
- [4 · node-reference](../4-code/node-reference.md)
- [4 · qml-kit](../4-code/qml-kit.md)
