#ifndef __OLED_H
#define __OLED_H

#include "stm32f1xx_hal.h"

#define OLED_WIDTH   128
#define OLED_HEIGHT   64
#define OLED_PAGES    (OLED_HEIGHT / 8)  /* 纵向8个像素一页 */

/* 列偏移：SH1106(1.3寸)显存132列需偏移2；SSD1306(0.96寸)为128列无偏移，填0 */
#define OLED_COLUMN_OFFSET  0

/* 软件180度旋转开关。
 * 1.3寸屏玻璃物理旋转了180度需要开启；0.96寸SSD1306方向正常填0即可。 */
#define OLED_ROTATE_180     0

/**
 * 初始化OLED（发送SSD1306初始化序列，清屏并开启显示）
 */
void OLED_Init(void);

/**
 * 清除显存缓冲区。需要在调用其他绘制函数后调用 OLED_Update() 才会生效
 */
void OLED_Clear(void);

/**
 * 将显存缓冲区刷新到OLED屏幕
 */
void OLED_Update(void);

/**
 * 在指定位置显示字符串（8x16字体，支持 ASCII 32-127）
 * @param x: 列坐标 0-127
 * @param y: 行坐标 0-7 （页坐标，0=顶部，7=底部）
 * @param str: 要显示的字符串
 */
void OLED_ShowString(uint8_t x, uint8_t y, const char *str);

/**
 * 显示温湿度数据（居中大字格式）
 * @param temp: 温度值
 * @param humi: 湿度值
 */
void OLED_ShowData(float temp, float humi);

/**
 * 在OLED中央显示一行文字（无参数版本，用于初始化提示）
 */
void OLED_ShowCenter(const char *line1, const char *line2);

#endif
