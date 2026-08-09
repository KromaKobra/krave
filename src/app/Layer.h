#pragma once

#include <SpectrumAnalyzer.h>

#include <QColor>
#include <QList>
#include <QObject>
#include <QQmlEngine>
#include <QString>

namespace mv {

/// Base class for everything stacked on the visualizer canvas.
///
/// Properties are declared with Q_PROPERTY(... MEMBER ...) on purpose: it gives
/// QML two-way bindings and lets Project.cpp serialize by walking the meta-object,
/// so adding a property here costs no serialization code and no panel plumbing.
///
/// They all share one `changed` NOTIFY signal. QML re-evaluates the affected
/// bindings on it, and the slight over-invalidation is far cheaper than
/// hand-writing a signal per property.
class Layer : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString typeId READ typeId CONSTANT)
    Q_PROPERTY(int layerId READ layerId CONSTANT)

    Q_PROPERTY(QString name MEMBER m_name NOTIFY changed)
    Q_PROPERTY(bool layerVisible MEMBER m_layerVisible NOTIFY changed)
    Q_PROPERTY(qreal layerOpacity MEMBER m_layerOpacity NOTIFY changed)
    Q_PROPERTY(QString blendMode MEMBER m_blendMode NOTIFY changed)

    // Geometry is normalized to the canvas (0..1) so a project renders the same
    // at 1280x720 preview and 3840x2160 export.
    Q_PROPERTY(qreal posX MEMBER m_posX NOTIFY changed)
    Q_PROPERTY(qreal posY MEMBER m_posY NOTIFY changed)
    Q_PROPERTY(qreal sizeW MEMBER m_sizeW NOTIFY changed)
    Q_PROPERTY(qreal sizeH MEMBER m_sizeH NOTIFY changed)
    Q_PROPERTY(qreal layerRotation MEMBER m_layerRotation NOTIFY changed)

public:
    explicit Layer(QObject *parent = nullptr);

    virtual QString typeId() const = 0;

    int layerId() const { return m_layerId; }
    void setLayerId(int id) { m_layerId = id; }

    QString name() const { return m_name; }
    void setName(const QString &n) { m_name = n; Q_EMIT changed(); }
    bool layerVisible() const { return m_layerVisible; }
    qreal layerOpacity() const { return m_layerOpacity; }

    /// True when this layer consumes spectrum data and must be analyzed each frame.
    virtual bool needsSpectrum() const { return false; }

Q_SIGNALS:
    void changed();

protected:
    int m_layerId = 0;
    QString m_name;
    bool m_layerVisible = true;
    qreal m_layerOpacity = 1.0;
    QString m_blendMode = QStringLiteral("normal");
    qreal m_posX = 0.5;
    qreal m_posY = 0.5;
    qreal m_sizeW = 1.0;
    qreal m_sizeH = 1.0;
    qreal m_layerRotation = 0.0;
};

/// Frequency-reactive EQ bars: the primary visualizer type.
class BarLayer : public Layer
{
    Q_OBJECT

    // --- Shape -------------------------------------------------------------
    Q_PROPERTY(int barCount MEMBER m_barCount NOTIFY changed)
    Q_PROPERTY(qreal barWidthRatio MEMBER m_barWidthRatio NOTIFY changed)
    Q_PROPERTY(qreal cornerRadius MEMBER m_cornerRadius NOTIFY changed)
    Q_PROPERTY(qreal minHeight MEMBER m_minHeight NOTIFY changed)
    Q_PROPERTY(qreal maxHeight MEMBER m_maxHeight NOTIFY changed)

    // --- Placement ---------------------------------------------------------
    /// "linear", "mirrored" or "radial"
    Q_PROPERTY(QString orientation MEMBER m_orientation NOTIFY changed)
    /// Radial only: hole size as a fraction of the layer's smaller dimension.
    Q_PROPERTY(qreal innerRadius MEMBER m_innerRadius NOTIFY changed)
    Q_PROPERTY(bool growUpward MEMBER m_growUpward NOTIFY changed)

    // --- Colour ------------------------------------------------------------
    /// "solid", "gradient" or "spectrum" (hue swept across the bars)
    Q_PROPERTY(QString colorMode MEMBER m_colorMode NOTIFY changed)
    Q_PROPERTY(QColor colorA MEMBER m_colorA NOTIFY changed)
    Q_PROPERTY(QColor colorB MEMBER m_colorB NOTIFY changed)
    /// Extra brightness applied in proportion to a bar's level, 0 = off.
    Q_PROPERTY(qreal reactiveBrightness MEMBER m_reactiveBrightness NOTIFY changed)

