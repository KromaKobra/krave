#!/usr/bin/env python3
#
# DELETE THIS FILE once your projects are converted.
#
# It exists because krave's format version went to 3 and the app deliberately
# rejects anything older: an in-app migration would have to keep migrateFromV1,
# migrateLegacyValues and a new migrateFromV2 alive forever, and pin the typeId
# strings it reads. A throwaway script here costs one run and nothing after that.
#
# Converts a v1 or v2 .mvproj -- a flat, ordered layer stack -- into the v3 node
# graph. Writes <name>-v3.mvproj ALONGSIDE the input rather than overwriting it:
# saving over the original under the new build would leave you with a file the old
# build cannot open and no way back.
#
#   ./tools/migrate_v2_to_v3.py examples/*.mvproj
#   ./tools/migrate_v2_to_v3.py --in-place examples/*.mvproj   # replaces originals
#
# What the shape change amounts to:
#
#   - "layers" becomes "nodes" (each with a stable `id`) plus "connections".
#   - Array order WAS the z-stack. Index 0 is the bottom, so index 0 is the HEAD
#     of the visual chain -- no reversal.
#   - "audio" (the provider) becomes "separator", and a new essential "audio" node
#     holding the PCM feeds every separator.
#   - An essential "output" node terminates the visual chain.
#   - "bars" becomes "spectrum", and its hidden providerId becomes a visible edge.
#   - Placement and compositing moved off the base class onto visual nodes, so a
#     separator's posX/sizeW/blendMode etc. are dropped -- they never did anything.
#   - "particles" is gone from the app. Those layers are DROPPED, loudly.

import argparse
import json
import os
import sys

# Must match NodeGraph::autoLayout(), or a migrated file will jump the first time
# anyone presses Arrange.
NODE_WIDTH = 176
NODE_HEIGHT = 62
GAP_X = 48
GAP_Y = 88
MARGIN = 40

# Properties that moved from the old Layer base class onto VisualNode. An audio
# provider serialized all of these and the panel showed seven dead controls for
# them; a separator has none of them now.
VISUAL_ONLY_PROPS = [
    "layerVisible", "layerOpacity", "blendMode",
    "posX", "posY", "sizeW", "sizeH", "layerRotation",
]

# v1 kept the analysis settings on every bar layer. v2 moved them to a provider.
V1_MOVED_PROPS = [
    "minHz", "maxHz", "floorDb", "ceilingDb",
    "scalingCurve", "gamma", "attackMs", "decayMs",
]


def warn(message):
    print("  ! " + message, file=sys.stderr)


def migrate_from_v1(layers):
    """Rewrite a v1 layer array in v2 shape: analysis settings onto a provider.

    A port of Project.cpp's migrateFromV1, which this change deletes. The first
    bar layer's settings become the provider's, since a v1 project had no way to
    express more than one analysis configuration anyway.
    """
    provider_props = {"name": "Audio provider"}
    have_settings = False
    out = []

    for layer in layers:
        if layer.get("type") != "bars":
            out.append(layer)
            continue

        props = dict(layer.get("props", {}))
        if not have_settings:
            for key in V1_MOVED_PROPS:
                if key in props:
                    provider_props[key] = props[key]
            if "barCount" in props:
                provider_props["bandCount"] = props["barCount"]
            # v1 stored the window as a sample count; the provider takes
            # milliseconds. 48 kHz is the app's fixed internal rate.
            if "fftSize" in props:
                provider_props["windowMs"] = props["fftSize"] * 1000.0 / 48000.0
            have_settings = True

        for key in V1_MOVED_PROPS:
            props.pop(key, None)
        props.pop("barCount", None)
        props.pop("fftSize", None)

        out.append({"type": "bars", "props": props})

    out.insert(0, {"type": "audio", "props": provider_props})
    return out


def fix_legacy_values(layer_type, props):
    """Fold retired property VALUES into the ones that replaced them.

    A port of Project.cpp's migrateLegacyValues. "mirrored" used to be a third
    orientation; it is now linear plus the two-sided bar toggle, which radial can
    have as well. Nothing validates the string, so left alone it would load
    verbatim and leave the orientation control with no segment selected.
    """
    if layer_type == "bars" and props.get("orientation") == "mirrored":
        props["orientation"] = "linear"
        props["mirrorBars"] = True
    return props


def assign_positions(nodes, connections):
    """Lay the graph out in rows by depth, the way autoLayout() does.

    Depth is the LONGEST path from a node with no connected inputs, so a node
    always sits below everything feeding it. Flow is top to bottom.
    """
    incoming = {n["id"]: [] for n in nodes}
    for edge in connections:
        incoming[edge["to"]].append(edge["from"])

    depth = {}

    def depth_of(node_id, on_path):
        if node_id in depth:
            return depth[node_id]
        # A converted file cannot be cyclic, but guard anyway rather than
        # recursing forever if this script is pointed at something hand-edited.
        if node_id in on_path:
            return 0
        on_path.add(node_id)
        sources = incoming[node_id]
        d = 0 if not sources else 1 + max(depth_of(s, on_path) for s in sources)
        on_path.discard(node_id)
        depth[node_id] = d
        return d

    for node in nodes:
        depth_of(node["id"], set())

    rows = {}
    for node in sorted(nodes, key=lambda n: n["id"]):
        rows.setdefault(depth[node["id"]], []).append(node)

    for row_depth, row in rows.items():
        for column, node in enumerate(row):
            node["props"]["graphX"] = MARGIN + column * (NODE_WIDTH + GAP_X)
            node["props"]["graphY"] = MARGIN + row_depth * (NODE_HEIGHT + GAP_Y)


