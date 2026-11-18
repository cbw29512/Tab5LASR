set(lasr_web_sources "")
set(lasr_web_components "")
set(lasr_embedded_files "")
if(CONFIG_LASR_WEB_ENABLED)
  list(APPEND lasr_web_sources
    "lasr_web_frame.c" "lasr_web_wifi.c" "lasr_web_http.c" "lasr_web_assets.c")
  list(APPEND lasr_web_components
    esp_driver_jpeg esp_http_server esp_wifi esp_netif nvs_flash)
  list(APPEND lasr_embedded_files
    "../../web/index.html" "../../web/app.js"
    "../../web/model.js" "../../web/styles.css")
endif()

idf_component_register(
  SRCS
    "main.c" "onboard_capture.c" "onboard_capture_buffers.c" "onboard_view.c" "onboard_ppa.c" "onboard_stats.c" "onboard_color.c" "onboard_color_ui.c"
    "tab5_board_power.c" "tab5_power.c" "tab5_power_policy.c"
    "tab5_power_diag.c" "tab5_battery_monitor.c"
    "tab5_recovery_policy.c" "tab5_power_recovery.c" "tab5_power_widget.c"
    ${lasr_web_sources}
  INCLUDE_DIRS "."
  REQUIRES esp_driver_ppa ${lasr_web_components}
  EMBED_TXTFILES ${lasr_embedded_files}
)
