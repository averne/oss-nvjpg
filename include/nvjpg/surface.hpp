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

#include <cstdlib>

#include <nvjpg/nv/ioctl_types.h>
#include <nvjpg/nv/map.hpp>

namespace nj {

enum class ColorFormat {
    R8G8B8X8,   // RGB
    A8,         // Monochrome
    YV12,       // Triplanar YVU with subsampling 4:2:0
    YV16,       // Triplanar YVU with subsampling 4:2:2
    YV24,       // Triplanar YVU with subsampling 4:4:4
};

enum class SamplingScheme {
    Monochrome = 0,
    S420       = 1,
    S422       = 2,
    S440       = 3,
    S444       = 4,
};

class SurfaceBase {
    public:
        ColorFormat type;

    public:
        constexpr SurfaceBase(ColorFormat type): type(type) { }

        const std::uint8_t *data() const {
            return static_cast<std::uint8_t *>(this->map.address());
        }

        std::size_t size() const {
            return this->map.size();
        }

        const NvMap &get_map() const {
            return this->map;
        }

    protected:
        NvMap map;
        nvhost_ctrl_fence render_fence = {};

        friend class Decoder;
};

class Surface: public SurfaceBase {
    public:
        constexpr static std::uint32_t surface_type = 3;
        std::size_t width, height, pitch;

    public:
        Surface(ColorFormat type, std::size_t width, std::size_t height):
            SurfaceBase(type), width(width), height(height) { }

        int allocate();

        constexpr int get_bpp() const {
            switch (this->type) {
                case ColorFormat::A8:
                    return 1;
                case ColorFormat::R8G8B8X8:
                    return 4;
                default:
                    return -1;
            }
        }

        constexpr int get_depth() const {
            return 3 * 8;
        }
};

class VideoSurface: public SurfaceBase {
    public:
        constexpr static std::uint32_t surface_type = 0;
        std::size_t width, height, luma_pitch, chroma_pitch;
        const std::uint8_t *luma_data, *chromab_data, *chromar_data;

    public:
        VideoSurface(ColorFormat type, std::size_t width, std::size_t height):
            SurfaceBase(type), width(width), height(height) { }

        int allocate();

        constexpr int get_depth() const {
            switch (this->type) {
                case ColorFormat::YV12:
                    return 12;
                case ColorFormat::YV16:
                    return 16;
                case ColorFormat::YV24:
                    return 24;
                default:
                    return -1;
            }
        }

        constexpr SamplingScheme get_sampling() const {
            switch (this->type) {
                case ColorFormat::YV12:
                    return SamplingScheme::S420;
                case ColorFormat::YV16:
                    return SamplingScheme::S422;
                case ColorFormat::YV24:
                    return SamplingScheme::S444;
                default:
                    return static_cast<SamplingScheme>(-1);
            }
        }
};

} // namespace nj
