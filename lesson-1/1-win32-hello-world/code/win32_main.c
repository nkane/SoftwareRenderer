/*
 * Program:         Software Renderer
 * File:            win32_main.c
 * Lesson:          1.0
 * Description:     example of creating a basic program in win32.
 *
 */

#include <windows.h>

int WINAPI
WinMain(HINSTANCE handleInstance, HINSTANCE handlePreviousInstance, LPSTR longPointerCommandLine, int numberCommmandShow)
{
    MessageBox(NULL, "Hello, World!", "Hello, World", 0);
    return 0;
}
