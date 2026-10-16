#pragma once

#include <vector>

namespace sde {

constexpr int kMaxAmbisonicOrder = 7;
constexpr int kMaxAmbisonicChannels = (kMaxAmbisonicOrder + 1) *
                                      (kMaxAmbisonicOrder + 1);

// Associated Legendre polynomial P_n^m(x), including the Condon-Shortley phase.
double assoc_legendre(int n, int m, double x);

// Real spherical harmonic in the N3D-ACN convention (matches
// scipy.special.sph_harm + the real conversion used by spaudiopy).
// `azi` is the azimuth, `zen` the colatitude/zenith. Returns a row of
// (order + 1)**2 values in ACN order (n outer, m = -n..n inner).
std::vector<double> real_sh_row(int order, double azi, double zen);

// Non-allocating variant of real_sh_row: writes (order + 1)**2 doubles into
// `out`, which must hold at least (order + 1)**2 elements. Safe to call from a
// real-time thread.
void real_sh_row_into(int order, double azi, double zen, double* out);

// Collapse the whole stereo -> probe -> SH-encode -> SN3D chain into a single
// constant matrix. Returns a row-major 2 x (order + 1)**2 matrix M such that
// `mix = mid_side @ M`.
std::vector<double> build_encoder_matrix(int order, double spread,
                                         int n_azimuth = 25);

// Non-allocating variant of build_encoder_matrix: writes the 2 x (order + 1)**2
// row-major matrix into `matrix_out`, which must hold at least
// 2 * (order + 1)**2 elements. Safe to call from a real-time thread.
void build_encoder_matrix_into(int order, double spread, double* matrix_out,
                               int n_azimuth = 25);

}  // namespace sde
