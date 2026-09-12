#pragma once

#include <QColor>
#include <QObject>
#include <QQmlEngine>
#include <QUrl>

namespace mv {

class NodeGraph;

/// Load/save of the visualizer configuration.
///
/// Node properties are read and written by walking each node's QMetaObject, so a
/// new Q_PROPERTY on a Node subclass is persisted automatically -- there is no
/// per-property serialization code to forget to update. A node's `id` sits
/// alongside `type` rather than inside `props`, which keeps `nodeId` non-writable
/// and so keeps it out of that walk.
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
    /// v3 replaced the ordered layer stack with a node graph: "layers" became
    /// "nodes" plus an explicit "connections" edge list. Nothing older loads --
    /// load() rejects it and names the conversion script. Keeping an in-app
    /// migration would have meant carrying migrateFromV1, migrateLegacyValues and
    /// a new migrateFromV2 forever, and pinning the typeId strings they read;
    /// tools/migrate_v2_to_v3.py does it once instead.
    static constexpr int kFormatVersion = 3;

    explicit Project(QObject *parent = nullptr);

    void setGraph(NodeGraph *graph);

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

    NodeGraph *m_graph = nullptr;
    QString m_filePath;
    bool m_modified = false;
    QString m_lastError;

    int m_canvasWidth = 1920;
    int m_canvasHeight = 1080;
    QColor m_canvasColor = QColor(QStringLiteral("#0b0b12"));
};

} // namespace mv
