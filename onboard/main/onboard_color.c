#include "onboard_color.h"

// RGB565 packs five bits red, six green, five blue into each 16-bit pixel.
// A red/blue reversal changes only those two five-bit channels.
// This diagnostic is independent of the camera's input V4L2 pixel format.
static uint16_t swap_red_blue(uint16_t rgb565) {
    const uint16_t red = (rgb565 >> 11) & 0x1Fu;
    const uint16_t green = rgb565 & 0x07E0u;
    const uint16_t blue = rgb565 & 0x001Fu;
    return (uint16_t)((blue << 11) | green | red);
}

// RGB565X is a byte-order distinction, not the same as swapping red/blue.
static uint16_t swap_16bit_bytes(uint16_t word) {
    return (uint16_t)((word >> 8) | (word << 8));
}

const char *onboard_color_mode_name(onboard_color_mode_t mode) {
    switch (mode) {
    case ONBOARD_COLOR_ORIGINAL: return "ORIGINAL";
    case ONBOARD_COLOR_SWAP_RED_BLUE: return "SWAP RED/BLUE";
    case ONBOARD_COLOR_SWAP_BYTES: return "SWAP BYTES";
    case ONBOARD_COLOR_SWAP_BOTH: return "SWAP BOTH";
    default: return "INVALID";
    }
}

bool onboard_color_apply_rgb565(uint16_t *pixels, size_t pixel_count,
                               onboard_color_mode_t mode) {
    if (!pixels || mode < ONBOARD_COLOR_ORIGINAL ||
        mode >= ONBOARD_COLOR_MODE_COUNT) return false;
    if (mode == ONBOARD_COLOR_ORIGINAL) return true;

    const bool red_blue = (mode == ONBOARD_COLOR_SWAP_RED_BLUE ||
                           mode == ONBOARD_COLOR_SWAP_BOTH);
    const bool bytes = (mode == ONBOARD_COLOR_SWAP_BYTES ||
                         mode == ONBOARD_COLOR_SWAP_BOTH);
    for (size_t i = 0; i < pixel_count; ++i) {
        uint16_t pixel = pixels[i];
        if (red_blue) pixel = swap_red_blue(pixel);
        if (bytes) pixel = swap_16bit_bytes(pixel);
        pixels[i] = pixel;
    }
    return true;
}
