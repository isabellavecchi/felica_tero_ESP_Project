#include "hal_otaRollback.h"

#include "esp_ota_ops.h"
// #include "esp_flash_partitions.h"
// #include "esp_partition.h"

uint8_t hal_ota_isImageToBeValidated(uint8_t otaImageState)
{
    return (    (otaImageState == ESP_OTA_IMG_NEW)
            ||  (otaImageState == ESP_OTA_IMG_PENDING_VERIFY)
        );
}


void hal_ota_validFw_cancelRollBack(void)
{
    esp_ota_mark_app_valid_cancel_rollback();
}


void hal_ota_invalidFw_rollBack(void)
{
    esp_ota_mark_app_invalid_rollback_and_reboot();
}