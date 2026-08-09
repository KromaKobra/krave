#include <AudioBuffer.h>
#include <Fft.h>
#include <SpectrumAnalyzer.h>

#include <QTest>
#include <cmath>

using namespace mv;

namespace {
constexpr double kPi = 3.14159265358979323846;

/// Reference DFT to check the fast path against.
void naiveDft(const QVector<float> &input, QVector<float> &re, QVector<float> &im)
{
    const int n = input.size();
    re.resize(n);
    im.resize(n);
    for (int k = 0; k < n; ++k) {
        double sr = 0.0;
        double si = 0.0;
        for (int t = 0; t < n; ++t) {
            const double angle = -2.0 * kPi * k * t / n;
            sr += input[t] * std::cos(angle);
            si += input[t] * std::sin(angle);
        }
        re[k] = float(sr);
        im[k] = float(si);
    }
}

AudioBuffer makeSine(double freqHz, double seconds, int sampleRate = 48000, float amp = 0.5f)
{
    const int frames = int(seconds * sampleRate);
    QVector<float> samples(frames * 2);
    for (int i = 0; i < frames; ++i) {
        const float v = amp * float(std::sin(2.0 * kPi * freqHz * i / sampleRate));
        samples[i * 2] = v;
        samples[i * 2 + 1] = v;
    }
    return AudioBuffer(std::move(samples), sampleRate, 2);
}

} // namespace

class TestFft : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void impulseGivesFlatSpectrum();
    void sineLandsInOneBin();
    void matchesNaiveDft();
    void rejectsNonPowerOfTwo();

    void bufferReadMonoPadsEdges();
    void bufferPeaksTrackAmplitude();

    void analyzerFindsToneInRightBand();
    void analyzerSmoothingIsFrameRateIndependent();
    void analyzerDecaysToSilence();
    void analyzerKeepsStateWhenParamsAreResent();
    void windowLengthSetsResolutionContinuously();
    void bandNormalizationFlattensWithoutCrushingDynamics();
};

void TestFft::impulseGivesFlatSpectrum()
{
    const int n = 64;
    Fft fft(n);

    QVector<float> input(n, 0.0f);
    input[0] = 1.0f;

    QVector<float> mags(n / 2), sr(n), si(n);
    fft.magnitudes(input.constData(), mags.data(), sr.data(), si.data());

    // A unit impulse has unit magnitude at every frequency.
    for (int i = 0; i < n / 2; ++i)
        QVERIFY2(std::abs(mags[i] - 1.0f) < 1e-4f,
                 qPrintable(QStringLiteral("bin %1 = %2").arg(i).arg(mags[i])));
}

void TestFft::sineLandsInOneBin()
{
    const int n = 1024;
    const int targetBin = 64;
    Fft fft(n);

    QVector<float> input(n);
    for (int i = 0; i < n; ++i)
        input[i] = float(std::sin(2.0 * kPi * targetBin * i / n));

    QVector<float> mags(n / 2), sr(n), si(n);
    fft.magnitudes(input.constData(), mags.data(), sr.data(), si.data());

    int peak = 0;
    for (int i = 1; i < n / 2; ++i)
        if (mags[i] > mags[peak])
            peak = i;

    QCOMPARE(peak, targetBin);
    // A bin-aligned sine should put essentially everything in that one bin.
    QVERIFY(mags[targetBin] > 100.0f * mags[targetBin + 3]);
}

void TestFft::matchesNaiveDft()
{
    const int n = 128;
    Fft fft(n);

    QVector<float> input(n);
    for (int i = 0; i < n; ++i)
        input[i] = float(std::sin(i * 0.37) * 0.6 + std::cos(i * 1.13) * 0.25);

    QVector<float> expectRe, expectIm;
    naiveDft(input, expectRe, expectIm);

    QVector<float> re = input;
    QVector<float> im(n, 0.0f);
    fft.forward(re.data(), im.data());

    for (int k = 0; k < n; ++k) {
        QVERIFY2(std::abs(re[k] - expectRe[k]) < 1e-2f,
                 qPrintable(QStringLiteral("re[%1]: %2 vs %3").arg(k).arg(re[k]).arg(expectRe[k])));
        QVERIFY2(std::abs(im[k] - expectIm[k]) < 1e-2f,
                 qPrintable(QStringLiteral("im[%1]: %2 vs %3").arg(k).arg(im[k]).arg(expectIm[k])));
    }
}

