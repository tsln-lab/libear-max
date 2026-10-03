/// @file
/// @brief   Sample-rate conversion for streamed audio (mc.ear.play~): a
///          windowed-sinc polyphase resampler (Kaiser window, 256 phases with
///          linear interpolation of the coefficients) that converts
///          interleaved frames from one rate to another as they arrive. For
///          downsampling the low-pass sits at the output's Nyquist, so the
///          kernel is as many input samples longer as the ratio demands.
///
///          The output frame at input position x = n + f is the sum over
///          k = -half+1 .. half of h(k - f) * in[n + k]; the history needed
///          before the start position is primed by the caller (`prime`) so
///          that the first output frame corresponds exactly to the start
///          position, and `flush` drains the tail at the end of the input.
/// @license Use of this source code is governed by the MIT License found in the License.md file.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace earmax::admio {

class resampler {
public:
    static constexpr size_t k_phases = 256;
    static constexpr size_t k_base_half = 32;    ///< taps per side at the band-limiting rate
    static constexpr double k_max_ratio = 8.0;   ///< input rate / output rate at most (and at least 1/8)
    static constexpr double k_kaiser_beta = 8.6; ///< about 90 dB of stopband attenuation
    static constexpr double k_pi = 3.14159265358979323846;

    /// Configure for `in_rate` -> `out_rate` with `channels` interleaved
    /// channels; the history is cleared. A ratio of 1 copies the input.
    void configure(uint32_t in_rate, uint32_t out_rate, size_t channels)
    {
        if (in_rate == m_in_rate && out_rate == m_out_rate && channels == m_channels && !m_table.empty() == !m_identity) {
            reset();    // the same conversion: the table is kept
            return;
        }
        m_in_rate = in_rate;
        m_out_rate = out_rate;
        m_channels = channels;
        m_step = (in_rate > 0 && out_rate > 0) ? static_cast<double>(in_rate) / out_rate : 1.0;
        m_step = std::max(1.0 / k_max_ratio, std::min(k_max_ratio, m_step));
        m_identity = std::abs(m_step - 1.0) < 1e-12;
        if (m_identity) {
            m_half = 0;
            m_table.clear();
        }
        else {
            // band-limit to the lower of the two Nyquist frequencies, in
            // cycles per input sample; a lower cutoff needs a longer kernel
            const double cutoff = 0.5 * std::min(1.0, 1.0 / m_step);
            m_half = static_cast<size_t>(std::ceil(k_base_half / (2.0 * cutoff)));
            build_table(cutoff);
        }
        reset();
    }

    size_t channels() const
    {
        return m_channels;
    }

    /// Input frames per output frame.
    double step() const
    {
        return m_step;
    }

    /// Input frames the kernel reaches before the start position: what
    /// `prime` wants.
    size_t history() const
    {
        return m_half > 0 ? m_half - 1 : 0;
    }

    /// Forget the input; the next `prime` or `process` starts afresh (a
    /// missing prime is as if silence preceded the input).
    void reset()
    {
        m_buffer.assign((history()) * m_channels, 0.0f);
        m_pos = static_cast<double>(history());
        m_in_total = 0;
        m_out_total = 0;
    }

    /// Load the `history()` input frames that precede the start position
    /// (fewer: the rest is taken as silence before them).
    void prime(const float* frames, size_t count)
    {
        const size_t need = history();
        const size_t take = std::min(count, need);
        std::fill(m_buffer.begin(), m_buffer.end(), 0.0f);
        if (take > 0) {
            std::copy(frames + (count - take) * m_channels, frames + count * m_channels,
                      m_buffer.begin() + static_cast<long>((need - take) * m_channels));
        }
    }

    /// Convert `count` input frames, appending the output frames that can
    /// be completed to `out` (interleaved). Returns the frames appended.
    size_t process(const float* frames, size_t count, std::vector<float>& out)
    {
        if (m_channels == 0) {
            return 0;
        }
        if (m_identity) {
            out.insert(out.end(), frames, frames + count * m_channels);
            m_in_total += count;
            m_out_total += count;
            return count;
        }
        m_buffer.insert(m_buffer.end(), frames, frames + count * m_channels);
        m_in_total += count;
        return drain(out);
    }

    /// Feed silence for the kernel's reach so the last input frames come
    /// out; appends to `out` and returns the frames appended.
    size_t flush(std::vector<float>& out)
    {
        if (m_identity || m_channels == 0) {
            return 0;
        }
        const std::vector<float> zeros(m_half * m_channels, 0.0f);
        m_buffer.insert(m_buffer.end(), zeros.begin(), zeros.end());
        return drain(out);
    }

