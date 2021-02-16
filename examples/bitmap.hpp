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
#include <cmath>
#include <algorithm>
#include <string_view>
#include <vector>
#include <nvjpg.hpp>

namespace nj {

class Bmp {
    public:
        enum class CompressionMethod: std::uint32_t {
            Rgb                                = 0,
            Rle8                               = 1,
            Rle4                               = 2,
            Bitfields                          = 3,
            Jpeg                               = 4,
            Png                                = 5,
            AlphaBitfields                     = 6,
            Cmyk                               = 11,
            CmykRle8                           = 12,
            CmykRle4                           = 13,

        };

        struct FileHeader {
            char magic[2]                      = { 'B', 'M' };
            std::uint32_t size;
            std::uint16_t reserved_1           = 0;
            std::uint16_t reserved_2           = 0;
            std::uint32_t offset               = sizeof(FileHeader) + sizeof(ImageHeader);
        } __attribute__((packed));

        struct ImageHeader {
            std::uint32_t     hdr_size         = sizeof(ImageHeader);
            std::int32_t      width;
            std::int32_t      height;
            std::uint16_t     num_planes       = 1;
            std::uint16_t     depth            = 24;
            CompressionMethod compression      = CompressionMethod::Rgb;
            std::uint32_t     size             = 0;
            std::int32_t      resolution_horiz = 0;
            std::int32_t      resolution_vert  = 0;
            std::uint32_t     num_comps        = 0;
            std::uint32_t     num_comps_2      = 0;
        };

    public:
        FileHeader  file_header;
        ImageHeader image_header;

    public:
        Bmp(int width, int height, std::uint32_t num_comps = 3) {
            this->image_header.width     = width;
            this->image_header.height    = -height; // Negative height -> top to bottom pixel data layout
            this->image_header.num_comps = num_comps;
            this->file_header.size       = sizeof(FileHeader) + sizeof(ImageHeader) + width * height * num_comps;
        }

        int write(const nj::Surface &surf, std::string_view path) {
            if (surf.type != nj::PixelFormat::BGRA) // Pixel data is in BGR order
                return -1;

            auto *fp = std::fopen(path.data(), "wb");
            if (!fp)
                return 1;

            if (auto ret = std::fwrite(&this->file_header, 1, sizeof(FileHeader), fp); ret != sizeof(FileHeader))
                return 2;

            if (auto ret = std::fwrite(&this->image_header, 1, sizeof(ImageHeader), fp); ret != sizeof(ImageHeader))
                return 3;

            auto line_width = nj::align_up(this->image_header.width * this->image_header.num_comps, 4u);
            std::vector<std::uint8_t> line_buf(line_width * std::abs(this->image_header.height), 0);

            for (auto i = 0; i < std::abs(this->image_header.height); ++i) {
                for (auto j = 0; j < this->image_header.width; ++j) {
                    auto *src = surf.data()     + i * surf.pitch + j * surf.get_bpp();
                    auto *dst = line_buf.data() + i * line_width + j * this->image_header.num_comps;
                    std::copy_n(src, 3, dst);
                }
            }

            if (auto ret = std::fwrite(line_buf.data(), 1, line_buf.size(), fp); ret != line_buf.size())
                return 4;

            return 0;
        }
};

} // namespace nj
