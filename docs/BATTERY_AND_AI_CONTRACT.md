# Battery continuity contract — Tab5 camera firmware

## Objective

The Tab5 LASR camera must remain portable on its removable 2-cell NP-F550 pack. **Battery-only operation is not certified by a successful ESP-IDF build**; physical unplug/reboot and USB-camera load tests are mandatory.

## Existing source and data schema

The prior dice-roller firmware (see `cbw29512/nothingbutadiceroller`, feature branch, `tab5/BATTERY_RECOVERY_2026-11-17.md`) restored USB-to-battery operation by preserving factory power-latch and charger controls. The camera firmware already carries over:
- Second PI4IOE5V6408 I/O expander at `0x44`; power-off P4 idle LOW (never pulse high)
- IP2326 charging: P7 HIGH means charging commanded on; P5 HIGH disables quick charging
- INA226 battery bus sensor `0x41`; displayed bus voltage is not the same as independently measured battery pack voltage or charging current

**Battery reading schema:** `tab5_power_reading_t { voltage_valid: bool, battery_mv: uint32, shunt_valid: bool, shunt_uv: int32, error: esp_err_t }`, reused from the dice-roller source.

**Battery operating state:** `INITIALIZING → VERIFIED_CHARGE_COMMAND | USB_ONLY_FAULT`. Monitor samples must not re-enable charging from a fault. `CHG CMD ON` is only a command flag, **not proof of charge current**.

## Startup/monitor logic

1. Initialize Tab5 display/shared I²C.
2. Set P4 factory power-off idle LOW; abort camera setup if unsuccessful.
3. Read INA226; only authorize charging if startup battery bus indicates a reasonable pack, subject to 8.45 V ceiling. Never attempt an unattended low-voltage recovery.
4. Start camera preview / USB host even if battery readings are unavailable; log that operation is USB-only/unverified.
5. A read-only telemetry task reports measured battery bus/shunt/charger command periodically. If measured voltage exceeds 8.45 V, command charging OFF and log the safety fault.
6. **Still missing**: the dice-roller's two-tap, time-limited, charger-OFF-verified battery recovery user interface. Do not declare an unattended battery-portable release until this interface is ported and physically tested.

## Physical release acceptance

- M5Stack Tab5 ESP32-P4 v1.3 starts and has visible camera frames on USB-C
- INA226 voltage/shunt and CHG CMD visible in logs; no unexpected P4/P5/P7 writes
- Normal healthy battery powers the entire camera preview with USB-C disconnected
- Camera draws and USB camera receives frames on battery **without brownouts**
- Cold-start while unplugged and reconnect/disconnect USB-C safely
- Battery recovery button and its guarded 30-second behavior restored before claiming production readiness
- Report actual battery/runtime and FPS measurements; never infer them from CPU frequency or camera spec

## AI phase

On-device ML using Espressif ESP-DL/ESP-VISION is feasible for paper target recognition and tracking; laser bright-point detection should be a deterministic low-latency camera path. Keep all ML optional and separate from the primary frame sampling path to avoid missed laser transitions. Benchmark CPU, RAM, FPS, power and thermals on physical hardware.
