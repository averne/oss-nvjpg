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
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include <nvjpg/nv/ioctl_types.h>

namespace nj {

class NvMap {
    public:
        static int initialize() {
            return NvMap::nvmap_fd = ::open("/dev/nvmap", O_RDWR | O_SYNC | O_CLOEXEC);
        }

        static int finalize() {
            return ::close(NvMap::nvmap_fd);
        }

        constexpr NvMap() = default;

        ~NvMap() {
            if (this->addr)
                this->unmap();

            if (this->hdl)
                this->free();
        }

        int allocate(std::uint32_t size, std::uint32_t align, std::uint32_t flags) {
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
        }

        int free() {
            auto rc = ::ioctl(NvMap::nvmap_fd, NVMAP_IOCTL_FREE, this->hdl);
            if (!rc)
                this->hdl = 0;

            return rc;
        }

        void *map(int prot = PROT_READ | PROT_WRITE, int flags = MAP_SHARED) {
            return this->addr = ::mmap(nullptr, this->sz, prot, flags, this->hdl, 0);
        }

        int unmap() {
            auto rc = ::munmap(this->addr, sz);
            if (!rc)
                this->addr = nullptr;

            return rc;
        }

        int cache(std::size_t size, std::int32_t op = NVMAP_CACHE_OP_WB) const {
            nvmap_cache_args args = {
                .addr   = reinterpret_cast<uintptr_t>(this->addr),
                .handle = this->hdl,
                .len    = static_cast<std::uint32_t>(size),
                .op     = op,
            };

            return ::ioctl(NvMap::nvmap_fd, NVMAP_IOCTL_CACHE, &args);
        }

        int cache(std::uint32_t op = NVMAP_CACHE_OP_WB) const {
            return this->cache(this->sz, op);
        }

        std::size_t size() const {
            return this->sz;
        }

        std::uint32_t handle() const {
            return this->hdl;
        }

        void *address() const {
            return this->addr;
        }

    private:
        std::uint32_t sz     = 0;
        std::uint32_t hdl    = 0;
        void         *addr   = nullptr;

        static inline int nvmap_fd;
};

} // namespace nj
