#include "Fft.h"

#include <cmath>

namespace mv {
namespace {
// MSVC does not define M_PI without _USE_MATH_DEFINES; keep our own.
constexpr double kPi = 3.14159265358979323846;
} // namespace

bool Fft::isPowerOfTwo(int n)
{
    return n >= 2 && (n & (n - 1)) == 0;
}

Fft::Fft(int size)
    : m_size(isPowerOfTwo(size) ? size : 2)
    , m_levels(0)
{
    while ((1 << m_levels) < m_size)
        ++m_levels;

    m_reversed.resize(m_size);
    for (int i = 0; i < m_size; ++i) {
        int r = 0;
        for (int b = 0; b < m_levels; ++b)
            if (i & (1 << b))
                r |= 1 << (m_levels - 1 - b);
        m_reversed[i] = r;
    }

    // Half a period is all a radix-2 butterfly ever needs.
    m_cos.resize(m_size / 2);
    m_sin.resize(m_size / 2);
    for (int i = 0; i < m_size / 2; ++i) {
        const double angle = -2.0 * kPi * double(i) / double(m_size);
        m_cos[i] = float(std::cos(angle));
        m_sin[i] = float(std::sin(angle));
    }
}

void Fft::forward(float *re, float *im) const
{
    // Bit-reversal permutation.
    for (int i = 0; i < m_size; ++i) {
        const int j = m_reversed[i];
        if (j > i) {
            std::swap(re[i], re[j]);
            std::swap(im[i], im[j]);
        }
    }

    for (int len = 2; len <= m_size; len <<= 1) {
        const int half = len / 2;
        const int step = m_size / len;
        for (int i = 0; i < m_size; i += len) {
            for (int k = 0; k < half; ++k) {
                const float wr = m_cos[k * step];
                const float wi = m_sin[k * step];
                const int a = i + k;
                const int b = a + half;
                const float tr = re[b] * wr - im[b] * wi;
                const float ti = re[b] * wi + im[b] * wr;
                re[b] = re[a] - tr;
                im[b] = im[a] - ti;
                re[a] += tr;
                im[a] += ti;
            }
        }
    }
}

void Fft::magnitudes(const float *input, float *outMagnitudes,
                     float *scratchRe, float *scratchIm) const
{
    for (int i = 0; i < m_size; ++i) {
        scratchRe[i] = input[i];
        scratchIm[i] = 0.0f;
    }

    forward(scratchRe, scratchIm);

    const int bins = m_size / 2;
    for (int i = 0; i < bins; ++i)
        outMagnitudes[i] = std::hypot(scratchRe[i], scratchIm[i]);
}

} // namespace mv
