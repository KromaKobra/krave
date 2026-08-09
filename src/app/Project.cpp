#include "Project.h"
#include "LayerModel.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaProperty>
#include <QSaveFile>

namespace mv {
namespace {

/// Write every writable, stored property a Layer subclass declares.
///
/// Starting at Layer's property offset skips QObject's own `objectName`; the loop
/// runs to the concrete class's property count so subclass properties come along
/// for free. Read-only properties (typeId, layerId, bands) are skipped because
/// they are either derived or live data.
QJsonObject serializeLayer(const Layer *layer)
{
    QJsonObject props;
    const QMetaObject *meta = layer->metaObject();

    for (int i = Layer::staticMetaObject.propertyOffset(); i < meta->propertyCount(); ++i) {
        const QMetaProperty prop = meta->property(i);
        if (!prop.isWritable() || !prop.isStored())
            continue;

        const QVariant value = prop.read(layer);
        if (value.typeId() == QMetaType::QColor)
            props.insert(QString::fromUtf8(prop.name()),
                         value.value<QColor>().name(QColor::HexArgb));
        else
            props.insert(QString::fromUtf8(prop.name()), QJsonValue::fromVariant(value));
    }

    QJsonObject obj;
    obj.insert(QStringLiteral("type"), layer->typeId());
    obj.insert(QStringLiteral("props"), props);
    return obj;
}

Layer *deserializeLayer(const QJsonObject &obj)
{
    const QString type = obj.value(QStringLiteral("type")).toString();
    Layer *layer = LayerModel::createLayer(type);
    if (!layer)
        return nullptr;

    const QJsonObject props = obj.value(QStringLiteral("props")).toObject();
    for (auto it = props.constBegin(); it != props.constEnd(); ++it) {
        // setProperty converts through the property system, so a "#ff4488"
        // string lands correctly in a QColor property.
        layer->setProperty(it.key().toUtf8().constData(), it.value().toVariant());
    }
    return layer;
}

} // namespace

Project::Project(QObject *parent)
    : QObject(parent)
{
}

void Project::setLayerModel(LayerModel *model)
{
    m_model = model;
    if (m_model) {
        connect(m_model, &LayerModel::layersChanged, this, [this] { setModified(true); });
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
    if (!m_model) {
        setError(tr("No layer model is attached."));
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

    QJsonArray layers;
    for (const Layer *layer : m_model->layers())
        layers.append(serializeLayer(layer));

    QJsonObject root;
    root.insert(QStringLiteral("format"), QStringLiteral("music-visualizer-project"));
    root.insert(QStringLiteral("version"), kFormatVersion);
    root.insert(QStringLiteral("canvas"), canvas);
    root.insert(QStringLiteral("layers"), layers);

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
    if (!m_model) {
        setError(tr("No layer model is attached."));
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

    const QJsonObject canvas = root.value(QStringLiteral("canvas")).toObject();
    m_canvasWidth = canvas.value(QStringLiteral("width")).toInt(1920);
    m_canvasHeight = canvas.value(QStringLiteral("height")).toInt(1080);
    m_canvasColor = QColor(canvas.value(QStringLiteral("color")).toString(QStringLiteral("#0b0b12")));

    QVector<Layer *> layers;
    const QJsonArray array = root.value(QStringLiteral("layers")).toArray();
    for (const QJsonValue &value : array) {
        if (Layer *layer = deserializeLayer(value.toObject()))
            layers.append(layer);
    }

    m_model->resetLayers(layers);

    setError({});
    setFilePath(path);
    Q_EMIT canvasChanged();
    Q_EMIT loaded();
    // resetLayers marked the document dirty; a freshly loaded file is not.
    setModified(false);
    return true;
}

void Project::newProject()
{
    if (!m_model)
        return;

    m_canvasWidth = 1920;
    m_canvasHeight = 1080;
    m_canvasColor = QColor(QStringLiteral("#0b0b12"));
    m_model->loadDefaults();

    setFilePath({});
    setError({});
    Q_EMIT canvasChanged();
    Q_EMIT loaded();
    setModified(false);
}

} // namespace mv
