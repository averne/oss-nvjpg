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
#include <cstdio>
#include <memory>
#include <span>
#include <string_view>
#include <vector>
#include <fcntl.h>

#include <nvjpg/bitstream.hpp>
#include <nvjpg/surface.hpp>

namespace nj {

enum class JpegMarker: std::uint8_t {
    Sof0  = 0xc0,
    Sof1  = 0xc1,
    Sof2  = 0xc2,
    Sof15 = 0xcf,

    Dht   = 0xc4,
    Soi   = 0xd8,
    Eoi   = 0xd9,
    Sos   = 0xda,
    Dqt   = 0xdb,
    Dri   = 0xdd,

    App0  = 0xe0,
    App15 = 0xef,

    Magic = 0xff,
};

struct JpegSegmentHeader {
    JpegMarker    magic;
    JpegMarker    marker;
    std::uint16_t size;
};

class Image {
    public:
        struct Component {
            std::uint8_t sampling_horiz, sampling_vert;
            std::uint8_t quant_table_id;
            std::uint8_t hm_ac_table_id, hm_dc_table_id;
        };

        struct QuantizationTable {
            std::array<std::uint8_t, 64> table;
        };

        struct HuffmanTable {
            std::array<std::uint32_t, 16>  codes;
            std::array<std::uint8_t,  162> symbols;
        };

    public:
        std::uint16_t  width                 = 0;
        std::uint16_t  height                = 0;
        std::uint8_t   mcu_size_horiz        = 0;
        std::uint8_t   mcu_size_vert         = 0;
        bool           progressive           = false;
        std::uint8_t   num_components        = 0;     // 1 (grayscale) and 3 (YUV) supported
        std::uint8_t   sampling_precision    = 0;     // 8 and 12-bit precision supported
        SamplingFormat sampling;
        std::uint16_t  restart_interval      = 0;
        std::uint8_t   spectral_selection_lo = 0;
        std::uint8_t   spectral_selection_hi = 0;

        std::array<Component,         3> components   = {};
        std::array<QuantizationTable, 4> quant_tables = {};
        std::array<HuffmanTable,      4> hm_ac_tables = {};
        std::array<HuffmanTable,      4> hm_dc_tables = {};

        std::uint8_t quant_mask = 0, hm_ac_mask = 0, hm_dc_mask = 0;

    public:
        Image() = default;
        Image(std::shared_ptr<std::vector<std::uint8_t>> data): data(data) { }
        Image(int fd);
        Image(FILE *fp): Image(fileno(fp)) { }
        Image(std::string_view path): Image(::open(path.data(), O_RDONLY)) { }

        bool is_valid() const {
            return this->valid;
        }

        int parse();

        std::span<std::uint8_t> get_scan_data() const {
            return std::span(this->data->begin() + this->scan_offset, this->data->size() - this->scan_offset);
        }

    private:
        JpegSegmentHeader find_next_segment(Bitstream &bs);

        int parse_app(JpegSegmentHeader seg, Bitstream &bs);
        int parse_sof(JpegSegmentHeader seg, Bitstream &bs);
        int parse_dqt(JpegSegmentHeader seg, Bitstream &bs);
        int parse_dht(JpegSegmentHeader seg, Bitstream &bs);
        int parse_dri(JpegSegmentHeader seg, Bitstream &bs);
        int parse_sos(JpegSegmentHeader seg, Bitstream &bs);

    private:
        bool valid = true;
        std::uint32_t scan_offset = 0;
        std::shared_ptr<std::vector<std::uint8_t>> data;
};

} // namespace nj
