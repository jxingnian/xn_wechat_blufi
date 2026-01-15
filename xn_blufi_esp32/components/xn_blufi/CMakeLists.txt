# BluFi组件CMakeLists.txt

idf_component_register(
    SRCS "xn_blufi.c" "xn_wifi_manager.c" "xn_wifi_storage.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash esp_wifi esp_event bt
)
