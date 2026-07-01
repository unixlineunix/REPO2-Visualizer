@echo off
REM Build script for Chills Visualizer on Windows
REM Requires: MinGW-w64 (g++), GLFW 3.4 (compiled for MinGW), and optionally NSIS

set CXX=g++
set CXXFLAGS=-std=c++20 -I. -D_GLFW_WIN32
set LDFLAGS=-L. -lglfw3 -lopengl32 -lgdi32 -lwinmm -static-libgcc -static-libstdc++ -static

echo Compiling...
%CXX% %CXXFLAGS% -c main.cpp -o main_mingw.o
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
%CXX% %CXXFLAGS% -c modes.cpp -o modes_mingw.o
%CXX% %CXXFLAGS% -c audio.cpp -o audio_mingw.o
%CXX% %CXXFLAGS% -c fft.cpp -o fft_mingw.o
%CXX% %CXXFLAGS% -c shader.cpp -o shader_mingw.o
%CXX% %CXXFLAGS% -c vis_state.cpp -o vis_state_mingw.o
%CXX% %CXXFLAGS% -c glad.c -o glad_mingw.o

echo Linking...
%CXX% main_mingw.o modes_mingw.o audio_mingw.o fft_mingw.o shader_mingw.o vis_state_mingw.o glad_mingw.o -o visualizer.exe %LDFLAGS%
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo Build successful: visualizer.exe

REM Optional: build NSIS installer (requires NSIS installed)
if exist "C:\Program Files (x86)\NSIS\makensis.exe" (
    echo Building NSIS installer...
    "C:\Program Files (x86)\NSIS\makensis.exe" installer.nsi
    if %ERRORLEVEL% equ 0 echo Installer built: Chills_Visualizer_Setup.exe
)
