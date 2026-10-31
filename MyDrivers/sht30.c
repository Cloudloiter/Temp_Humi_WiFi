#include "sht30.h"

#define SHT30_ADDR  0x44    /* 7位I2C地址，HAL库自动左移1位 */
#define SHT30_CMD   0x2C06  /* 单次测量，高重复性，关闭时钟拉伸 */

extern I2C_HandleTypeDef hi2c1;  /* CubeMX 生成的 I2C1 句柄 */

/* CRC-8 校验多项式 x^8 + x^5 + x^4 + 1 */
static uint8_t crc8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0xFF;
    uint8_t i, j;
    for (i = 0; i < len; i++) {
        crc ^= data[i];
        for (j = 0; j < 8; j++) {
            crc = (crc & 0x80) ? ((crc << 1) ^ 0x31) : (crc << 1);
        }
    }
    return crc;
}

void SHT30_Init(void)
{
    /* SHT30 上电后自动进入空闲模式，只需等待稳定 */
    HAL_Delay(100);
}

uint8_t SHT30_Read(SHT30_Data *data)
{
    uint8_t cmd[2] = {SHT30_CMD >> 8, SHT30_CMD & 0xFF};
    uint8_t buf[6];

    /* 发送测量命令 */
    if (HAL_I2C_Master_Transmit(&hi2c1, SHT30_ADDR << 1, cmd, 2, 100) != HAL_OK) {
        return 0;
    }

    /* 等待测量完成（高重复性约15ms，留20ms余量） */
    HAL_Delay(20);

    /* 读取6字节：温度高、温度低、温度CRC、湿度高、湿度低、湿度CRC */
    if (HAL_I2C_Master_Receive(&hi2c1, SHT30_ADDR << 1, buf, 6, 100) != HAL_OK) {
        return 0;
    }

    /* CRC校验 —— 任何一位出错都丢弃本次读数 */
    if (crc8(buf, 2) != buf[2])     return 0;
    if (crc8(buf + 3, 2) != buf[5]) return 0;

    uint16_t raw_t = ((uint16_t)buf[0] << 8) | buf[1];
    uint16_t raw_h = ((uint16_t)buf[3] << 8) | buf[4];

    /* 官方公式 */
    data->temperature = -45.0f + 175.0f * (float)raw_t / 65535.0f;
    data->humidity    = 100.0f * (float)raw_h / 65535.0f;

    /* 裁剪到合理范围 */
    if (data->humidity > 100.0f) data->humidity = 100.0f;
    if (data->humidity < 0.0f)   data->humidity = 0.0f;

    return 1;
}
