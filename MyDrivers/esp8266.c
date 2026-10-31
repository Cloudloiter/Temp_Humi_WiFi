#include "esp8266.h"
#include <stdio.h>
#include <string.h>

extern UART_HandleTypeDef huart1;  /* CubeMX 生成的 USART1 句柄 */

static uint8_t *g_rx_buf      = NULL;  /* 用户提供的接收缓冲区 */
static uint16_t  g_rx_buf_size = 0;
static volatile uint16_t g_rx_index = 0;   /* 缓冲区写入位置 */

/* 单字节接收缓冲：HAL 库把收到的字节存到这里，回调再搬到 g_rx_buf。
 * 这样避免 HAL 自动存储和手动读 DR 冲突（旧代码的重复写入 bug）。 */
static uint8_t s_rx_byte;

/* USART1 接收中断回调 —— HAL 库每收到一个字节调用一次 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1 && g_rx_buf) {
        if (g_rx_index < g_rx_buf_size) {
            g_rx_buf[g_rx_index++] = s_rx_byte;  /* HAL 已存好，搬过去即可 */
        }
        /* 重新使能接收中断，等下一个字节 */
        HAL_UART_Receive_IT(&huart1, &s_rx_byte, 1);
    }
}

/* ---- 内部辅助：发 AT 命令并等响应 ---- */

/* 清空接收缓冲区，准备接收新响应 */
static void rx_clear(void)
{
    g_rx_index = 0;
    if (g_rx_buf) memset(g_rx_buf, 0, g_rx_buf_size);
}

/* 仅等待响应，不清空、不发送。用于 payload 发送后等 "SEND OK" 等场景。 */
static uint8_t wait_response(uint32_t wait_ms, const char *expect)
{
    if (expect == NULL) {
        HAL_Delay(wait_ms);
        return 1;
    }

    uint32_t start = HAL_GetTick();
    while ((HAL_GetTick() - start) < wait_ms) {
        if (strstr((char *)g_rx_buf, expect) != NULL) {
            return 1;
        }
        HAL_Delay(10);
    }
    return 0;
}

/* 发送 AT 命令并等待指定时间，检查响应里有没有关键字。
 * 改为循环检测：一旦收到期望关键字立即返回，避免固定死等或错过边界响应。
 * @param cmd    AT 命令字符串（含\r\n）
 * @param wait_ms 最大等待毫秒数
 * @param expect 期望在响应中出现的关键字（如 "OK"），传 NULL 只发不等
 * @return 1=找到关键字，0=未找到
 */
static uint8_t send_at_cmd(const char *cmd, uint32_t wait_ms, const char *expect)
{
    rx_clear();

    /* 发送命令 */
    HAL_UART_Transmit(&huart1, (uint8_t *)cmd, strlen(cmd), 1000);

    if (expect == NULL) {
        HAL_Delay(wait_ms);
        return 1;
    }

    /* 循环检测响应，匹配到关键字立即返回 */
    uint32_t start = HAL_GetTick();
    while ((HAL_GetTick() - start) < wait_ms) {
        if (strstr((char *)g_rx_buf, expect) != NULL) {
            return 1;
        }
        HAL_Delay(10);
    }
    return 0;
}

void ESP8266_Init(uint8_t *rx_buf, uint16_t rx_buf_size)
{
    g_rx_buf      = rx_buf;
    g_rx_buf_size = rx_buf_size;
    g_rx_index    = 0;

    memset(g_rx_buf, 0, g_rx_buf_size);

    /* 启动中断接收：HAL 把字节存到 s_rx_byte，回调搬到 g_rx_buf */
    HAL_UART_Receive_IT(&huart1, &s_rx_byte, 1);

    /* 关闭回显（减少响应数据量，便于解析） */
    send_at_cmd("ATE0\r\n", 500, "OK");

    /* 设置为 Station 模式 */
    send_at_cmd("AT+CWMODE=1\r\n", 500, "OK");

    /* 降低 WiFi 发射功率（默认82→20），大幅减少电流消耗，
     * 避免蓝板 AMS1117 供电不足导致 I2C 总线异常。
     * AT+RFPOWER=<tx_power>，范围0-82 */
    send_at_cmd("AT+RFPOWER=20\r\n", 500, "OK");
}

uint8_t ESP8266_ATTest(void)
{
    /* 发 AT，等 500ms，检查响应里有没有 OK */
    return send_at_cmd("AT\r\n", 500, "OK");
}

