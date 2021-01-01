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

class NvHostCtrl {
    public:
        static int initialize() {
            return NvHostCtrl::nvhostctrl_fd = ::open("/dev/nvhost-ctrl", O_RDWR | O_SYNC | O_CLOEXEC);
        }

        static int finalize() {
            return ::close(NvHostCtrl::nvhostctrl_fd);
        }

        static int wait(nvhost_ctrl_fence fence, std::int32_t timeout) {
            nvhost_ctrl_syncpt_waitex_args args = {
                .id      = fence.syncpt,
                .thresh  = fence.thresh,
                .timeout = timeout,
                .value   = 0,
            };

            return ::ioctl(NvHostCtrl::nvhostctrl_fd, NVHOST_IOCTL_CTRL_SYNCPT_WAITEX, &args);
        }

    private:
        static inline int nvhostctrl_fd = 0;
};

} // namespace nj

