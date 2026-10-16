#include "spherical_harmonics.hpp"

#include <cmath>

namespace sde
{

    double assoc_legendre(int n, int m, double x)
    {
        if (m < 0 || m > n)
        {
            return 0.0;
        }
        // Standard recurrence with the Condon-Shortley phase folded in.
        double pmm = 1.0;
        if (m > 0)
        {
            const double somx2 = std::sqrt(std::max(0.0, 1.0 - x * x));
            double fact = 1.0;
            for (int i = 1; i <= m; ++i)
            {
                pmm *= -fact * somx2;
                fact += 2.0;
            }
        }
        if (n == m)
        {
            return pmm;
        }
        double pmmp1 = x * (2.0 * m + 1.0) * pmm;
        if (n == m + 1)
        {
            return pmmp1;
        }
        double pll = 0.0;
        for (int ll = m + 2; ll <= n; ++ll)
        {
            pll = ((2.0 * ll - 1.0) * x * pmmp1 -
                   (ll + m - 1.0) * pmm) /
                  (ll - m);
            pmm = pmmp1;
            pmmp1 = pll;
        }
        return pll;
    }

    namespace
    {

        // |Y_n^m(azi, zen)| normalization: sqrt((2n+1)/(4pi) * (n-m)!/(n+m)!),
        // computed in log space for numerical stability at high orders.
        double sh_norm(int n, int m)
        {
            const double pi = 3.14159265358979323846;
            const double log_term = std::log(2.0 * n + 1.0) - std::log(4.0 * pi) +
                                    std::lgamma(static_cast<double>(n - m + 1)) -
                                    std::lgamma(static_cast<double>(n + m + 1));
            return std::exp(0.5 * log_term);
        }

    } // namespace

    void real_sh_row_into(int order, double azi, double zen, double *out)
    {
        const double cos_zen = std::cos(zen);
        int idx = 0;
        for (int n = 0; n <= order; ++n)
        {
            for (int m = -n; m <= n; ++m)
            {
                const int am = std::abs(m);
                const double p = assoc_legendre(n, am, cos_zen);
                const double norm = sh_norm(n, am);
                if (m == 0)
                {
                    out[idx] = norm * p;
                }
                else if (m < 0)
                {
                    const double sign = (am % 2 == 0) ? 1.0 : -1.0;
                    out[idx] = std::sqrt(2.0) * sign * norm * p * std::sin(am * azi);
                }
                else
                {
                    const double sign = (m % 2 == 0) ? 1.0 : -1.0;
                    out[idx] = std::sqrt(2.0) * sign * norm * p * std::cos(m * azi);
                }
                ++idx;
            }
        }
    }

    std::vector<double> real_sh_row(int order, double azi, double zen)
    {
        const int num_channels = (order + 1) * (order + 1);
        std::vector<double> row(static_cast<size_t>(num_channels), 0.0);
        real_sh_row_into(order, azi, zen, row.data());
        return row;
    }

    void build_encoder_matrix_into(int order, double spread, double *matrix_out,
                                   int n_azimuth)
    {
        const int num_channels = (order + 1) * (order + 1);
        const double pi = 3.14159265358979323846;
        const double zen = pi / 2.0;

        for (int i = 0; i < 2 * num_channels; ++i)
        {
            matrix_out[i] = 0.0;
        }

        // sh_row: real SH values for a single azimuth probe (max 64 channels).
        double sh_row[kMaxAmbisonicChannels];

        for (int i = 0; i < n_azimuth; ++i)
        {
            // Azimuth probes spanning -pi/2 .. pi/2 (front half-plane).
            const double azi =
                (n_azimuth == 1) ? 0.0
                                 : -pi / 2.0 + (pi * i) / (n_azimuth - 1);
            real_sh_row_into(order, azi * spread, zen, sh_row);

            // probe_dirs row for this azimuth: [cos(azi), sin(azi)].
            const double px = std::cos(azi);
            const double py = std::sin(azi);

            for (int c = 0; c < num_channels; ++c)
            {
                matrix_out[c] += px * sh_row[c];
                matrix_out[num_channels + c] += py * sh_row[c];
            }
        }

        // SN3D normalize each channel: n_vec = floor(sqrt(c));
        // sn3d = 1/sqrt(2*n_vec+1).
        for (int c = 0; c < num_channels; ++c)
        {
            const int n_vec =
                static_cast<int>(std::floor(std::sqrt(static_cast<double>(c))));
            const double sn3d = 1.0 / std::sqrt(2.0 * n_vec + 1.0);
            matrix_out[c] *= sn3d;
            matrix_out[num_channels + c] *= sn3d;
        }
    }

    std::vector<double> build_encoder_matrix(int order, double spread,
                                             int n_azimuth)
    {
        const int num_channels = (order + 1) * (order + 1);
        std::vector<double> matrix(static_cast<size_t>(2) * num_channels, 0.0);
        build_encoder_matrix_into(order, spread, matrix.data(), n_azimuth);
        return matrix;
    }

} // namespace sde
