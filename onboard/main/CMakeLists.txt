idf_component_register(
  SRCS
    "main.c" "onboard_capture.c" "onboard_capture_buffers.c" "onboard_view.c" "onboard_ppa.c" "onboard_stats.c" "onboard_color.c" "onboard_color_ui.c"
    "tab5_board_power.c" "tab5_power.c" "tab5_power_policy.c"
    "tab5_power_diag.c" "tab5_battery_monitor.c"
    "tab5_recovery_policy.c" "tab5_power_recovery.c" "tab5_power_widget.c"
    # Compiled storage primitives, intentionally NOT started from app_main.
    # No camera, battery, partition, or flash behavior changes in this review.
    "lasr_session_fs.c" "lasr_session_store.c" "lasr_transfer_guard.c" "lasr_sd_device.c"
  INCLUDE_DIRS "."
  REQUIRES esp_driver_ppa fatfs sdmmc
)