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

// In this example, we render the decoded data by directly blitting it onto the framebuffer
// For more conventional use of textures within deko3d, see:
//   https://github.com/switchbrew/switch-examples/tree/master/graphics/deko3d/deko_examples

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <array>
#include <chrono>
#include <tuple>
#include <switch.h>
#include <deko3d.hpp>
#include <nvjpg.hpp>

extern "C" void userAppInit() {
    socketInitializeDefault();
    nxlinkStdio();
}

extern "C" void userAppExit() {
    socketExit();
}

namespace {

constexpr std::uint32_t FB_WIDTH    = 1280;
constexpr std::uint32_t FB_HEIGHT   = 720;
constexpr std::uint32_t FB_NUM      = 2;
constexpr std::size_t   CMDBUF_SIZE = 10 * DK_MEMBLOCK_ALIGNMENT;

dk::UniqueDevice    device;
dk::UniqueMemBlock  fb_memblock, cmdbuf_memblock, image_memblock;
dk::UniqueSwapchain swapchain;
dk::UniqueCmdBuf    cmdbuf;
dk::UniqueQueue     queue;

dk::Image image;
std::array<dk::Image, FB_NUM> framebuffer_images;

std::array<DkCmdList, FB_NUM> render_cmdlists;
std::array<DkCmdList, FB_NUM> bind_fb_cmdlists;

} // namespace

void deko_init() {
    device = dk::DeviceMaker().create();

    dk::ImageLayout fb_layout;
    dk::ImageLayoutMaker(device)
        .setFlags(DkImageFlags_UsageRender | DkImageFlags_UsagePresent | DkImageFlags_HwCompression | DkImageFlags_Usage2DEngine)
        .setFormat(DkImageFormat_RGBA8_Unorm)
        .setDimensions(FB_WIDTH, FB_HEIGHT)
        .initialize(fb_layout);

    auto fb_size = nj::align_up(static_cast<std::uint32_t>(fb_layout.getSize()), fb_layout.getAlignment());
    fb_memblock = dk::MemBlockMaker(device, FB_NUM * fb_size)
        .setFlags(DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image)
        .create();

    std::array<const DkImage *, framebuffer_images.size()> swapchain_images = {
        &framebuffer_images[0], &framebuffer_images[1],
    };

    for (std::size_t i = 0; i < framebuffer_images.size(); ++i)
        framebuffer_images[i].initialize(fb_layout, fb_memblock, i * fb_size);

    swapchain = dk::SwapchainMaker(device, nwindowGetDefault(), swapchain_images)
        .create();

    queue = dk::QueueMaker(device)
        .setFlags(DkQueueFlags_Graphics)
        .create();

    cmdbuf_memblock = dk::MemBlockMaker(device, CMDBUF_SIZE)
        .setFlags(DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached)
        .create();

    cmdbuf = dk::CmdBufMaker(device)
        .create();

    cmdbuf.addMemory(cmdbuf_memblock, 0, cmdbuf_memblock.getSize());

    for (std::size_t i = 0; i < framebuffer_images.size(); ++i) {
        auto view = dk::ImageView(framebuffer_images[i]);
        cmdbuf.bindRenderTargets(&view);
        bind_fb_cmdlists[i] = cmdbuf.finishList();
    }
}

void deko_gencmdlist(const nj::Surface &surf) {
    std::tie(image_memblock, image) = surf.to_deko3d(device, queue, DkImageFlags_HwCompression | DkImageFlags_Usage2DEngine);

    DkImageRect rect = {
        0, 0, 0,
        std::min(static_cast<std::uint32_t>(surf.width),  FB_WIDTH),
        std::min(static_cast<std::uint32_t>(surf.height), FB_HEIGHT),
        1,
    };

    // TODO: Use dkCmdBufCopyImage, which is not implemented yet
    for (std::size_t i = 0; i < framebuffer_images.size(); ++i) {
        cmdbuf.setViewports(0, DkViewport(0.0f, 0.0f, FB_WIDTH, FB_HEIGHT, 0.0f, 1.0f));
        cmdbuf.setScissors(0, DkScissor(0, 0, FB_WIDTH, FB_HEIGHT));
        cmdbuf.clearColor(0, DkColorMask_RGBA, 0.1f, 0.1f, 0.1f);
        cmdbuf.blitImage(dk::ImageView(image), rect, dk::ImageView(framebuffer_images[i]), rect, 0, 0);
        render_cmdlists[i] = cmdbuf.finishList();
    }
}

void deko_render() {
    auto slot = queue.acquireImage(swapchain);

    queue.submitCommands(bind_fb_cmdlists[slot]);
    queue.submitCommands(render_cmdlists[slot]);
    queue.presentImage(swapchain, slot);
}

void deko_exit() {
    queue.waitIdle();

    queue.destroy();
    cmdbuf.destroy();
    image_memblock.destroy();
    cmdbuf_memblock.destroy();
    fb_memblock.destroy();
    swapchain.destroy();
    device.destroy();
}

int main(int argc, char **argv) {
    // Call this before dkDeviceCreate
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

    nj::Surface surf(image.width, image.height, nj::PixelFormat::BGRA);
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

    deko_init();
    deko_gencmdlist(surf);

    PadState pad;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);

    while (appletMainLoop()) {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus)
            break;

        deko_render();
    }

    deko_exit();

    return 0;
}
