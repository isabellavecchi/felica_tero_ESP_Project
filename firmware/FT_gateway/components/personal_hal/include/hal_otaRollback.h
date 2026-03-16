#ifndef __PERSONAL_HAL_OTA_ROLLBACK_H__
#define __PERSONAL_HAL_OTA_ROLLBACK_H__

#include <stdint.h>

uint8_t hal_ota_isImageToBeValidated(uint8_t imageState);
void hal_ota_validFw_cancelRollBack(void);
void hal_ota_invalidFw_rollBack(void);

#endif//__PERSONAL_HAL_OTA_ROLLBACK_H__