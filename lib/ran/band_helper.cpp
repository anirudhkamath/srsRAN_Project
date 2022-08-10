/*
 *
 * Copyright 2013-2022 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#include "srsgnb/ran/band_helper.h"
#include "srsgnb/ran/bs_channel_bandwidth.h"
#include "srsgnb/ran/duplex_mode.h"
#include "srsgnb/ran/ssb_mapping.h"
#include "srsgnb/ran/subcarrier_spacing.h"

using namespace srsgnb;

using namespace band_helper;

// Possible values of delta f_raster in Table 5.4.2.3-1 and Table 5.4.2.3-2
enum class delta_freq_raster {
  DEFAULT = 0, // for bands with 2 possible values for delta_f_raster (e.g. 15 and 30 kHz), the lower is chosen
  KHz15,
  KHz30,
  KHz60,
  KHz100,
  KHz120
};

// NR operating band and DL ARFCN lower-bound and upper-bound. See Table 5.4.2.3-1 in TS 38.104.
struct nr_band {
  nr_band_fr1       band;
  delta_freq_raster delta_f_rast;
  uint32_t          ul_nref_first;
  uint32_t          ul_nref_step;
  uint32_t          ul_nref_last;
  uint32_t          dl_nref_first;
  uint32_t          dl_nref_step;
  uint32_t          dl_nref_last;
};

// From Table 5.4.2.3-1 in TS 38.104, this is the number of NR FR1 bands that has a DL allocated band (FDD, TDD or SDL).
// NOTE: Band 41 has two different Freq raster, we only consider raster 15kHz.
const uint32_t nof_nr_DL_bands_fr1 = 36;

// Table with NR operating FR1 band and related ARFCN lower-bound and upper-bound. See Table 5.4.2.3-1 in TS 38.104.
// NOTE: It only includes FDD, TDD, and SDL bands.
// NOTE: Band 2 is a subset of band 25
static constexpr std::array<nr_band, nof_nr_DL_bands_fr1> nr_band_table_fr1 = {{
    // clang-format off
    {nr_band_fr1::n1,  delta_freq_raster::KHz100, 384000, 20, 396000, 422000, 20, 434000},
    {nr_band_fr1::n2,  delta_freq_raster::KHz100, 370000, 20, 382000, 386000, 20, 398000},
    {nr_band_fr1::n3,  delta_freq_raster::KHz100, 342000, 20, 357000, 361000, 20, 376000},
    {nr_band_fr1::n5,  delta_freq_raster::KHz100, 164800, 20, 169800, 173800, 20, 178800},
    {nr_band_fr1::n7,  delta_freq_raster::KHz100, 500000, 20, 514000, 524000, 20, 538000},
    {nr_band_fr1::n8,  delta_freq_raster::KHz100, 176000, 20, 183000, 185000, 20, 192000},
    {nr_band_fr1::n12, delta_freq_raster::KHz100, 139800, 20, 143200, 145800, 20, 149200},
    {nr_band_fr1::n20, delta_freq_raster::KHz100, 166400, 20, 172400, 158200, 20, 164200},
    {nr_band_fr1::n25, delta_freq_raster::KHz100, 370000, 20, 383000, 386000, 20, 399000},
    {nr_band_fr1::n28, delta_freq_raster::KHz100, 140600, 20, 149600, 151600, 20, 160600},
    {nr_band_fr1::n34, delta_freq_raster::KHz100, 402000, 20, 405000, 402000, 20, 405000},
    {nr_band_fr1::n38, delta_freq_raster::KHz100, 514000, 20, 524000, 514000, 20, 524000},
    {nr_band_fr1::n39, delta_freq_raster::KHz100, 376000, 20, 384000, 376000, 20, 384000},
    {nr_band_fr1::n40, delta_freq_raster::KHz100, 460000, 20, 480000, 460000, 20, 480000},
    {nr_band_fr1::n41, delta_freq_raster::KHz15,  499200,  3, 537999, 499200,  3, 537999},
    {nr_band_fr1::n41, delta_freq_raster::KHz30,  499200,  6, 537996, 499200,  6, 537996},
    {nr_band_fr1::n50, delta_freq_raster::KHz100, 286400, 20, 303400, 286400, 20, 303400},
    {nr_band_fr1::n51, delta_freq_raster::KHz100, 285400, 20, 286400, 285400, 20, 286400},
    {nr_band_fr1::n66, delta_freq_raster::KHz100, 342000, 20, 356000, 422000, 20, 440000},
    {nr_band_fr1::n70, delta_freq_raster::KHz100, 339000, 20, 342000, 399000, 20, 404000},
    {nr_band_fr1::n71, delta_freq_raster::KHz100, 132600, 20, 139600, 123400, 20, 130400},
    {nr_band_fr1::n74, delta_freq_raster::KHz100, 285400, 20, 294000, 295000, 20, 303600},
    {nr_band_fr1::n75, delta_freq_raster::KHz100,      0,  0,      0, 286400, 20, 303400},
    {nr_band_fr1::n76, delta_freq_raster::KHz100,      0,  0,      0, 285400, 20, 286400},
    {nr_band_fr1::n77, delta_freq_raster::KHz15,  620000,  1, 680000, 620000,  1, 680000},
    {nr_band_fr1::n77, delta_freq_raster::KHz30,  620000,  2, 680000, 620000,  2, 680000},
    {nr_band_fr1::n78, delta_freq_raster::KHz15,  620000,  1, 653333, 620000,  1, 653333},
    {nr_band_fr1::n78, delta_freq_raster::KHz30,  620000,  2, 653332, 620000,  2, 653332},
    {nr_band_fr1::n79, delta_freq_raster::KHz15,  693334,  2, 733333, 693334,  2, 733333},
    {nr_band_fr1::n79, delta_freq_raster::KHz30,  693334,  2, 733332, 693334,  2, 733332},
    {nr_band_fr1::n80, delta_freq_raster::KHz100, 342000,  20, 357000,     0,  0,      0},
    {nr_band_fr1::n81, delta_freq_raster::KHz100, 176000,  20, 183000,     0,  0,      0},
    {nr_band_fr1::n82, delta_freq_raster::KHz100, 166400,  20, 172400,     0,  0,      0},
    {nr_band_fr1::n83, delta_freq_raster::KHz100, 140600,  20, 149600,     0,  0,      0},
    {nr_band_fr1::n84, delta_freq_raster::KHz100, 384000,  20, 396000,     0,  0,      0},
    {nr_band_fr1::n86, delta_freq_raster::KHz100, 342000,  20, 356000,     0,  0,      0}
    // clang-format on
}};

// NR operating band in FR1 with related Duplex Mode. See TS 38.101-1 Table 5.2-1.
struct nr_operating_band {
  nr_band_fr1 band;
  duplex_mode duplex;
};
static const uint32_t                                                     nof_nr_operating_band_fr1 = 32;
static constexpr std::array<nr_operating_band, nof_nr_operating_band_fr1> nr_operating_bands_fr1    = {{
    // clang-format off
    {nr_band_fr1::n1,  duplex_mode::FDD},
    {nr_band_fr1::n2,  duplex_mode::FDD},
    {nr_band_fr1::n3,  duplex_mode::FDD},
    {nr_band_fr1::n5,  duplex_mode::FDD},
    {nr_band_fr1::n7,  duplex_mode::FDD},
    {nr_band_fr1::n8,  duplex_mode::FDD},
    {nr_band_fr1::n12, duplex_mode::FDD},
    {nr_band_fr1::n20, duplex_mode::FDD},
    {nr_band_fr1::n25, duplex_mode::FDD},
    {nr_band_fr1::n28, duplex_mode::FDD},
    {nr_band_fr1::n34, duplex_mode::TDD},
    {nr_band_fr1::n38, duplex_mode::TDD},
    {nr_band_fr1::n39, duplex_mode::TDD},
    {nr_band_fr1::n40, duplex_mode::TDD},
    {nr_band_fr1::n41, duplex_mode::TDD},
    {nr_band_fr1::n50, duplex_mode::TDD},
    {nr_band_fr1::n51, duplex_mode::TDD},
    {nr_band_fr1::n66, duplex_mode::FDD},
    {nr_band_fr1::n70, duplex_mode::FDD},
    {nr_band_fr1::n71, duplex_mode::FDD},
    {nr_band_fr1::n74, duplex_mode::FDD},
    {nr_band_fr1::n75, duplex_mode::TDD},
    {nr_band_fr1::n76, duplex_mode::TDD},
    {nr_band_fr1::n77, duplex_mode::TDD},
    {nr_band_fr1::n78, duplex_mode::TDD},
    {nr_band_fr1::n79, duplex_mode::TDD},
    {nr_band_fr1::n80, duplex_mode::SUL},
    {nr_band_fr1::n81, duplex_mode::SUL},
    {nr_band_fr1::n82, duplex_mode::SUL},
    {nr_band_fr1::n83, duplex_mode::SUL},
    {nr_band_fr1::n84, duplex_mode::SUL},
    {nr_band_fr1::n86, duplex_mode::SUL}
    // clang-format on
}};

// NR operating band in FR1 with SSB Subcarrier Spacing and SSB pattern case. See TS 38.101-1 Table 5.2-1.
struct nr_band_ssb_scs_case {
  nr_band_fr1        band;
  subcarrier_spacing scs;
  ssb_pattern_case   pattern;
};
static const uint32_t                                                   nof_nr_ssb_bands_fr1           = 32;
static constexpr std::array<nr_band_ssb_scs_case, nof_nr_ssb_bands_fr1> nr_ssb_band_scs_case_table_fr1 = {{
    // clang-format off
    {nr_band_fr1::n1,  subcarrier_spacing::kHz15, ssb_pattern_case::A},
    {nr_band_fr1::n2,  subcarrier_spacing::kHz15, ssb_pattern_case::A},
    {nr_band_fr1::n3,  subcarrier_spacing::kHz15, ssb_pattern_case::A},
    {nr_band_fr1::n5,  subcarrier_spacing::kHz15, ssb_pattern_case::A},
    {nr_band_fr1::n5,  subcarrier_spacing::kHz30, ssb_pattern_case::B},
    {nr_band_fr1::n7,  subcarrier_spacing::kHz15, ssb_pattern_case::A},
    {nr_band_fr1::n8,  subcarrier_spacing::kHz15, ssb_pattern_case::A},
    {nr_band_fr1::n12, subcarrier_spacing::kHz15, ssb_pattern_case::A},
    {nr_band_fr1::n20, subcarrier_spacing::kHz15, ssb_pattern_case::A},
    {nr_band_fr1::n25, subcarrier_spacing::kHz15, ssb_pattern_case::A},
    {nr_band_fr1::n28, subcarrier_spacing::kHz15, ssb_pattern_case::A},
    {nr_band_fr1::n34, subcarrier_spacing::kHz15, ssb_pattern_case::A},
    {nr_band_fr1::n34, subcarrier_spacing::kHz30, ssb_pattern_case::C},
    {nr_band_fr1::n38, subcarrier_spacing::kHz15, ssb_pattern_case::A},
    {nr_band_fr1::n38, subcarrier_spacing::kHz30, ssb_pattern_case::C},
    {nr_band_fr1::n39, subcarrier_spacing::kHz15, ssb_pattern_case::A},
    {nr_band_fr1::n39, subcarrier_spacing::kHz30, ssb_pattern_case::C},
    {nr_band_fr1::n40, subcarrier_spacing::kHz30, ssb_pattern_case::C},
    {nr_band_fr1::n41, subcarrier_spacing::kHz15, ssb_pattern_case::A},
    {nr_band_fr1::n41, subcarrier_spacing::kHz30, ssb_pattern_case::C},
    {nr_band_fr1::n50, subcarrier_spacing::kHz30, ssb_pattern_case::C},
    {nr_band_fr1::n51, subcarrier_spacing::kHz15, ssb_pattern_case::A},
    {nr_band_fr1::n66, subcarrier_spacing::kHz15, ssb_pattern_case::A},
    {nr_band_fr1::n66, subcarrier_spacing::kHz30, ssb_pattern_case::B},
    {nr_band_fr1::n70, subcarrier_spacing::kHz15, ssb_pattern_case::A},
    {nr_band_fr1::n71, subcarrier_spacing::kHz15, ssb_pattern_case::A},
    {nr_band_fr1::n74, subcarrier_spacing::kHz15, ssb_pattern_case::A},
    {nr_band_fr1::n75, subcarrier_spacing::kHz15, ssb_pattern_case::A},
    {nr_band_fr1::n76, subcarrier_spacing::kHz15, ssb_pattern_case::A},
    {nr_band_fr1::n77, subcarrier_spacing::kHz30, ssb_pattern_case::C},
    {nr_band_fr1::n78, subcarrier_spacing::kHz30, ssb_pattern_case::C},
    {nr_band_fr1::n79, subcarrier_spacing::kHz30, ssb_pattern_case::C},
    // clang-format on
}};

struct nr_raster_params {
  double   freq_range_start;
  double   freq_range_end;
  double   delta_F_global_kHz;
  double   F_REF_Offs_MHz;
  uint32_t N_REF_Offs;
  uint32_t N_REF_min;
  uint32_t N_REF_max;

  bool operator==(const nr_raster_params& rhs) const
  {
    return freq_range_start == rhs.freq_range_start && freq_range_end == rhs.freq_range_end &&
           delta_F_global_kHz == rhs.delta_F_global_kHz && F_REF_Offs_MHz == rhs.F_REF_Offs_MHz &&
           N_REF_Offs == rhs.N_REF_Offs && N_REF_min == rhs.N_REF_min && N_REF_max == rhs.N_REF_max;
  }
};

static const uint32_t                            max_nr_arfcn = 3279165;
static constexpr std::array<nr_raster_params, 3> nr_fr_params = {{
    // clang-format off
    // Frequency range 0 - 3000 MHz
    {0, 3000, 5, 0.0, 0, 0, 599999},
    // Frequency range 3000 - 24250 MHz
    {3000, 24250, 15, 3000.0, 600000, 600000, 2016666},
    // Frequency range 24250 - 100000 MHz
    {24250, 100000, 60, 24250.08, 2016667, 2016667, max_nr_arfcn}
    // clang-format on
}};

struct n_rb_per_scs {
  bs_channel_bandwidth_fr1 bw;
  unsigned                 n_rb_15kHz;
  unsigned                 n_rb_30kHz;
  unsigned                 n_rb_60kHz;
};

// This implements Table 5.3.2-1 in TS 38.104. Value N_RB = 0 represent N/A.
static std::array<n_rb_per_scs, 13> tx_bw_config_fr1 = {{
    // clang-format off
    // BW = 5MHz.
    {bs_channel_bandwidth_fr1::MHz5, 25, 11, 0},
    // BW = 10MHz.
    {bs_channel_bandwidth_fr1::MHz10, 52, 24, 11},
    // BW = 15MHz.
    {bs_channel_bandwidth_fr1::MHz15, 79, 38, 18},
    // BW = 20MHz.
    {bs_channel_bandwidth_fr1::MHz20, 106, 51, 24},
    // BW = 25MHz.
    {bs_channel_bandwidth_fr1::MHz25, 133, 65, 31},
    // BW = 30MHz.
    {bs_channel_bandwidth_fr1::MHz30, 160, 78, 38},
    // BW = 40MHz.
    {bs_channel_bandwidth_fr1::MHz40, 216, 106, 51},
    // BW = 50MHz.
    {bs_channel_bandwidth_fr1::MHz50, 270, 133, 65},
    // BW = 60MHz.
    {bs_channel_bandwidth_fr1::MHz60, 0, 162, 79},
    // BW = 70MHz.
    {bs_channel_bandwidth_fr1::MHz70, 0, 189, 93},
    // BW = 80MHz.
    {bs_channel_bandwidth_fr1::MHz80, 0, 217, 107},
    // BW = 90MHz.
    {bs_channel_bandwidth_fr1::MHz90, 0, 245, 121},
    // BW = 100MHz.
    {bs_channel_bandwidth_fr1::MHz100, 0, 273, 135}
    // clang-format on
}};

/// Helper to calculate F_REF according to Table 5.4.2.1-1.
static nr_raster_params get_raster_params(uint32_t nr_arfcn)
{
  for (const nr_raster_params& fr : nr_fr_params) {
    if (nr_arfcn >= fr.N_REF_min && nr_arfcn <= fr.N_REF_max) {
      return fr;
    }
  }
  // Return empty params.
  return {};
}

static nr_raster_params get_raster_params(double freq)
{
  for (const nr_raster_params& fr : nr_fr_params) {
    if (freq >= fr.freq_range_start * 1e6 && freq <= fr.freq_range_end * 1e6) {
      return fr;
    }
  }
  // Return empty params.
  return {};
}

static bool is_valid_raster_param(const nr_raster_params& raster)
{
  for (const nr_raster_params& fr : nr_fr_params) {
    if (fr == raster) {
      return true;
    }
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

nr_band_fr1 srsgnb::band_helper::get_band_from_dl_arfcn(uint32_t arfcn)
{
  for (const nr_band& band : nr_band_table_fr1) {
    // Check given ARFCN is between the first and last possible ARFCN.
    if (arfcn >= band.dl_nref_first and arfcn <= band.dl_nref_last) {
      return band.band;
    }
  }
  return nr_band_fr1::invalid;
}

uint32_t srsgnb::band_helper::get_ul_arfcn_from_dl_arfcn(uint32_t dl_arfcn)
{
  // NOTE: The procedure implemented in this function is implementation-defined.

  // Return same ARFCN for TDD bands.
  if (get_duplex_mode(get_band_from_dl_arfcn(dl_arfcn)) == duplex_mode::TDD) {
    return dl_arfcn;
  }

  // Derive UL ARFCN for FDD bands.
  for (const auto& band : nr_band_table_fr1) {
    if (band.band == get_band_from_dl_arfcn(dl_arfcn)) {
      uint32_t offset = (dl_arfcn - band.dl_nref_first) / band.dl_nref_step;
      return (band.ul_nref_first + offset * band.ul_nref_step);
    }
  }

  return 0;
}

double srsgnb::band_helper::nr_arfcn_to_freq(uint32_t nr_arfcn)
{
  nr_raster_params params = get_raster_params(nr_arfcn);
  if (not is_valid_raster_param(params)) {
    return 0.0;
  }
  return (params.F_REF_Offs_MHz * 1e6 + params.delta_F_global_kHz * (nr_arfcn - params.N_REF_Offs) * 1e3);
}

uint32_t srsgnb::band_helper::freq_to_nr_arfcn(double freq)
{
  nr_raster_params params = get_raster_params(freq);
  if (not is_valid_raster_param(params)) {
    return 0;
  }
  return static_cast<uint32_t>(((freq - params.F_REF_Offs_MHz * 1e6) / 1e3 / params.delta_F_global_kHz) +
                               params.N_REF_Offs);
}

ssb_pattern_case srsgnb::band_helper::get_ssb_pattern(nr_band_fr1 band, subcarrier_spacing scs)
{
  // Look for the given band and SCS.
  for (const nr_band_ssb_scs_case& ssb_scs_case : nr_ssb_band_scs_case_table_fr1) {
    // As bands are in ascending order, do not waste more time if the current band is bigger.
    if (ssb_scs_case.band > band) {
      return ssb_pattern_case::invalid;
    }

    if (ssb_scs_case.band == band and ssb_scs_case.scs == scs) {
      return ssb_scs_case.pattern;
    }
  }

  // Band is out of range, so consider invalid.
  return ssb_pattern_case::invalid;
}

subcarrier_spacing srsgnb::band_helper::get_lowest_ssb_scs(nr_band_fr1 band)
{
  // Look for the given band and SCS
  for (const nr_band_ssb_scs_case& ss_raster : nr_ssb_band_scs_case_table_fr1) {
    // Check if band and SCS match!
    if (nr_band_to_uint(ss_raster.band) == nr_band_to_uint(band)) {
      return ss_raster.scs;
    }

    // As bands are in ascending order, do not waste more time if the current band is bigger
    if (nr_band_to_uint(ss_raster.band) > nr_band_to_uint(band)) {
      return subcarrier_spacing::invalid;
    }
  }

  // Band is out of range, so consider invalid.
  return subcarrier_spacing::invalid;
}

duplex_mode srsgnb::band_helper::get_duplex_mode(nr_band_fr1 band)
{
  // Look for the given band.
  for (const nr_operating_band& b : nr_operating_bands_fr1) {
    // Check if band and SCS match!
    if (nr_band_to_uint(b.band) == nr_band_to_uint(band)) {
      return b.duplex;
    }

    // As bands are in ascending order, do not waste more time if the current band is bigger.
    if (b.band > band) {
      return duplex_mode::INVALID;
    }
  }

  // Band is out of range, so consider invalid.
  return duplex_mode::INVALID;
}

bool srsgnb::band_helper::is_paired_spectrum(nr_band_fr1 band)
{
  duplex_mode mode = get_duplex_mode(band);
  srsgnb_assert(mode < duplex_mode::INVALID, "Returned invalid duplex MODE");
  return mode == duplex_mode::FDD;
}

double srsgnb::band_helper::get_abs_freq_point_a_from_center_freq(uint32_t nof_prb, double center_freq)
{
  constexpr static unsigned NRE = 12;

  // For FR1 unit of resources blocks for freq calc is always 180kHz regardless for actual SCS of carrier.
  // TODO: add offset_to_carrier.
  return center_freq - (nof_prb / 2 * scs_to_khz(subcarrier_spacing::kHz15) * 1000 * NRE);
}

uint32_t srsgnb::band_helper::get_abs_freq_point_a_arfcn(uint32_t nof_prb, uint32_t arfcn)
{
  return freq_to_nr_arfcn(get_abs_freq_point_a_from_center_freq(nof_prb, nr_arfcn_to_freq(arfcn)));
}

double srsgnb::band_helper::get_center_freq_from_abs_freq_point_a(uint32_t nof_prb, uint32_t freq_point_a_arfcn)
{
  constexpr static unsigned NRE = 12;
  // for FR1 unit of resources blocks for freq calc is always 180kHz regardless for actual SCS of carrier.
  // TODO: add offset_to_carrier
  double abs_freq_point_a_freq = nr_arfcn_to_freq(freq_point_a_arfcn);
  return abs_freq_point_a_freq + (nof_prb / 2 * scs_to_khz(subcarrier_spacing::kHz15) * 1000 * NRE);
}

double srsgnb::band_helper::get_abs_freq_point_a_from_f_ref(double f_ref, uint32_t nof_rbs, subcarrier_spacing scs)
{
  // NOTE (i): It is unclear whether the SCS should always be 15kHz for FR1 (\ref get_abs_freq_point_a_from_center_freq
  // and see note).
  // NOTE (ii): TS 38.104, Section 5.4.2.2, reports <em>[...] The mapping must apply to at least one numerology
  // supported by the BS.<\em>. Therefore, the correct SCS to be used in this procedure still needs to determined.

  // Half of the number of subcarriers in a RE.
  constexpr static unsigned NRE_half = 6;

  // The procedure, which is explained in TS 38.104, Section 5.4.2.2, gives the position of f_ref in terms of subcarrier
  // and CRB index, depending on the size of N_RB. Below we compute the value in unit of subcarriers, meaning we don't
  // need to separate the cases of even and odd N_RB.
  unsigned delta_point_a_f_ref = nof_rbs * NRE_half;
  return f_ref - static_cast<double>(delta_point_a_f_ref * scs_to_khz(scs) * 1000U);
}

double
srsgnb::band_helper::get_f_ref_from_abs_freq_point_a(double abs_freq_point_a, uint32_t nof_rbs, subcarrier_spacing scs)
{
  // See notes in \ref get_abs_freq_point_a_from_f_ref.

  // Half of the number of subcarriers in a RE.
  constexpr static unsigned NRE_half = 6;
  // The procedure used in this function is the inverse of what explained in TS 38.104, Section 5.4.2.2.
  unsigned delta_point_a_f_ref = nof_rbs * NRE_half;
  return abs_freq_point_a + static_cast<double>(delta_point_a_f_ref * scs_to_khz(scs) * 1000U);
}

unsigned srsgnb::band_helper::get_n_rbs_from_bw(bs_channel_bandwidth_fr1 bw, subcarrier_spacing scs, frequency_range fr)
{
  // Return an invalid value in case the input parameters are not valid.
  if (fr != frequency_range::FR1 or scs > subcarrier_spacing::kHz60)
    return 0;

  // Search on the table \ref tx_bw_config_fr1 for the BS channel bandwidth and return the N_RB corresponding to SCS.
  for (unsigned bw_idx = 0; bw_idx < tx_bw_config_fr1.size(); ++bw_idx) {
    if (tx_bw_config_fr1[bw_idx].bw == bw) {
      if (scs == subcarrier_spacing::kHz15) {
        return tx_bw_config_fr1[bw_idx].n_rb_15kHz;
      } else if (scs == subcarrier_spacing::kHz30) {
        return tx_bw_config_fr1[bw_idx].n_rb_30kHz;
      } else {
        return tx_bw_config_fr1[bw_idx].n_rb_60kHz;
      }
    }
  }

  return 0;
}