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

Result Decoder::initialize(std::size_t num_ring_entries, std::size_t capacity) {
    this->entries.resize(num_ring_entries);

    for (auto &entry: this->entries) {
        NJ_TRY_RET(entry.cmdbuf_map   .allocate(0x8000,                   32,     0x1));
        NJ_TRY_RET(entry.pic_info_map .allocate(sizeof(NvjpgPictureInfo), 16,     0x1));
        NJ_TRY_RET(entry.read_data_map.allocate(sizeof(NvjpgStatus),      16,     0x1));
        NJ_TRY_RET(entry.scan_data_map.allocate(capacity,                 0x1000, 0x1));
    }

#ifdef __SWITCH__
    NJ_TRY_RET(this->channel.open("/dev/nvhost-nvjpg"));

    for (auto &entry: this->entries) {
        NJ_TRY_RET(entry.cmdbuf_map   .map(this->channel.get_fd()));
        NJ_TRY_RET(entry.pic_info_map .map(this->channel.get_fd()));
        NJ_TRY_RET(entry.read_data_map.map(this->channel.get_fd()));
        NJ_TRY_RET(entry.scan_data_map.map(this->channel.get_fd()));
    }

    NJ_TRY_RET(mmuRequestInitialize(&this->request, MmuModuleId_Nvjpg, 8, false));
#else
    NJ_TRY_ERRNO(this->channel.open("/dev/nvhost-nvjpg"));

    for (auto &entry: this->entries) {
        NJ_TRY_ERRNO(entry.cmdbuf_map   .map());
        NJ_TRY_ERRNO(entry.pic_info_map .map());
        NJ_TRY_ERRNO(entry.read_data_map.map());
        NJ_TRY_ERRNO(entry.scan_data_map.map());
    }
#endif

    this->next_entry = this->entries.begin();

    return 0;
}

Result Decoder::finalize() {
    for (auto &entry: this->entries) {
        NJ_TRY_RET(entry.cmdbuf_map   .free());
        NJ_TRY_RET(entry.pic_info_map .free());
        NJ_TRY_RET(entry.read_data_map.free());
        NJ_TRY_RET(entry.scan_data_map.free());
    }

#ifdef __SWITCH__
    NJ_TRY_RET(this->channel.close());

    NJ_TRY_RET(mmuRequestFinalize(&this->request));
#else
    NJ_TRY_ERRNO(this->channel.close());
#endif

    return 0;
}

Result Decoder::resize(std::size_t capacity) {
    for (auto &entry: this->entries) {
        NJ_TRY_RET(entry.scan_data_map.free());
        NJ_TRY_RET(entry.scan_data_map.allocate(capacity, 0x1000, 0x1));

#ifdef __SWITCH__
        NJ_TRY_RET(entry.scan_data_map.map(this->channel.get_fd()));
#else
        NJ_TRY_ERRNO(entry.scan_data_map.map());
#endif
    }

    return 0;
}

Decoder::RingEntry &Decoder::get_ring_entry() const {
    auto &entry = *this->next_entry;

    if (entry.fence.value != -1u)
        NvHostCtrl::wait(entry.fence, -1);

    return entry;
}

