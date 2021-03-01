@echo off
echo Compiling UNFLoader Example ROM 3


:Setup
set ROOT=C:\ultra
set ALLOWMOVE=0
set MOVEFOLDER=Z:\
set FORCEMOVE=0
set ROMREGISTER=C:\n64roms\nrdc.exe
set ALLOWREGISTER=0

:Don't touch
set gccdir=%ROOT%\gcc
set PATH=%ROOT%\gcc\mipse\bin;%ROOT%\usr\sbin;C:\WINDOWS\system32;
set gccsw=-mips3 -mgp32 -mfp32 -funsigned-char -D_LANGUAGE_C -D_ULTRA64 -D__EXTENSIONS__
set n64align=on
set DEBUG_MODE=1
goto CheckDirectories


:CheckDirectories
if not exist %ROOT% (
    echo ERROR: Unable to find Ultra64 folder.
    goto Finish
)
if not exist %MOVEFOLDER% (
    echo WARNING: Unable to find 64Drive folder.
    echo.
    set ALLOWMOVE=0
)
if not exist %ROMREGISTER% (
    echo WARNING: Unable to find NRDC.exe.
    echo.
    set ALLOWREGISTER=0
)
goto MoveFromOut


:MoveFromOut
if not exist out goto make
cd out >nul 2>&1
move *.o .. >nul 2>&1
move *.out .. >nul 2>&1
move *.n64 .. >nul 2>&1
move *.cvt .. >nul 2>&1
cd..
goto Make


:Make
make
set MAKEERROR=%errorlevel%
echo.
goto ErrorCheck


:ErrorCheck
if "%MAKEERROR%"=="2" (
    echo An error occurred during compilation.
)
goto MoveToOut


:MoveToOut
md out >nul 2>&1
move *.o out >nul 2>&1
move *.out out >nul 2>&1
move *.n64 out >nul 2>&1
move *.cvt out >nul 2>&1
if "%MAKEERROR%"=="0" (
	echo Project compiled sucessfully!
    if "%ALLOWMOVE%"=="1" goto MoveTo64DrivePoll
)
goto Finish


:MoveTo64DrivePoll
if "%FORCEMOVE%"=="1" goto MoveTo64Drive
set /P PROMPT=Move the ROM to the 64Drive Folder[Y/N]?
if /I "%PROMPT%" EQU "N" goto Finish 
if /I "%PROMPT%" EQU "Y" goto MoveTo64Drive
goto MoveTo64DrivePoll


:MoveTo64Drive
cd out
for /R "%~dp0out" %%f in (*.n64) do copy /y "%%f" "%MOVEFOLDER%" >nul
if "%FORCEMOVE%"=="0" (
	echo Done! 
) else (
	echo Moving ROM to the 64Drive folder
)
goto Finish


:Finish
echo.
pause
exit