    // --- Analysis ----------------------------------------------------------
    Q_PROPERTY(int fftSize MEMBER m_fftSize NOTIFY changed)
    Q_PROPERTY(qreal minHz MEMBER m_minHz NOTIFY changed)
    Q_PROPERTY(qreal maxHz MEMBER m_maxHz NOTIFY changed)
    Q_PROPERTY(qreal floorDb MEMBER m_floorDb NOTIFY changed)
    Q_PROPERTY(qreal ceilingDb MEMBER m_ceilingDb NOTIFY changed)
    /// "linear", "log" or "power"
    Q_PROPERTY(QString scalingCurve MEMBER m_scalingCurve NOTIFY changed)
    Q_PROPERTY(qreal gamma MEMBER m_gamma NOTIFY changed)
    Q_PROPERTY(qreal attackMs MEMBER m_attackMs NOTIFY changed)
    Q_PROPERTY(qreal decayMs MEMBER m_decayMs NOTIFY changed)

    // --- Live data, written by SpectrumEngine each frame --------------------
    Q_PROPERTY(QList<qreal> bands READ bands NOTIFY bandsChanged)
    Q_PROPERTY(qreal bass READ bass NOTIFY bandsChanged)
    Q_PROPERTY(qreal mid READ mid NOTIFY bandsChanged)
    Q_PROPERTY(qreal treble READ treble NOTIFY bandsChanged)

public:
    explicit BarLayer(QObject *parent = nullptr);

    QString typeId() const override { return QStringLiteral("bars"); }
    bool needsSpectrum() const override { return true; }

    const QList<qreal> &bands() const { return m_bands; }
    qreal bass() const { return m_bass; }
    qreal mid() const { return m_mid; }
    qreal treble() const { return m_treble; }

    void applyFrame(const SpectrumFrame &frame);

    /// Translate this layer's editable properties into analyzer parameters.
    SpectrumParams analysisParams() const;

Q_SIGNALS:
    void bandsChanged();

private:
    int m_barCount = 64;
    qreal m_barWidthRatio = 0.65;
    qreal m_cornerRadius = 2.0;
    qreal m_minHeight = 0.005;
    qreal m_maxHeight = 0.8;

    QString m_orientation = QStringLiteral("linear");
    qreal m_innerRadius = 0.35;
    bool m_growUpward = true;

    QString m_colorMode = QStringLiteral("gradient");
    QColor m_colorA = QColor(QStringLiteral("#4cc9f0"));
    QColor m_colorB = QColor(QStringLiteral("#f72585"));
    qreal m_reactiveBrightness = 0.0;

    int m_fftSize = 2048;
    qreal m_minHz = 30.0;
    qreal m_maxHz = 16000.0;
    qreal m_floorDb = -72.0;
    qreal m_ceilingDb = -6.0;
    QString m_scalingCurve = QStringLiteral("linear");
    qreal m_gamma = 1.0;
    qreal m_attackMs = 10.0;
    qreal m_decayMs = 180.0;

    QList<qreal> m_bands;
    qreal m_bass = 0.0;
    qreal m_mid = 0.0;
    qreal m_treble = 0.0;
};

/// Flat colour or image behind everything else.
class BackgroundLayer : public Layer
{
    Q_OBJECT

    Q_PROPERTY(QColor color MEMBER m_color NOTIFY changed)
    Q_PROPERTY(QString imageSource MEMBER m_imageSource NOTIFY changed)
    /// "stretch", "fit" or "crop"
    Q_PROPERTY(QString fillMode MEMBER m_fillMode NOTIFY changed)

public:
    explicit BackgroundLayer(QObject *parent = nullptr);
    QString typeId() const override { return QStringLiteral("background"); }

private:
    QColor m_color = QColor(QStringLiteral("#0b0b12"));
    QString m_imageSource;
    QString m_fillMode = QStringLiteral("crop");
};

/// Static text, e.g. the track title.
class TextLayer : public Layer
{
    Q_OBJECT

    Q_PROPERTY(QString text MEMBER m_text NOTIFY changed)
    Q_PROPERTY(QString fontFamily MEMBER m_fontFamily NOTIFY changed)
    /// Point size as a fraction of canvas height, so text scales with resolution.
    Q_PROPERTY(qreal fontScale MEMBER m_fontScale NOTIFY changed)
    Q_PROPERTY(bool bold MEMBER m_bold NOTIFY changed)
    Q_PROPERTY(QColor color MEMBER m_color NOTIFY changed)
    /// "left", "center" or "right"
    Q_PROPERTY(QString alignment MEMBER m_alignment NOTIFY changed)

public:
    explicit TextLayer(QObject *parent = nullptr);
    QString typeId() const override { return QStringLiteral("text"); }

private:
    QString m_text = QStringLiteral("Track title");
    QString m_fontFamily = QStringLiteral("Segoe UI");
    qreal m_fontScale = 0.06;
    bool m_bold = true;
    QColor m_color = QColor(QStringLiteral("#ffffff"));
    QString m_alignment = QStringLiteral("center");
};

} // namespace mv