NvjpgPictureInfo *Decoder::build_picture_info_common(RingEntry &entry, const Image &image, std::uint32_t downscale) {
    auto *info = static_cast<NvjpgPictureInfo *>(entry.pic_info_map.address());
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

Result Decoder::render_common(RingEntry &entry, const Image &image, SurfaceBase &surf) {
    if (image.progressive)
        return EINVAL;

    if (image.width == 0 || image.height == 0)
        return EINVAL;

    if (image.num_components == 1 && (image.components[0].sampling_horiz != 1 || image.components[0].sampling_vert != 1))
        return EINVAL;

    auto scan_data = image.get_scan_data();

    if (scan_data.size() > entry.scan_data_map.size())
        return ENOMEM;

    std::copy_n(scan_data.begin(), std::min(scan_data.size(), entry.scan_data_map.size()),
        static_cast<std::uint8_t *>(entry.scan_data_map.address()));

    // Add syncpt increment to signal the end of the processing of our commands
    entry.cmdbuf.begin(Decoder::class_id);
    entry.cmdbuf.push_raw(OpcodeNonIncr(NJ_REGPOS(ThiRegisters, incr_syncpt), 1));
    entry.cmdbuf.push_raw(this->channel.get_syncpt() | (true << 8)); // Condition: 0 = immediate, 1 = when done
    entry.cmdbuf.end();

    std::array incrs = {
        nvhost_syncpt_incr{
            .syncpt_id    = this->channel.get_syncpt(),
            .syncpt_incrs = 1,
        },
    };

    auto render_fence = nvhost_ctrl_fence{
        .id    = this->channel.get_syncpt(),
        .value = 0,
    };

#ifdef __SWITCH__
    NJ_TRY_RET(this->channel.submit(entry.cmdbuf.get_bufs(), {}, {}, incrs, std::span(&render_fence, 1)));
#else
    std::array fences = {
        0u,
    };

    auto &&[cmdbufs, exts,   class_ids] = entry.cmdbuf.get_bufs();
    auto &&[relocs,  shifts, types]     = entry.cmdbuf.get_relocs();

    NJ_TRY_RET(this->channel.submit(cmdbufs, exts, class_ids, relocs, shifts, types, incrs, fences, render_fence));
#endif

    entry.fence = surf.render_fence = render_fence;

    if (++this->next_entry == this->entries.end())
        this->next_entry = this->entries.begin();

    return 0;
}

Result Decoder::render(const Image &image, Surface &surf, std::uint8_t alpha, std::uint32_t downscale) {
#ifdef __SWITCH__
    if (!surf.map.iova())
        NJ_TRY_RET(surf.map.map(this->channel.get_fd()));
#endif

    if (surf.width == 0 || surf.height == 0)
        return EINVAL;

    auto &entry = this->get_ring_entry();

    auto *info = this->build_picture_info_common(entry, image, downscale);
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

    entry.cmdbuf.clear();
    entry.cmdbuf.begin(Decoder::class_id);
    entry.cmdbuf.push_value(NJ_REGPOS(NvjpgRegisters, operation_type),      1);
    entry.cmdbuf.push_reloc(NJ_REGPOS(NvjpgRegisters, picture_info_offset), entry.pic_info_map);
    entry.cmdbuf.push_reloc(NJ_REGPOS(NvjpgRegisters, read_info_offset),    entry.read_data_map);
    entry.cmdbuf.push_reloc(NJ_REGPOS(NvjpgRegisters, scan_data_offset),    entry.scan_data_map);
    entry.cmdbuf.push_reloc(NJ_REGPOS(NvjpgRegisters, out_data_offset),     surf.get_map());
    entry.cmdbuf.push_value(NJ_REGPOS(NvjpgRegisters, execute),             0x100);
    entry.cmdbuf.end();

    return this->render_common(entry, image, surf);
}

Result Decoder::render(const Image &image, VideoSurface &surf, std::uint32_t downscale) {
#ifdef __SWITCH__
    if (!surf.map.iova())
        NJ_TRY_RET(surf.map.map(this->channel.get_fd()));
#endif

    if (surf.width == 0 || surf.height == 0)
        return EINVAL;

    auto &entry = this->get_ring_entry();

    auto sampling = (image.num_components == 1) ? SamplingFormat::Monochrome : surf.sampling;

    auto *info = this->build_picture_info_common(entry, image, downscale);
    info->out_data_samp_layout  = static_cast<std::uint32_t>(sampling);
    info->out_surf_type         = static_cast<std::uint32_t>(surf.type);
    info->out_luma_surf_pitch   = surf.luma_pitch;
    info->out_chroma_surf_pitch = surf.chroma_pitch;
    info->memory_mode           = static_cast<std::uint32_t>(surf.get_memory_mode());

    entry.cmdbuf.clear();
    entry.cmdbuf.begin(Decoder::class_id);
    entry.cmdbuf.push_value(NJ_REGPOS(NvjpgRegisters, operation_type),      1);
    entry.cmdbuf.push_reloc(NJ_REGPOS(NvjpgRegisters, picture_info_offset), entry.pic_info_map);
    entry.cmdbuf.push_reloc(NJ_REGPOS(NvjpgRegisters, read_info_offset),    entry.read_data_map);
    entry.cmdbuf.push_reloc(NJ_REGPOS(NvjpgRegisters, scan_data_offset),    entry.scan_data_map);
    entry.cmdbuf.push_reloc(NJ_REGPOS(NvjpgRegisters, out_data_offset),     surf.get_map(), 0);
    entry.cmdbuf.push_reloc(NJ_REGPOS(NvjpgRegisters, out_data_2_offset),   surf.get_map(), surf.chromab_data - surf.data());
    entry.cmdbuf.push_reloc(NJ_REGPOS(NvjpgRegisters, out_data_3_offset),   surf.get_map(), surf.chromar_data - surf.data());
    entry.cmdbuf.push_value(NJ_REGPOS(NvjpgRegisters, execute),             0x100);
    entry.cmdbuf.end();

    return this->render_common(entry, image, surf);
}

Result Decoder::wait(const SurfaceBase &surf, std::size_t *num_read_bytes, std::int32_t timeout_us) {
    auto it = std::find_if(this->entries.begin(), this->entries.end(),
        [&surf](auto &entry) {
            return (entry.fence.id == surf.render_fence.id) && (entry.fence.value == surf.render_fence.value);
        }
    );

    if (it == this->entries.end())
        return 0;

    NJ_TRY_RET(NvHostCtrl::wait(it->fence, timeout_us));
    if (num_read_bytes)
        *num_read_bytes = reinterpret_cast<NvjpgStatus *>(it->read_data_map.address())->used_bytes;
    return 0;
}

} // namespace nj