void TestFft::rejectsNonPowerOfTwo()
{
    QVERIFY(!Fft::isPowerOfTwo(1000));
    QVERIFY(Fft::isPowerOfTwo(1024));
    QVERIFY(!Fft::isPowerOfTwo(0));
    QVERIFY(!Fft::isPowerOfTwo(1));
}

void TestFft::bufferReadMonoPadsEdges()
{
    QVector<float> samples{1.0f, 1.0f, 2.0f, 2.0f, 3.0f, 3.0f}; // 3 stereo frames
    AudioBuffer buffer(samples, 48000, 2);

    QCOMPARE(buffer.frameCount(), 3);
    QCOMPARE(buffer.channels(), 2);

    QVector<float> out(5);
    buffer.readMono(-1, out.data(), 5);

    // Reading across both edges must pad with silence, not read out of bounds.
    QCOMPARE(out[0], 0.0f);
    QCOMPARE(out[1], 1.0f);
    QCOMPARE(out[2], 2.0f);
    QCOMPARE(out[3], 3.0f);
    QCOMPARE(out[4], 0.0f);
}

void TestFft::bufferPeaksTrackAmplitude()
{
    const AudioBuffer buffer = makeSine(100.0, 1.0, 48000, 0.5f);

    QVector<float> mins, maxs;
    buffer.computePeaks(100, mins, maxs);

    QCOMPARE(mins.size(), 100);
    QCOMPARE(maxs.size(), 100);

    for (int i = 0; i < 100; ++i) {
        QVERIFY(maxs[i] > 0.4f && maxs[i] <= 0.51f);
        QVERIFY(mins[i] < -0.4f && mins[i] >= -0.51f);
    }
}

void TestFft::analyzerFindsToneInRightBand()
{
    const AudioBuffer buffer = makeSine(1000.0, 1.0);

    SpectrumParams params;
    params.bandCount = 32;
    params.attackTau = 0.0; // no smoothing: respond immediately
    params.decayTau = 0.0;

    SpectrumAnalyzer analyzer;
    analyzer.setParams(params);

    const SpectrumFrame frame = analyzer.analyze(buffer, 0.5, 1.0 / 60.0);
    QCOMPARE(frame.bands.size(), 32);

    int peak = 0;
    for (int i = 1; i < frame.bands.size(); ++i)
        if (frame.bands[i] > frame.bands[peak])
            peak = i;

    // Which band should hold 1 kHz, given log spacing from minHz to maxHz?
    const double logMin = std::log(params.minHz);
    const double logMax = std::log(params.maxHz);
    const int expected = int((std::log(1000.0) - logMin) / (logMax - logMin) * params.bandCount);

    QVERIFY2(std::abs(peak - expected) <= 1,
             qPrintable(QStringLiteral("peak band %1, expected near %2").arg(peak).arg(expected)));
    QVERIFY(frame.bands[peak] > 0.5f);

    // 1 kHz is a mid, so mid energy should dominate.
    QVERIFY(frame.mid > frame.bass);
    QVERIFY(frame.mid > frame.treble);
}

void TestFft::analyzerSmoothingIsFrameRateIndependent()
{
    // The core guarantee behind preview/export parity: decaying for a fixed
    // amount of TIME must give the same result regardless of step size.
    const AudioBuffer silence;

    SpectrumParams params;
    params.bandCount = 4;
    params.decayTau = 0.100;

    auto decayOver = [&](double dt, int steps) {
        SpectrumAnalyzer analyzer;
        analyzer.setParams(params);

        // Seed the envelope with a loud frame, then let it fall through silence.
        const AudioBuffer loud = makeSine(1000.0, 0.5, 48000, 0.9f);
        for (int i = 0; i < 200; ++i)
            analyzer.analyze(loud, 0.25, 0.001);

        SpectrumFrame frame;
        for (int i = 0; i < steps; ++i)
            frame = analyzer.analyze(silence, 0.0, dt);
        return frame.bands;
    };

    const QVector<float> at60 = decayOver(1.0 / 60.0, 30); // 0.5 s
    const QVector<float> at30 = decayOver(1.0 / 30.0, 15); // 0.5 s

    QCOMPARE(at60.size(), at30.size());
    for (int i = 0; i < at60.size(); ++i) {
        QVERIFY2(std::abs(at60[i] - at30[i]) < 0.02f,
                 qPrintable(QStringLiteral("band %1: 60fps %2 vs 30fps %3")
                                .arg(i).arg(at60[i]).arg(at30[i])));
    }
}