    /// Output frames produced so far, since the reset.
    uint64_t produced() const
    {
        return m_out_total;
    }

    /// Output frames that `count` more input frames would let `process`
    /// complete at most (for sizing the output buffer).
    size_t output_bound(size_t count) const
    {
        return static_cast<size_t>(std::ceil(static_cast<double>(count) / m_step)) + 2;
    }

private:
    static double bessel_i0(double x)
    {
        // power series; converges quickly for the window's arguments
        double sum = 1.0, term = 1.0;
        const double q = x * x / 4.0;
        for (int k = 1; k < 60; ++k) {
            term *= q / (static_cast<double>(k) * k);
            sum += term;
            if (term < sum * 1e-15) {
                break;
            }
        }
        return sum;
    }

    /// kernel value at offset `x` input samples
    double kernel(double x, double cutoff) const
    {
        const double reach = static_cast<double>(m_half);
        if (x <= -reach || x >= reach) {
            return 0.0;
        }
        const double sinc = x == 0.0 ? 1.0 : std::sin(2.0 * k_pi * cutoff * x) / (2.0 * k_pi * cutoff * x);
        const double r = x / reach;
        const double window = bessel_i0(k_kaiser_beta * std::sqrt(std::max(0.0, 1.0 - r * r))) / bessel_i0(k_kaiser_beta);
        return 2.0 * cutoff * sinc * window;
    }

    void build_table(double cutoff)
    {
        const size_t taps = 2 * m_half;
        m_table.assign((k_phases + 1) * taps, 0.0f);
        for (size_t p = 0; p <= k_phases; ++p) {
            const double f = static_cast<double>(p) / k_phases;
            double sum = 0.0;
            for (size_t j = 0; j < taps; ++j) {
                const double k = static_cast<double>(j) - static_cast<double>(m_half) + 1.0;
                const double v = kernel(k - f, cutoff);
                m_table[p * taps + j] = static_cast<float>(v);
                sum += v;
            }
            // unity gain for DC at every phase
            if (sum != 0.0) {
                for (size_t j = 0; j < taps; ++j) {
                    m_table[p * taps + j] = static_cast<float>(m_table[p * taps + j] / sum);
                }
            }
        }
    }

    /// produce every output frame the buffered input allows
    size_t drain(std::vector<float>& out)
    {
        const size_t taps = 2 * m_half;
        const size_t buffered = m_buffer.size() / m_channels;
        size_t produced = 0;
        std::vector<double> acc(m_channels);
        while (true) {
            const double n_d = std::floor(m_pos);
            const size_t n = static_cast<size_t>(n_d);
            if (n + m_half >= buffered) {
                break;    // the kernel would reach past the input
            }
            const double f = m_pos - n_d;
            const double phase = f * k_phases;
            const size_t p = static_cast<size_t>(phase);
            const float mix = static_cast<float>(phase - static_cast<double>(p));
            const float* c0 = &m_table[p * taps];
            const float* c1 = &m_table[(p + 1) * taps];
            std::fill(acc.begin(), acc.end(), 0.0);
            const float* in = &m_buffer[(n + 1 - m_half) * m_channels];
            for (size_t j = 0; j < taps; ++j) {
                const float c = c0[j] + (c1[j] - c0[j]) * mix;
                for (size_t ch = 0; ch < m_channels; ++ch) {
                    acc[ch] += c * in[j * m_channels + ch];
                }
            }
            for (size_t ch = 0; ch < m_channels; ++ch) {
                out.push_back(static_cast<float>(acc[ch]));
            }
            ++produced;
            m_pos += m_step;
        }
        m_out_total += produced;
        // drop the input the kernel no longer reaches (in batches)
        const size_t keep_from = static_cast<size_t>(std::floor(m_pos)) + 1 - std::min(static_cast<size_t>(std::floor(m_pos)) + 1, m_half);
        if (keep_from >= 4096) {
            m_buffer.erase(m_buffer.begin(), m_buffer.begin() + static_cast<long>(keep_from * m_channels));
            m_pos -= static_cast<double>(keep_from);
        }
        return produced;
    }

    uint32_t m_in_rate{ 0 };
    uint32_t m_out_rate{ 0 };
    size_t m_channels{ 0 };
    double m_step{ 1.0 };
    bool m_identity{ true };
    size_t m_half{ 0 };
    std::vector<float> m_table;     ///< (k_phases + 1) rows of 2 * m_half coefficients
    std::vector<float> m_buffer;    ///< interleaved input, from the oldest frame the kernel reaches
    double m_pos{ 0.0 };            ///< position of the next output frame in the buffer (input frames)
    uint64_t m_in_total{ 0 };
    uint64_t m_out_total{ 0 };
};

} // namespace earmax::admio
