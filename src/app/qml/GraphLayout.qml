pragma Singleton

import QtQuick

// All of the node editor's geometry maths, in one place.
//
// ------------------------------------------------------------------------
// WHY THIS FILE EXISTS: mapToItem IS NOT BINDING-REACTIVE.
// ------------------------------------------------------------------------
//
// QML cannot track an `x`/`y` dependency through mapToItem, so an edge endpoint
// computed that way is correct on first paint and then stops following the node it
// is attached to. There is no warning; it simply never updates again.
//
// So every endpoint here is pure ARITHMETIC over graphX/graphY. Those are real
// properties with a NOTIFY signal, so anything binding to these functions tracks a
// dragged node for free. mapToItem is fine inside an event handler, where it runs
// once rather than being re-evaluated -- which is exactly where NodeCard and
// GraphCanvas use it.
//
// Flow is top to bottom: inputs spread across a node's TOP edge, the single output
// centred on the BOTTOM edge. Downstream is down.
QtObject {
    readonly property int nodeWidth: Theme.nodeWidth
    readonly property int nodeHeight: Theme.nodeHeight
    readonly property int snapRadius: Theme.portSnapRadius

    /// Centre of input port `i` of `n`, in graph space.
    ///
    /// Spread evenly at (i+1)/(n+1) across the width, so two ports sit at a third
    /// and two thirds rather than hard against the corners.
    function inputPos(node, i, n) {
        if (!node)
            return Qt.point(0, 0)
        return Qt.point(node.graphX + nodeWidth * (i + 1) / (n + 1), node.graphY)
    }

    /// Centre of the output port, in graph space.
    function outputPos(node) {
        if (!node)
            return Qt.point(0, 0)
        return Qt.point(node.graphX + nodeWidth / 2, node.graphY + nodeHeight)
    }

    /// Control-point offset for an edge between p1 and p2.
    ///
    /// Scales with vertical distance so short hops stay gentle and long ones bow
    /// properly, with a floor so a zero-distance edge is still a curve.
    function curveBow(p1, p2) {
        return Math.max(28, Math.abs(p2.y - p1.y) * 0.4)
    }

    /// The nearest input port that would accept an output of `outKind`, or null.
    ///
    /// Returns { nodeId, port, pos }. `excludeId` is the node being dragged FROM:
    /// a node cannot feed itself, and offering its own inputs as targets reads as a
    /// bug even though canLink would refuse.
    function hitTestInput(graph, gx, gy, outKind, excludeId) {
        let best = null
        let bestDistance = snapRadius * snapRadius

        for (let row = 0; row < graph.count; ++row) {
            const node = graph.nodeAt(row)
            if (!node || node.nodeId === excludeId)
                continue

            const ports = graph.portsFor(node.typeId)
            for (let i = 0; i < ports.length; ++i) {
                if (ports[i].accepts.indexOf(outKind) < 0)
                    continue

                const p = inputPos(node, i, ports.length)
                const dx = p.x - gx
                const dy = p.y - gy
                const d2 = dx * dx + dy * dy
                if (d2 <= bestDistance) {
                    bestDistance = d2
                    best = { nodeId: node.nodeId, port: ports[i].name, pos: p }
                }
            }
        }
        return best
    }
}
