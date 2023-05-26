::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
::                          Sexy ObjDump Executor                               ::
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

@echo off


::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
::                               Change Here                                    ::
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

set ROOT=C:\ultra
set OUTFOLDER=%CD%\out\
set FILE=disassembly.txt


::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
::                          Color Function Setup                                ::
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

cls && color 07
for /F "tokens=1,2 delims=#" %%a in ('"prompt #$H#$E# & echo on & for %%b in (1) do rem"') do (set "DEL=%%a")


::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
::                            Find the .Out file                                ::
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

set CURD=%CD%
set DUMP=
cd %OUTFOLDER% > nul 2> nul
for %%f in (*.out) do (
    if "%%~xf"==".out" (
		set DUMP=%%f
	)
)
IF "%DUMP%"=="" (
    echo ERROR: .out file not found
    goto Finish
)

::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
::                     Dump the file to a text document                         ::
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

:CheckDirectories
if not exist %ROOT% (
    echo ERROR: Unable to find Ultra64 folder.
    goto Finish
)

set PATH=%ROOT%\GCC\MIPSE\BIN\
FOR %%i IN ("%DUMP%") DO (
    set DUMPNAME=%%~ni.out
)

copy /Y %DUMP% %PATH% >NUL
cd /d %PATH%
objdump --disassemble-all --source  --wide --all-header --line-numbers %DUMPNAME% > %FILE%
del %DUMPNAME%
copy /Y %FILE% %CURD% >NUL
del %FILE%

echo Dumped %FILE% to %CURD%

:Finish
pause