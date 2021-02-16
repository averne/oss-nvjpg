// Copyright (C) 2021 averne
//
// This file is part of oss-nvjpg.
//
// oss-nvjpg is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// oss-nvjpg is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with oss-nvjpg.  If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <cstdint>
#include <array>

namespace nj {

struct NvjpgRegisters {
    std::array<std::uint32_t, 128> reserved_x0;
    std::uint32_t                  operation_type;      // 1: decode, 2: encode
    std::array<std::uint32_t, 63>  reserved_x204;
    std::uint32_t                  execute;
    std::array<std::uint32_t, 255> reserved_x304;       // nvdec registers?
    std::uint32_t                  control_params;      // Bitflags of various debugging options
    std::uint32_t                  picture_index;
    std::uint32_t                  picture_info_offset;
    std::uint32_t                  read_info_offset;
    std::uint32_t                  scan_data_offset;
    std::uint32_t                  out_data_offset;
    std::uint32_t                  out_data_2_offset;
    std::uint32_t                  out_data_3_offset;
};

struct ThiRegisters {
    std::uint32_t                  incr_syncpt;
    std::uint32_t                  reserved_x4;
    std::uint32_t                  incr_syncpt_err;
    std::uint32_t                  ctxsw_incr_syncpt;
    std::array<std::uint32_t, 4>   reserved_x10;
    std::uint32_t                  ctxsw;
    std::uint32_t                  reserved_x24;
    std::uint32_t                  cont_syncpt_eof;
    std::array<std::uint32_t, 5>   reserved_x2c;
    std::uint32_t                  method_0;
    std::uint32_t                  method_1;
    std::array<std::uint32_t, 12>  reserved_x48;
    std::uint32_t                  int_status;
    std::uint32_t                  int_mask;
};

#define NJ_REGPOS(regs, member) (offsetof(regs, member) / sizeof(std::uint32_t))

struct NvjpgPictureInfo {
    struct alignas(std::uint32_t) Component {
        std::uint8_t sampling_horiz, sampling_vert;
        std::uint8_t quant_table_id;
        std::uint8_t hm_ac_table_id, hm_dc_table_id;
    };

    struct alignas(std::uint32_t) HuffmanTable {
        std::array<std::uint32_t, 16> codes;
        std::array<std::uint8_t,  80> reserved;         // Zero
        std::array<std::uint8_t, 162> symbols;
    };

    struct alignas(std::uint32_t) QuantizationTable {
        std::array<std::uint8_t, 64> table;
    };

    std::array<HuffmanTable,      4> hm_ac_tables;
    std::array<HuffmanTable,      4> hm_dc_tables;
    std::array<Component,         4> components;
    std::array<QuantizationTable, 4> quant_tables;
    std::uint32_t                    restart_interval;
    std::uint32_t                    width, height;
    std::uint32_t                    num_mcu_h, num_mcu_v;
    std::uint32_t                    num_components;
    std::uint32_t                    scan_data_offset;
    std::uint32_t                    scan_data_size;
    std::uint32_t                    scan_data_samp_layout;
    std::uint32_t                    out_data_samp_layout;
    std::uint32_t                    out_surf_type;
    std::uint32_t                    out_luma_surf_pitch;
    std::uint32_t                    out_chroma_surf_pitch;
    std::uint32_t                    alpha;
    std::array<std::uint32_t, 6>     yuv2rgb_kernel;    // Y gain, VR, UG, VG, UB, Y offset
    std::uint32_t                    tile_mode;         // 0: pitch linear, 1, block linear
    std::uint32_t                    gob_height;        // If tile mode is block linear
    std::uint32_t                    memory_mode;
    std::uint32_t                    downscale_log_2;
    std::array<std::uint32_t, 3>     reserved_xb1c;
};
static_assert(sizeof(NvjpgPictureInfo) == 0xb2c);

struct NvjpgStatus {
    std::uint32_t used_bytes;
    std::uint32_t mcu_x;
    std::uint32_t mcu_y;
    std::uint32_t reserved_xc;
    std::uint32_t result;
    std::uint32_t reserved_x14[3];
};
static_assert(sizeof(NvjpgStatus) == 0x20);

} // namespace nj
