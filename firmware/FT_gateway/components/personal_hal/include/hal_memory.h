#pragma once

#include "esp_timer.h"
#include "esp_attr.h"
#include "stdio.h"
#include "nvs_flash.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_err.h"

typedef struct mem_address_saver_s
{
    char  * partition_name;
    void  * obj_addr;
    size_t  obj_syze;
    char  * namespace;
    char  * storage_key;
    bool    toSave;
} mem_address_saver_t;

void hal_memory_initSpecificPartition(mem_address_saver_t * strutcParams);

void hal_memory_setup_blob(mem_address_saver_t * strutcParams);
esp_err_t hal_memory_save_blob(mem_address_saver_t * strutcParams);
esp_err_t hal_memory_load_blob(mem_address_saver_t * strutcParams);

void hal_memory_setup_u8(mem_address_saver_t * strutcParams);
esp_err_t hal_memory_save_u8(mem_address_saver_t * strutcParams);
esp_err_t hal_memory_load_u8(mem_address_saver_t * strutcParams);

esp_err_t hal_memory_erase_key(mem_address_saver_t * strutcParams);
esp_err_t hal_memory_erase_allNamespaceKeys(mem_address_saver_t * strutcParams);
