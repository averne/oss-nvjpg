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

#include <nvjpg/nv/ctrl.hpp>
#include <nvjpg/nv/map.hpp>
#include <nvjpg/decoder.hpp>
#include <nvjpg/image.hpp>
#include <nvjpg/surface.hpp>
#include <nvjpg/utils.hpp>

namespace nj {

#ifdef __SWITCH__
extern "C" std::uint32_t __nx_applet_type;
#endif

[[maybe_unused]]
static Result initialize() {
#ifdef __SWITCH__
    // Override the applet type, which controls what flavour of nvdrv gets initialized
    // To get access to /dev/nvhost-nvjpg, we need nvdrv:a/s/t
    auto saved_applet_type = std::exchange(__nx_applet_type, AppletType_LibraryApplet);
    NJ_SCOPEGUARD([&saved_applet_type] { __nx_applet_type = saved_applet_type; });

    NJ_TRY_RET(nvInitialize());
    NJ_TRY_RET(nj::NvMap::initialize());
    NJ_TRY_RET(nj::NvHostCtrl::initialize());

    NJ_TRY_RET(mmuInitialize());
#else
    NJ_TRY_ERRNO(nj::NvMap::initialize());
    NJ_TRY_ERRNO(nj::NvHostCtrl::initialize());
#endif
    return 0;
}

[[maybe_unused]]
static Result finalize() {
#ifdef __SWITCH__
    NJ_TRY_RET(nj::NvMap::finalize());
    NJ_TRY_RET(nj::NvHostCtrl::finalize());
    nvExit();

    mmuExit();
#else
    NJ_TRY_ERRNO(nj::NvMap::finalize());
    NJ_TRY_ERRNO(nj::NvHostCtrl::finalize());
#endif
    return 0;
}

} // namespace nj
