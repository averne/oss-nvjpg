# oss-nvjpg

The Tegra X1 provides a hardware module dedicated to JPEG encoding/decoding (NVJPG).

Using this library, images can be rendered to an RGB or YUV (triplanar) surface. YUV&#10141;RGB conversion is handled in hardware. In addition, images can be downscaled to up to 8, also done in hardware.

Note: only baseline JPEGs are supported. Progressive and arithmetic coded files will return an error.

### Performance

Decoding times are largely faster than those obtained by software rendering, even with SIMD optimizations. The results below were obtained on an Aarch64 Cortex-A57 clocked at max 2.091GHz, by decoding the same image to an RGB surface, averaging 1000 iterations:
| Library | 70Kib 720x1080 | 1.6Mib 3200x1800 |
| --- |:---:|:---:|
| NVJPG | 2.036ms | 15.997ms |
| libjpeg-turbo | 6.127ms | 66.341ms |
| stb_image (with NEON routines) | 16.512ms | 158.474ms |
| Python-pillow | 11.688ms | 114.918ms |
| nanoJPEG | 48.299ms | 531.575ms |

## Building
```sh
meson build && meson compile -C build -j$(nproc)
```
(requires C++20 support)

## Credits
- The [Ryujinx](https://github.com/Ryujinx/Ryujinx) project for extensive documentation of the Host1x interface