def convert(doc, label):
    version = doc.get("version", 1)
    if version >= 3:
        raise ValueError("already format version %d" % version)

    layers = doc.get("layers", [])
    if version < 2:
        layers = migrate_from_v1(layers)

    # Reserve the two essential ids so they are the same in every converted file.
    output_id = 1
    audio_id = 2
    next_id = 3

    nodes = [
        {"id": output_id, "type": "output", "props": {"name": "Output"}},
        {"id": audio_id, "type": "audio", "props": {"name": "Audio"}},
    ]

    separators = []    # node ids, in stack order
    spectra = []       # (node id, providerId read off the old bar layer)
    visual_chain = []  # node ids, bottom of the z-stack first

    for index, layer in enumerate(layers):
        layer_type = layer.get("type")
        props = fix_legacy_values(layer_type, dict(layer.get("props", {})))
        # A v2 file's implicit layerId for array element i was i+1: resetLayers
        # numbered from 1 in array order. That coincidence is exactly what made
        # providerId look like it worked, and it is what lets it be resolved here.
        implicit_layer_id = index + 1

        if layer_type == "audio":
            for key in VISUAL_ONLY_PROPS:
                props.pop(key, None)
            node_id = next_id
            next_id += 1
            nodes.append({"id": node_id, "type": "separator", "props": props})
            separators.append((node_id, implicit_layer_id))

        elif layer_type == "bars":
            provider_id = props.pop("providerId", 0)
            node_id = next_id
            next_id += 1
            nodes.append({"id": node_id, "type": "spectrum", "props": props})
            spectra.append((node_id, provider_id))
            visual_chain.append(node_id)

        elif layer_type == "background":
            # An Image node comes later; until then a background is a flat colour.
            if props.get("imageSource"):
                warn("%s: layer %d '%s' had a background image (%s) -- dropped, "
                     "the background node is colour-only for now"
                     % (label, index, props.get("name", "?"), props["imageSource"]))
            props.pop("imageSource", None)
            props.pop("fillMode", None)
            node_id = next_id
            next_id += 1
            nodes.append({"id": node_id, "type": "background", "props": props})
            visual_chain.append(node_id)

        elif layer_type == "text":
            node_id = next_id
            next_id += 1
            nodes.append({"id": node_id, "type": "text", "props": props})
            visual_chain.append(node_id)

        elif layer_type == "particles":
            warn("%s: layer %d '%s' is a particle layer -- DROPPED, the type no "
                 "longer exists. Recover it from git if you need it back."
                 % (label, index, props.get("name", "?")))

        else:
            warn("%s: layer %d has unknown type '%s' -- dropped"
                 % (label, index, layer_type))

    connections = []

    # One audio node feeds every separator. This is the one-output-many-inputs
    # rule, and it is why a stack could never express this graph.
    for separator_id, _ in separators:
        connections.append({"from": audio_id, "to": separator_id, "port": "audio"})

    first_separator = separators[0][0] if separators else None
    if spectra and first_separator is None:
        warn("%s: has spectrum layers but no audio provider -- their audio inputs "
             "are left unconnected, so they will draw nothing until wired up"
             % label)

    by_implicit_id = {implicit: node_id for node_id, implicit in separators}
    for spectrum_id, provider_id in spectra:
        source = by_implicit_id.get(provider_id, first_separator)
        if provider_id and provider_id not in by_implicit_id:
            warn("%s: spectrum node %d pointed at providerId %d, which is not an "
                 "audio layer -- wired to the first separator instead"
                 % (label, spectrum_id, provider_id))
        if source is not None:
            connections.append({"from": source, "to": spectrum_id, "port": "audio"})

    # Array index 0 was the BOTTOM of the z-stack and is the head of the chain.
    for upstream, downstream in zip(visual_chain, visual_chain[1:]):
        connections.append({"from": upstream, "to": downstream, "port": "visual"})
    if visual_chain:
        connections.append({"from": visual_chain[-1], "to": output_id, "port": "visual"})
    else:
        warn("%s: nothing visual survived -- the output node has no input" % label)

    assign_positions(nodes, connections)

    return {
        "format": "music-visualizer-project",
        "version": 3,
        "canvas": doc.get("canvas", {"width": 1920, "height": 1080, "color": "#ff0b0b12"}),
        "nodes": nodes,
        "connections": connections,
    }


def main():
    parser = argparse.ArgumentParser(
        description="Convert v1/v2 .mvproj layer stacks to the v3 node graph.")
    parser.add_argument("files", nargs="+", metavar="FILE")
    parser.add_argument("--in-place", action="store_true",
                        help="overwrite the inputs instead of writing -v3 copies")
    args = parser.parse_args()

    failures = 0
    for path in args.files:
        print(path)
        try:
            with open(path, encoding="utf-8") as handle:
                doc = json.load(handle)
            result = convert(doc, os.path.basename(path))
        except (OSError, ValueError, KeyError) as error:
            warn("%s: %s" % (os.path.basename(path), error))
            failures += 1
            continue

        if args.in_place:
            out_path = path
        else:
            stem, ext = os.path.splitext(path)
            out_path = stem + "-v3" + ext

        with open(out_path, "w", encoding="utf-8") as handle:
            json.dump(result, handle, indent=4, sort_keys=True)
            handle.write("\n")
        print("  -> %s (%d nodes, %d connections)"
              % (out_path, len(result["nodes"]), len(result["connections"])))

    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
