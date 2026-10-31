/* ============================================
 * wifi_real_template.h —— 配置模板
 *
 * 这个仓库不含真实 WiFi 密码（安全原因）。
 * 想编译运行本项目：
 *   1. 复制本文件，重命名为 wifi_real.h
 *   2. 把下面的占位符改成你自己的 WiFi 名、密码、电脑 IP
 *   3. wifi_real.h 已被 .gitignore 排除，不会被上传
 * ============================================ */
#ifndef WIFI_REAL_H
#define WIFI_REAL_H

#define WIFI_SSID      "YOUR_WIFI_NAME"
#define WIFI_PASSWORD  "YOUR_WIFI_PASSWORD"

#define PC_IP          "192.168.1.100"   /* 接收端电脑的局域网 IP */
#define PC_PORT        8080

#endif
