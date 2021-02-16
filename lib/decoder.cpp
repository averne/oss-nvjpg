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

#include <cstddef>
#include <cstring>
#include <cerrno>
#include <algorithm>
#include <span>
#include <tuple>
#include <vector>

#include <nvjpg/nv/cmdbuf.hpp>
#include <nvjpg/nv/ctrl.hpp>
#include <nvjpg/nv/registers.hpp>
#include <nvjpg/utils.hpp>

#include <nvjpg/decoder.hpp>

namespace nj {

namespace {

constexpr std::uint32_t float_to_fixed(float f) {
    return static_cast<int>(f * 65536.0f + 0.5f);
}

constinit std::array kernel_bt601 = {
    float_to_fixed( 1.164f),
    float_to_fixed( 1.596f), float_to_fixed(-0.391f),
    float_to_fixed(-0.813f), float_to_fixed( 2.018f),
    16u,
};

constinit std::array kernel_bt709 = {
    float_to_fixed( 1.164f),
    float_to_fixed( 1.793f), float_to_fixed(-0.213f),
    float_to_fixed(-0.534f), float_to_fixed( 2.115f),
    16u,
};

constinit std::array kernel_bt601ex = {
    float_to_fixed( 1.000f  ),
    float_to_fixed( 1.402f  ), float_to_fixed(-0.34416f),
    float_to_fixed(-0.71415f), float_to_fixed( 1.772f  ),
    0u,
};

} // namespace

int Decoder::initialize(std::size_t capacity) {
    NJ_TRY_RET(this->cmdbuf_map   .allocate(0x8000,                   32,     0x1));
    NJ_TRY_RET(this->pic_info_map .allocate(sizeof(NvjpgPictureInfo), 16,     0x1));
    NJ_TRY_RET(this->read_data_map.allocate(sizeof(NvjpgStatus),      16,     0x1));
    NJ_TRY_RET(this->scan_data_map.allocate(capacity,                 0x1000, 0x1));

    NJ_TRY_ERRNO(this->cmdbuf_map   .map());
    NJ_TRY_ERRNO(this->pic_info_map .map());
    NJ_TRY_ERRNO(this->read_data_map.map());
    NJ_TRY_ERRNO(this->scan_data_map.map());

    NJ_TRY_ERRNO(this->channel.open("/dev/nvhost-nvjpg"));

    return 0;
}

int Decoder::finalize() {
    NJ_TRY_RET(this->cmdbuf_map   .free());
    NJ_TRY_RET(this->pic_info_map .free());
    NJ_TRY_RET(this->read_data_map.free());
    NJ_TRY_RET(this->scan_data_map.free());

    NJ_TRY_ERRNO(this->channel.close());

    return 0;
}

int Decoder::resize(std::size_t capacity) {
    NJ_TRY_ERRNO(this->scan_data_map.unmap());
    NJ_TRY_RET  (this->scan_data_map.free());
    NJ_TRY_RET  (this->scan_data_map.allocate(capacity, 0x1000, 0x1));
    NJ_TRY_ERRNO(this->scan_data_map.map());
    return 0;
}

NvjpgPictureInfo *Decoder::build_picture_info_common(const Image &image, std::uint32_t downscale) {
    auto *info = static_cast<NvjpgPictureInfo *>(this->pic_info_map.address());
    std::memset(info, 0, sizeof(NvjpgPictureInfo));

    if (downscale)
        downscale = std::clamp(__builtin_ctz(downscale), 0, 3);

    for (std::size_t i = 0; i < image.hm_ac_tables.size(); ++i) {
        if (!(image.hm_ac_mask & bit(i)))
            continue;

        info->hm_ac_tables[i].codes   = image.hm_ac_tables[i].codes;
        info->hm_ac_tables[i].symbols = image.hm_ac_tables[i].symbols;
    }

    for (std::size_t i = 0; i < image.hm_dc_tables.size(); ++i) {
        if (!(image.hm_dc_mask & bit(i)))
            continue;

        info->hm_dc_tables[i].codes   = image.hm_dc_tables[i].codes;
        info->hm_dc_tables[i].symbols = image.hm_dc_tables[i].symbols;
    }

    for (std::size_t i = 0; i < image.quant_tables.size(); ++i) {
        if (!(image.quant_mask & bit(i)))
            continue;

        info->quant_tables[i].table = image.quant_tables[i].table;
    }

    for (std::size_t i = 0; i < image.num_components; ++i) {
        info->components[i].sampling_horiz = image.components[i].sampling_horiz;
        info->components[i].sampling_vert  = image.components[i].sampling_vert;
        info->components[i].quant_table_id = image.components[i].quant_table_id;
        info->components[i].hm_ac_table_id = image.components[i].hm_ac_table_id;
        info->components[i].hm_dc_table_id = image.components[i].hm_dc_table_id;
    }

    info->restart_interval      = image.restart_interval;
    info->width                 = image.width;
    info->height                = image.height;
    info->num_mcu_h             = (image.width + image.mcu_size_horiz - 1) / image.mcu_size_horiz;
    info->num_mcu_v             = (image.height + image.mcu_size_vert - 1) / image.mcu_size_vert;
    info->num_components        = image.num_components;
    info->scan_data_offset      = 0;
    info->scan_data_size        = image.get_scan_data().size();
    info->scan_data_samp_layout = static_cast<std::uint32_t>(image.sampling);
    info->alpha                 = 0;
    info->downscale_log_2       = downscale;

    return info;
}

int Decoder::render_common(const Image &image, SurfaceBase &surf) {
    if (image.progressive)
        return EINVAL;

    if (image.width == 0 || image.height == 0)
        return EINVAL;

    if (image.num_components == 1 && (image.components[0].sampling_horiz != 1 || image.components[0].sampling_vert != 1))
        return EINVAL;

    auto scan_data = image.get_scan_data();

    if (scan_data.size() > this->scan_data_map.size())
        return ENOMEM;

    std::copy_n(scan_data.begin(), std::min(scan_data.size(), this->scan_data_map.size()),
        static_cast<std::uint8_t *>(this->scan_data_map.address()));

    // Add syncpt increment to signal the end of the processing of our commands
    this->cmdbuf.begin(Decoder::class_id);
    this->cmdbuf.push_raw(OpcodeNonIncr(NJ_REGPOS(ThiRegisters, incr_syncpt), 1));
    this->cmdbuf.push_raw(this->channel.get_syncpt() | (true << 8)); // Condition: 0 = immediate, 1 = when done
    this->cmdbuf.end();

    auto &&[cmdbufs, exts,   class_ids] = this->cmdbuf.get_bufs();
    auto &&[relocs,  shifts, types]     = this->cmdbuf.get_relocs();

    std::array incrs = {
        nvhost_syncpt_incr{
            .syncpt_id    = this->channel.get_syncpt(),
            .syncpt_incrs = 1,
        },
    };

    std::array fences = {
        0u,
    };

    surf.render_fence.syncpt = this->channel.get_syncpt();

    return this->channel.submit(cmdbufs, exts, class_ids, relocs, shifts, types, incrs, fences, surf.render_fence);
}

int Decoder::render(const Image &image, Surface &surf, std::uint8_t alpha, std::uint32_t downscale) {
    if (surf.width == 0 || surf.height == 0)
        return EINVAL;

    auto *info = this->build_picture_info_common(image, downscale);
    info->out_data_samp_layout  = static_cast<std::uint32_t>(image.sampling);
    info->out_surf_type         = static_cast<std::uint32_t>(surf.type);
    info->out_luma_surf_pitch   = surf.pitch;
    info->out_chroma_surf_pitch = 0;
    info->alpha                 = alpha;
    info->memory_mode           = static_cast<std::uint32_t>(surf.get_memory_mode());

    switch (this->colorspace) {
        case ColorSpace::BT601:
            info->yuv2rgb_kernel = kernel_bt601;
            break;
        case ColorSpace::BT709:
            info->yuv2rgb_kernel = kernel_bt709;
            break;
        case ColorSpace::BT601Ex:
        default:
            info->yuv2rgb_kernel = kernel_bt601ex;
            break;
    }

    this->cmdbuf.clear();
    this->cmdbuf.begin(Decoder::class_id);
    this->cmdbuf.push_value(NJ_REGPOS(NvjpgRegisters, operation_type),      1);
    this->cmdbuf.push_reloc(NJ_REGPOS(NvjpgRegisters, picture_info_offset), this->pic_info_map);
    this->cmdbuf.push_reloc(NJ_REGPOS(NvjpgRegisters, read_info_offset),    this->read_data_map);
    this->cmdbuf.push_reloc(NJ_REGPOS(NvjpgRegisters, scan_data_offset),    this->scan_data_map);
    this->cmdbuf.push_reloc(NJ_REGPOS(NvjpgRegisters, out_data_offset),     surf.get_map());
    this->cmdbuf.push_value(NJ_REGPOS(NvjpgRegisters, execute),             0x100);
    this->cmdbuf.end();

    return this->render_common(image, surf);
}

int Decoder::render(const Image &image, VideoSurface &surf, std::uint32_t downscale) {
    if (surf.width == 0 || surf.height == 0)
        return EINVAL;

    auto sampling = (image.num_components == 1) ? SamplingFormat::Monochrome : surf.sampling;

    auto *info = this->build_picture_info_common(image, downscale);
    info->out_data_samp_layout  = static_cast<std::uint32_t>(sampling);
    info->out_surf_type         = static_cast<std::uint32_t>(surf.type);
    info->out_luma_surf_pitch   = surf.luma_pitch;
    info->out_chroma_surf_pitch = surf.chroma_pitch;
    info->memory_mode           = static_cast<std::uint32_t>(surf.get_memory_mode());

    this->cmdbuf.clear();
    this->cmdbuf.begin(Decoder::class_id);
    this->cmdbuf.push_value(NJ_REGPOS(NvjpgRegisters, operation_type),      1);
    this->cmdbuf.push_reloc(NJ_REGPOS(NvjpgRegisters, picture_info_offset), this->pic_info_map);
    this->cmdbuf.push_reloc(NJ_REGPOS(NvjpgRegisters, read_info_offset),    this->read_data_map);
    this->cmdbuf.push_reloc(NJ_REGPOS(NvjpgRegisters, scan_data_offset),    this->scan_data_map);
    this->cmdbuf.push_reloc(NJ_REGPOS(NvjpgRegisters, out_data_offset),     surf.get_map(), 0);
    this->cmdbuf.push_reloc(NJ_REGPOS(NvjpgRegisters, out_data_2_offset),   surf.get_map(), surf.chromab_data - surf.data());
    this->cmdbuf.push_reloc(NJ_REGPOS(NvjpgRegisters, out_data_3_offset),   surf.get_map(), surf.chromar_data - surf.data());
    this->cmdbuf.push_value(NJ_REGPOS(NvjpgRegisters, execute),             0x100);
    this->cmdbuf.end();

    return this->render_common(image, surf);
}

int Decoder::wait(const SurfaceBase &surf, std::size_t *num_read_bytes, std::int32_t timeout_us) {
    NJ_TRY_RET(NvHostCtrl::wait(surf.render_fence, timeout_us));
    if (num_read_bytes)
        *num_read_bytes = reinterpret_cast<NvjpgStatus *>(this->read_data_map.address())->used_bytes;
    return 0;
}

} // namespace nj
