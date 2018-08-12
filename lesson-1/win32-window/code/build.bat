:: Lesson:  1.1
:: File:    build.bat
IF NOT EXIST ..\build MKDIR ..\build
PUSHD ..\build

cl /Od /MTd /Zi /nologo ..\code\win32_main.c /link user32.lib

POPD
