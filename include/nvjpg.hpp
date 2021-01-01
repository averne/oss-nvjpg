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

int initialize() {
    NJ_TRY_ERRNO(nj::NvMap::initialize());
    NJ_TRY_ERRNO(nj::NvHostCtrl::initialize());
    return 0;
}

int finalize() {
    NJ_TRY_ERRNO(nj::NvMap::finalize());
    NJ_TRY_ERRNO(nj::NvHostCtrl::finalize());
    return 0;
}

} // namespace nj
