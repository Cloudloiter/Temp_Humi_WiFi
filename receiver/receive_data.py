# -*- coding: utf-8 -*-
"""
STM32 温湿度数据接收端
电脑上运行此程序，接收 ESP8266 通过 TCP 发来的温湿度数据。
运行方式：双击 run_server.bat（或命令行执行 python receive_data.py）
按 Ctrl+C 退出。
"""
import socket
import time

HOST = "0.0.0.0"   # 监听所有网卡（本机IP: 192.168.101.117）
PORT = 8080         # 端口，必须和 STM32 代码里的 PC_PORT 一致

def main():
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind((HOST, PORT))
    srv.listen(5)
    print("=" * 52)
    print("  STM32 温湿度接收端已启动")
    print("  监听端口: %d" % PORT)
    print("  等待数据... (Ctrl+C 退出)")
    print("=" * 52)

    while True:
        conn, addr = srv.accept()
        print("\n[连接] 来自 %s:%d" % (addr[0], addr[1]))
        data = b""
        try:
            conn.settimeout(3)
            while True:
                chunk = conn.recv(1024)
                if not chunk:
                    break
                data += chunk
        except socket.timeout:
            pass
        conn.close()

        text = data.decode("utf-8", errors="ignore").strip()
        now = time.strftime("%H:%M:%S")
        if text:
            print("[%s] 收到 %d 字节: %s" % (now, len(data), text))
            # 尝试解析 JSON 并友好显示
            try:
                import json
                d = json.loads(text)
                t = d.get("temp")
                h = d.get("humidity")
                if t is not None and h is not None:
                    print("      -> 温度: %.1f C    湿度: %.1f %%" % (t, h))
            except Exception:
                pass
        else:
            print("[%s] 连接未发送数据 (收到 0 字节)" % now)

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n已退出")
    except Exception as e:
        print("出错: %s" % e)
        print("提示: 若报端口被占用，请关闭占用该端口的程序或改 PORT 端口号")
        input("按回车退出...")
