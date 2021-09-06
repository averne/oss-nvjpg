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
#include <algorithm>
#include <chrono>
#include <nvjpg.hpp>

extern "C" void userAppInit() {
    socketInitializeDefault();
    nxlinkStdio();
}

extern "C" void userAppExit() {
    socketExit();
}

static void display_image(const nj::Surface &surface) {
    constexpr std::size_t fb_width = 1280, fb_height = 720;
    Framebuffer fb;
    framebufferCreate(&fb, nwindowGetDefault(), fb_width, fb_height, PIXEL_FORMAT_RGBA_8888, 1);
    framebufferMakeLinear(&fb);

    auto *framebuf = static_cast<std::uint8_t *>(framebufferBegin(&fb, nullptr));
    for (std::size_t i = 0; i < std::min(fb_height, surface.height); ++i)
        std::copy_n(surface.data() + i * surface.pitch, std::min(surface.width, fb_width) * surface.get_bpp(), framebuf + i * fb.stride);
    framebufferEnd(&fb);

    PadState pad;
    padConfigureInput(1, HidNpadStyleTag_NpadHandheld);
    padInitializeDefault(&pad);
    while (appletMainLoop()) {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus)
            break;
        consoleUpdate(nullptr);
    }

    framebufferClose(&fb);
}

int main(int argc, char **argv) {
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

    nj::Image image((argc < 2) ? "sdmc:/test.jpg" : argv[1]);
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
    if (auto rc = decoder.render(image, surf, 255); rc)
        std::fprintf(stderr, "Failed to render image: %#x (%s)\n", rc, std::strerror(errno));

    std::size_t read = 0;
    decoder.wait(surf, &read);
    auto time = std::chrono::system_clock::now() - start;
    std::printf("Rendered in %ldµs, read bytes: %lu\n",
        std::chrono::duration_cast<std::chrono::microseconds>(time).count(), read);

    std::printf("Press + to exit\n");

    display_image(surf);

    return 0;
}