void TestFft::analyzerDecaysToSilence()
{
    SpectrumParams params;
    params.bandCount = 8;
    params.decayTau = 0.05;

    SpectrumAnalyzer analyzer;
    analyzer.setParams(params);

    const AudioBuffer loud = makeSine(500.0, 0.5, 48000, 0.9f);
    for (int i = 0; i < 100; ++i)
        analyzer.analyze(loud, 0.25, 0.01);

    SpectrumFrame frame;
    for (int i = 0; i < 200; ++i)
        frame = analyzer.analyze(AudioBuffer(), 0.0, 0.01);

    for (float v : frame.bands)
        QVERIFY(v < 0.01f);
}

void TestFft::analyzerKeepsStateWhenParamsAreResent()
{
    // SpectrumEngine hands the parameters over on every frame. If setParams
    // treats that as a change and rebuilds, the smoothing envelope is cleared
    // each frame: attack/decay stop behaving as a filter and become a plain
    // gain, and per-band auto-gain can never converge.
    SpectrumParams params;
    params.bandCount = 8;
    params.attackTau = 0.0;
    params.decayTau = 0.400;

    const AudioBuffer loud = makeSine(500.0, 0.5, 48000, 0.9f);

    auto runFor = [&](bool resendEachFrame) {
        SpectrumAnalyzer analyzer;
        analyzer.setParams(params);
        for (int i = 0; i < 60; ++i) {
            if (resendEachFrame)
                analyzer.setParams(params);
            analyzer.analyze(loud, 0.25, 1.0 / 60.0);
        }
        SpectrumFrame frame;
        for (int i = 0; i < 12; ++i) { // 0.2s of silence
            if (resendEachFrame)
                analyzer.setParams(params);
            frame = analyzer.analyze(AudioBuffer(), 0.0, 1.0 / 60.0);
        }
        return frame.bands;
    };

    const QVector<float> quiet = runFor(false);
    const QVector<float> resent = runFor(true);

    QCOMPARE(quiet.size(), resent.size());
    for (int i = 0; i < quiet.size(); ++i) {
        QVERIFY2(std::abs(quiet[i] - resent[i]) < 1e-5f,
                 qPrintable(QStringLiteral("band %1: %2 without resend vs %3 with")
                                .arg(i).arg(quiet[i]).arg(resent[i])));
    }

    // And the envelope must actually still be falling, not already at zero --
    // otherwise the comparison above could pass on two broken runs.
    bool anyAboveZero = false;
    for (float v : resent)
        anyAboveZero = anyAboveZero || v > 0.01f;
    QVERIFY(anyAboveZero);
}

void TestFft::windowLengthSetsResolutionContinuously()
{
    // How wide a pure tone appears is set by the analysis window, and the window
    // is zero-padded rather than rounded to a power of two. Rounding would
    // quantise resolution into factor-of-two jumps, making it impossible to dial
    // a peak to a specific width.
    const AudioBuffer tone = makeSine(50.0, 2.0, 48000, 0.8f);

    // Measure the tone's width in Hz at half maximum.
    auto fwhmHz = [&](int windowSamples) {
        SpectrumParams p;
        p.windowSamples = windowSamples;
        p.bandCount = 400;
        p.minHz = 0.1;
        p.maxHz = 400.0;
        p.hzLinearFactor = 1.0;         // linear, so band index maps to Hz evenly
        p.amplitude = AmplitudeMode::Linear;
        p.linearGain = 1.0;
        p.curve = ScalingCurve::Linear;
        p.spatialSmoothing = 0.0;
        p.gateDb = 0.0;
        p.attackTau = 0.0;
        p.decayTau = 0.0;

        SpectrumAnalyzer a;
        a.setParams(p);
        const SpectrumFrame f = a.analyze(tone, 1.0, 1.0 / 60.0);

        const int peak = int(std::max_element(f.bands.begin(), f.bands.end())
                             - f.bands.begin());
        const float half = f.bands[peak] * 0.5f;
        int lo = peak, hi = peak;
        while (lo > 0 && f.bands[lo] > half)
            --lo;
        while (hi < f.bands.size() - 1 && f.bands[hi] > half)
            ++hi;
        const double hzPerBand = (p.maxHz - p.minHz) / p.bandCount;
        return (hi - lo) * hzPerBand;
    };

    // A longer window must give a narrower peak, roughly 2*sampleRate/window.
    const double w4096 = fwhmHz(4096);
    const double w8192 = fwhmHz(8192);
    QVERIFY2(w8192 < w4096 * 0.7,
             qPrintable(QStringLiteral("4096 -> %1 Hz, 8192 -> %2 Hz").arg(w4096).arg(w8192)));

    // The point of zero padding: a window BETWEEN two powers of two must give a
    // width between them, rather than snapping to one of them.
    const double w5486 = fwhmHz(5486);
    QVERIFY2(w5486 < w4096 && w5486 > w8192,
             qPrintable(QStringLiteral("4096 -> %1, 5486 -> %2, 8192 -> %3 Hz")
                            .arg(w4096).arg(w5486).arg(w8192)));

    // And it should land near the theoretical 2*sr/window for a Hann taper.
    const double expected = 2.0 * 48000.0 / 5486.0;
    QVERIFY2(std::abs(w5486 - expected) < 0.5 * expected,
             qPrintable(QStringLiteral("5486 -> %1 Hz, expected near %2")
                            .arg(w5486).arg(expected)));
}

