/*
 * Program:         Software Renderer
 * File:            win32_main.c
 * Lesson:          1.1
 * Description:     example of creating a basic window in win32.
 *
 */

#include <windows.h>
#include <stdint.h>

#define global_variable         static

typedef int8_t      int8;
typedef int16_t     int16;
typedef int32_t     int32;
typedef int64_t     int64;

typedef uint8_t     uint8;
typedef uint16_t    uint16;
typedef uint32_t    uint32;
typedef uint64_t    uint64;

global_variable uint8 GlobalRunning;
global_variable HWND GlobalWindowHandle;

LRESULT CALLBACK
Win32MainWindowCallback(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = 0;
    switch (message)
    {
        case WM_CLOSE:
        {
            GlobalRunning = 0;
        } break;

        case WM_ACTIVATEAPP:
        {
            OutputDebugStringA("WM_ACTIVEAPP\n");
        } break;

        case WM_PAINT:
        {
            PAINTSTRUCT paintStruct;
            HDC deviceContext = BeginPaint(windowHandle, &paintStruct);
            EndPaint(windowHandle, &paintStruct);
        } break;

        default:
        {
            result = DefWindowProcA(windowHandle, message, wParam, lParam);
        } break;
    }
    return result;
}

int WINAPI
WinMain(HINSTANCE handleInstance, HINSTANCE handlePreviousInstance, LPSTR longPointerCommandLine, int numberCommmandShow)
{
    WNDCLASSA windowClass = { 0 };

    windowClass.style               = (CS_HREDRAW | CS_VREDRAW | CS_OWNDC);
    windowClass.lpfnWndProc         = Win32MainWindowCallback;
    windowClass.hInstance           = handleInstance;
    windowClass.hCursor             = LoadCursor(NULL, IDC_ARROW);
    windowClass.lpszClassName       = "WindowClass";

    if (RegisterClassA(&windowClass))
    {
        GlobalWindowHandle = CreateWindowExA(0,
                                          windowClass.lpszClassName,
                                          "Software Renderer - Lesson 1.1",
                                          WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                          CW_USEDEFAULT,
                                          CW_USEDEFAULT,
                                          800,
                                          600,
                                          0,
                                          0,
                                          handleInstance,
                                          0);
        if (GlobalWindowHandle)
        {
            GlobalRunning = 1;

            while (GlobalRunning)
            {
                MSG message;
                while (PeekMessage(&message, 0, 0, 0, PM_REMOVE))
                {
                    if (message.message == WM_QUIT)
                    {
                        GlobalRunning = 0;
                    }
                    TranslateMessage(&message);
                    DispatchMessage(&message);
                }
            }
        }
    }
    return 0;
}
