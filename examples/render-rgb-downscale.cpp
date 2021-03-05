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
#include <cstring>
#include <chrono>
#include <filesystem>
#include <SDL2/SDL.h>
#include <nvjpg.hpp>

#include "bitmap.hpp"

// Has to be a power of two and smaller than 8
#define DOWNSCALE_FACTOR 4

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "Usage: %s directory\n", argv[0]);
        return 1;
    }

    if (auto rc = nj::initialize(); rc) {
        std::fprintf(stderr, "Failed to initialize library: %d: %s\n", rc, std::strerror(rc));
        return 1;
    }
    NJ_SCOPEGUARD([] { nj::finalize(); });

    nj::Decoder decoder;
    if (auto rc = decoder.initialize(); rc) {
        std::fprintf(stderr, "Failed to initialize decoder: %#x\n", rc);
        return rc;
    }
    NJ_SCOPEGUARD([&decoder] { decoder.finalize(); });

    nj::Surface surf(0x1000, 0x1000, nj::PixelFormat::BGRA);
    if (auto rc = surf.allocate(); rc) {
        std::fprintf(stderr, "Failed to allocate surface: %#x\n", rc);
        return 1;
    }

    std::printf("Surface pitch: %#lx, size %#lx\n", surf.pitch, surf.size());

    for (auto entry: std::filesystem::directory_iterator(argv[1])) {
        if (entry.is_directory())
            continue;

        auto path = entry.path();
        if (path.extension() != ".jpg" && path.extension() != ".jpeg")
            continue;

        std::puts(path.c_str());

        nj::Image image(path.string());
        if (!image.is_valid() || image.parse()) {
            std::perror("Invalid file");
            return 1;
        }

        std::printf("Image dimensions: %ux%u\n", image.width, image.height);

        auto start = std::chrono::system_clock::now();
        if (auto rc = decoder.render(image, surf, 0, DOWNSCALE_FACTOR); rc)
            std::fprintf(stderr, "Failed to render image: %#x (%s)\n", rc, std::strerror(errno));

        std::size_t read = 0;
        decoder.wait(surf, &read);
        auto time = std::chrono::system_clock::now() - start;
        std::printf("Rendered in %ldµs, read bytes: %lu\n",
            std::chrono::duration_cast<std::chrono::microseconds>(time).count(), read);

        nj::Bmp bmp(image.width / DOWNSCALE_FACTOR, image.height / DOWNSCALE_FACTOR);
        if (auto rc = bmp.write(surf, path.replace_extension(".bmp").string()); rc) {
            std::fprintf(stderr, "Failed to write bmp: %d\n", rc);
            return rc;
        }
    }

    return 0;
}
