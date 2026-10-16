#include "encoder.hpp"

#include "spherical_harmonics.hpp"

namespace sde
{

    ProbeEncoder::ProbeEncoder(int order, double spread,
                               const std::vector<double> *matrix)
        : order_(order), num_channels_((order + 1) * (order + 1))
    {
        if (matrix != nullptr)
        {
            matrix_ = *matrix;
        }
        else
        {
            matrix_ = build_encoder_matrix(order, spread);
        }
    }

    void ProbeEncoder::process_block(const float *stereo_interleaved,
                                     std::size_t frames, double *out) const
    {
        const std::size_t channels = static_cast<std::size_t>(num_channels_);
        for (std::size_t f = 0; f < frames; ++f)
        {
            const double left = static_cast<double>(stereo_interleaved[2 * f]);
            const double right = static_cast<double>(stereo_interleaved[2 * f + 1]);
            const double mid = (left + right) / 2.0;
            const double side = (left - right) / 2.0;
            double *dst = out + f * channels;
            for (std::size_t c = 0; c < channels; ++c)
            {
                dst[c] = mid * matrix_[c] + side * matrix_[channels + c];
            }
        }
    }

    void encode_planar(const float *const *in, float *const *out,
                       std::size_t frames, const double *matrix, int num_channels)
    {
        const float *left_ch = in[0];
        const float *right_ch = in[1];
        const int channels = num_channels;
        const double *mid_gains = matrix;
        const double *side_gains = matrix + channels;

        for (std::size_t f = 0; f < frames; ++f)
        {
            const double left = static_cast<double>(left_ch[f]);
            const double right = static_cast<double>(right_ch[f]);
            const double mid = (left + right) / 2.0;
            const double side = (left - right) / 2.0;
            for (int c = 0; c < channels; ++c)
            {
                out[c][f] =
                    static_cast<float>(mid * mid_gains[c] + side * side_gains[c]);
            }
        }
    }

} // namespace sde
