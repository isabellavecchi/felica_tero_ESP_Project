idf_component_register(
    SRC_DIRS "src"
    INCLUDE_DIRS "include"
    REQUIRES    driver
                nvs_flash
                esp_system
                esp_timer
                app_update
)