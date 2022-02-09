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
#include <algorithm>

#include <nvjpg/nv/ioctl_types.h>
#include <nvjpg/nv/map.hpp>

#if defined(__SWITCH__) && __has_include(<deko3d.hpp>)
#   include <deko3d.hpp>
#endif

namespace nj {

enum class PixelFormat {
    YUV  = 0,
    RGB  = 1,
    BGR  = 2,
    RGBA = 3,
    BGRA = 4,
    ABGR = 5,
    ARGB = 6,
};

enum class SamplingFormat {
    Monochrome = 0,
    S420       = 1,
    S422       = 2,
    S440       = 3,
    S444       = 4,
};

enum class MemoryMode {
    SemiPlanarNv12 = 0,
    SemiPlanarNv21 = 1,
    SinglyPlanar   = 2,
    Planar         = 3,
};

class SurfaceBase {
    public:
        std::size_t width, height;
        PixelFormat type;

    public:
        constexpr SurfaceBase(std::size_t width, std::size_t height, PixelFormat type):
            width(width), height(height), type(type) { }

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

#ifdef __SWITCH__
        NvFence           render_fence = {};
#else
        nvhost_ctrl_fence render_fence = {};
#endif

        friend class Decoder;
};

class Surface: public SurfaceBase {
    public:
        std::size_t pitch;

    public:
        constexpr Surface(std::size_t width, std::size_t height, PixelFormat pixel_fmt = PixelFormat::RGBA):
            SurfaceBase(width, height, pixel_fmt) { }

        int allocate();

        constexpr int get_bpp() const {
            switch (this->type) {
                case PixelFormat::RGB  ... PixelFormat::BGR:
                    return 3;
                case PixelFormat::RGBA ... PixelFormat::ARGB:
                default:
                    return 4;
            }
        }

#if defined(__SWITCH__) && __has_include(<deko3d.hpp>)
        std::tuple<dk::MemBlock, dk::Image> to_deko3d(dk::Device device, std::uint32_t flags = 0) const {
            auto map_dk_fmt = [](PixelFormat fmt) {
                switch (fmt) {
                    case PixelFormat::RGB:
                        return DkImageFormat_RGBX8_Unorm;
                    case PixelFormat::BGR:
                        return DkImageFormat_BGRX8_Unorm;
                    case PixelFormat::RGBA:
                    default:
                        return DkImageFormat_RGBA8_Unorm;
                    case PixelFormat::BGRA:
                        return DkImageFormat_BGRA8_Unorm;
                }
            };

            dk::ImageLayout layout;
            dk::ImageLayoutMaker{device}
                .setFlags(flags | DkImageFlags_PitchLinear)
                .setPitchStride(this->pitch)
                .setFormat(map_dk_fmt(this->type))
                .setDimensions(this->width, this->height)
                .initialize(layout);

            auto image_size = align_up(static_cast<std::uint32_t>(layout.getSize()), layout.getAlignment());
            auto image_memblock = dk::MemBlockMaker(device, image_size)
                .setFlags(DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image)
                .setStorage(this->map.address())
                .create();

            dk::Image image;
            image.initialize(layout, image_memblock, 0);

            return { image_memblock, image };
        }
#endif

        constexpr auto get_memory_mode() const {
            return MemoryMode::Planar;
        }
};

class VideoSurface: public SurfaceBase {
    public:
        SamplingFormat sampling;
        std::size_t luma_pitch, chroma_pitch;
        const std::uint8_t *luma_data, *chromab_data, *chromar_data;

    public:
        constexpr VideoSurface(std::size_t width, std::size_t height, SamplingFormat sampling = SamplingFormat::S420):
            SurfaceBase(width, height, PixelFormat::YUV), sampling(sampling) { }

        int allocate();

        constexpr int get_depth() const  {
            switch (this->sampling) {
                case SamplingFormat::S420:
                default:
                    return 12;
                case SamplingFormat::S422:
                    return 16;
                case SamplingFormat::S440:
                    return 16;
                case SamplingFormat::S444:
                    return 24;
            }
        }

        constexpr auto get_memory_mode() const {
            return MemoryMode::Planar;
        }
};

} // namespace nj
