#ifndef __ESP8266_H
#define __ESP8266_H

#include "stm32f1xx_hal.h"

/**
 * ESP8266 初始化（模块上电、切换Station模式）
 * @param rx_buf:    接收缓冲区（全局静态数组）
 * @param rx_buf_size: 缓冲区大小（建议512字节以上）
 */
void ESP8266_Init(uint8_t *rx_buf, uint16_t rx_buf_size);

/**
 * 连接WiFi热点
 * @param ssid:     WiFi名称
 * @param password: WiFi密码
 * @return 1=成功，0=失败
 */
uint8_t ESP8266_ConnectWiFi(const char *ssid, const char *password);

/**
 * 发送温湿度数据到TCP服务器
 * @param server_ip:   服务器IP地址
 * @param port:        服务器端口
 * @param temperature: 温度
 * @param humidity:    湿度
 * @return 1=发送成功，0=发送失败
 */
uint8_t ESP8266_SendData(const char *server_ip, uint16_t port,
                         float temperature, float humidity);

/**
 * 检查模块是否正常响应（发送AT并等OK）
 * @return 1=正常，0=无响应
 */
uint8_t ESP8266_ATTest(void);

/**
 * 联网实测：TCP连接公网服务器(www.baidu.com:80)并发HTTP GET请求
 * 收到 "HTTP/1.x 200" 响应 = 真正连上公网（同时验证了 DNS 域名解析）
 * @return 1=联网成功，0=失败
 */
uint8_t ESP8266_NetTest(void);

#endif