void TestFft::bandNormalizationFlattensWithoutCrushingDynamics()
{
    // Auto-gain should even out the level ACROSS bands while leaving each band's
    // variation over time intact. Referencing bands to their recent peak instead
    // of their average pins everything at full scale, which is the failure this
    // guards against.
    const int sampleRate = 48000;
    const int frames = sampleRate * 3;
    QVector<float> samples(frames * 2);
    for (int i = 0; i < frames; ++i) {
        const double t = double(i) / sampleRate;
        // A loud low tone and a much quieter high tone, both pulsing.
        const double env = 0.5 + 0.5 * std::sin(2.0 * kPi * 2.0 * t);
        const float v = float(env * (0.6 * std::sin(2.0 * kPi * 120.0 * t)
                                     + 0.02 * std::sin(2.0 * kPi * 4000.0 * t)));
        samples[i * 2] = v;
        samples[i * 2 + 1] = v;
    }
    const AudioBuffer buffer(std::move(samples), sampleRate, 2);

    auto run = [&](double amount) {
        SpectrumParams p;
        p.bandCount = 16;
        p.bandNormalization = amount;
        p.normalizationTau = 0.5;
        p.spatialSmoothing = 0.0;
        p.gateDb = 0.0;
        SpectrumAnalyzer a;
        a.setParams(p);

        QVector<QVector<float>> history;
        for (int i = 0; i < 180; ++i) {
            const SpectrumFrame f = a.analyze(buffer, 0.5 + i / 60.0, 1.0 / 60.0);
            if (i >= 90) // let the reference settle first
                history.append(f.bands);
        }
        return history;
    };

    auto spread = [](const QVector<QVector<float>> &h) {
        // Difference between the most and least active band, averaged over time.
        QVector<float> means(h.first().size(), 0.0f);
        for (const auto &f : h)
            for (int b = 0; b < f.size(); ++b)
                means[b] += f[b] / h.size();
        return *std::max_element(means.begin(), means.end())
             - *std::min_element(means.begin(), means.end());
    };

    auto motion = [](const QVector<QVector<float>> &h) {
        // Average per-band variation over time: the dynamics that must survive.
        double total = 0.0;
        for (int b = 0; b < h.first().size(); ++b) {
            double mean = 0.0;
            for (const auto &f : h)
                mean += f[b];
            mean /= h.size();
            double var = 0.0;
            for (const auto &f : h)
                var += (f[b] - mean) * (f[b] - mean);
            total += std::sqrt(var / h.size());
        }
        return total / h.first().size();
    };

    const auto off = run(0.0);
    const auto on = run(0.9);

    QVERIFY2(spread(on) < spread(off),
             qPrintable(QStringLiteral("spread %1 -> %2").arg(spread(off)).arg(spread(on))));
    QVERIFY2(motion(on) > 0.2 * motion(off),
             qPrintable(QStringLiteral("motion %1 -> %2").arg(motion(off)).arg(motion(on))));
}

QTEST_MAIN(TestFft)
#include "tst_fft.moc"
