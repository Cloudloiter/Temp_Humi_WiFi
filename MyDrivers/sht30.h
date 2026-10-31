#ifndef __SHT30_H
#define __SHT30_H

#include "stm32f1xx_hal.h"

typedef struct {
    float temperature;  /* 温度，单位摄氏度 */
    float humidity;     /* 相对湿度，单位 %RH */
} SHT30_Data;

/**
 * 初始化 SHT30（等传感器上电稳定即可，无需额外配置）
 */
void SHT30_Init(void);

/**
 * 读取一次温湿度数据
 * @param data: 输出，存放温度和湿度
 * @return 1=成功，0=失败(C RC校验不通过或I2C通信失败)
 */
uint8_t SHT30_Read(SHT30_Data *data);

#endif
