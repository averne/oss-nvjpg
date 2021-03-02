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
#include <new>

#ifdef __SWITCH__
#   include <switch.h>
#else
#   include <sys/stat.h>
#   include <sys/ioctl.h>
#   include <sys/mman.h>
#   include <fcntl.h>
#   include <unistd.h>
#endif

#include <nvjpg/nv/ioctl_types.h>
#include <nvjpg/utils.hpp>

namespace nj {

class NvMap {
    public:
        static Result initialize() {
#ifdef __SWITCH__
            return nvMapInit();
#else
            return NvMap::nvmap_fd = ::open("/dev/nvmap", O_RDWR | O_SYNC | O_CLOEXEC);
#endif
        }

        static Result finalize() {
#ifdef __SWITCH__
            nvMapExit();
            return 0;
#else
            return ::close(NvMap::nvmap_fd);
#endif
        }

        constexpr NvMap() = default;

        ~NvMap() {
#ifdef __SWITCH__
            if (this->addr)
                this->unmap();

            if (this->nvmap.cpu_addr)
                this->free();
#else
            if (this->addr)
                this->unmap();

            if (this->hdl)
                this->free();
#endif
        }

        Result allocate(std::uint32_t size, std::uint32_t align, std::uint32_t flags) {
#ifdef __SWITCH__
            NJ_UNUSED(flags);

            size = align_up(size, 0x1000u), align = align_up(align, 0x1000u);
            auto *mapmem = new (std::align_val_t(align)) std::uint8_t[size];
            if (!mapmem)
                return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);

            return nvMapCreate(&this->nvmap, mapmem, size, align, NvKind_Pitch, false);
#else
            nvmap_create_args create = {
                .size   = size,
                .handle = 0,
            };

            auto rc = ::ioctl(NvMap::nvmap_fd, NVMAP_IOCTL_CREATE, &create);
            if (rc)
                return rc;

            this->sz     = size;
            this->hdl    = create.handle;

            nvmap_alloc_args alloc = {
                .handle    = this->hdl,
                .heap_mask = 0x40000000,
                .flags     = flags,
                .align     = align,
            };

            return ::ioctl(NvMap::nvmap_fd, NVMAP_IOCTL_ALLOC, &alloc);
#endif
        }

        Result free() {
            if (this->addr)
                NJ_TRY_RET(this->unmap());

#ifdef __SWITCH__
            delete[] static_cast<std::uint8_t *>(this->nvmap.cpu_addr);
            nvMapClose(&this->nvmap);
            return 0;
#else
            auto rc = ::ioctl(NvMap::nvmap_fd, NVMAP_IOCTL_FREE, this->hdl);
            if (!rc)
                this->hdl = 0;

            return rc;
#endif
        }

#ifdef __SWITCH__
        Result map(int channel_fd, bool compressed = false) {
            nvioctl_command_buffer_map params = { .handle = this->nvmap.handle };
            NJ_TRY_RET(nvioctlChannel_MapCommandBuffer(channel_fd, &params, 1, compressed));
            this->owner = channel_fd, this->compressed = compressed, this->addr = params.iova;
            return 0;
        }
#else
        void *map(int prot = PROT_READ | PROT_WRITE, int flags = MAP_SHARED) {
            return this->addr = ::mmap(nullptr, this->sz, prot, flags, this->hdl, 0);
        }
#endif

        Result unmap() {
#ifdef __SWITCH__
            nvioctl_command_buffer_map params = { .handle = this->nvmap.handle };
            NJ_TRY_RET(nvioctlChannel_UnmapCommandBuffer(this->owner, &params, 1, this->compressed));
            this->addr = 0;
            return 0;
#else
            auto rc = ::munmap(this->addr, sz);
            if (!rc)
                this->addr = nullptr;

            return rc;
#endif
        }

        Result cache(std::size_t size, std::int32_t op = NVMAP_CACHE_OP_WB) const {
#ifdef __SWITCH__
            return 0;
#else
            nvmap_cache_args args = {
                .addr   = reinterpret_cast<uintptr_t>(this->addr),
                .handle = this->hdl,
                .len    = static_cast<std::uint32_t>(size),
                .op     = op,
            };

            return ::ioctl(NvMap::nvmap_fd, NVMAP_IOCTL_CACHE, &args);
#endif
        }

        Result cache(std::uint32_t op = NVMAP_CACHE_OP_WB) const {
#ifdef __SWITCH__
            return 0;
#else
            return this->cache(this->sz, op);
#endif
        }

        std::size_t size() const {
#ifdef __SWITCH__
            return this->nvmap.size;
#else
            return this->sz;
#endif
        }

        std::uint32_t handle() const {
#ifdef __SWITCH__
            return this->nvmap.handle;
#else
            return this->hdl;
#endif
        }

        void *address() const {
#ifdef __SWITCH__
            return this->nvmap.cpu_addr;
#else
            return this->addr;
#endif
        }

#ifdef __SWITCH__
        std::uint32_t iova() const {
            return this->addr;
        }
#endif

    private:
#ifdef __SWITCH__
        ::NvMap       nvmap      = {};
        std::uint32_t addr       = 0;
        std::uint32_t owner      = 0;
        bool          compressed = false;
#else
        std::uint32_t sz         = 0;
        std::uint32_t hdl        = 0;
        void         *addr       = nullptr;

        static inline int nvmap_fd;
#endif
};

} // namespace nj
