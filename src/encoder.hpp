#pragma once

#include <cstddef>
#include <vector>

namespace sde {

constexpr int kDefaultOrder = 3;
constexpr double kDefaultSpread = 1.5;
constexpr double kDefaultGain = 0.1;

// Zero-latency stereo -> SN3D ambisonic encoder. Stateless apart from the
// constant encoder matrix; memoryless, so it accepts any block size.
class ProbeEncoder {
public:
    static constexpr int kNumInputChannels = 2;

    // If `matrix` is non-null it must already hold the 2 x num_channels
    // row-major encoder matrix (lets callers build it once and share it).
    ProbeEncoder(int order, double spread, const std::vector<double>* matrix);

    int order() const { return order_; }
    int num_channels() const { return num_channels_; }

    // Encode `frames` interleaved stereo samples into `frames * num_channels`
    // interleaved doubles.
    void process_block(const float* stereo_interleaved, std::size_t frames,
                       double* out) const;

private:
    int order_;
    int num_channels_;
    std::vector<double> matrix_;
};

// Stateless, signal-independent fixed gain stage.
class FixedGain {
public:
    explicit FixedGain(double gain) : gain_(gain) {}

    void process(double* block, std::size_t count) const {
        if (gain_ == 1.0) {
            return;
        }
        for (std::size_t i = 0; i < count; ++i) {
            block[i] *= gain_;
        }
    }

private:
    double gain_;
};

// Apply a 2 x num_channels encoder matrix (row-major, as produced by
// build_encoder_matrix / build_encoder_matrix_into) to planar stereo input.
//
// `in`  must point to 2 channel pointers (left, right).
// `out` must point to `num_channels` channel pointers.
// `matrix` layout: [0, num_channels) scales mid, [num_channels, 2*num_channels)
// scales side.
//
// Non-allocating and memoryless, so it is safe to call from a real-time thread
// with any block size.
void encode_planar(const float* const* in, float* const* out,
                   std::size_t frames, const double* matrix, int num_channels);

}  // namespace sde
