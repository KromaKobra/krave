#pragma once

#include <QVector>

namespace mv {

/// Iterative in-place radix-2 Cooley-Tukey FFT.
///
/// Twiddle factors and the bit-reversal permutation are computed once in the
/// constructor, so transforming a frame is allocation-free. Instances are const-
/// reentrant: `forward()` writes only through its arguments, so one Fft can be
/// shared by the preview and export analyzers.
class Fft
{
public:
    /// `size` must be a power of two and at least 2.
    explicit Fft(int size);

    int size() const { return m_size; }
    static bool isPowerOfTwo(int n);

    /// In-place complex forward transform. `re` and `im` must each hold size()
    /// elements.
    void forward(float *re, float *im) const;

    /// Transform `size()` real samples and write `size()/2` magnitudes.
    /// `scratchRe` and `scratchIm` must each hold size() elements; they are
    /// supplied by the caller so this stays reentrant and allocation-free.
    void magnitudes(const float *input, float *outMagnitudes,
                    float *scratchRe, float *scratchIm) const;

private:
    int m_size;
    int m_levels;
    QVector<int> m_reversed;
    QVector<float> m_cos;
    QVector<float> m_sin;
};

} // namespace mv
