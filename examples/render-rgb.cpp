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
#include <SDL2/SDL.h>
#include <nvjpg.hpp>

static void display_image(const nj::Surface &surface) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::fprintf(stderr, "Could not initialize sdl2: %s\n", SDL_GetError());
        return;
    }
    NJ_SCOPEGUARD([] { SDL_Quit(); });

    auto *window = SDL_CreateWindow("nvjpg",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        1080, 720, SDL_WINDOW_SHOWN);
    if (!window) {
        std::fprintf(stderr, "Could not create window: %s\n", SDL_GetError());
        return;
    }
    NJ_SCOPEGUARD([&window] { SDL_DestroyWindow(window); });

    auto *screen_surf = SDL_GetWindowSurface(window);
    if (!screen_surf) {
        std::fprintf(stderr, "Could not get window surface: %s\n", SDL_GetError());
        return;
    }
    NJ_SCOPEGUARD([&screen_surf] { SDL_FreeSurface(screen_surf); });

    auto *surf = SDL_CreateRGBSurfaceWithFormatFrom(const_cast<std::uint8_t *>(surface.data()),
        surface.width, surface.height, surface.get_bpp() * 8, surface.pitch, SDL_PIXELFORMAT_RGBA32);
    if (!surf) {
        std::fprintf(stderr, "Failed to create image surface: %s\n", SDL_GetError());
        return;
    }
    NJ_SCOPEGUARD([&surf] { SDL_FreeSurface(surf); });

    auto *converted_surf = SDL_ConvertSurface(surf, screen_surf->format, 0);
    if (!converted_surf) {
        std::fprintf(stderr, "Could not convert surface: %s\n", SDL_GetError());
        return;
    }
    NJ_SCOPEGUARD([&converted_surf] { SDL_FreeSurface(converted_surf); });

    SDL_BlitSurface(converted_surf, nullptr, screen_surf, nullptr);
    SDL_UpdateWindowSurface(window);

    SDL_Event e;
    while (true) {
        if (!SDL_WaitEvent(&e)) {
            std::fprintf(stderr, "Error while waiting on an event: %s\n", SDL_GetError());
            return;
        }

        if (e.type == SDL_QUIT)
            break;
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "Usage: %s jpg\n", argv[0]);
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

    nj::Image image(argv[1]);
    if (!image.is_valid() || image.parse()) {
        std::perror("Invalid file");
        return 1;
    }

    std::printf("Image dimensions: %ux%u\n", image.width, image.height);

    nj::Surface surf(image.width, image.height, nj::PixelFormat::RGBA);
    if (auto rc = surf.allocate(); rc) {
        std::fprintf(stderr, "Failed to allocate surface: %#x\n", rc);
        return 1;
    }

    std::printf("Surface pitch: %#lx, size %#lx\n", surf.pitch, surf.size());

    auto start = std::chrono::system_clock::now();
    if (auto rc = decoder.render(image, surf); rc)
        std::fprintf(stderr, "Failed to render image: %#x (%s)\n", rc, std::strerror(errno));

    std::size_t read = 0;
    decoder.wait(surf, &read);
    auto time = std::chrono::system_clock::now() - start;
    printf("Rendered in %ldµs, read bytes: %lu\n",
        std::chrono::duration_cast<std::chrono::microseconds>(time).count(), read);

    display_image(surf);

    return 0;
}
