#pragma once

#include "Layer.h"

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QVector>

namespace mv {

/// Ordered stack of layers. Index 0 is the BOTTOM of the z-stack, so a QML
/// Repeater over this model paints in the natural back-to-front order.
class LayerModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int selectedIndex READ selectedIndex WRITE setSelectedIndex NOTIFY selectedIndexChanged)
    Q_PROPERTY(mv::Layer *selectedLayer READ selectedLayer NOTIFY selectedIndexChanged)

public:
    enum Roles
    {
        LayerRole = Qt::UserRole + 1,
        NameRole,
        TypeRole,
        VisibleRole,
    };

    explicit LayerModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    int count() const { return int(m_layers.size()); }
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// `typeId` is one of "bars", "background", "text". Returns the new index,
    /// or -1 if the type is unknown. New layers go on top.
    Q_INVOKABLE int addLayer(const QString &typeId);
    Q_INVOKABLE void removeLayer(int index);
    Q_INVOKABLE void moveLayer(int from, int to);
    Q_INVOKABLE void clear();

    /// The Layer at `index`, or nullptr. Ownership stays with the model, so QML
    /// gets CppOwnership and will not garbage-collect it out from under us.
    Q_INVOKABLE mv::Layer *at(int index) const;

    int selectedIndex() const { return m_selectedIndex; }
    void setSelectedIndex(int index);
    Layer *selectedLayer() const { return at(m_selectedIndex); }

    const QVector<Layer *> &layers() const { return m_layers; }

    /// Every layer that needs per-frame spectrum data.
    QVector<BarLayer *> spectrumLayers() const;

    /// Build a layer of the given type without inserting it. Used by project load.
    static Layer *createLayer(const QString &typeId, QObject *parent = nullptr);

    /// Replace the whole stack, taking ownership of `layers`.
    void resetLayers(const QVector<Layer *> &layers);

    /// Set up a reasonable starting project so a new user sees something.
    void loadDefaults();

Q_SIGNALS:
    void countChanged();
    void selectedIndexChanged();
    /// Any layer property changed, or the stack itself changed. The document is dirty.
    void layersChanged();

private:
    void adoptLayer(Layer *layer);

    QVector<Layer *> m_layers;
    int m_selectedIndex = -1;
    int m_nextLayerId = 1;
};

} // namespace mv
