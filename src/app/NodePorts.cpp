#include "NodePorts.h"

#include <QObject>
#include <QtGlobal>

namespace mv {
namespace {

/// Port ORDER is layout order: inputs run left to right along a node's top edge.
///
/// For a future Merge node it is also compositing order -- port 0 is the bottom
/// of the stack -- which is why RenderList walks the inputs in index order rather
/// than by name.
const QVector<NodeTypeSpec> &table()
{
    static const QVector<NodeTypeSpec> specs = [] {
        QVector<NodeTypeSpec> t;

        const auto visualIn = [](bool required = false) {
            PortSpec p;
            p.name = QStringLiteral("visual");
            p.kind = PortKind::Visual;
            p.accepts = {PortKind::Visual};
            p.stacked = true;
            // Normally optional: a visual input with nothing in it just means this
            // node is the bottom of the chain. The Output is the exception.
            p.required = required;
            p.label = QObject::tr("Visual");
            return p;
        };
        const auto audioIn = [] {
            PortSpec p;
            p.name = QStringLiteral("audio");
            p.kind = PortKind::Audio;
            p.accepts = {PortKind::Audio};
            // An audio edge selects a SOURCE, it does not carry pixels into a
            // composite. Nothing about it belongs in the draw list.
            p.stacked = false;
            // And a node with no audio has nothing to work with at all.
            p.required = true;
            p.label = QObject::tr("Audio");
            return p;
        };

        // The root of the render. Exactly one exists; its visual input is what
        // gets drawn, and walking that input backwards produces the draw order.
        NodeTypeSpec output;
        output.typeId = QStringLiteral("output");
        output.inputs = {visualIn(/*required=*/true)};
        output.essential = true;
        t.append(output);

        // The PCM itself, as a 1-band analyzer over the full range -- so "one bar
        // = overall volume" falls out of the type system rather than needing a
        // node of its own.
        NodeTypeSpec audio;
        audio.typeId = QStringLiteral("audio");
        audio.hasOutput = true;
        audio.outputKind = PortKind::Audio;
        audio.essential = true;
        t.append(audio);

        // Conditions PCM into bands. Its audio input selects which PCM, so it
        // does NOT read its input's band values -- see Node.h.
        NodeTypeSpec separator;
        separator.typeId = QStringLiteral("separator");
        separator.inputs = {audioIn()};
        separator.hasOutput = true;
        separator.outputKind = PortKind::Audio;
        t.append(separator);

        NodeTypeSpec background;
        background.typeId = QStringLiteral("background");
        background.hasOutput = true;
        background.visual = true;
        t.append(background);

        NodeTypeSpec spectrum;
        spectrum.typeId = QStringLiteral("spectrum");
        spectrum.inputs = {visualIn(), audioIn()};
        spectrum.hasOutput = true;
        spectrum.visual = true;
        t.append(spectrum);

        NodeTypeSpec text;
        text.typeId = QStringLiteral("text");
        text.inputs = {visualIn()};
        text.hasOutput = true;
        text.visual = true;
        t.append(text);

        return t;
    }();
    return specs;
}

} // namespace

const NodeTypeSpec *nodeTypeSpec(const QString &typeId)
{
    for (const NodeTypeSpec &spec : table()) {
        if (spec.typeId == typeId)
            return &spec;
    }
    qWarning("mv: no node type spec for '%s'", qUtf8Printable(typeId));
    return nullptr;
}

const QVector<NodeTypeSpec> &allNodeTypes()
{
    return table();
}

QString portKindName(PortKind kind)
{
    switch (kind) {
    case PortKind::Audio:
        return QStringLiteral("audio");
    case PortKind::Mask:
        return QStringLiteral("mask");
    case PortKind::Visual:
        break;
    }
    return QStringLiteral("visual");
}

PortKind portKindFromName(const QString &name)
{
    if (name == QLatin1String("audio"))
        return PortKind::Audio;
    if (name == QLatin1String("mask"))
        return PortKind::Mask;
    return PortKind::Visual;
}

} // namespace mv
