@echo off
chcp 65001 >nul
echo 正在启动 STM32 温湿度接收端...
"C:\Users\32165\.workbuddy\binaries\python\versions\3.13.12\python.exe" "%~dp0receive_data.py"
pause
