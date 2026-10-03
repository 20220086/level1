@echo off
chcp 65001 >nul
rem ==== raylib 설치 경로를 본인 PC에 맞게 수정하세요 ====
set RAYLIB=C:\raylib\raylib-6.0_win64_mingw-w64

rem 공식 릴리스 zip(include, lib 폴더) 또는 raylib 설치 프로그램(src 폴더) 모두 지원
set RL_INC=%RAYLIB%\include
set RL_LIB=%RAYLIB%\lib
if not exist "%RL_INC%\raylib.h" set RL_INC=%RAYLIB%\src
if not exist "%RL_LIB%\libraylib.a" set RL_LIB=%RAYLIB%\src

echo [1/2] Building maze.exe ...
g++ src\main.cpp src\input.cpp src\logic.cpp src\pathfind.cpp src\fileio.cpp src\view.cpp -o maze.exe -std=c++17 -O2 -Wall -I"%RL_INC%" -L"%RL_LIB%" -lraylib -lopengl32 -lgdi32 -lwinmm
if errorlevel 1 goto fail

echo [2/2] Building logic_test.exe ...
g++ tests\logic_test.cpp src\logic.cpp src\pathfind.cpp src\fileio.cpp -Isrc -std=c++17 -o logic_test.exe
if errorlevel 1 goto fail

echo BUILD OK. Run: maze.exe    Logic test: logic_test.exe
exit /b 0

:fail
echo BUILD FAILED
pause
exit /b 1
