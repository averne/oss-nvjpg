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

#include <cstdint>
#include <span>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include <nvjpg/nv/ioctl_types.h>

namespace nj {

class NvChannel {
    public:
        constexpr NvChannel() = default;

        ~NvChannel() {
            if (this->fd != -1)
                this->close();
        }

        int open(const char *path) {
            this->fd = ::open(path, O_RDWR | O_CLOEXEC);
            if (this->fd < 0)
                return this->fd;

            nvhost_get_param_args args = {
                .param = 0,
                .value = -1u,
            };

            auto rc = ::ioctl(this->fd, NVHOST_IOCTL_CHANNEL_GET_SYNCPOINT, &args);
            if (!rc)
                this->syncpt = args.value;

            return this->fd;
        }

        int close() {
            auto rc = ::close(this->fd);
            if (rc != -1)
                this->fd = -1;
            return rc;
        }

        int get_clock_rate(std::uint32_t id, std::uint32_t &rate) const {
            nvhost_clk_rate_args args = {
                .rate     = 0,
                .moduleid = id,
            };

            auto rc = ::ioctl(this->fd, NVHOST_IOCTL_CHANNEL_GET_CLK_RATE, &args);
            if (!rc)
                rate = args.rate;

            return rc;
        }

        int set_clock_rate(std::uint32_t id, std::uint32_t rate) const {
            nvhost_clk_rate_args args = {
                .rate     = rate,
                .moduleid = id,
            };

            return ::ioctl(this->fd, NVHOST_IOCTL_CHANNEL_SET_CLK_RATE, &args);
        }

        int submit(std::span<nvhost_cmdbuf> cmdbufs, std::span<nvhost_cmdbuf_ext> exts, std::span<std::uint32_t> class_ids,
                std::span<nvhost_reloc> relocs, std::span<nvhost_reloc_shift> shifts, std::span<nvhost_reloc_type> types,
                std::span<nvhost_syncpt_incr> incrs, std::span<std::uint32_t> fences, nvhost_ctrl_fence &fence) const {
            nvhost_submit_args args = {
                .submit_version          = NVHOST_SUBMIT_VERSION_V2,
                .num_syncpt_incrs        = static_cast<std::uint32_t>(incrs.size()),
                .num_cmdbufs             = static_cast<std::uint32_t>(cmdbufs.size()),
                .num_relocs              = static_cast<std::uint32_t>(relocs.size()),
                .num_waitchks            = 0,
                .timeout                 = 0,
                .flags                   = 0,
                .fence                   = 0,
                .syncpt_incrs            = reinterpret_cast<uintptr_t>(incrs.data()),
                .cmdbuf_exts             = reinterpret_cast<uintptr_t>(exts.data()),
                .checksum_methods        = 0,
                .checksum_falcon_methods = 0,
                .pad                     = { 0 },
                .reloc_types             = reinterpret_cast<uintptr_t>(types.data()),
                .cmdbufs                 = reinterpret_cast<uintptr_t>(cmdbufs.data()),
                .relocs                  = reinterpret_cast<uintptr_t>(relocs.data()),
                .reloc_shifts            = reinterpret_cast<uintptr_t>(shifts.data()),
                .waitchks                = reinterpret_cast<uintptr_t>(nullptr),
                .waitbases               = reinterpret_cast<uintptr_t>(nullptr),
                .class_ids               = reinterpret_cast<uintptr_t>(class_ids.data()),
                .fences                  = reinterpret_cast<uintptr_t>(fences.data()),
            };

            auto rc = ::ioctl(this->fd, NVHOST_IOCTL_CHANNEL_SUBMIT, &args);
            if (!rc)
                fence.thresh = args.fence;

            return rc;
        }

        int get_fd() const {
            return this->fd;
        }

        std::uint32_t get_syncpt() const {
            return this->syncpt;
        }

    private:
        int           fd     = 0;
        std::uint32_t syncpt = 0;
};

} // namespace nj
