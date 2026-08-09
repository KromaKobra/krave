#pragma once

#include <QColor>
#include <QObject>
#include <QQmlEngine>
#include <QUrl>

namespace mv {

class LayerModel;

/// Load/save of the visualizer configuration.
///
/// Layer properties are read and written by walking each layer's QMetaObject, so
/// a new Q_PROPERTY on a Layer subclass is persisted automatically -- there is no
/// per-property serialization code to forget to update.
///
/// The project file intentionally stores NO audio path. Projects are meant to be
/// reusable templates; the user re-links a track after loading one.
class Project : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString filePath READ filePath NOTIFY filePathChanged)
    Q_PROPERTY(QString displayName READ displayName NOTIFY filePathChanged)
    Q_PROPERTY(bool modified READ isModified NOTIFY modifiedChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

    Q_PROPERTY(int canvasWidth MEMBER m_canvasWidth NOTIFY canvasChanged)
    Q_PROPERTY(int canvasHeight MEMBER m_canvasHeight NOTIFY canvasChanged)
    Q_PROPERTY(QColor canvasColor MEMBER m_canvasColor NOTIFY canvasChanged)

public:
    /// Bumped when the on-disk shape changes incompatibly.
    ///
    /// v2 moved all audio conditioning off BarLayer onto a dedicated
    /// AudioProviderLayer. v1 files still load: see migrateFromV1.
    static constexpr int kFormatVersion = 2;

    explicit Project(QObject *parent = nullptr);

    void setLayerModel(LayerModel *model);

    Q_INVOKABLE bool save(const QUrl &url);
    Q_INVOKABLE bool load(const QUrl &url);
    /// Save back to the current filePath; false if there is not one yet.
    Q_INVOKABLE bool saveCurrent();
    Q_INVOKABLE void newProject();

    QString filePath() const { return m_filePath; }
    QString displayName() const;
    bool isModified() const { return m_modified; }
    QString lastError() const { return m_lastError; }

    int canvasWidth() const { return m_canvasWidth; }
    int canvasHeight() const { return m_canvasHeight; }
    QColor canvasColor() const { return m_canvasColor; }

Q_SIGNALS:
    void filePathChanged();
    void modifiedChanged();
    void lastErrorChanged();
    void canvasChanged();
    void loaded();

private:
    void setFilePath(const QString &path);
    void setModified(bool modified);
    void setError(const QString &message);

    LayerModel *m_model = nullptr;
    QString m_filePath;
    bool m_modified = false;
    QString m_lastError;

    int m_canvasWidth = 1920;
    int m_canvasHeight = 1080;
    QColor m_canvasColor = QColor(QStringLiteral("#0b0b12"));
};

} // namespace mv
