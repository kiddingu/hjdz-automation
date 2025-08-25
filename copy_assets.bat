@echo off
chcp 936 > nul
xcopy /E /I /Y "%~dp0游戏图片" "%~dp0..\build\Desktop_Qt_5_15_19_MSVC2019_64bit-Debug\游戏图片\"