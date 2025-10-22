@echo off
setlocal enableextensions

rem Resolve caminho absoluto da raiz do repo (pasta pai de \scripts)
for %%I in ("%~dp0..") do set "ROOT=%%~fI"

set "IDF_PATH=%ROOT%\toolchain\esp-idf"
set "IDF_PYTHON_ENV_PATH=%ROOT%\toolchain\.venv"

if not exist "%IDF_PATH%\export.bat" (
  echo [ERRO] Nao achei "%IDF_PATH%\export.bat"
  exit /b 1
)
if not exist "%IDF_PYTHON_ENV_PATH%\Scripts\python.exe" (
  echo [ERRO] Nao achei "%IDF_PYTHON_ENV_PATH%\Scripts\python.exe"
  exit /b 1
)

call "%IDF_PATH%\export.bat" || exit /b 1

pushd "%ROOT%"
"%IDF_PYTHON_ENV_PATH%\Scripts\python.exe" "%IDF_PATH%\tools\idf.py" set-target esp32s3 || (popd & exit /b 1)
"%IDF_PYTHON_ENV_PATH%\Scripts\python.exe" "%IDF_PATH%\tools\idf.py" build
set "ERR=%ERRORLEVEL%"
popd
exit /b %ERR%
