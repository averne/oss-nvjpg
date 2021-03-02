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

class NvHostCtrl {
    public:
        static Result initialize() {
#ifdef __SWITCH__
            return nvFenceInit();
#else
            return NvHostCtrl::nvhostctrl_fd = ::open("/dev/nvhost-ctrl", O_RDWR | O_SYNC | O_CLOEXEC);
#endif
        }

        static Result finalize() {
#ifdef __SWITCH__
            nvFenceExit();
            return 0;
#else
            return ::close(NvHostCtrl::nvhostctrl_fd);
#endif
        }

#ifdef __SWITCH__
        static Result wait(NvFence fence, std::int32_t timeout) {
            return nvFenceWait(&fence, timeout);
        }
#else
        static Result wait(nvhost_ctrl_fence fence, std::int32_t timeout) {
            nvhost_ctrl_syncpt_waitex_args args = {
                .id      = fence.id,
                .thresh  = fence.value,
                .timeout = timeout,
                .value   = 0,
            };

            return ::ioctl(NvHostCtrl::nvhostctrl_fd, NVHOST_IOCTL_CTRL_SYNCPT_WAITEX, &args);
        }
#endif

    private:
#ifndef __SWITCH__
        static inline int nvhostctrl_fd = 0;
#endif
};

} // namespace nj

