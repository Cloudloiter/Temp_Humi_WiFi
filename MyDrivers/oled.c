#include "oled.h"
#include "oled_font.h"
#include <string.h>
#include <stdio.h>

#define OLED_ADDR  0x3C     /* SSD1306 7位I2C地址 */
#define OLED_CMD   0x00     /* 命令字节前缀（Co=0, D/C#=0） */
#define OLED_DATA  0x40     /* 数据字节前缀（Co=0, D/C#=1） */

extern I2C_HandleTypeDef hi2c1;

static uint8_t oled_buf[OLED_WIDTH * OLED_PAGES];  /* 1024字节显存 */

/* I2C 底层写函数，带返回值检查和错误恢复 */
static HAL_StatusTypeDef oled_i2c_tx(const uint8_t *data, uint16_t len, uint32_t timeout)
{
    HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(&hi2c1, OLED_ADDR << 1, (uint8_t *)data, len, timeout);
    if (status != HAL_OK) {
        /* HAL I2C 一旦出错容易卡在 BUSY，这里做外设复位 */
        HAL_I2C_DeInit(&hi2c1);
        HAL_I2C_Init(&hi2c1);
        HAL_Delay(5);
        /* 再试一次 */
        status = HAL_I2C_Master_Transmit(&hi2c1, OLED_ADDR << 1, (uint8_t *)data, len, timeout);
    }
    return status;
}

/* 发送一条命令 */
static void oled_write_cmd(uint8_t cmd)
{
    uint8_t buf[2] = {OLED_CMD, cmd};
    oled_i2c_tx(buf, 2, 100);
}

/* 发送一批命令（合并为一次I2C传输，提高效率） */
static void oled_write_cmds(const uint8_t *cmds, uint8_t len)
{
    uint8_t buf[16];  /* 命令序列不会太长 */
    if (len == 0 || len > 15) return;

    buf[0] = OLED_CMD;
    memcpy(buf + 1, cmds, len);
    oled_i2c_tx(buf, len + 1, 100);
}

/* 发送一批数据 */
static void oled_write_data(const uint8_t *data, uint16_t len)
{
    /* 每次最多发128字节，分批发，每批前面加 OLED_DATA 控制字节 */
    while (len > 0) {
        uint8_t chunk = (len > 128) ? 128 : (uint8_t)len;
        uint8_t tmp[129];
        tmp[0] = OLED_DATA;
        memcpy(tmp + 1, data, chunk);
        oled_i2c_tx(tmp, chunk + 1, 200);
        data += chunk;
        len  -= chunk;
    }
}

/* SSD1306 初始化序列 — 使用页寻址模式，与 OLED_Update 的页地址设置匹配 */
static const uint8_t oled_init_cmds[] = {
    0xAE,           /* 关闭显示 */
    0x20, 0x02,     /* 页寻址模式（Page Addressing Mode） */
    0xB0,           /* 页起始地址：page 0 */
    0xC8,           /* COM扫描方向：从COM[N-1]到COM0（该屏需翻转上下） */
    0x00,           /* 低列起始地址 */
    0x10,           /* 高列起始地址 */
    0x40,           /* 显示起始行地址 */
    0x81, 0xCF,     /* 对比度 */
    0xA0,           /* 段重映射：column 0 = SEG0（正常方向） */
    0xA6,           /* 正常显示（非反色） */
    0xA8, 0x3F,     /* 多路复用比：64 */
    0xA4,           /* 输出跟随RAM内容 */
    0xD3, 0x00,     /* 显示偏移 */
    0xD5, 0x80,     /* 显示时钟分频/振荡器频率 */
    0xD9, 0xF1,     /* 预充电周期 */
    0xDA, 0x12,     /* COM引脚硬件配置 */
    0xDB, 0x40,     /* VCOMH 电压 */
    0x8D, 0x14,     /* 电荷泵使能 */
    0xAF            /* 开启显示 */
};

void OLED_Init(void)
{
    HAL_Delay(100);  /* 等OLED上电稳定 */

    /* 发送初始化序列 */
    for (uint8_t i = 0; i < sizeof(oled_init_cmds); i++) {
        oled_write_cmd(oled_init_cmds[i]);
    }

    OLED_Clear();
    OLED_Update();
}

