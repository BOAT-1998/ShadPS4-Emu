#include "common/types.h"

#include <algorithm>
#include <cstring>

namespace VideoCore::AMD {

namespace {
// PS4 uses 8x8 micro-tiles for its linear <-> tiled swizzle.
constexpr u32 kMicroTileDim = 8;
}

// Bit-interleave x/y for an 8x8 tile (Morton/Z-order).
constexpr u32 MortonCode2(u32 x, u32 y) {
    x = (x | (x << 8)) & 0x00FF00FF;
    x = (x | (x << 4)) & 0x0F0F0F0F;
    x = (x | (x << 2)) & 0x33333333;
    x = (x | (x << 1)) & 0x55555555;

    y = (y | (y << 8)) & 0x00FF00FF;
    y = (y | (y << 4)) & 0x0F0F0F0F;
    y = (y | (y << 2)) & 0x33333333;
    y = (y | (y << 1)) & 0x55555555;

    return x | (y << 1);
}

// Compute byte offset for a pixel inside the PS4 8x8 tiled layout.
u64 GetTiledOffset(u32 x, u32 y, u32 width, u32 bytes_per_pixel) {
    const u32 tile_x = x / kMicroTileDim;
    const u32 tile_y = y / kMicroTileDim;
    const u32 pixel_x = x % kMicroTileDim;
    const u32 pixel_y = y % kMicroTileDim;

    const u32 tiles_per_row =
        std::max<u32>(1, (width + (kMicroTileDim - 1)) / kMicroTileDim);
    const u64 macro_offset =
        static_cast<u64>(tile_y * tiles_per_row + tile_x) * kMicroTileDim * kMicroTileDim;
    const u64 micro_offset = MortonCode2(pixel_x, pixel_y);
    return (macro_offset + micro_offset) * bytes_per_pixel;
}

// Compute byte offset for a pixel in a linear surface with the provided pitch.
u64 GetLinearOffset(u32 x, u32 y, u32 pitch_bytes, u32 bytes_per_pixel) {
    return static_cast<u64>(y) * pitch_bytes + static_cast<u64>(x) * bytes_per_pixel;
}

// Swizzle a linear surface into the PS4 8x8 tiled layout.
void SwizzleLinearToTiled(const u8* src, u8* dst, u32 width, u32 height,
                          u32 pitch_bytes, u32 bytes_per_pixel) {
    for (u32 y = 0; y < height; ++y) {
        for (u32 x = 0; x < width; ++x) {
            const u64 src_offset = GetLinearOffset(x, y, pitch_bytes, bytes_per_pixel);
            const u64 dst_offset = GetTiledOffset(x, y, width, bytes_per_pixel);
            std::memcpy(dst + dst_offset, src + src_offset, bytes_per_pixel);
        }
    }
}

// Deswizzle a tiled surface back to linear.
void SwizzleTiledToLinear(const u8* src, u8* dst, u32 width, u32 height,
                          u32 pitch_bytes, u32 bytes_per_pixel) {
    for (u32 y = 0; y < height; ++y) {
        for (u32 x = 0; x < width; ++x) {
            const u64 src_offset = GetTiledOffset(x, y, width, bytes_per_pixel);
            const u64 dst_offset = GetLinearOffset(x, y, pitch_bytes, bytes_per_pixel);
            std::memcpy(dst + dst_offset, src + src_offset, bytes_per_pixel);
        }
    }
}

} // namespace VideoCore::AMD
