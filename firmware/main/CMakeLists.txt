idf_component_register(
  SRCS
    "main.c" "camera_uvc.c" "camera_preview.c" "camera_metrics.c"
    "tab5_board_power.c" "tab5_power.c" "tab5_power_policy.c"
  INCLUDE_DIRS "."
  REQUIRES usb esp_lcd esp_timer
)
