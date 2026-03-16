/**
 * @file otaUpdate.c
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2025-06-06
 * 
 * @copyright Public Domain (or CC0 licensed, at your option.) 2025
 * 
 */

#include "esp_log.h"

#include "httpServer.h"
#include "otaUpdate.h"
#include "esp_timer.h"
#include "hal_system.h"

static const char TAG[] = "ota_update";

/// Firmware update status
int g_fw_update_status = OTA_UPDATE_PENDING;

/**
 * ESP32 timer configuration passed to esp_timer_create.
 */
const esp_timer_create_args_t fw_update_reset_args = {
		.callback = &ota_fw_update_reset_callback,
		.arg = NULL,
		.dispatch_method = ESP_TIMER_TASK,
		.name = "fw_update_reset"
};
esp_timer_handle_t fw_update_reset;

/**
 * Checks the g_fw_update_status and creates the fw_update_reset timer if g_fw_update_status is true.
 */
void ota_fw_update_reset_timer(void)
{
	if (g_fw_update_status == OTA_UPDATE_SUCCESSFUL)
	{
		ESP_LOGI(TAG, "ota_fw_update_reset_timer: FW updated successful starting FW update reset timer");
		// Give the web page a chance to receive an acknowledge back and initialize the timer
		ESP_ERROR_CHECK(esp_timer_create(&fw_update_reset_args, &fw_update_reset));
		ESP_ERROR_CHECK(esp_timer_start_once(fw_update_reset, 8000000));
	}
	else
	{
		ESP_LOGI(TAG, "ota_fw_update_reset_timer: FW update unsuccessful");
	}
}

/**
 * @brief 
 * 
 * @param arg 
 */
void ota_fw_update_reset_callback(void *arg)
{
	ESP_LOGI(TAG, "ota_fw_update_reset_callback: Timer timed-out, restarting the device");
	esp_restart();
}

/**
 * @brief Inicia OTA
 * 
 * @param partition 
 * @param handle 
 * @return esp_err_t ESP_OK
 */
esp_err_t ota_begin_partition(const esp_partition_t *partition, esp_ota_handle_t *handle) {
    esp_err_t err = esp_ota_begin(partition, OTA_SIZE_UNKNOWN, handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Error with OTA begin, cancelling OTA");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%lx", partition->subtype, partition->address);
    return ESP_OK;
}

/**
 * @brief Escreve dados OTA
 * 
 * @param handle 
 * @param data 
 * @param len 
 * @return esp_err_t 
 */
esp_err_t ota_write_data(esp_ota_handle_t handle, const char *data, int len) {
    esp_err_t err = esp_ota_write(handle, data, len);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "OTA write error");
    }
    return err;
}

/**
 * @brief Finaliza OTA e define partição de boot
 * 
 * @param handle 
 * @param partition 
 * @return true 
 * @return false 
 */
bool ota_finalize_and_set_boot(esp_ota_handle_t handle, const esp_partition_t *partition) {
    if (esp_ota_end(handle) == ESP_OK) {
        if (esp_ota_set_boot_partition(partition) == ESP_OK) {
            const esp_partition_t *boot_partition = esp_ota_get_boot_partition();
            ESP_LOGI(TAG, "Next boot partition subtype %d at offset 0x%lx", boot_partition->subtype, boot_partition->address);
            return true;
        } else {
            ESP_LOGI(TAG, "FLASHED ERROR!!!");
        }
    } else {
        ESP_LOGI(TAG, "esp_ota_end ERROR!!!");
    }
    return false;
}

/**
 * @brief Processa o primeiro bloco do corpo da requisição
 * 
 * @param ota_buff 
 * @param recv_len 
 * @param content_received 
 * @param ota_handle 
 * @param update_partition 
 * @return esp_err_t 
 */
esp_err_t ota_process_first_chunk(
    const char *ota_buff, int recv_len, int *content_received,
    esp_ota_handle_t *ota_handle, const esp_partition_t *update_partition)
{
    char *body_start_p = strstr(ota_buff, "\r\n\r\n");
    if (!body_start_p) {
        ESP_LOGI(TAG, "Invalid OTA HTTP body");
        return ESP_FAIL;
    }
    body_start_p += 4;
    int body_part_len = recv_len - (body_start_p - ota_buff);

    if (ota_begin_partition(update_partition, ota_handle) != ESP_OK)
        return ESP_FAIL;

    if (ota_write_data(*ota_handle, body_start_p, body_part_len) != ESP_OK)
        return ESP_FAIL;

    *content_received += body_part_len;
    return ESP_OK;
}

/**
 * @brief Processa os próximos blocos do corpo da requisição
 * 
 * @param ota_buff 
 * @param recv_len 
 * @param content_received 
 * @param ota_handle 
 * @return esp_err_t 
 */
esp_err_t ota_process_next_chunk(
    const char *ota_buff, int recv_len, int *content_received,
    esp_ota_handle_t ota_handle)
{
    if (ota_write_data(ota_handle, ota_buff, recv_len) != ESP_OK)
        return ESP_FAIL;
    *content_received += recv_len;
    return ESP_OK;
}

/**
 * @brief Atualiza status global de OTA
 * 
 * @param flash_successful 
 */
void ota_update_status(bool flash_successful) {
    if (flash_successful) {
        ESP_LOGI(TAG, "OTA_UPDATE_SUCCESSFUL");
        g_fw_update_status = OTA_UPDATE_SUCCESSFUL;
        ota_fw_update_reset_timer();
    } else {
        ESP_LOGI(TAG, "OTA_UPDATE_FAILED");
        g_fw_update_status = OTA_UPDATE_FAILED;
    }
}