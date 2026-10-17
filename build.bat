@echo off
REM build.bat — Windows build script (no make required)
REM Usage: build.bat [all|test|clean]

SET CC=gcc
SET CFLAGS=-Wall -Wextra -O2 -Iinclude

IF NOT EXIST obj mkdir obj

IF "%1"=="clean" (
    del /Q /F obj\*.o scheduler.exe test_runner.exe 2>NUL
    rmdir /S /Q obj 2>NUL
    echo Cleaned.
    GOTO :EOF
)

echo Compiling source files...
FOR %%f IN (src\process.c src\queue.c src\stats.c src\scheduler.c src\display.c) DO (
    echo   %%f
    %CC% %CFLAGS% -c %%f -o obj\%%~nf.o
    IF ERRORLEVEL 1 (echo Build failed at %%f & EXIT /B 1)
)

echo Compiling main...
%CC% %CFLAGS% -c src\main.c -o obj\main.o
IF ERRORLEVEL 1 (echo Build failed & EXIT /B 1)

echo Linking scheduler.exe...
%CC% obj\process.o obj\queue.o obj\stats.o obj\scheduler.o obj\display.o obj\main.o -o scheduler.exe
IF ERRORLEVEL 1 (echo Link failed & EXIT /B 1)

IF "%1"=="test" (
    echo Compiling test runner...
    %CC% %CFLAGS% tests\test_runner.c obj\process.o obj\queue.o obj\stats.o obj\scheduler.o obj\display.o -o test_runner.exe
    IF ERRORLEVEL 1 (echo Test build failed & EXIT /B 1)
    echo Running tests...
    test_runner.exe
    GOTO :EOF
)

echo.
echo Build successful! Run with:
echo   scheduler.exe --algo mlfq --procs 8 --ticks 120
echo   scheduler.exe --compare --procs 10
echo   scheduler.exe --help
