#include "Project.h"
#include "NodeGraph.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaProperty>
#include <QSaveFile>

namespace mv {
namespace {

/// Write every writable, stored property a Node subclass declares.
///
/// Starting at Node's property offset skips QObject's own `objectName`; the loop
/// runs to the concrete class's property count so subclass properties come along
/// for free. Read-only properties (typeId, nodeId, bands) are skipped because they
/// are either derived or live data.
QJsonObject serializeNode(const Node *node)
{
    QJsonObject props;
    const QMetaObject *meta = node->metaObject();

    for (int i = Node::staticMetaObject.propertyOffset(); i < meta->propertyCount(); ++i) {
        const QMetaProperty prop = meta->property(i);
        if (!prop.isWritable() || !prop.isStored())
            continue;

        const QVariant value = prop.read(node);
        if (value.typeId() == QMetaType::QColor)
            props.insert(QString::fromUtf8(prop.name()),
                         value.value<QColor>().name(QColor::HexArgb));
        else
            props.insert(QString::fromUtf8(prop.name()), QJsonValue::fromVariant(value));
    }

    QJsonObject obj;
    obj.insert(QStringLiteral("id"), node->nodeId());
    obj.insert(QStringLiteral("type"), node->typeId());
    obj.insert(QStringLiteral("props"), props);
    return obj;
}

Node *deserializeNode(const QJsonObject &obj)
{
    const QString type = obj.value(QStringLiteral("type")).toString();
    const int id = obj.value(QStringLiteral("id")).toInt();
    Node *node = NodeGraph::createNode(type, id);
    if (!node)
        return nullptr;

    const QJsonObject props = obj.value(QStringLiteral("props")).toObject();
    for (auto it = props.constBegin(); it != props.constEnd(); ++it) {
        // setProperty converts through the property system, so a "#ff4488"
        // string lands correctly in a QColor property.
        node->setProperty(it.key().toUtf8().constData(), it.value().toVariant());
    }
    return node;
}

} // namespace

Project::Project(QObject *parent)
    : QObject(parent)
{
}

void Project::setGraph(NodeGraph *graph)
{
    m_graph = graph;
    if (m_graph) {
        // graphChanged, not topologyChanged: a property edit and a moved node are
        // both edits, and both should mark the document dirty.
        connect(m_graph, &NodeGraph::graphChanged, this, [this] { setModified(true); });
    }
}

QString Project::displayName() const
{
    if (m_filePath.isEmpty())
        return tr("Untitled");
    return QFileInfo(m_filePath).completeBaseName();
}

void Project::setFilePath(const QString &path)
{
    if (m_filePath == path)
        return;
    m_filePath = path;
    Q_EMIT filePathChanged();
}

void Project::setModified(bool modified)
{
    if (m_modified == modified)
        return;
    m_modified = modified;
    Q_EMIT modifiedChanged();
}

void Project::setError(const QString &message)
{
    m_lastError = message;
    Q_EMIT lastErrorChanged();
}

bool Project::save(const QUrl &url)
{
    if (!m_graph) {
        setError(tr("No node graph is attached."));
        return false;
    }

    const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    if (path.isEmpty()) {
        setError(tr("No file name given."));
        return false;
    }

    QJsonObject canvas;
    canvas.insert(QStringLiteral("width"), m_canvasWidth);
    canvas.insert(QStringLiteral("height"), m_canvasHeight);
    canvas.insert(QStringLiteral("color"), m_canvasColor.name(QColor::HexArgb));

    QJsonArray nodes;
    QJsonArray connections;
    for (const Node *node : m_graph->nodes()) {
        nodes.append(serializeNode(node));

        // Edges are written as a flat list keyed by port NAME rather than as a
        // nested "inputs" object per node: it is readable, diffable, and survives
        // the port table being reordered.
        const QVector<PortSpec> &ports = node->spec().inputs;
        for (int port = 0; port < ports.size(); ++port) {
            const Node *source = node->input(port);
            if (!source)
                continue;
            QJsonObject edge;
            edge.insert(QStringLiteral("from"), source->nodeId());
            edge.insert(QStringLiteral("to"), node->nodeId());
            edge.insert(QStringLiteral("port"), ports.at(port).name);
            connections.append(edge);
        }
    }

    QJsonObject root;
    root.insert(QStringLiteral("format"), QStringLiteral("music-visualizer-project"));
    root.insert(QStringLiteral("version"), kFormatVersion);
    root.insert(QStringLiteral("canvas"), canvas);
    root.insert(QStringLiteral("nodes"), nodes);
    root.insert(QStringLiteral("connections"), connections);

    // QSaveFile so a failed write cannot destroy the previous project file.
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setError(tr("Could not open %1 for writing.").arg(path));
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        setError(tr("Could not write %1.").arg(path));
        return false;
    }

