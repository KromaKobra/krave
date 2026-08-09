#include "LayerModel.h"

#include <QQmlEngine>

namespace mv {

LayerModel::LayerModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int LayerModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_layers.size();
}

QVariant LayerModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_layers.size())
        return {};

    Layer *layer = m_layers.at(index.row());
    switch (role) {
    case LayerRole:
        return QVariant::fromValue(layer);
    case NameRole:
        return layer->name();
    case TypeRole:
        return layer->typeId();
    case VisibleRole:
        return layer->layerVisible();
    default:
        return {};
    }
}

QHash<int, QByteArray> LayerModel::roleNames() const
{
    // Deliberately not "layer": a delegate exposing that role as a required
    // property would collide with QQuickItem's FINAL `layer` property.
    return {
        {LayerRole, "layerObject"},
        {NameRole, "name"},
        {TypeRole, "typeId"},
        {VisibleRole, "layerVisible"},
    };
}

Layer *LayerModel::createLayer(const QString &typeId, QObject *parent)
{
    if (typeId == QLatin1String("bars"))
        return new BarLayer(parent);
    if (typeId == QLatin1String("background"))
        return new BackgroundLayer(parent);
    if (typeId == QLatin1String("text"))
        return new TextLayer(parent);
    return nullptr;
}

void LayerModel::adoptLayer(Layer *layer)
{
    layer->setParent(this);
    layer->setLayerId(m_nextLayerId++);
    QQmlEngine::setObjectOwnership(layer, QQmlEngine::CppOwnership);

    // Re-emit any property edit as a document-level change so the canvas repaints
    // and the title bar can show a dirty marker.
    connect(layer, &Layer::changed, this, [this, layer] {
        const int row = m_layers.indexOf(layer);
        if (row >= 0) {
            const QModelIndex idx = index(row);
            Q_EMIT dataChanged(idx, idx, {NameRole, VisibleRole});
        }
        Q_EMIT layersChanged();
    });
}

int LayerModel::addLayer(const QString &typeId)
{
    Layer *layer = createLayer(typeId, this);
    if (!layer)
        return -1;

    const int row = m_layers.size();
    beginInsertRows({}, row, row);
    adoptLayer(layer);
    m_layers.append(layer);
    endInsertRows();

    Q_EMIT countChanged();
    Q_EMIT layersChanged();
    setSelectedIndex(row);
    return row;
}

void LayerModel::removeLayer(int index)
{
    if (index < 0 || index >= m_layers.size())
        return;

    beginRemoveRows({}, index, index);
    Layer *layer = m_layers.takeAt(index);
    layer->deleteLater();
    endRemoveRows();

    Q_EMIT countChanged();
    Q_EMIT layersChanged();

    if (m_layers.isEmpty())
        setSelectedIndex(-1);
    else
        setSelectedIndex(qBound(0, m_selectedIndex, m_layers.size() - 1));
}

void LayerModel::moveLayer(int from, int to)
{
    if (from == to)
        return;
    if (from < 0 || from >= m_layers.size())
        return;
    if (to < 0 || to >= m_layers.size())
        return;

    // beginMoveRows wants the destination row in pre-move coordinates.
    const int destination = (to > from) ? to + 1 : to;
    if (!beginMoveRows({}, from, from, {}, destination))
        return;
    m_layers.move(from, to);
    endMoveRows();

    Q_EMIT layersChanged();
    setSelectedIndex(to);
}

void LayerModel::clear()
{
    if (m_layers.isEmpty())
        return;

    beginResetModel();
    qDeleteAll(m_layers);
    m_layers.clear();
    endResetModel();

    m_selectedIndex = -1;
    Q_EMIT countChanged();
    Q_EMIT selectedIndexChanged();
    Q_EMIT layersChanged();
}

Layer *LayerModel::at(int index) const
{
    if (index < 0 || index >= m_layers.size())
        return nullptr;
    return m_layers.at(index);
}

void LayerModel::setSelectedIndex(int index)
{
    const int clamped = (index >= 0 && index < m_layers.size()) ? index : -1;
    if (m_selectedIndex == clamped)
        return;
    m_selectedIndex = clamped;
    Q_EMIT selectedIndexChanged();
}

QVector<BarLayer *> LayerModel::spectrumLayers() const
{
    QVector<BarLayer *> result;
    for (Layer *layer : m_layers) {
        if (auto *bars = qobject_cast<BarLayer *>(layer))
            result.append(bars);
    }
    return result;
}

void LayerModel::resetLayers(const QVector<Layer *> &layers)
{
    beginResetModel();
    qDeleteAll(m_layers);
    m_layers.clear();
    m_nextLayerId = 1;
    for (Layer *layer : layers) {
        adoptLayer(layer);
        m_layers.append(layer);
    }
    endResetModel();

    m_selectedIndex = m_layers.isEmpty() ? -1 : 0;
    Q_EMIT countChanged();
    Q_EMIT selectedIndexChanged();
    Q_EMIT layersChanged();
}

void LayerModel::loadDefaults()
{
    QVector<Layer *> defaults;
    defaults.append(new BackgroundLayer);
    defaults.append(new BarLayer);
    resetLayers(defaults);
}

} // namespace mv
