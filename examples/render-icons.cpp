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

namespace {

PadState g_pad;

constexpr std::size_t fb_width = 256 * 16 / 9, fb_height = 256; // Preserve aspect ratio
Framebuffer g_fb;

}

extern "C" void userAppInit() {
    socketInitializeDefault();
    nxlinkStdio();
    nsInitialize();
}

extern "C" void userAppExit() {
    nsExit();
    socketExit();
}

static void display_image(const nj::Surface &surface) {
    auto *framebuf = static_cast<std::uint8_t *>(framebufferBegin(&g_fb, nullptr));
    for (std::size_t i = 0; i < std::min(fb_height, surface.height); ++i)
        std::copy_n(surface.data() + i * surface.pitch, std::min(surface.width, fb_width) * surface.get_bpp(), framebuf + i * g_fb.stride);
    framebufferEnd(&g_fb);

    while (appletMainLoop()) {
        padUpdate(&g_pad);
        if (padGetButtonsDown(&g_pad) & (HidNpadButton_A | HidNpadButton_Plus))
            break;
    }
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

    std::printf("Press A to show the next icon, + to exit\n");

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&g_pad);

    framebufferCreate(&g_fb, nwindowGetDefault(), fb_width, fb_height, PIXEL_FORMAT_RGBA_8888, 2);
    framebufferMakeLinear(&g_fb);

    framebufferBegin(&g_fb, nullptr);
    framebufferEnd(&g_fb);

    int title_offset = 0;
    NsApplicationRecord record;
    auto control_data = std::make_shared<std::vector<std::uint8_t>>(sizeof(NsApplicationControlData));
    while (appletMainLoop()) {
        s32 count;
        if (R_FAILED(nsListApplicationRecord(&record, 1, title_offset, &count)) || !record.application_id) {
            title_offset = 0;
            continue;
        }

        title_offset++;

        std::printf("Showing icon for %#018lx\n", record.application_id);

        NJ_TRY_RET(nsGetApplicationControlData(NsApplicationControlSource_Storage, record.application_id,
            reinterpret_cast<NsApplicationControlData *>(control_data->data()), control_data->size(), nullptr));

        // We avoid copying the icon data by directly passing the whole control structure
        // The JFIF parser will skip the nacp data while looking for the SOI tag
        nj::Image image(control_data);
        if (!image.is_valid() || image.parse()) {
            std::perror("Invalid file");
            return 1;
        }

        nj::Surface surf(image.width, image.height, nj::PixelFormat::RGBA);
        if (auto rc = surf.allocate(); rc) {
            std::fprintf(stderr, "Failed to allocate surface: %#x\n", rc);
            return 1;
        }

        auto start = std::chrono::system_clock::now();
        if (auto rc = decoder.render(image, surf, 255); rc)
            std::fprintf(stderr, "Failed to render image: %#x (%s)\n", rc, std::strerror(errno));

        std::size_t read = 0;
        decoder.wait(surf, &read);
        auto time = std::chrono::system_clock::now() - start;
        std::printf("Rendered in %ldµs, read bytes: %lu\n",
            std::chrono::duration_cast<std::chrono::microseconds>(time).count(), read);

        display_image(surf);

        if (padGetButtonsDown(&g_pad) & HidNpadButton_Plus)
            break;
    }

    framebufferClose(&g_fb);

    return 0;
}
