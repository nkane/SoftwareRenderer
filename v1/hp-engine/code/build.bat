IF NOT EXIST ..\build mkdir ..\build
PUSHD ..\build

cl /Od /MTd /Zi /nologo ..\code\win32_hp.c /link user32.lib gdi32.lib

POPD
