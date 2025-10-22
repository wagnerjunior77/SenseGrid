@echo off
setlocal EnableExtensions

rem ====== Descobre pastas do projeto ======
pushd "%~dp0\.."
set "ROOT=%CD%"
popd
set "TOOLCHAIN=%ROOT%\toolchain"
set "VENV=%TOOLCHAIN%\.venv"
set "IDF=%TOOLCHAIN%\esp-idf"

rem ====== Escolhe interprete Python de forma robusta ======
set "PY_CMD="
set "PY_ARGS="

if exist "%VENV%\Scripts\python.exe" (
  set "PY_CMD=%VENV%\Scripts\python.exe"
) else if exist "%TOOLCHAIN%\py311\python.exe" (
  set "PY_CMD=%TOOLCHAIN%\py311\python.exe"
) else (
  rem tenta o Python Launcher (py -3.11); NÃO use aspas aqui
  where py >NUL 2>NUL && (set "PY_CMD=py" & set "PY_ARGS=-3.11")
  if not defined PY_CMD (
    where python >NUL 2>NUL && set "PY_CMD=python"
  )
)

if not defined PY_CMD (
  echo ERRO: Nenhum Python encontrado. Rode o bootstrap primeiro.
  exit /b 1
)

rem ====== Prepara ambiente p/ ESP-IDF ======
if exist "%VENV%\Scripts" set "PATH=%VENV%\Scripts;%PATH%"
set "IDF_PATH=%IDF%"
set "IDF_PYTHON_ENV_PATH=%VENV%"
set "IDF_GITHUB_ASSETS=dl.espressif.com/github_assets"
set "PYTHONNOUSERSITE=1"

echo == Using ==
echo   ROOT=%ROOT%
echo   IDF_PATH=%IDF_PATH%
echo   IDF_PYTHON_ENV_PATH=%IDF_PYTHON_ENV_PATH%
echo   PY_CMD=%PY_CMD% %PY_ARGS%
echo.

rem ====== Gera (se preciso) a constraints oficial da 5.5 ======
set "CONS=%USERPROFILE%\.espressif\espidf.constraints.v5.5.txt"
if not exist "%CONS%" goto GEN_CONS
goto CONS_OK

:GEN_CONS
echo Gerando constraints (install-python-env)...
rem MUITO importante: --non-interactive ANTES do subcomando
%PY_CMD% %PY_ARGS% "%IDF%\tools\idf_tools.py" --non-interactive install-python-env
if exist "%CONS%" goto CONS_OK

echo Constraints ainda ausente; chamando install.bat (uma vez)...
call "%IDF%\install.bat" || goto ERROR
if not exist "%CONS%" (
  echo.
  echo ERRO: Nao consegui criar %CONS%
  echo Verifique rede/proxy/SSL p/ dl.espressif.com e github assets.
  goto ERROR
)

:CONS_OK
rem ====== Instala requirements travados pela constraints ======
set "REQ=%IDF%\tools\requirements\requirements.core.txt"
echo Instalando pacotes Python (pinned)...
%PY_CMD% %PY_ARGS% -m pip install --upgrade --force-reinstall -r "%REQ%" -c "%CONS%" || goto ERROR

rem ====== Exporta ambiente do IDF e compila ======
call "%IDF_PATH%\export.bat" || goto ERROR

cd /d "%ROOT%" || goto ERROR
%PY_CMD% %PY_ARGS% "%IDF_PATH%\tools\idf.py" set-target esp32s3 || goto ERROR
%PY_CMD% %PY_ARGS% "%IDF_PATH%\tools\idf.py" build || goto ERROR

echo.
echo Build OK.
exit /b 0

:ERROR
echo.
echo FAILED (exit %ERRORLEVEL%)
exit /b %ERRORLEVEL%
