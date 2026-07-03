#ifndef __DRV_BMI088_HEATER_H
#define __DRV_BMI088_HEATER_H

#include "drv_bmi088.h"

#if defined(HAL_SPI_MODULE_ENABLED) && defined(HAL_GPIO_MODULE_ENABLED)

int8_t BMI088_HeaterInit(BMI088Instance *inst);
void BMI088HeaterStart(BMI088Instance *inst);
void BMI088_HeaterFaultCallback(void *owner);

#endif /* defined(BSP_SPI_MODULE_ENABLED) && defined(BSP_GPIO_MODULE_ENABLED) */

#endif /* __DRV_BMI088_HEATER_H */
