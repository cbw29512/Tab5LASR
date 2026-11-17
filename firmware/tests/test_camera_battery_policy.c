#include "../main/tab5_power_policy.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    // INA226 decoding must remain unchanged from the proven dice roller.
    assert(tab5_power_decode_ina226_mv(0x12, 0xC0) == 6000);
    assert(tab5_power_decode_ina226_shunt_uv(0xFF, 0xFE) == -5);

    // In the camera test, unattended low-voltage recovery is forbidden.
    // Charging is enabled only for an already independently plausible 2S pack.
    assert(!tab5_power_pack_present_mv(0));
    assert(!tab5_power_pack_present_mv(4000));
    assert(!tab5_power_pack_present_mv(5499));
    assert(tab5_power_pack_present_mv(5500));
    assert(tab5_power_pack_present_mv(7400));
    assert(tab5_power_pack_present_mv(8400));
    assert(tab5_power_pack_present_mv(8450));
    assert(!tab5_power_pack_present_mv(8451));
    assert(!tab5_power_pack_present_mv(8800));
    puts("Camera battery startup policy and INA226 conversion passed");
    return 0;
}
