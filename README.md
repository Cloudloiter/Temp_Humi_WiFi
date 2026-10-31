# Temp_Humi_WiFi — STM32 温湿度监测 + WiFi 上传项目

## 项目功能

基于 STM32F103C8T6 的温湿度监测系统：
- SHT30 传感器读取温度和湿度（I2C 总线，带 CRC 校验）
- 0.96 寸 OLED(SSD1306) 显示温湿度数据（I2C 总线）
- ESP8266(ESP-01S) 通过 WiFi 将数据 TCP 上传到电脑端接收程序

## 硬件平台

| 器件 | 说明 |
|------|------|
| MCU | STM32F103C8T6（Blue Pill 蓝板），HSE 8MHz x PLL9 = 72MHz |
| 传感器 | SHT30，I2C 地址 0x44 |
| 显示屏 | 0.96 寸 SSD1306 OLED，I2C 地址 0x3C |
| WiFi 模块 | ESP-01S（ESP8266），AT 指令模式，115200 baud |

## 引脚连接

| 外设 | STM32 引脚 | 说明 |
|------|-----------|------|
| SHT30 SCL | PB8 | I2C1_SCL（已重映射，原 PB6 损坏） |
| SHT30 SDA | PB9 | I2C1_SDA |
| OLED SCK | PB8 | 与 SHT30 并联 |
| OLED SDA | PB9 | 与 SHT30 并联 |
| ESP-01S TX | PA10 | USART1_RX |
| ESP-01S RX | PA9 | USART1_TX |
| ESP-01S VCC | 3.3V | |
| ESP-01S GND | GND | |
| ESP-01S EN | 3.3V | 必须接，否则不启动 |
| 板载 LED | PC13 | 低电平点亮，用于状态诊断 |

## 工程结构

```
Temp_Humi_WiFi/
├── Core/                    # STM32 HAL 框架代码
│   ├── Inc/                 # 头文件
│   └── Src/
│       ├── main.c           # 主程序（初始化 + 主循环）
│       ├── stm32f1xx_hal_msp.c  # 外设 MSP 初始化（GPIO/时钟/中断）
│       ├── stm32f1xx_it.c   # 中断服务函数
│       └── system_stm32f1xx.c  # 系统时钟配置
├── Drivers/                 # ST HAL 库 + CMSIS
├── MyDrivers/               # 自主编写的驱动
│   ├── oled.c / oled.h      # SSD1306 OLED 驱动（I2C 页寻址模式）
│   ├── oled_font.h          # 8x16 点阵字体（Lucida Console 渲染生成）
│   ├── sht30.c / sht30.h    # SHT30 温湿度驱动（含 CRC-8 校验）
│   └── esp8266.c / esp8266.h  # ESP8266 AT 指令驱动（中断接收）
├── MDK-ARM/                 # Keil5 工程
│   ├── Temp_Humi_WiFi.uvprojx  # Keil 工程文件（双击打开）
│   └── startup_stm32f103xb.s   # 启动文件（栈 4KB）
├── receiver/                # 电脑端数据接收程序
│   ├── receive_data.py      # Python TCP 服务器，监听 8080 端口
│   └── run_server.bat       # 双击启动接收端
├── Temp_Humi_WiFi.ioc       # STM32CubeMX 工程文件
└── README.md                # 本文件
```

## 如何编译

0. **先配置 WiFi（必做）**：本仓库不含真实 WiFi 密码（安全原因）。复制 `Core/Inc/wifi_real_template.h` 重命名为 `wifi_real.h`，填入你自己的 WiFi 名称、密码和接收端电脑 IP。该文件已被 `.gitignore` 排除，不会被上传
1. 用 Keil5 MDK-ARM 打开 `MDK-ARM/Temp_Humi_WiFi.uvprojx`
2. 按 F7 编译（需安装 STM32F1 HAL 库包）
3. 用 ST-Link 烧录到 STM32F103C8T6

## 如何运行

1. 电脑连接 WiFi（与 ESP8266 同一局域网）
2. 双击 `receiver/run_server.bat` 启动接收端（需 Python 3 环境）
3. 给 STM32 上电
4. 电脑端每 6 秒收到一条 JSON 格式温湿度数据：
   ```
   {"temp":25.5,"humidity":65.2}
   ```

## LED 状态编码（上电诊断）

| LED 表现 | 含义 |
|---------|------|
| 闪 1 次 | ESP8266 AT 通信正常 |
| 闪 2 次 | WiFi 连接成功 |
| 闪 3 次 | 公网联网实测通过（TCP 连上 baidu.com） |
| 持续快闪 | 某步骤失败 |
| 不闪 | 主循环正常运行中 |

## 关键技术点

1. **I2C 总线复用**：OLED 和 SHT30 并联在 PB8/PB9，靠设备地址区分
2. **I2C 错误恢复**：HAL 库 I2C 出错后卡在 BUSY 状态，代码中用 DeInit+Init 复位
3. **USART1 中断接收**：ESP8266 的 AT 响应通过中断逐字节接收，避免阻塞
4. **AT 指令驱动**：send_at_cmd() 发送指令并轮询检测响应关键字
5. **TCP 数据上传**：AT+CIPSTART 建连 → AT+CIPSEND 发数据 → AT+CIPCLOSE 关连
