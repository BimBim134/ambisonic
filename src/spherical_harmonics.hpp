#pragma once

#include <vector>

namespace sde {

// Associated Legendre polynomial P_n^m(x), including the Condon-Shortley phase.
double assoc_legendre(int n, int m, double x);

// Real spherical harmonic in the N3D-ACN convention (matches
// scipy.special.sph_harm + the real conversion used by spaudiopy).
// `azi` is the azimuth, `zen` the colatitude/zenith. Returns a row of
// (order + 1)**2 values in ACN order (n outer, m = -n..n inner).
std::vector<double> real_sh_row(int order, double azi, double zen);

// Collapse the whole stereo -> probe -> SH-encode -> SN3D chain into a single
// constant matrix. Returns a row-major 2 x (order + 1)**2 matrix M such that
// `mix = mid_side @ M`.
std::vector<double> build_encoder_matrix(int order, double spread,
                                         int n_azimuth = 25);

}  // namespace sde
