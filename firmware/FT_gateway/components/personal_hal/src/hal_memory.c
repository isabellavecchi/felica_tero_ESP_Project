#include "hal_memory.h"

static const char TAG[] = "memory";


void hal_memory_initSpecificPartition(mem_address_saver_t * strutcParams)
{
    nvs_flash_init_partition(strutcParams->partition_name);
}


void hal_memory_setup_blob(mem_address_saver_t * strutcParams)
{
    ESP_LOGI(TAG, "LOADING...");
	if (ESP_OK != hal_memory_load_blob(strutcParams)) { //Nenhuma configuração encontrada, criar default
		ESP_ERROR_CHECK(hal_memory_save_blob(strutcParams));
		return;
	}
}


esp_err_t hal_memory_save_blob(mem_address_saver_t * strutcParams)
{
    ESP_LOGW(TAG, "SALVANDO...");
    nvs_handle_t handle;
    esp_err_t err = nvs_open_from_partition(
        strutcParams->partition_name,
        strutcParams->namespace,
        NVS_READWRITE,
        &handle
    );

    if (err != ESP_OK)
    {
        ESP_LOGE("NVS", "Erro ao abrir NVS[%llu]: %s", err, esp_err_to_name(err));
        return err;
    }

    err = nvs_set_blob( 
		handle,
		strutcParams->storage_key,
		strutcParams->obj_addr,
		strutcParams->obj_syze
	);

    if (err == ESP_OK)
    {
        err = nvs_commit(handle);
    }

    nvs_close(handle);
    return err;
}


esp_err_t hal_memory_load_blob(mem_address_saver_t * strutcParams)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open_from_partition(
        strutcParams->partition_name,
        strutcParams->namespace,
        NVS_READONLY,
        &handle
    
    );
    if (err != ESP_OK) {
        ESP_LOGE("NVS", "Erro ao abrir NVS: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI("NVS", "Acessou NVS com sucesso");

    err = nvs_get_blob(
		handle,
		strutcParams->storage_key,
		strutcParams->obj_addr,
		&(strutcParams->obj_syze)
	);

    nvs_close(handle);
    return err;
}




void hal_memory_setup_u8(mem_address_saver_t * strutcParams)
{
    ESP_LOGI(TAG, "LOADING...");
	if (ESP_OK != hal_memory_load_u8(strutcParams)) { //Nenhuma configuração encontrada, criar default
		ESP_ERROR_CHECK(hal_memory_save_u8(strutcParams));
		return;
	}
}


esp_err_t hal_memory_save_u8(mem_address_saver_t * strutcParams)
{
    ESP_LOGW(TAG, "SALVANDO...");
    nvs_handle_t handle;
    esp_err_t err = nvs_open_from_partition(
        strutcParams->partition_name,
        strutcParams->namespace,
        NVS_READWRITE,
        &handle
    );

    if (err != ESP_OK)
    {
        ESP_LOGE("NVS", "Erro ao abrir NVS[%llu]: %s", err, esp_err_to_name(err));
        return err;
    }

    err = nvs_set_u8(
		handle,
		strutcParams->storage_key,
		*(uint8_t*)
        (strutcParams->obj_addr)
	);

    if (err == ESP_OK)
    {
        err = nvs_commit(handle);
    }

    nvs_close(handle);
    return err;
}


esp_err_t hal_memory_load_u8(mem_address_saver_t * strutcParams)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open_from_partition(
        strutcParams->partition_name,
        strutcParams->namespace,
        NVS_READONLY,
        &handle
    
    );
    if (err != ESP_OK) {
        ESP_LOGE("NVS", "Erro ao abrir NVS: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI("NVS", "Acessou NVS com sucesso");

    err = nvs_get_u8( 
		handle,
		strutcParams->storage_key,
		strutcParams->obj_addr
	);

    nvs_close(handle);
    return err;
}


esp_err_t hal_memory_erase_key(mem_address_saver_t * strutcParams)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open_from_partition(
        strutcParams->partition_name,
        strutcParams->namespace,
        NVS_READWRITE,
        &handle
    );

    if (err != ESP_OK) return err;

    err = nvs_erase_key(handle, strutcParams->storage_key);
    if (err == ESP_OK)
        err = nvs_commit(handle);

    nvs_close(handle);
    return err;
}


esp_err_t hal_memory_erase_allNamespaceKeys(mem_address_saver_t * strutcParams)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open_from_partition(
        strutcParams->partition_name,
        strutcParams->namespace,
        NVS_READWRITE,
        &handle
    );

    if (err != ESP_OK) return err;

    err = nvs_erase_all(handle);
    if (err == ESP_OK)
        err = nvs_commit(handle);

    nvs_close(handle);
    return err;
}
