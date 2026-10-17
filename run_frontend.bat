@echo off
echo ========================================================
echo   Smart CPU Scheduler // C Backend & Web Visualizer
echo ========================================================
echo.

IF NOT EXIST "scheduler.exe" (
    echo [!] scheduler.exe not found. Compiling with MinGW gcc...
    gcc -O2 -Wall -Wextra -Iinclude src/process.c src/queue.c src/stats.c src/scheduler.c src/display.c src/main.c -o scheduler.exe
    if errorlevel 1 (
        echo [X] Compilation failed. Please ensure gcc is installed.
        pause
        exit /b 1
    )
    echo [+] Compilation successful!
)

echo Starting backend API server and opening visualizer...
start "" http://localhost:8765
python backend_server.py
pause
