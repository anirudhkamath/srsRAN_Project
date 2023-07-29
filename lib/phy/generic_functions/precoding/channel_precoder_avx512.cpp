/*
 *
 * Copyright 2021-2023 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#include "channel_precoder_avx512.h"
#include <immintrin.h>

using namespace srsran;

// Size of an AVX512 register in complex numbers with 32-bit floating point precision.
static constexpr unsigned AVX512_CF_SIZE = 8;

// Representation of a set of complex numbers using a pair of AVX512 registers, for the real and imaginary parts.
struct simd_cf_t {
  __m512 re;
  __m512 im;

  // Sets the registers using a complex constant.
  simd_cf_t& operator=(const cf_t a)
  {
    re = _mm512_set1_ps(a.real());
    im = _mm512_set1_ps(a.imag());
    return *this;
  }
};

// Type to hold a set of complex numbers using an AVX512 register, with interleaved real and imaginary parts.
using simd_cf_interleaved = __m512;

// Multiplication operator for the precoding weights.
simd_cf_interleaved operator*(const simd_cf_interleaved& re, const simd_cf_t& weight)
{
  return _mm512_fmaddsub_ps(re, weight.re, _mm512_mul_ps(_mm512_shuffle_ps(re, re, 0xb1), weight.im));
}

void channel_precoder_avx512::apply_precoding_port(span<cf_t>              port_re,
                                                   const re_buffer_reader& input_re,
                                                   span<const cf_t>        port_weights)
{
  unsigned nof_re     = input_re.get_nof_re();
  unsigned nof_layers = input_re.get_nof_slices();

  // Create a list of all the input layer RE views.
  std::array<span<const cf_t>, precoding_constants::MAX_NOF_LAYERS> layer_re_view_list;

  // Array holding SIMD registers initialized with the precoding weights.
  std::array<simd_cf_t, precoding_constants::MAX_NOF_LAYERS> port_weights_simd;

  for (unsigned i_layer = 0; i_layer != nof_layers; ++i_layer) {
    // Fill the RE view list.
    layer_re_view_list[i_layer] = input_re.get_slice(i_layer);

    // Set the SIMD registers with the port weights.
    port_weights_simd[i_layer] = port_weights[i_layer];
  }

  unsigned i_re = 0;

  for (unsigned max_re = (nof_re / AVX512_CF_SIZE) * AVX512_CF_SIZE; i_re != max_re; i_re += AVX512_CF_SIZE) {
    // Load layer 0 RE.
    simd_cf_interleaved re_in = _mm512_loadu_ps(reinterpret_cast<const float*>(&layer_re_view_list[0][i_re]));

    // Multiply the input REs by the precoding coefficient.
    simd_cf_interleaved re_out = re_in * port_weights_simd[0];

    for (unsigned i_layer = 1; i_layer != nof_layers; ++i_layer) {
      // Load layer RE.
      re_in = _mm512_loadu_ps(reinterpret_cast<const float*>(&layer_re_view_list[i_layer][i_re]));

      // Multiply the input REs by the coefficient and add to the contributions of other layers.
      re_out += re_in * port_weights_simd[i_layer];
    }

    // Store.
    _mm512_storeu_ps(reinterpret_cast<float*>(&port_re[i_re]), re_out);
  }

  for (; i_re != nof_re; ++i_re) {
    port_re[i_re] = layer_re_view_list[0][i_re] * port_weights[0];

    for (unsigned i_layer = 1; i_layer != nof_layers; ++i_layer) {
      port_re[i_re] += layer_re_view_list[i_layer][i_re] * port_weights[i_layer];
    }
  }
}