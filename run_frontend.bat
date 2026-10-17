@echo off
echo ========================================================
echo   Smart CPU Scheduler // Web Visualizer Live Server
echo ========================================================
echo.
echo Starting local web server on port 8765...
start "" http://localhost:8765
python -m http.server 8765 --directory frontend
pause