    setError({});
    setFilePath(path);
    setModified(false);
    return true;
}

bool Project::saveCurrent()
{
    if (m_filePath.isEmpty())
        return false;
    return save(QUrl::fromLocalFile(m_filePath));
}

bool Project::load(const QUrl &url)
{
    if (!m_graph) {
        setError(tr("No node graph is attached."));
        return false;
    }

    const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setError(tr("Could not open %1.").arg(path));
        return false;
    }

    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        setError(tr("%1 is not a valid project file: %2")
                     .arg(QFileInfo(path).fileName(), parseError.errorString()));
        return false;
    }

    const QJsonObject root = doc.object();
    const int version = root.value(QStringLiteral("version")).toInt();
    if (version > kFormatVersion) {
        setError(tr("This project was made with a newer version of the app "
                    "(format %1, this build understands %2).")
                     .arg(version)
                     .arg(kFormatVersion));
        return false;
    }
    if (version < kFormatVersion) {
        setError(tr("%1 is an older project format (%2); this build reads format %3 "
                    "only. Convert it with tools/migrate_v2_to_v3.py, which writes a "
                    "new file and leaves the original alone.")
                     .arg(QFileInfo(path).fileName())
                     .arg(version)
                     .arg(kFormatVersion));
        return false;
    }

    const QJsonObject canvas = root.value(QStringLiteral("canvas")).toObject();
    m_canvasWidth = canvas.value(QStringLiteral("width")).toInt(1920);
    m_canvasHeight = canvas.value(QStringLiteral("height")).toInt(1080);
    m_canvasColor = QColor(canvas.value(QStringLiteral("color")).toString(QStringLiteral("#0b0b12")));

    // Two passes: create every node with the id from the file, then resolve the
    // edge list against those ids. resetNodes validates each edge and drops the
    // ones that fail, so one bad connection does not lose the project.
    QVector<Node *> nodes;
    for (const QJsonValue &value : root.value(QStringLiteral("nodes")).toArray()) {
        if (Node *node = deserializeNode(value.toObject()))
            nodes.append(node);
    }

    QVector<NodeEdge> edges;
    for (const QJsonValue &value : root.value(QStringLiteral("connections")).toArray()) {
        const QJsonObject obj = value.toObject();
        NodeEdge edge;
        edge.fromId = obj.value(QStringLiteral("from")).toInt();
        edge.toId = obj.value(QStringLiteral("to")).toInt();
        edge.port = obj.value(QStringLiteral("port")).toString();
        edges.append(edge);
    }

    m_graph->resetNodes(nodes, edges);

    setError({});
    setFilePath(path);
    Q_EMIT canvasChanged();
    Q_EMIT loaded();
    // resetNodes marked the document dirty; a freshly loaded file is not.
    setModified(false);
    return true;
}

void Project::newProject()
{
    if (!m_graph)
        return;

    m_canvasWidth = 1920;
    m_canvasHeight = 1080;
    m_canvasColor = QColor(QStringLiteral("#0b0b12"));
    m_graph->loadDefaults();

    setFilePath({});
    setError({});
    Q_EMIT canvasChanged();
    Q_EMIT loaded();
    setModified(false);
}

} // namespace mv