uint8_t ESP8266_ConnectWiFi(const char *ssid, const char *password)
{
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"\r\n", ssid, password);

    rx_clear();
    HAL_UART_Transmit(&huart1, (uint8_t *)cmd, strlen(cmd), 1000);

    /* 等WiFi连接，最多等 20 秒（ESP8266 连路由器较慢） */
    for (uint8_t i = 0; i < 20; i++) {
        HAL_Delay(1000);
        /* 连成功后 ESP8266 会返回 "WIFI CONNECTED" + "WIFI GOT IP" + "OK" */
        if (strstr((char *)g_rx_buf, "WIFI GOT IP") ||
            strstr((char *)g_rx_buf, "OK")) {
            return 1;
        }
        /* 如果返回 "FAIL" 或 "ERROR" 说明密码错误或找不到AP */
        if (strstr((char *)g_rx_buf, "FAIL") ||
            strstr((char *)g_rx_buf, "ERROR")) {
            return 0;
        }
    }
    return 0;  /* 超时 */
}

uint8_t ESP8266_SendData(const char *server_ip, uint16_t port,
                         float temperature, float humidity)
{
    char cmd[256];

    /* TCP连接。部分旧固件返回 "ALREADY CONNECTED"，也视为成功。 */
    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%d\r\n", server_ip, port);
    if (!send_at_cmd(cmd, 5000, "CONNECT") &&
        !send_at_cmd(cmd, 5000, "ALREADY CONNECTED")) {
        return 0;
    }

    /* 等 CONNECT 后的 OK 也到齐，避免后续 AT+CIPSEND 被串扰 */
    HAL_Delay(200);

    /* 构造JSON数据。注意：payload 末尾不带 \r\n，ESP8266 按长度发送即可 */
    char payload[128];
    snprintf(payload, sizeof(payload),
             "{\"temp\":%.1f,\"humidity\":%.1f}",
             temperature, humidity);
    uint16_t payload_len = strlen(payload);

    /* 告诉 ESP8266 要发多少字节，等待 ">" 提示符 */
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d\r\n", payload_len);
    if (!send_at_cmd(cmd, 2000, ">")) return 0;

    /* 发送数据 */
    HAL_UART_Transmit(&huart1, (uint8_t *)payload, payload_len, 1000);

    /* 等待 ESP8266 确认发送完成 "SEND OK"（不清空缓冲区） */
    if (!wait_response(3000, "SEND OK")) {
        /* 没等到 SEND OK 也尝试关闭连接 */
        send_at_cmd("AT+CIPCLOSE\r\n", 500, NULL);
        return 0;
    }

    /* 关闭连接 */
    send_at_cmd("AT+CIPCLOSE\r\n", 500, NULL);

    return 1;
}

uint8_t ESP8266_NetTest(void)
{
    char cmd[128];

    /* 1. TCP 连接百度首页：公网可达，域名解析同时验证了 DNS 功能 */
    rx_clear();
    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"www.baidu.com\",80\r\n");
    HAL_UART_Transmit(&huart1, (uint8_t *)cmd, strlen(cmd), 1000);

    uint8_t connected = 0;
    for (uint8_t i = 0; i < 12; i++) {      /* 最多等 6 秒 TCP 建立 */
        HAL_Delay(500);
        if (strstr((char *)g_rx_buf, "CONNECT")) { connected = 1; break; }
        if (strstr((char *)g_rx_buf, "ERROR") ||
            strstr((char *)g_rx_buf, "FAIL")) break;
    }
    if (!connected) return 0;

    /* 2. 构造 HTTP GET 请求 */
    const char *http_req =
        "GET / HTTP/1.1\r\n"
        "Host: www.baidu.com\r\n"
        "User-Agent: STM32\r\n"
        "Accept: */*\r\n"
        "Connection: close\r\n"
        "\r\n";
    uint16_t req_len = strlen(http_req);

    /* 3. 通知 ESP8266 要发送的字节数，等 ">" 提示符 */
    rx_clear();
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d\r\n", req_len);
    HAL_UART_Transmit(&huart1, (uint8_t *)cmd, strlen(cmd), 1000);

    uint8_t got_prompt = 0;
    for (uint8_t i = 0; i < 10; i++) {      /* 最多等 2 秒 */
        HAL_Delay(200);
        if (strstr((char *)g_rx_buf, ">")) { got_prompt = 1; break; }
    }
    if (!got_prompt) {
        send_at_cmd("AT+CIPCLOSE\r\n", 300, NULL);
        return 0;
    }

    /* 4. 发送 HTTP 数据，等服务器响应 */
    rx_clear();
    HAL_UART_Transmit(&huart1, (uint8_t *)http_req, req_len, 1000);

    uint8_t http_ok = 0;
    for (uint8_t i = 0; i < 10; i++) {      /* 最多等 5 秒 */
        HAL_Delay(500);
        if (strstr((char *)g_rx_buf, "HTTP/1.1 200") ||
            strstr((char *)g_rx_buf, "HTTP/1.0 200")) {
            http_ok = 1;
            break;
        }
        if (strstr((char *)g_rx_buf, "CLOSED") ||
            strstr((char *)g_rx_buf, "ERROR")) break;
    }

    send_at_cmd("AT+CIPCLOSE\r\n", 300, NULL);
    return http_ok;
}
