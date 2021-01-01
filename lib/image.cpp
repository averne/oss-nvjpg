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

#include <nvjpg/image.hpp>
#include <sys/stat.h>
#include <unistd.h>

#include <nvjpg/bitstream.hpp>
#include <nvjpg/utils.hpp>

namespace nj {

namespace {

void skip_segment(JpegSegmentHeader seg, Bitstream &bs) {
    bs.skip(seg.size - sizeof(seg.size));
}

} // namespace

Image::Image(int fd) {
    struct stat st;
    if (auto rc = ::fstat(fd, &st); rc == -1) {
        this->valid = false;
        return;
    }

    this->data = std::make_shared<std::vector<std::uint8_t>>(st.st_size);
    if (auto rc = ::read(fd, this->data->data(), this->data->size()); rc == -1) {
        this->valid = false;
        return;
    }
}

JpegSegmentHeader Image::find_next_segment(Bitstream &bs) {
    JpegSegmentHeader hdr;
    do
        hdr.magic = bs.get<JpegMarker>();
    while ((hdr.magic != JpegMarker::Magic) && !bs.empty());

    hdr.marker = bs.get<JpegMarker>();
    hdr.size   = bs.get_be<std::uint16_t>();
    return hdr;
}

int Image::parse_app(JpegSegmentHeader seg, Bitstream &bs) {
    skip_segment(seg, bs);
    return 0;
}

int Image::parse_sof(JpegSegmentHeader seg, Bitstream &bs) {
    if (seg.size < 11)
        return ENODATA;

    this->progressive = seg.marker == JpegMarker::Sof2;

    this->sampling_precision = bs.get<std::uint8_t>();

    this->height = bs.get_be<std::uint16_t>();
    this->width  = bs.get_be<std::uint16_t>();

    this->num_components = bs.get<std::uint8_t>();
    if (this->num_components > 3)
        return EINVAL;

    std::uint8_t max_samp_h = 0, max_samp_v = 0;
    for (std::size_t i = 0; i < this->num_components; ++i) {
        auto id = bs.get<std::uint8_t>() - 1;

        auto sampling = bs.get<std::uint8_t>();
        this->components[id].sampling_vert  = sampling >> 0 & mask(4u);
        this->components[id].sampling_horiz = sampling >> 4 & mask(4u);

        this->components[id].quant_table_id = bs.get<std::uint8_t>();

        max_samp_h = std::max(max_samp_h, this->components[id].sampling_horiz);
        max_samp_v = std::max(max_samp_v, this->components[id].sampling_vert);
    }

    this->mcu_size_horiz = 8 * max_samp_h;
    this->mcu_size_vert  = 8 * max_samp_v;

    if (this->num_components == 3) {
        if ((this->components[0].sampling_vert == 2) && (this->components[0].sampling_horiz == 2))
            this->sampling = SamplingScheme::S420;
        if ((this->components[0].sampling_vert == 2) && (this->components[0].sampling_horiz != 2))
            this->sampling = SamplingScheme::S422;
        if ((this->components[0].sampling_vert != 2) && (this->components[0].sampling_horiz == 2))
            this->sampling = SamplingScheme::S440;
        if ((this->components[0].sampling_vert != 2) && (this->components[0].sampling_horiz != 2))
            this->sampling = SamplingScheme::S444;
    } else {
        this->sampling = SamplingScheme::Monochrome;
    }

    return 0;
}

int Image::parse_dri(JpegSegmentHeader seg, Bitstream &bs) {
    if (seg.size != 4)
        return ENODATA;

    this->restart_interval = bs.get_be<std::uint16_t>();
    return 0;
}

int Image::parse_dqt(JpegSegmentHeader seg, Bitstream &bs) {
    if (seg.size < 67)
        return ENODATA;

    auto start = bs.current();
    while (bs.current() - start < seg.size - 65) {
        auto info = bs.get<std::uint8_t>();

        auto id        = info >> 0 & mask(4u);
        auto precision = info >> 8 & mask(4u);

        this->quant_mask |= bit(static_cast<std::uint8_t>(id));

        if (precision == 0)
            for (auto i = 0; i < 0x40; ++i) // 8-bit precision
                this->quant_tables[id].table[i] = bs.get<std::uint8_t>();
        else
            for (auto i = 0; i < 0x40; ++i) // 16-bit precision
                // Discard high byte
                this->quant_tables[id].table[i] = bs.get<std::uint8_t>(), bs.get<std::uint8_t>();
    }

    return 0;
}

int Image::parse_dht(JpegSegmentHeader seg, Bitstream &bs) {
    if (seg.size < 18)
        return ENODATA;

    auto start = bs.current();
    while (bs.current() - start < seg.size - 16) {
        auto info = bs.get<std::uint8_t>();

        auto id   = info >> 0 & mask(4u);
        auto type = info >> 4 & mask(1u);

        HuffmanTable *table;
        if (type == 0) {
            this->hm_ac_mask |= bit(static_cast<std::uint8_t>(id));
            table = &this->hm_ac_tables[id];
        } else {
            this->hm_dc_mask |= bit(static_cast<std::uint8_t>(id));
            table = &this->hm_dc_tables[id];
        }

        int num_symbols = 0;
        for (auto i = 0; i < 16; ++i)
            num_symbols += table->codes[i] = bs.get<std::uint8_t>();
        for (auto i = 0; i < num_symbols; ++i)
            table->symbols[i] = bs.get<std::uint8_t>();
    }

    return 0;
}

int Image::parse_sos(JpegSegmentHeader seg, Bitstream &bs) {
    if (seg.size < 8)
        return ENODATA;

    auto num_comps = bs.get<std::uint8_t>();
    if (num_comps != this->num_components)
        return EINVAL;

    for (std::size_t i = 0; i < num_comps; ++i) {
        auto id   = bs.get<std::uint8_t>() - 1;
        auto info = bs.get<std::uint8_t>();

        this->components[id].hm_ac_table_id = info >> 0 & mask(4u);
        this->components[id].hm_dc_table_id = info >> 4 & mask(4u);
    }

    this->spectral_selection_lo = bs.get<std::uint8_t>();
    this->spectral_selection_hi = bs.get<std::uint8_t>();

    bs.skip(1);

    return 0;
}

int Image::parse() {
    if (!this->valid || !this->data)
        return EINVAL;

    auto bs = Bitstream(*this->data);

    // Find SOI
    JpegSegmentHeader seg;
    do
        seg = find_next_segment(bs);
    while (!bs.empty() && (seg.marker != JpegMarker::Soi));
    bs.rewind(sizeof(seg.size));

    while (!bs.empty()) {
        seg = find_next_segment(bs);
        if (bs.empty())
            return ENODATA;

        switch (seg.marker) {
            case JpegMarker::Soi:
                bs.rewind(sizeof(seg.size));
                return EINVAL;

            case JpegMarker::App0 ... JpegMarker::App15:
                NJ_TRY_RET(this->parse_app(seg, bs));
                break;

            case JpegMarker::Sof0 ... JpegMarker::Sof2:
                NJ_TRY_RET(this->parse_sof(seg, bs));
                break;

            case JpegMarker::Dqt:
                NJ_TRY_RET(this->parse_dqt(seg, bs));
                break;

            case JpegMarker::Dht:
                NJ_TRY_RET(this->parse_dht(seg, bs));
                break;

            case JpegMarker::Dri:
                NJ_TRY_RET(this->parse_dri(seg, bs));
                break;

            case JpegMarker::Sos:
                NJ_TRY_RET(this->parse_sos(seg, bs));
                this->scan_offset = bs.current() - this->data->begin();
                return 0;

            case JpegMarker::Eoi:
                bs.rewind(sizeof(seg.size));
                return ENODATA;

            default:
                skip_segment(seg, bs);
                break;
        }
    }

    return ENODATA;
}

} // namespace nj
