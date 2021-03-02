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

#include <nvjpg/nv/cmdbuf.hpp>
#include <nvjpg/nv/channel.hpp>
#include <nvjpg/nv/map.hpp>
#include <nvjpg/image.hpp>
#include <nvjpg/surface.hpp>

#ifdef __SWITCH__
#   include <switch.h>
#endif

namespace nj {

class Decoder {
    public:
        enum class ColorSpace {
            BT601,      // ITUR BT-601
            BT709,      // ITUR BT-709
            BT601Ex,    // ITUR BT-601 extended range (16-235 -> 0-255), JFIF
        };

        constexpr static std::uint32_t class_id = 0xc0;

    public:
        ColorSpace colorspace = ColorSpace::BT601Ex;

    public:
        Decoder(): cmdbuf(this->cmdbuf_map) { }

        Result initialize(std::size_t capacity = 0x500000); // 5 Mib
        Result finalize();

        Result resize(std::size_t capacity);

        std::size_t capacity() const {
            return this->scan_data_map.size();
        }

        Result render(const Image &image, Surface      &surf, std::uint8_t alpha = 0, std::uint32_t downscale = 0);
        Result render(const Image &image, VideoSurface &surf, std::uint32_t downscale = 0);

        Result wait(const SurfaceBase &surf, std::size_t *num_read_bytes = nullptr, std::int32_t timeout_us = -1);

        // In Hz
        std::uint32_t get_clock_rate() const {
            std::uint32_t rate = 0;
#ifdef __SWITCH__
            std::uint32_t tmp = 0;
            if (auto rc = mmuRequestGet(&this->request, &tmp); R_SUCCEEDED(rc))
                rate = tmp;
#else
            this->channel.get_clock_rate(Decoder::class_id, rate);
#endif
            return rate;
        }

        // In Hz
        Result set_clock_rate(std::uint32_t rate) const {
#ifdef __SWITCH__
            return mmuRequestSetAndWait(&this->request, rate, -1u);
#else
            return this->channel.set_clock_rate(Decoder::class_id, rate);
#endif
        }

    private:
        NvjpgPictureInfo *build_picture_info_common(const Image &image, std::uint32_t downscale);

        Result render_common(const Image &image, SurfaceBase &surf);

    private:
        NvChannel channel;
        NvMap cmdbuf_map, pic_info_map, read_data_map, scan_data_map;

        CmdBuf cmdbuf;

#ifdef __SWITCH__
        MmuRequest request;
#endif
};

} // namespace nj
