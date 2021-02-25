::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
::                          Sexy ObjDump Executor                               ::
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

@echo off


::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
::                               Change Here                                    ::
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

set ROOT=C:\ultra
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

cd out
for %%f in (*.out) do (
    	if "%%~xf"==".out" (
		set DUMP=%%f
	)
)


::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
::                     Dump the file to a text document                         ::
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

:CheckDirectories
if not exist %ROOT% (
    echo ERROR: Unable to find Ultra64 folder.
    goto Finish
)

move %DUMP% %ROOT%\GCC\MIPSE\BIN
cd %ROOT%\GCC\MIPSE\BIN
objdump --disassemble-all --source  --wide --all-header --line-numbers %DUMP% >%FILE%
move disassembly.txt %CURD%
move %DUMP% %CURD%\out\bin

echo Dumped %FILE% to %CURD%

pause