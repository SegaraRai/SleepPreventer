@ECHO OFF

RMDIR /S /Q Dist
MKDIR Dist

COPY Win32\Release\SleepPreventer.exe Dist\SleepPreventer-x86.exe
COPY x64\Release\SleepPreventer.exe Dist\SleepPreventer-x64.exe
