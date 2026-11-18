# Web camera firmware size investigation

## Ground truth

Original installed ESP32-P4 factory partition starts at `0x10000` and is **1,048,576 bytes**. Adjacent `0x110000..0x210000` is not free: two current reads match one another and match the original dice firmware byte-for-byte, with **1,045,461 non-erased bytes**. Ownership unknown. **No partition migration or erasing approved.**

Successful compiled web candidate in GitHub run `44219184773` is **1,346,992 bytes**, so **298,416 bytes** over the real app partition. It was compiled in a *CI-only* 2MiB candidate configuration; **do not flash it**.

The archived size-components report (run `44224178107`) identifies significant flash contributors:

| Linked archive | Flash bytes | Notes |
|---|---:|---|
| `liblvgl__lvgl.a` | 304,119 | Camera canvas + local battery recovery touch UI; cannot remove recovery blindly |
| `libesp_stdio.a` | 162,571 | Per-archive attribution includes large linker-region padding: actual `stdio_vfs.c.obj` named symbols are only hundreds of bytes. **Not a genuine 163KB removable console feature.** |
| `liblwip.a` | 86,484 | Network TCP/IP |
| `libeh_proto_v2.a` | 80,862 | ESP-Hosted protocol |
| `libespressif__esp_video.a` | 57,888 | Onboard CSI video |
| `libesp_ipa_p4_less_v3.a` | 56,086 | Camera color/ISP |
| `libespressif__esp_ipa.a` | 42,151 | Camera ISP tuning |
| `libesp_http_server.a` | 11,426 | HTTP server |
| `libesp_driver_jpeg.a` | 8,775 | Hardware JPEG encoder |

Numbers are **per-archive contributions to the ELF flash sections**, not direct savings projections. Static linker elimination, alignment, and dependencies prevent simply subtracting them.

## Safe optimization experiments

1. **Minimal LVGL.** Keep `BUTTON`, `LABEL`, `CANVAS`, `IMAGE` widgets, touchscreen and **guarded battery recovery**; omit unused widgets in `sdkconfig.lvgl_slim_probe`. Build separately under `web-slim-probe.yml`; compare exact app bytes against 1,346,992 baseline.
2. **Stdio disproved as a 163KB saving.** CI [symbol probe run 44224906755](https://github.com/cbw29512/Tab5LASR/actions/runs/44224906755) showed the largest named `stdio_vfs.c.obj` symbols are about 166 bytes and 142 bytes, with most other functions tens of bytes. The size report's 162KB attribution is dominated by linker region/alignment overhead, not that source file's actual contents. Do **not** disable console logging expecting 163KB savings.
3. **Use supported LVGL widget switches, not `LV_CONF_MINIMAL`.** The managed LVGL revision explicitly rejects the removed `LV_CONF_MINIMAL` flag. Enable required `BUTTON`, `LABEL`, `CANVAS`, `IMAGE`, theme and touch, disable individual unused widgets; benchmark the real image size in the revised slim CI build.
4. **LTO candidate.** Try documented ESP-IDF `CONFIG_COMPILER_LTO_LINKTIME` and `CONFIG_COMPILER_LTO_COMPILETIME` only in CI. Preserve serial logs and charger state machine, and compare exact image size.
3. If still too large, create a smaller display backend only after proving equivalent battery recovery and touch behavior, or split streaming from the main ESP32-P4 app into an independently verified co-processor architecture. **Do not silently drop battery functionality or laser capture accuracy.**

## Acceptance gates (independent)

- App **must be <=1,048,576 bytes** if existing partition retained.
- Reuse known-good ESP32-P4 v1.3 image metadata, upright camera orientation, Color 2 (swap red/blue) as the user-selected display baseline; true camera color accuracy remains unverified.
- Keep charger P4/P5/P7 safety sequence, INA226 monitor, and two-tap timed recovery available.
- Onboard C6 ESP-Hosted factory firmware compatibility remains **unproven**, no automatic C6 reflash.
- JPEG pipeline reports actual frame freshness; never mark browser LIVE because only the CSI sensor is running.
- Battery-only operation with camera+Wi-Fi+browser verified **on physical hardware** before release.
- No full-device or partition-table writes without separate independent recovery review.

This is a size reduction exercise in CI only. Neither variant is approved for physical flashing.