void OLED_Clear(void)
{
    memset(oled_buf, 0x00, sizeof(oled_buf));
}

void OLED_Update(void)
{
    for (uint8_t page = 0; page < OLED_PAGES; page++) {
#if OLED_ROTATE_180
        /* 软件180度旋转：把RAM页page的内容送到屏幕页(7-page)，
         * 并且数据左右镜像+位反转(上下镜像)，抵消玻璃物理旋转180度 */
        uint8_t screen_page = OLED_PAGES - 1 - page;
#else
        uint8_t screen_page = page;
#endif
        const uint8_t cmds[3] = {
            (uint8_t)(0xB0 + screen_page),                         /* 设置页地址 */
            (uint8_t)(0x00 + (OLED_COLUMN_OFFSET & 0x0F)),         /* 低列起始地址 + 偏移 */
            (uint8_t)(0x10 + ((OLED_COLUMN_OFFSET >> 4) & 0x0F))   /* 高列起始地址 + 偏移 */
        };
        oled_write_cmds(cmds, 3);

#if OLED_ROTATE_180
        uint8_t reversed[OLED_WIDTH];
        const uint8_t *src = oled_buf + page * OLED_WIDTH;
        for (uint16_t i = 0; i < OLED_WIDTH; i++) {
            uint8_t b = src[OLED_WIDTH - 1 - i];
            b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
            b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
            b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
            reversed[i] = b;
        }
        oled_write_data(reversed, OLED_WIDTH);
#else
        oled_write_data(oled_buf + page * OLED_WIDTH, OLED_WIDTH);
#endif
    }
}

/* 在(x,y)画一个8x16的字符，x是像素列(0-127)，y是页(0-7)
 * 8x16 字符占 2 页，字体数组布局：font8x16[ch][0..7]=上半(8列,每列8像素)
 *                                font8x16[ch][8..15]=下半(8列,每列8像素)
 * 每列(1字节)写到 oled_buf 中同一 page 内连续的 8 个字节。 */
static void oled_draw_char(uint8_t x, uint8_t y, char ch)
{
    if (ch < 32 || ch > 126) ch = ' ';  /* 不支持的字符显示空格 */
    if (x > OLED_WIDTH - 8) return;
    if (y > OLED_PAGES - 2) return;     /* 8x16 占 2 页，y 最大到 OLED_PAGES-2 */

    uint16_t idx = (uint16_t)y * OLED_WIDTH + x;  /* 起始位置：page y, column x */
    uint8_t  ci  = (uint8_t)ch - 32;

    /* 上半部分：8 列写进 page y 的 8 个连续字节 */
    for (uint8_t col = 0; col < 8; col++) {
        oled_buf[idx + col] = font8x16[ci][col];
    }
    /* 下半部分：移到下一页（不是下 8 页！），同样写 8 个连续字节 */
    idx += OLED_WIDTH;
    for (uint8_t col = 0; col < 8; col++) {
        oled_buf[idx + col] = font8x16[ci][col + 8];
    }
}

void OLED_ShowString(uint8_t x, uint8_t y, const char *str)
{
    while (*str) {
        oled_draw_char(x, y, *str);
        x += 8;
        if (x >= OLED_WIDTH) break;
        str++;
    }
}

void OLED_ShowData(float temp, float humi)
{
    OLED_Clear();
    char buf[20];

    /* 2行布局：Temp在上，Hum在下，中间留页间隙，避免跨页边界撕裂 */
    sprintf(buf, "Temp: %.1f C", temp);
    OLED_ShowString(2, 1, buf);

    sprintf(buf, "Hum : %.1f %%", humi);
    OLED_ShowString(2, 4, buf);

    OLED_Update();
}

void OLED_ShowCenter(const char *line1, const char *line2)
{
    OLED_Clear();

    if (line1) {
        uint8_t x = (OLED_WIDTH - (uint8_t)strlen(line1) * 8) / 2;
        OLED_ShowString(x, 2, line1);
    }
    if (line2) {
        uint8_t x = (OLED_WIDTH - (uint8_t)strlen(line2) * 8) / 2;
        OLED_ShowString(x, 4, line2);
    }

    OLED_Update();
}
