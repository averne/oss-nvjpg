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

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#ifdef __SWITCH__
#   include <switch.h>
#else
#   include <linux/ioctl.h>
#endif

#ifdef __SWITCH__

typedef nvioctl_fence       nvhost_ctrl_fence;
typedef nvioctl_cmdbuf      nvhost_cmdbuf;
typedef nvioctl_syncpt_incr nvhost_syncpt_incr;

#else

typedef struct {
    uint32_t size;
    uint32_t handle;
} nvmap_create_args;

typedef struct {
    uint32_t handle;
    uint32_t heap_mask;
    uint32_t flags;
    uint32_t align;
} nvmap_alloc_args;

typedef struct {
    uint64_t addr;
    uint32_t handle;
    uint32_t len;
    int32_t  op;
} nvmap_cache_args;

#define NVMAP_IOCTL_MAGIC 'N'
#define NVMAP_IOCTL_CREATE   _IOWR(NVMAP_IOCTL_MAGIC, 0,  nvmap_create_args)
#define NVMAP_IOCTL_ALLOC    _IOW (NVMAP_IOCTL_MAGIC, 3,  nvmap_alloc_args)
#define NVMAP_IOCTL_FREE     _IO  (NVMAP_IOCTL_MAGIC, 4)
#define NVMAP_IOCTL_CACHE    _IOW (NVMAP_IOCTL_MAGIC, 12, nvmap_cache_args)

typedef struct {
    uint32_t id;
    uint32_t value;
} nvhost_ctrl_fence;

typedef struct {
    uint32_t id;
    uint32_t thresh;
    int32_t  timeout;
    uint32_t value;
} nvhost_ctrl_syncpt_waitex_args;

#define NVHOST_IOCTL_MAGIC 'H'
#define NVHOST_IOCTL_CTRL_SYNCPT_WAITEX _IOWR(NVHOST_IOCTL_MAGIC, 6, nvhost_ctrl_syncpt_waitex_args)

typedef struct {
    uint32_t rate;
    uint32_t moduleid;
} nvhost_clk_rate_args;

typedef struct {
    uint32_t param;
    uint32_t value;
} nvhost_get_param_args;

typedef struct {
    uint32_t mem;
    uint32_t offset;
    uint32_t words;
} nvhost_cmdbuf;

typedef struct {
    int32_t  pre_fence;
    uint32_t reserved;
} nvhost_cmdbuf_ext;

typedef struct {
    uint32_t cmdbuf_mem;
    uint32_t cmdbuf_offset;
    uint32_t target_mem;
    uint32_t target_offset;
} nvhost_reloc;

typedef struct {
    uint32_t shift;
} nvhost_reloc_shift;

typedef struct {
    uint32_t reloc_type;
    uint32_t padding;
} nvhost_reloc_type;

typedef struct {
    uint32_t mem;
    uint32_t offset;
    uint32_t syncpt_id;
    uint32_t thresh;
} nvhost_waitchk;

typedef struct {
    uint32_t syncpt_id;
    uint32_t syncpt_incrs;
} nvhost_syncpt_incr;

typedef struct {
    uint32_t  submit_version;
    uint32_t  num_syncpt_incrs;
    uint32_t  num_cmdbufs;
    uint32_t  num_relocs;
    uint32_t  num_waitchks;
    uint32_t  timeout;
    uint32_t  flags;
    uint32_t  fence;
    uintptr_t syncpt_incrs;
    uintptr_t cmdbuf_exts;

    uint32_t  checksum_methods;
    uint32_t  checksum_falcon_methods;

    uint64_t  pad[1];

    uintptr_t reloc_types;
    uintptr_t cmdbufs;
    uintptr_t relocs;
    uintptr_t reloc_shifts;
    uintptr_t waitchks;
    uintptr_t waitbases;
    uintptr_t class_ids;
    uintptr_t fences;
} nvhost_submit_args;

#define NVHOST_SUBMIT_VERSION_V2 2

#define NVHOST_IOCTL_CHANNEL_GET_CLK_RATE  _IOWR(NVHOST_IOCTL_MAGIC, 9,  nvhost_clk_rate_args)
#define NVHOST_IOCTL_CHANNEL_SET_CLK_RATE  _IOW (NVHOST_IOCTL_MAGIC, 10, nvhost_clk_rate_args)
#define NVHOST_IOCTL_CHANNEL_GET_SYNCPOINT _IOWR(NVHOST_IOCTL_MAGIC, 16, nvhost_get_param_args)
#define NVHOST_IOCTL_CHANNEL_SUBMIT        _IOWR(NVHOST_IOCTL_MAGIC, 26, nvhost_submit_args)

#endif

enum {
    NVMAP_CACHE_OP_WB              = 0,
    NVMAP_CACHE_OP_INV             = 1,
    NVMAP_CACHE_OP_WB_INV          = 2,
};

enum {
    NVHOST_RELOC_TYPE_DEFAULT      = 0,
    NVHOST_RELOC_TYPE_PITCH_LINEAR = 1,
    NVHOST_RELOC_TYPE_BLOCK_LINEAR = 2,
    NVHOST_RELOC_TYPE_NVLINK       = 3,
};

#ifdef __cplusplus
}
#endif // extern "C"
