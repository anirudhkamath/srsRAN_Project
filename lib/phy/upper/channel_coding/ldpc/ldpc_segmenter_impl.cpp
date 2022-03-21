#include "ldpc_segmenter_impl.h"
#include "../crc_calculator_impl.h"
#include "srsgnb/phy/upper/channel_coding/ldpc/ldpc_codeblock_description.h"
#include "srsgnb/srsvec/bit.h"
#include "srsgnb/support/math_utils.h"
#include "srsgnb/support/srsran_assert.h"

using namespace srsgnb;
using namespace srsgnb::ldpc;
using segment_meta_t = ldpc_segmenter::described_segment_t;

// Length of the CRC checksum added to the segments.
static constexpr unsigned seg_crc_length = 24;
// Number of bits in one byte.
static constexpr unsigned bits_per_byte = 8;

ldpc_segmenter_impl::ldpc_segmenter_impl(ldpc_segmenter_impl::sch_crc c)
{
  srsran_assert(c.crc16->get_generator_poly() == crc_generator_poly::CRC16, "Not a CRC generator of type CRC16.");
  srsran_assert(c.crc24A->get_generator_poly() == crc_generator_poly::CRC24A, "Not a CRC generator of type CRC24A.");
  srsran_assert(c.crc24B->get_generator_poly() == crc_generator_poly::CRC24B, "Not a CRC generator of type CRC24B.");

  crc_set.crc16  = std::move(c.crc16);
  crc_set.crc24A = std::move(c.crc24A);
  crc_set.crc24B = std::move(c.crc24B);
}

void ldpc_segmenter_impl::compute_nof_segments()
{
  if (nof_tb_bits_in <= max_segment_length) {
    nof_segments    = 1;
    nof_tb_bits_out = nof_tb_bits_in;
  } else {
    nof_segments    = divide_ceil(nof_tb_bits_in, max_segment_length - seg_crc_length);
    nof_tb_bits_out = nof_tb_bits_in + nof_segments * seg_crc_length;
  }
}

void ldpc_segmenter_impl::compute_lifting_size()
{
  unsigned ref_length{22};
  if (base_graph == base_graph_t::BG2) {
    if (nof_tb_bits_in > 640) {
      ref_length = 10;
    } else if (nof_tb_bits_in > 560) {
      ref_length = 9;
    } else if (nof_tb_bits_in > 192) {
      ref_length = 8;
    } else {
      ref_length = 6;
    }
  }

  unsigned total_ref_length = nof_segments * ref_length;

  lifting_size = 0;
  for (auto ls : all_lifting_sizes) {
    if (ls * total_ref_length > nof_tb_bits_out) {
      lifting_size = ls;
      break;
    }
  }
  assert(lifting_size != 0);
}

void ldpc_segmenter_impl::compute_segment_length()
{
  constexpr unsigned base_length_BG1 = BG1_N_full - BG1_M;
  constexpr unsigned base_length_BG2 = BG2_N_full - BG2_M;
  unsigned           base_length     = (base_graph == base_graph_t::BG1) ? base_length_BG1 : base_length_BG2;

  segment_length = base_length * lifting_size;
}

unsigned ldpc_segmenter_impl::compute_rm_length(unsigned i_seg, modulation_scheme mod, unsigned nof_layers) const
{
  unsigned tmp = 0;
  if (i_seg < nof_short_segments) {
    // For unsigned, division then floor is the same as integer division.
    tmp = nof_symbols_per_layer / nof_segments;
  } else {
    tmp = divide_ceil(nof_symbols_per_layer, nof_segments);
  }
  return tmp * nof_layers * static_cast<unsigned>(mod);
}

static void fill_segment(span<uint8_t>                    segment,
                         span<const uint8_t>              tr_block,
                         std::unique_ptr<crc_calculator>& crc,
                         unsigned                         nof_crc_bits,
                         unsigned                         nof_filler)
{
  assert(segment.size() == tr_block.size() + nof_crc_bits + nof_filler);

  // First, copy information bits from the transport block.
  std::copy(tr_block.begin(), tr_block.end(), segment.begin());

  unsigned nof_used_bits = tr_block.size();
  // If needed, compute the CRC and append it to the information bits.
  if (nof_crc_bits > 0) {
    unsigned      tmp_crc  = crc->calculate_bit(tr_block);
    span<uint8_t> tmp_bits = segment.subspan(nof_used_bits, nof_crc_bits);
    srsvec::bit_unpack(tmp_crc, tmp_bits, nof_crc_bits);
    nof_used_bits += nof_crc_bits;
  }

  // If needed, fill the segment tail with filler bits.
  std::fill_n(segment.begin() + nof_used_bits, nof_filler, filler_bit);
}

static void check_inputs(const static_vector<segment_meta_t, ldpc_segmenter::MAX_NOF_SEGMENTS>& segments,
                         span<const uint8_t>                                                    transport_block,
                         const ldpc_segmenter::config_t&                                        cfg)
{
  srsran_assert(segments.empty(), "Argument segments should be empty.");
  srsran_assert(!transport_block.empty(), "Argument transport_block should not be empty.");
  srsran_assert(transport_block.size() * 8 + 24 <= max_tbs,
                "Transport block too long. The admissible size, including CRC, is %d bytes.",
                max_tbs / bits_per_byte);

  srsran_assert((cfg.rv >= 0) && (cfg.rv <= 3), "Invalid redundancy version.");

  srsran_assert((cfg.nof_layers >= 1) && (cfg.nof_layers <= 4), "Invalid number of layers.");

  srsran_assert(cfg.nof_ch_symbols % (cfg.nof_layers) == 0,
                "The number of channel symbols should be a multiple of the product between the number of layers.");
}

