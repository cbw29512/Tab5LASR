#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "../main/onboard_color.h"

static void check_reversible_modes(void) {
    const uint16_t reference[] = {
        0xF800u, // pure RGB565 red
        0x07E0u, // pure RGB565 green
        0x001Fu, // pure RGB565 blue
        0xFFFFu, // white
        0x0000u, // black
        0x6B4Du  // arbitrary mixed color
    };
    for (int mode = 0; mode < ONBOARD_COLOR_MODE_COUNT; ++mode) {
        uint16_t buffer[sizeof(reference) / sizeof(reference[0])];
        memcpy(buffer, reference, sizeof(buffer));
        assert(onboard_color_apply_rgb565(
            buffer, sizeof(buffer) / sizeof(buffer[0]), (onboard_color_mode_t)mode));
        assert(onboard_color_apply_rgb565(
            buffer, sizeof(buffer) / sizeof(buffer[0]), (onboard_color_mode_t)mode));
        assert(memcmp(buffer, reference, sizeof(reference)) == 0);
    }
}

int main(void) {
    uint16_t colors[] = {0xF800u, 0x07E0u, 0x001Fu};
    assert(!onboard_color_apply_rgb565(NULL, 3, ONBOARD_COLOR_ORIGINAL));
    assert(!onboard_color_apply_rgb565(colors, 3, ONBOARD_COLOR_MODE_COUNT));
    assert(onboard_color_apply_rgb565(colors, 3, ONBOARD_COLOR_SWAP_RED_BLUE));
    assert(colors[0] == 0x001Fu && colors[1] == 0x07E0u && colors[2] == 0xF800u);
    assert(strcmp(onboard_color_mode_name(ONBOARD_COLOR_ORIGINAL), "ORIGINAL") == 0);
    check_reversible_modes();
    return 0;
}
