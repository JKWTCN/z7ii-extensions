@echo off
setlocal
if "%~1"=="" (
    echo Drag a merged .NEFX file or its folder onto this launcher.
    pause
    exit /b 2
)
set "PYTHONUTF8=1"
set "PYTHONDONTWRITEBYTECODE=1"
set "Z72_PYTHON=%LOCALAPPDATA%\Programs\Python\Python312\python.exe"
if exist "%Z72_PYTHON%" (
    "%Z72_PYTHON%" "%~dp0ibis_nefx_repair.py" %*
) else (
    py -3 "%~dp0ibis_nefx_repair.py" %*
)
set "Z72_RESULT=%ERRORLEVEL%"
pause
exit /b %Z72_RESULT%