void ldpc_segmenter_impl::segment(
    static_vector<described_segment_t, ldpc_segmenter::MAX_NOF_SEGMENTS>& described_segments,
    span<const uint8_t>                                                   transport_block,
    const config_t&                                                       cfg)
{
  check_inputs(described_segments, transport_block, cfg);

  base_graph         = cfg.base_graph;
  max_segment_length = (base_graph == base_graph_t::BG1) ? max_BG1_block_length : max_BG2_block_length;
  // Each transport_block entry is a byte, and TBS can always be expressed as an integer number of bytes (see, e.g.,
  // TS38.214 Section 5.1.3.2).
  unsigned        nof_tb_bits_tmp = transport_block.size() * bits_per_byte;
  crc_calculator& tb_crc          = *crc_set.crc24A;
  unsigned        nof_tb_crc_bits = 24;
  if (nof_tb_bits_tmp <= 3824) {
    tb_crc          = *crc_set.crc16;
    nof_tb_crc_bits = 16;
  }
  nof_tb_bits_in = nof_tb_bits_tmp + nof_tb_crc_bits;

  buffer.resize(nof_tb_bits_in);
  srsvec::bit_unpack(transport_block, span<uint8_t>(buffer).first(nof_tb_bits_tmp));
  unsigned      tb_checksum = tb_crc.calculate_byte(transport_block);
  span<uint8_t> p           = span<uint8_t>(buffer).last(nof_tb_crc_bits);
  srsvec::bit_unpack(tb_checksum, p, nof_tb_crc_bits);

  nof_available_coded_bits = cfg.nof_ch_symbols * static_cast<unsigned>(cfg.mod);

  compute_nof_segments();
  compute_lifting_size();
  compute_segment_length();

  unsigned nof_crc_bits{0};
  if (nof_segments > 1) {
    nof_crc_bits = seg_crc_length;
  }
  // Compute the maximum number of information bits that can be assigned to a segment.
  unsigned max_info_bits = divide_ceil(nof_tb_bits_out, nof_segments) - nof_crc_bits;

  // Number of channel symbols assigned to a transmission layer.
  nof_symbols_per_layer = cfg.nof_ch_symbols / cfg.nof_layers;
  // Number of segments that will have a short rate-matched length. In TS38.212 Section 5.4.2.1, these correspond to
  // codeblocks whose length E_r is computed by rounding down - floor. For the remaining codewords, the length is
  // rounded up.
  nof_short_segments = nof_segments - (nof_symbols_per_layer % nof_segments);

  unsigned input_idx{0};
  for (unsigned i_segment = 0; i_segment != nof_segments; ++i_segment) {
    segment_data_t tmp_data(segment_length);
    // Number of bits to copy to this segment.
    unsigned nof_info_bits = std::min(max_info_bits, nof_tb_bits_in - input_idx);
    // Number of filler bits in this segment.
    unsigned nof_filler_bits = segment_length - nof_info_bits - nof_crc_bits;

    fill_segment(tmp_data,
                 span<uint8_t>(buffer).subspan(input_idx, nof_info_bits),
                 crc_set.crc24B,
                 nof_crc_bits,
                 nof_filler_bits);
    input_idx += nof_info_bits;

    codeblock_description_t tmp_description{};

    tmp_description.tb_common.base_graph   = base_graph;
    tmp_description.tb_common.lifting_size = static_cast<lifting_size_t>(lifting_size);
    tmp_description.tb_common.rv           = cfg.rv;
    tmp_description.tb_common.mod          = cfg.mod;
    tmp_description.tb_common.Nref         = cfg.Nref;

    // BG1 has rate 1/3 and BG2 has rate 1/5.
    constexpr unsigned inverse_BG1_rate = 3;
    constexpr unsigned inverse_BG2_rate = 5;
    unsigned           inverse_rate     = (base_graph == base_graph_t::BG1) ? inverse_BG1_rate : inverse_BG2_rate;

    tmp_description.cb_specific.full_length     = segment_length * inverse_rate;
    tmp_description.cb_specific.nof_filler_bits = nof_filler_bits;
    tmp_description.cb_specific.rm_length       = compute_rm_length(i_segment, cfg.mod, cfg.nof_layers);

    described_segments.push_back({tmp_data, tmp_description});
  }
}

std::unique_ptr<ldpc_segmenter> srsgnb::create_ldpc_segmenter()
{
  return std::make_unique<ldpc_segmenter_impl>(
      ldpc_segmenter_impl::sch_crc{std::make_unique<crc_calculator_impl>(crc_generator_poly::CRC16),
                                   std::make_unique<crc_calculator_impl>(crc_generator_poly::CRC24A),
                                   std::make_unique<crc_calculator_impl>(crc_generator_poly::CRC24B)});
}