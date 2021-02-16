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

#include <cstdio>

#include <nvjpg/utils.hpp>

#include <nvjpg/surface.hpp>

namespace nj {

namespace {

std::size_t compute_pitch(std::size_t width, std::size_t bpp) {
    return align_up(width * bpp, 0x100ul);
}

std::size_t compute_size(std::size_t pitch, std::size_t height) {
    return align_up(pitch * height, 0x20000ul);
}

} // namespace

int Surface::allocate()  {
    this->pitch = compute_pitch(this->width, this->get_bpp());
    NJ_TRY_RET(this->map.allocate(compute_size(this->pitch, this->height), 0x400, 0x1));
    NJ_TRY_ERRNO(this->map.map());
    return 0;
}

int VideoSurface::allocate() {
    auto hsubsamp = 0, vsubsamp = 0;
    switch (this->sampling) {
        case SamplingFormat::S420:
            hsubsamp = 2, vsubsamp = 2;
            break;
        case SamplingFormat::S422:
            hsubsamp = 2, vsubsamp = 1;
            break;
        case SamplingFormat::S444:
            hsubsamp = 1, vsubsamp = 1;
            break;
        default:
            return EINVAL;
    }

    this->luma_pitch   = compute_pitch(this->width, 1);
    this->chroma_pitch = compute_pitch(this->width / hsubsamp, 1);

    auto luma_size   = compute_size(this->luma_pitch, this->height);
    auto chroma_size = compute_size(this->chroma_pitch, this->height / vsubsamp);
    NJ_TRY_RET(this->map.allocate(luma_size + 2 * chroma_size, 0x400, 0x1));
    NJ_TRY_ERRNO(this->map.map());

    this->luma_data    = static_cast<std::uint8_t *>(this->map.address());
    this->chromab_data = static_cast<std::uint8_t *>(this->map.address()) + luma_size;
    this->chromar_data = static_cast<std::uint8_t *>(this->map.address()) + luma_size + chroma_size;
    return 0;
}

} // namespace nj
