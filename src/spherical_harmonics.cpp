#include "spherical_harmonics.hpp"

#include <cmath>

namespace sde {

double assoc_legendre(int n, int m, double x) {
    if (m < 0 || m > n) {
        return 0.0;
    }
    // Standard recurrence with the Condon-Shortley phase folded in.
    double pmm = 1.0;
    if (m > 0) {
        const double somx2 = std::sqrt(std::max(0.0, 1.0 - x * x));
        double fact = 1.0;
        for (int i = 1; i <= m; ++i) {
            pmm *= -fact * somx2;
            fact += 2.0;
        }
    }
    if (n == m) {
        return pmm;
    }
    double pmmp1 = x * (2.0 * m + 1.0) * pmm;
    if (n == m + 1) {
        return pmmp1;
    }
    double pll = 0.0;
    for (int ll = m + 2; ll <= n; ++ll) {
        pll = ((2.0 * ll - 1.0) * x * pmmp1 -
               (ll + m - 1.0) * pmm) / (ll - m);
        pmm = pmmp1;
        pmmp1 = pll;
    }
    return pll;
}

namespace {

// |Y_n^m(azi, zen)| normalization: sqrt((2n+1)/(4pi) * (n-m)!/(n+m)!),
// computed in log space for numerical stability at high orders.
double sh_norm(int n, int m) {
    const double pi = 3.14159265358979323846;
    const double log_term = std::log(2.0 * n + 1.0) - std::log(4.0 * pi) +
                            std::lgamma(static_cast<double>(n - m + 1)) -
                            std::lgamma(static_cast<double>(n + m + 1));
    return std::exp(0.5 * log_term);
}

}  // namespace

std::vector<double> real_sh_row(int order, double azi, double zen) {
    const int num_channels = (order + 1) * (order + 1);
    std::vector<double> row(static_cast<size_t>(num_channels), 0.0);

    const double cos_zen = std::cos(zen);
    int idx = 0;
    for (int n = 0; n <= order; ++n) {
        for (int m = -n; m <= n; ++m) {
            const int am = std::abs(m);
            const double p = assoc_legendre(n, am, cos_zen);
            const double norm = sh_norm(n, am);
            if (m == 0) {
                row[static_cast<size_t>(idx)] = norm * p;
            } else if (m < 0) {
                const double sign = (am % 2 == 0) ? 1.0 : -1.0;
                row[static_cast<size_t>(idx)] =
                    std::sqrt(2.0) * sign * norm * p * std::sin(am * azi);
            } else {
                const double sign = (m % 2 == 0) ? 1.0 : -1.0;
                row[static_cast<size_t>(idx)] =
                    std::sqrt(2.0) * sign * norm * p * std::cos(m * azi);
            }
            ++idx;
        }
    }
    return row;
}

std::vector<double> build_encoder_matrix(int order, double spread,
                                         int n_azimuth) {
    const int num_channels = (order + 1) * (order + 1);
    const double pi = 3.14159265358979323846;
    const double zen = pi / 2.0;

    // Azimuth probes spanning -pi/2 .. pi/2 (front half-plane).
    std::vector<double> azimuth(static_cast<size_t>(n_azimuth));
    if (n_azimuth == 1) {
        azimuth[0] = 0.0;
    } else {
        for (int i = 0; i < n_azimuth; ++i) {
            azimuth[static_cast<size_t>(i)] =
                -pi / 2.0 + (pi * i) / (n_azimuth - 1);
        }
    }

    // probe_dirs: 2 x n_azimuth, rows [cos(az), sin(az)].
    std::vector<double> probe_dirs(static_cast<size_t>(2 * n_azimuth));
    for (int i = 0; i < n_azimuth; ++i) {
        probe_dirs[static_cast<size_t>(i)] =
            std::cos(azimuth[static_cast<size_t>(i)]);
        probe_dirs[static_cast<size_t>(n_azimuth + i)] =
            std::sin(azimuth[static_cast<size_t>(i)]);
    }

    // sh_weights: n_azimuth x num_channels.
    std::vector<double> sh_weights(
        static_cast<size_t>(n_azimuth) * num_channels);
    for (int i = 0; i < n_azimuth; ++i) {
        const auto row = real_sh_row(
            order, azimuth[static_cast<size_t>(i)] * spread, zen);
        for (int c = 0; c < num_channels; ++c) {
            sh_weights[static_cast<size_t>(i) * num_channels + c] =
                row[static_cast<size_t>(c)];
        }
    }

    // M = probe_dirs @ sh_weights, then SN3D normalize each channel.
    std::vector<double> matrix(static_cast<size_t>(2) * num_channels, 0.0);
    for (int c = 0; c < num_channels; ++c) {
        for (int r = 0; r < 2; ++r) {
            double acc = 0.0;
            for (int i = 0; i < n_azimuth; ++i) {
                acc += probe_dirs[static_cast<size_t>(r * n_azimuth + i)] *
                       sh_weights[static_cast<size_t>(i) * num_channels + c];
            }
            // n_vec = floor(sqrt(channel_index)); sn3d = 1/sqrt(2*n_vec+1).
            const int n_vec =
                static_cast<int>(std::floor(std::sqrt(static_cast<double>(c))));
            const double sn3d = 1.0 / std::sqrt(2.0 * n_vec + 1.0);
            matrix[static_cast<size_t>(r * num_channels + c)] = acc * sn3d;
        }
    }
    return matrix;
}

}  // namespace sde
